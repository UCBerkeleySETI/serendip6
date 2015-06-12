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

#include <sched.h>

#include "hashpipe.h"
#include "s6_databuf.h"
#include "s6_obs_data.h"
#include "s6_obs_data_gbt.h"
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
#ifdef SOURCE_S6
    scram_t   scram;
    scram_t * scram_p = &scram;
    int prior_receiver = 0;    
#elif SOURCE_DIBAS
    gbtstatus_t gbtstatus;
    gbtstatus_t * gbtstatus_p = &gbtstatus;
    char *prior_receiver = (char *)malloc(32);
    strcpy(prior_receiver,"");
#endif    

    int run_always, prior_run_always=0;                 // 1 = run even if no receiver

    size_t num_coarse_chan = 0;

    extern const char *receiver[];

    char hostname[200];

#if 0
    // raise this thread to maximum scheduling priority
    struct sched_param SchedParam;
    int retval;
    SchedParam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    fprintf(stderr, "Setting scheduling priority to %d\n", SchedParam.sched_priority);
    retval = sched_setscheduler(0, SCHED_FIFO, &SchedParam);
    if(retval) {
        perror("sched_setscheduler :");
    }
#endif

    // Initialization for etfits file output.
    hashpipe_status_lock_safe(&st);
    hgets(st.buf, "BINDHOST", 80, hostname);
    hashpipe_status_unlock_safe(&st);
    strcpy(etf.hostname, strtok(hostname, "."));    // just use the host portion
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
#ifdef SOURCE_S6
        rv = get_obs_info_from_redis(scram_p, (char *)"redishost", 6379);
#elif SOURCE_DIBAS
        rv = get_obs_gbt_info_from_redis(gbtstatus_p, (char *)"redishost", 6379);
#endif
rv=0;
        if(rv) {
            hashpipe_error(__FUNCTION__, "error returned from get_obs_info_from_redis()");
            pthread_exit(NULL);
        }

#ifdef SOURCE_S6
        scram.coarse_chan_id = db->block[block_idx].header.coarse_chan_id;
#elif SOURCE_DIBAS
        gbtstatus.coarse_chan_id = db->block[block_idx].header.coarse_chan_id;
#endif
        hashpipe_status_lock_safe(&st);
#ifdef SOURCE_S6
        hputs(st.buf,  "TELESCOP", receiver[scram.receiver]);
        hputi4(st.buf, "COARCHID", scram.coarse_chan_id);
        hputi4(st.buf, "CLOCKFRQ", scram.CLOCKFRQ);
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
        hputi4(st.buf, "SCRIF2SR", scram.IF2SIGSR);
#elif SOURCE_DIBAS
// TODO - put other GBT status items to status shmem?
        hputs(st.buf,  "TELESCOP", gbtstatus.RECEIVER);
        hputi4(st.buf, "COARCHID", gbtstatus.coarse_chan_id);
        // hputi4(st.buf, "CLOCKFRQ", gbtstatus.CLOCKFRQ);
        hputi4(st.buf, "GBTCLKFQ", gbtstatus.CLOCKFRQ);
        // hputr4(st.buf, "AZACTUAL", gbtstatus.AZACTUAL);
        hputr4(st.buf, "GBTAZACT", gbtstatus.AZACTUAL);
        // hputr4(st.buf, "ELACTUAL", gbtstatus.ELACTUAL);
        hputr4(st.buf, "GBTELACT", gbtstatus.ELACTUAL);
        // hputr4(st.buf, "RA_DEG", gbtstatus.RADG_DRV);
        hputr4(st.buf, "GBTRADG", gbtstatus.RADG_DRV);
        // hputr4(st.buf, "DEC_DEG", gbtstatus.DEC_DRV);
        hputr4(st.buf, "GBTDECDG", gbtstatus.DEC_DRV);
        // hputr4(st.buf, "IFFRQ1ST", gbtstatus.IFFRQ1ST);
        hputr4(st.buf, "GBTIFFQ1", gbtstatus.IFFRQ1ST);
        // hputr4(st.buf, "FREQ", gbtstatus.FREQ);
        hputr4(st.buf, "GBTFREQ", gbtstatus.FREQ);
#endif
        hashpipe_status_unlock_safe(&st);

        // test for and handle file change events

#ifdef SOURCE_S6
        if(scram.receiver  != prior_receiver    ||
#elif SOURCE_DIBAS
        if(strcmp(gbtstatus.RECEIVER,prior_receiver) != 0 ||
#endif
            run_always      != prior_run_always  ||
            num_coarse_chan != db->block[block_idx].header.num_coarse_chan)  {

#if 0
            uint32_t total_missed_pkts[N_BEAM_SLOTS];
            char missed_key[9] = "MISSPKBX";
            const char missed_beam[7] = {'0', '1', '2', '3', '4', '5', '6'};
            for(int i=0; i < N_BEAMS; i++) {
                missed_key[7] = missed_beam[i];
                hgetu4(st.buf, missed_key, &total_missed_pkts[i]);
                hputu4(st.buf, missed_key, 0);
            }

            hashpipe_info(__FUNCTION__, "Missed packet totals : beam0 %lu beam1 %lu beam2 %lu beam3 %lu beam4 %lu beam5 %lu beam6 %lu \n",
                         total_missed_pkts[0], total_missed_pkts[1], total_missed_pkts[2], total_missed_pkts[3], 
                         total_missed_pkts[4], total_missed_pkts[5], total_missed_pkts[6]);
#endif

            hashpipe_info(__FUNCTION__, "Initializing output for %ld coarse channels, using receiver %s\n",
#ifdef SOURCE_S6
                          num_coarse_chan, receiver[scram.receiver]);
#elif SOURCE_DIBAS
                          num_coarse_chan, gbtstatus.RECEIVER);
#endif

            // change files
            if(etf.file_open) {
                etfits_close(&etf);     
            }
            etf.new_file = 1; 
            // re-init
#ifdef SOURCE_S6
            prior_receiver   = scram.receiver;
#elif SOURCE_DIBAS
            strcpy(prior_receiver,gbtstatus.RECEIVER);
#endif
            num_coarse_chan  = db->block[block_idx].header.num_coarse_chan; 
            prior_run_always = run_always;
        }

#ifdef SOURCE_S6
        // write hits and metadata to etFITS file only if there is a receiver
        // in focus or the run_always flag in on
        if(scram.receiver || run_always) {
#elif SOURCE_DIBAS
// TODO - put GBT acquisition trigger logic, if any, here
        if(run_always) {
#endif
#ifdef SOURCE_S6
            etf.file_chan = scram.coarse_chan_id;          
            rv = write_etfits(db, block_idx, &etf, scram_p);
#elif SOURCE_DIBAS
            etf.file_chan = gbtstatus.coarse_chan_id;           // TODO - sensible for GBT?
            rv = write_etfits_gbt(db, block_idx, &etf, gbtstatus_p);
#endif
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
    if(etf.file_open) {
        etfits_close(&etf);     // final close
    }
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
