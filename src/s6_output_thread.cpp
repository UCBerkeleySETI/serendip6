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

#define SET_BIT(val, bitIndex) val |= (1 << bitIndex)
#define CLEAR_BIT(val, bitIndex) val &= ~(1 << bitIndex)
#define BIT_IS_SET(val, bitIndex) (val & (1 << bitIndex))

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
#elif SOURCE_FAST
// TODO all of the SOURCE_FAST sections are copies of SOURCE_DIBAS sections as place holders.
    gbtstatus_t gbtstatus;
    gbtstatus_t * gbtstatus_p = &gbtstatus;
    char *prior_receiver = (char *)malloc(32);
    strcpy(prior_receiver,"");
#endif    

    int run_always, prior_run_always=0;                 // 1 = run even if no receiver

    int idle=0;                                         // 1 = idle output, 0 = good to go
    uint32_t idle_flag=0;                               // bit field for data driven idle conditions    
    int testmode=0;                                     // 1 = write output file regardless of other
                                                        //   flags and do not attempt to obtain observatory
                                                        //   status data (in fact, write zeros to the obs
                                                        //   status structure).  0 = operate normally and
                                                        //   respect other flags.

    // data driven idle bit indexes
    int idle_redis_error = 1; 
    int idle_zero_IFV1BW = 2; 

    size_t num_coarse_chan = 0;

    extern const char *receiver[];

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

    int i, rv=0, debug=20;
    int block_idx=0;
    int error_count, max_error_count = 0;
    float error, max_error = 0.0;

    char current_filename[200] = "\0";  // init as a null string

    init_etfits(&etf);                  // init for ETFITS output 

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

        // get metadata
        hgeti4(st.buf, "IDLE", &idle);
        hgeti4(st.buf, "TESTMODE", &testmode);
        if(!testmode) {
#ifdef SOURCE_S6
            rv = get_obs_info_from_redis(scram_p, (char *)"redishost", 6379);
#elif SOURCE_DIBAS
            rv = get_obs_gbt_info_from_redis(gbtstatus_p, (char *)"redishost", 6379);
#endif
        } else {
#ifdef SOURCE_S6
            memset((void *)scram_p, 0, sizeof(scram_t));            // test mode - zero entire scram
#elif SOURCE_DIBAS
            memset((void *)gbtstatus_p, 0, sizeof(gbtstatus_t));    // test mode - zero entire gbtstatus
            gbtstatus.WEBCNTRL = 1;                                 // ... except for WEBCNTRL!
#endif
        }

    // Start idle checking
        // generic redis error check. 
        if(rv) {
            if(!BIT_IS_SET(idle_flag, idle_redis_error)) {   // if bit not already set
                hashpipe_warn(__FUNCTION__, "error returned from get_obs_info_from_redis() -  adding as an idle condition");
                SET_BIT(idle_flag, idle_redis_error);
            }
        } else {
            if(BIT_IS_SET(idle_flag, idle_redis_error)) {   // if bit not already set
                hashpipe_warn(__FUNCTION__, "OK returned from get_obs_info_from_redis() - removing as an idle condition");
                CLEAR_BIT(idle_flag, idle_redis_error);
            }
        }
#ifdef SOURCE_DIBAS
        // receiver check
        if(!atoi(gbtstatus.IFV1BW)) {
            if(!BIT_IS_SET(idle_flag, idle_zero_IFV1BW)) {   // if bit not already set
                hashpipe_warn(__FUNCTION__, "Receiver not properly set up (bandwidth is zero) - adding as an idle condition");
                SET_BIT(idle_flag, idle_zero_IFV1BW);
            }
        } else {
            if(BIT_IS_SET(idle_flag, idle_zero_IFV1BW)) {    // if bit is currently set
                hashpipe_warn(__FUNCTION__, "Receiver properly set up - removing as an idle condition");
                CLEAR_BIT(idle_flag, idle_zero_IFV1BW);
            }
        }
