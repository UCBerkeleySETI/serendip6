/*
 * s6_output_thread.c
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <hiredis/hiredis.h>

#include <cuda.h>
#include <cufft.h>
#include <s6GPU.h>

#include "hashpipe.h"
#include "s6_databuf.h"
#include "s6_obs_data.h"
#include "s6_etfits.h"


static int init(hashpipe_thread_args_t *args)
{
    hashpipe_status_t st = args->st;

    hashpipe_status_lock_safe(&st);
    //hputr4(st.buf, "CGOMXERR", 0.0);
    //hputi4(st.buf, "CGOERCNT", 0);
    //hputi4(st.buf, "CGOMXECT", 0);
    hashpipe_status_unlock_safe(&st);

    // Success!
    return 0;
}

static void *run(hashpipe_thread_args_t * args)
{
    // Local aliases to shorten access to args fields
    // Our input buffer happens to be a s6_ouput_databuf
    s6_output_databuf_t *db = (s6_output_databuf_t *)args->ibuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

    etfits_t  etf;
    scram_t   scram;
    scram_t * scram_p = &scram;
    
    int prior_receiver = 0;    
    int run_always, prior_run_always=0;                 // 1 = run even if no receiver

    size_t num_coarse_chan = 0;

    extern const char *receiver[];

    int file_num_start = -1;
    if(file_num_start == -1) file_num_start = 0;
    init_etfits(&etf, file_num_start+1);

    int i, rv, debug=20;
    int block_idx=0;
    int error_count, max_error_count = 0;
    float error, max_error = 0.0;

    /* Main loop */
    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "OUTBLKIN", block_idx);
        hputs(st.buf, status_key, "waiting");
        hgeti4(st.buf, "RUNALWYS", &run_always);
        hashpipe_status_unlock_safe(&st);

        // The run always mechanism allows data taking
        // even if there is no receiver in focus.
        if(run_always == 0 && prior_run_always != 0) {        
            // run over - close file
            etfits_close(&etf);     
        }
        if(run_always == 1 && prior_run_always == 0) {        
            // new run - open new file
            etf.new_file = 1; 
        }
        prior_run_always = run_always;

       // get new data
       while ((rv=s6_output_databuf_wait_filled(db, block_idx))
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
                pthread_exit(NULL);
                break;
            }
        }

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "processing");
        hashpipe_status_unlock_safe(&st);

        // TODO check mcnt

        // get scram, etc data
        rv = get_obs_info_from_redis(scram_p, (char *)"redishost", 6379);
        if(rv) {
            hashpipe_error(__FUNCTION__, "error returned from get_obs_info_from_redis()");
            pthread_exit(NULL);
        }

        scram.coarse_chan_id = db->block[block_idx].header.coarse_chan_id;

        hashpipe_status_lock_safe(&st);
        hputs(st.buf,  "TELESCOP", receiver[scram.receiver]);
        hputi4(st.buf, "COARCHID", scram.coarse_chan_id);
        hputi4(st.buf, "SCRALFON", scram.IF2ALFON);
        hputr4(st.buf, "SCRAGCAZ", scram.AGCAZ);
        hputr4(st.buf, "SCRAGCZA", scram.AGCZA);
        hputr4(st.buf, "SCRB0RA",  scram.ra_by_beam[0]);
        hputr4(st.buf, "SCRB0DEC", scram.dec_by_beam[0]);
        hputr4(st.buf, "SCRALFMO", scram.ALFMOPOS);
        hputr4(st.buf, "SCRIF1LO", scram.IF1SYNHZ);
        hputr4(st.buf, "SCRIF2LO", scram.IF2SYNHZ);
        hputr4(st.buf, "SCRIF1RF", scram.IF1RFFRQ);
        //hputr4(st.buf, "SCRIF1IF", scram.IF1IFFRQ);   // I don't think we use this - take out?
        hputr4(st.buf, "SCRTTDEG", scram.TTTURDEG);
        hputi4(st.buf, "SCRBIAS1", scram.ALFBIAS1);
        hputi4(st.buf, "SCRBIAS2", scram.ALFBIAS2);
        hputi4(st.buf, "SCRALFFB", scram.IF1ALFFB);
        hputi4(st.buf, "SCRALFON", scram.IF2ALFON);
        hashpipe_status_unlock_safe(&st);
    
        // write hits and metadata to etFITS file only if alfa is enabled
        // alfa_enabled might be a second or so out of sync with data
        if(scram.receiver || run_always) {
            etf.file_chan = scram.coarse_chan_id;
            // start new file on important parameter change (incl startup)
            if(num_coarse_chan != db->block[block_idx].header.num_coarse_chan ||
               scram.receiver  != prior_receiver) {
                etf.new_file = 1; 
                num_coarse_chan = db->block[block_idx].header.num_coarse_chan; 
                prior_receiver  = scram.receiver;
                // TODO timestamped log messages should be functionalized
                char timebuf[256];
                time_t now;
                time(&now);
                ctime_r(&now, timebuf);
                timebuf[strlen(timebuf)-1] = '\0';  // strip the newline
                fprintf(stderr, "%s : Initializing output for %ld coarse channels, using receiver %s\n",
                        timebuf, num_coarse_chan, receiver[scram.receiver]); 
            }
            rv = write_etfits(db, block_idx, &etf, scram_p);
            if(rv) {
                hashpipe_error(__FUNCTION__, "error error returned from write_etfits()");
                pthread_exit(NULL);
            }
        }

        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "FILENUM", etf.file_num);
        // TODO error counts not yet implemented
        hputr4(st.buf, "OUTMXERR", max_error);
        hputi4(st.buf, "OUTERCNT", error_count);
        hputi4(st.buf, "OUTMXECT", max_error_count);
        hashpipe_status_unlock_safe(&st);

        // Mark block as free
        memset((void *)&db->block[block_idx], 0, sizeof(s6_output_block_t));   
        s6_output_databuf_set_free(db, block_idx);

        // Setup for next block
        block_idx = (block_idx + 1) % db->header.n_block;    

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    // Thread success!
    etfits_close(&etf);     // final close
    return NULL;
}

static hashpipe_thread_desc_t output_thread = {
    name: "s6_output_thread",
    skey: "OUTSTAT",
    init: init,
    run:  run,
    ibuf_desc: {s6_output_databuf_create},
    obuf_desc: {NULL}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&output_thread);
}
