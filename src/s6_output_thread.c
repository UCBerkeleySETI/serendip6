/*
 * s6_output_thread.c
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <s6gpu.h>

#include "hashpipe.h"
#include "paper_databuf.h"

#define TOL (1e-5)

//static XGPUInfo xgpu_info;    // jeffc

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

#define zabs(x,i) hypot(x[i],x[i+1])

static void *run(hashpipe_thread_args_t * args)
{
    // Local aliases to shorten access to args fields
    // Our input buffer happens to be a paper_ouput_databuf
    s6_output_databuf_t *db = (s6_output_databuf_t *)args->ibuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

    struct etfits etf;

    scram_t scram;

    redisContext * redis_context;       // move to s6_write_etfits.c

    redis_context = redis_init();       // move to s6_write_etfits.c

    /* Main loop */
    int i, rv, debug=20;
    int block_idx;
    int error_count, max_error_count = 0;
    //float *gpu_data, *cpu_data;     // jeffc
    float error, max_error = 0.0;
    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);

       if(!etf.fptr) {
            etfits_create(...);     // will also init primary header
       }

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

        // check mcnt

        // write hits to etFITS file
        rv = write_etfits(s6_output_databuf_t *db, block_idx, etfits * etf_p);

        // write coarse spectra to etFITS file - deferred

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

        // Mark blocks as free
        for(i=0; i<2; i++) {
            paper_output_databuf_set_free(db, block_idx);
        }

        // Setup for next block
        block_idx = (block_idx + 1) % db->header.n_block;    

        if(....) {
            etfits_close(...);
        }

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    // Thread success!
    return NULL;
}

#if 0
int write_hits_to_etfits(s6_output_databuf_t *db, struct etfits * etf_p) {      // use typedef for etfits

    s6_hits * hits;

    hits = (s6_hits *)db;   // add any header offset

    for(i=0; i<nhits; i++) {
        etfits_write_hits();
    }

}

int write_header_to_etfits() {

}
#endif

static hashpipe_thread_desc_t gpu_cpu_output_thread = {
    name: "paper_gpu_cpu_output_thread",
    skey: "CGOSTAT",
    init: init,
    run:  run,
    ibuf_desc: {paper_output_databuf_create},
    obuf_desc: {NULL}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&gpu_cpu_output_thread);
}