#endif
        if(idle_flag) {
            if(!idle) {    // if not already idling
                hashpipe_info(__FUNCTION__, "Data acquisition is idled");
                idle = 1;
            }
        } else {
            if(idle) {    // if currently idling
                hashpipe_info(__FUNCTION__, "Data acquisition is de-idled");
                idle = 0;
            }
        }

        hputi4(st.buf, "IDLE", idle);   // finally, make our idle condition live
    // End idle checking

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
        hputs(st.buf,  "TELESCOP", gbtstatus.IFV1TNCI);
        hputi4(st.buf, "COARCHID", gbtstatus.coarse_chan_id);
        hputs(st.buf, "BANDWDTH", gbtstatus.IFV1BW);
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
        hputi4(st.buf, "WEBCNTRL", gbtstatus.WEBCNTRL);
#endif
        hashpipe_status_unlock_safe(&st);

#ifdef SOURCE_DIBAS
        //if(gbtstatus.WEBCNTRL == 0) {
        //    pthread_exit(NULL);
        //}
#endif

        // test for and handle file change events
        // no such events exit while in test mode
        if(!testmode) {
#ifdef SOURCE_S6
            if(scram.receiver  != prior_receiver              ||
#elif SOURCE_DIBAS
            if(strcmp(gbtstatus.IFV1TNCI,prior_receiver) != 0 ||
               gbtstatus.WEBCNTRL == 0                        ||
#elif SOURCE_FAST
            if(strcmp(gbtstatus.IFV1TNCI,prior_receiver) != 0 ||
               gbtstatus.WEBCNTRL == 0                        ||
#endif
               run_always      != prior_run_always            ||
               num_coarse_chan != db->block[block_idx].header.num_coarse_chan) {

                // change files
                hashpipe_info(__FUNCTION__, "Initializing output for %ld coarse channels, using receiver %s\n",
#ifdef SOURCE_S6
                              num_coarse_chan, receiver[scram.receiver]);
#elif SOURCE_DIBAS
                              num_coarse_chan, gbtstatus.IFV1TNCI);
#elif SOURCE_FAST
                              num_coarse_chan, gbtstatus.IFV1TNCI);
#endif

                if(etf.file_open) {
                    etfits_close(&etf);     
                }
                etf.new_file = 1; 
                // re-init
#ifdef SOURCE_S6
                prior_receiver   = scram.receiver;
#elif SOURCE_DIBAS
                strcpy(prior_receiver,gbtstatus.IFV1TNCI);
#elif SOURCE_FAST
                strcpy(prior_receiver,gbtstatus.IFV1TNCI);
#endif
                num_coarse_chan  = db->block[block_idx].header.num_coarse_chan; 
                prior_run_always = run_always;
            }
        }   // end test for and handle file change events

