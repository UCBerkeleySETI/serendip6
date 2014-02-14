/*
 * s6_output_thread.c
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <hiredis.h>

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
    hputr4(st.buf, "CGOMXERR", 0.0);
    hputi4(st.buf, "CGOERCNT", 0);
    hputi4(st.buf, "CGOMXECT", 0);
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

    //                         0           1
    char *alfa_state[2] = {"disabled", "enabled"};

    init_etfits(&etf);

    /* Main loop */
    int i, rv, debug=20;
    int block_idx;
    int error_count, max_error_count = 0;
    float error, max_error = 0.0;
    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
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

        // TODO check mcnt

        // get scram, etc data
        rv = get_obs_info_from_redis(scram_p, (char *)"redishost", 6379);

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, "ALFASTAT", alfa_state[scram.alfa_enabled]);
        // TODO lots more scram to go here
        hashpipe_status_unlock_safe(&st);
    
        if(scram.alfa_enabled != prior_alfa_enabled) {
            fprintf(stderr, "alfa_enabled had changed from %d to %d\n", prior_alfa_enabled, scram.alfa_enabled);
            prior_alfa_enabled = scram.alfa_enabled;
        }

        // write hits and metadata to etFITS file only if alfa is enabled
        // alfa_enabled might be a second or so out of sync with data
        if(scram.alfa_enabled) {
            rv = write_etfits(db, block_idx, &etf, scram_p);
        }

        // Note processing status, current input block
        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "processing");
        hputi4(st.buf, "CGOBLKIN", block_idx);
        hashpipe_status_unlock_safe(&st);

        // Update status values
        hashpipe_status_lock_safe(&st);
        hputr4(st.buf, "CGOMXERR", max_error);
        hputi4(st.buf, "CGOERCNT", error_count);
        hputi4(st.buf, "CGOMXECT", max_error_count);
        hashpipe_status_unlock_safe(&st);

        // re-init output block
        // TODO

        // Mark blocks as free
        for(i=0; i<2; i++) {
            s6_output_databuf_set_free(db, block_idx);
        }

        // Setup for next block
        block_idx = (block_idx + 1) % db->header.n_block;    

        // TODO ?
        //if(....) {
        //    etfits_close(...);
        //}

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    // Thread success!
    return NULL;
}

static hashpipe_thread_desc_t output_thread = {
    name: "s6_output_thread",
    skey: "CGOSTAT",
    init: init,
    run:  run,
    ibuf_desc: {s6_output_databuf_create},
    obuf_desc: {NULL}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&output_thread);
}
