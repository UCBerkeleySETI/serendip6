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
    
    int prior_alfa_enabled=-1;      // initial value - should change very fast
    int run_always;                 // 1 = run even if alfa_enabled is 0

    size_t num_coarse_chan = 0;

    //                         0           1
    const char *alfa_state[2] = {"disabled", "enabled"};

    int file_num_start = -1;
    hashpipe_status_lock_safe(&st);
    hgeti4(st.buf, "RUNALWYS", &run_always);
    hashpipe_status_unlock_safe(&st);
    if(file_num_start == -1) file_num_start = 0;
    init_etfits(&etf, file_num_start+1);

    /* Main loop */
    int i, rv, debug=20;
    int block_idx=0;
    int error_count, max_error_count = 0;
    float error, max_error = 0.0;

    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "OUTBLKIN", block_idx);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);

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
            hashpipe_error(__FUNCTION__, "error error returned from get_obs_info_from_redis()");
            pthread_exit(NULL);
        }
        //scram.alfa_enabled = 1;  // TODO remove once get_obs_info_from_redis() is working
        scram.coarse_chan_id = db->block[block_idx].header.coarse_chan_id;

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, "ALFASTAT", alfa_state[scram.alfa_enabled]);
        hputi4(st.buf, "COARCHID", scram.coarse_chan_id);
        // TODO lots more scram to go here
        hashpipe_status_unlock_safe(&st);
    
        if(scram.alfa_enabled != prior_alfa_enabled) {
            fprintf(stderr, "alfa_enabled has changed from %d to %d\n", prior_alfa_enabled, scram.alfa_enabled);
            prior_alfa_enabled = scram.alfa_enabled;
        }

        // write hits and metadata to etFITS file only if alfa is enabled
        // alfa_enabled might be a second or so out of sync with data
        if(scram.alfa_enabled || run_always) {
            etf.file_chan = scram.coarse_chan_id;
              if(num_coarse_chan != db->block[block_idx].header.num_coarse_chan) {
                  etf.new_file = 1; 
                  num_coarse_chan = db->block[block_idx].header.num_coarse_chan; 
            }
            rv = write_etfits(db, block_idx, &etf, scram_p);
            if(rv) {
                hashpipe_error(__FUNCTION__, "error error returned from write_etfits()");
                pthread_exit(NULL);
            }
        }

        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "FILENUM", etf.file_num);
        hputr4(st.buf, "OUTMXERR", max_error);
        hputi4(st.buf, "OUTERCNT", error_count);
        hputi4(st.buf, "OUTMXECT", max_error_count);
        hashpipe_status_unlock_safe(&st);

        // Mark block as free
        memset((void *)&db->block[block_idx], 0, sizeof(s6_output_block_t));    // TODO re-init first
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