#ifdef SOURCE_S6
        // write hits and metadata to etFITS file only if there is a receiver
        // in focus or the run_always flag in on
        if(testmode || scram.receiver || run_always) {
#elif SOURCE_DIBAS
// TODO - put GBT acquisition trigger logic, if any, here
        //if(run_always && gbtstatus.WEBCNTRL == 1 && !idle) {
        if(testmode || run_always && gbtstatus.WEBCNTRL) {
#elif SOURCE_FAST
        if(testmode || run_always && gbtstatus.WEBCNTRL) {
#endif
#ifdef SOURCE_S6
            etf.file_chan = scram.coarse_chan_id;          
            rv = write_etfits(db, block_idx, &etf, scram_p);
#elif SOURCE_DIBAS
            etf.file_chan = gbtstatus.coarse_chan_id;           // TODO - sensible for GBT?
            rv = write_etfits_gbt(db, block_idx, &etf, gbtstatus_p);
#elif SOURCE_FAST
            etf.file_chan = gbtstatus.coarse_chan_id;           // TODO - sensible for GBT?
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
        // put a few selected coarse channel powers to the status buffer
#ifdef SOURCE_S6
        hputr4(st.buf, "CCAV000X", db->block[block_idx].cc_pwrs_x[0]);
        hputr4(st.buf, "CCAV000Y", db->block[block_idx].cc_pwrs_y[0]);
        hputr4(st.buf, "CCAV060X", db->block[block_idx].cc_pwrs_x[60]);
        hputr4(st.buf, "CCAV060Y", db->block[block_idx].cc_pwrs_y[60]);
        hputr4(st.buf, "CCAV120X", db->block[block_idx].cc_pwrs_x[120]);
        hputr4(st.buf, "CCAV120Y", db->block[block_idx].cc_pwrs_y[120]);
        hputr4(st.buf, "CCAV180X", db->block[block_idx].cc_pwrs_x[180]);
        hputr4(st.buf, "CCAV180Y", db->block[block_idx].cc_pwrs_y[180]);
        hputr4(st.buf, "CCAV240X", db->block[block_idx].cc_pwrs_x[240]);
        hputr4(st.buf, "CCAV240Y", db->block[block_idx].cc_pwrs_y[240]);
        hputr4(st.buf, "CCAV319X", db->block[block_idx].cc_pwrs_x[319]);
        hputr4(st.buf, "CCAV319Y", db->block[block_idx].cc_pwrs_y[319]);
#endif
#ifdef SOURCE_DIBAS
        // first, 1/3, 2/3, final, skipping to new subspectrum for each
        // Number of coarse channels should never change with the DiBAS design!
        hputr4(st.buf, "CCS0FSTX", db->block[block_idx].cc_pwrs_x[0][0]);
        hputr4(st.buf, "CCS0FSTY", db->block[block_idx].cc_pwrs_y[0][0]);
        hputr4(st.buf, "CCS2.33X", db->block[block_idx].cc_pwrs_x[2][(int)(N_COARSE_CHAN_PER_BORS*0.33)]);
        hputr4(st.buf, "CCS2.33Y", db->block[block_idx].cc_pwrs_y[2][(int)(N_COARSE_CHAN_PER_BORS*0.33)]);
        hputr4(st.buf, "CCS4.67X", db->block[block_idx].cc_pwrs_x[4][(int)(N_COARSE_CHAN_PER_BORS*0.67)]);
        hputr4(st.buf, "CCS4.67Y", db->block[block_idx].cc_pwrs_y[4][(int)(N_COARSE_CHAN_PER_BORS*0.67)]);
        hputr4(st.buf, "CCS7LSTX", db->block[block_idx].cc_pwrs_x[7][N_COARSE_CHAN_PER_BORS-1]);
        hputr4(st.buf, "CCS7LSTY", db->block[block_idx].cc_pwrs_y[7][N_COARSE_CHAN_PER_BORS-1]);


#if 0
        hputr4(st.buf, "CCAV100X", db->block[block_idx].cc_pwrs_x[100]);
        hputr4(st.buf, "CCAV100Y", db->block[block_idx].cc_pwrs_y[100]);
        hputr4(st.buf, "CCAV200X", db->block[block_idx].cc_pwrs_x[200]);
        hputr4(st.buf, "CCAV200Y", db->block[block_idx].cc_pwrs_y[200]);
        hputr4(st.buf, "CCAV300X", db->block[block_idx].cc_pwrs_x[300]);
        hputr4(st.buf, "CCAV511Y", db->block[block_idx].cc_pwrs_y[511]);
#endif
#endif
        hashpipe_status_unlock_safe(&st);

#ifdef SOURCE_DIBAS
        if(strcmp(etf.filename_working, current_filename) != 0) {     // check for filename change
            strcpy(current_filename, etf.filename_working);
            hashpipe_info(__FUNCTION__, "New file : %s", current_filename);
            rv = put_obs_gbt_info_to_redis(current_filename, st.instance_id, (char *)"redishost", 6379);
            if(rv) {
                hashpipe_error(__FUNCTION__, "error returned from put_obs_info_to_redis()");
                pthread_exit(NULL);
            }
        }
#endif

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
