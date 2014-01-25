/*
 * s6_gpu_thread.c
 *
 * Performs spectroscopy of incoming data using s6GPU
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <s6GPU.h>

#include "hashpipe.h"
#include "paper_databuf.h"

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

static void *run(hashpipe_thread_args_t * args, int doCPU)
{
    // Local aliases to shorten access to args fields
    s6_gpu_input_databuf_t *db_in = (s6_gpu_input_databuf_t *)args->ibuf;
    s6_output_databuf_t *db_out = (s6_output_databuf_t *)args->obuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

#ifdef DEBUG_SEMS
    fprintf(stderr, "s/tid %lu/                      GPU/\n", pthread_self());
#endif

    // Init integration control status variables
    int gpu_dev = 0;
    hashpipe_status_lock_safe(&st);
    hputs(st.buf,  "INTSTAT", "off");
    hputi8(st.buf, "INTSYNC", 0);
    hputi4(st.buf, "INTCOUNT", N_SUB_BLOCKS_PER_INPUT_BLOCK);
    hgeti4(st.buf, "GPUDEV", &gpu_dev); // No change if not found
    hputi4(st.buf, "GPUDEV", gpu_dev);
    hashpipe_status_unlock_safe(&st);

    int rv;
    uint64_t start_mcount, last_mcount=0;
    int s6gpu_error = 0;
    int curblock_in=0;
    int curblock_out=0;

    struct timespec start, stop;
    uint64_t elapsed_gpu_ns  = 0;
    uint64_t gpu_block_count = 0;

    // init s6gpu
    device_vectors_t dv;
    s6_gpu_error = init_device_vectors(NELEMENT, dv);
    if (S6GPU_OK != s6gpu_error) {
        fprintf(stderr, "ERROR: xGPU initialization failed (error code %d)\n", xgpu_error);
        return THREAD_ERROR;
    }

    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);

        // Wait for new input block to be filled
        while ((rv=hashpipe_databuf_wait_filled((hashpipe_databuf_t *)db_in, curblock_in)) != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked_in");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
                pthread_exit(NULL);
                break;
            }
        }

        // Got a new data block, update status and determine how to handle it
        hashpipe_status_lock_safe(&st);
        hputi4(st.buf, "GPUBLKIN", curblock_in);
        hputu8(st.buf, "GPUMCNT", db_in->block[curblock_in].header.mcnt);
        hashpipe_status_unlock_safe(&st);

        // Note processing status
        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "processing gpu");
        hashpipe_status_unlock_safe(&st);

        if(db_in->block[curblock_in].header.mcnt >= last_mcount) {
          // Wait for new output block to be free
          while ((rv=paper_output_databuf_wait_free(db_out, curblock_out)) != HASHPIPE_OK) {
              if (rv==HASHPIPE_TIMEOUT) {
                  hashpipe_status_lock_safe(&st);
                  hputs(st.buf, status_key, "blocked gpu out");
                  hashpipe_status_unlock_safe(&st);
                  continue;
              } else {
                  hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                  pthread_exit(NULL);
                  break;
              }
          }
        }

        clock_gettime(CLOCK_MONOTONIC, &start);

        for(beam_i = 0; beam_i < NUM_BEAMS; beam_i++) {
            // spectroscopy() writes directly to the output buffer.
            nhits = spectroscopy(NTIME, NCHAN, POWER_THRESH, SMOOTH_SCALE, MAX_HITS, 
                                 h_raw_timeseries, db_out->block[curblock_out].data, dv);

            clock_gettime(CLOCK_MONOTONIC, &stop);
            elapsed_gpu_ns += ELAPSED_NS(start, stop);
            gpu_block_count++;
        }

        // Mark output block as full and advance
        paper_output_databuf_set_filled(db_out, curblock_out);
        curblock_out = (curblock_out + 1) % db_out->header.n_block;

        // Mark input block as free and advance
        hashpipe_databuf_set_free((hashpipe_databuf_t *)db_in, curblock_in);
        curblock_in = (curblock_in + 1) % db_in->header.n_block;

        /* Check for cancel */
        pthread_testcancel();
    }

    // Thread success!
    return NULL;
}

static void *run_gpu_only(hashpipe_thread_args_t * args)
{
  return run(args, 0);
}

static void *run_gpu_cpu(hashpipe_thread_args_t * args)
{
  return run(args, 1);
}

static hashpipe_thread_desc_t gpu_thread = {
    name: "paper_gpu_thread",
    skey: "GPUSTAT",
    init: NULL,
    run:  run_gpu_only,
    ibuf_desc: {paper_gpu_input_databuf_create},
    obuf_desc: {paper_output_databuf_create}
};

static hashpipe_thread_desc_t gpu_cpu_thread = {
    name: "paper_gpu_cpu_thread",
    skey: "GPUSTAT",
    init: NULL,
    run:  run_gpu_cpu,
    ibuf_desc: {paper_gpu_input_databuf_create},
    obuf_desc: {paper_output_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&gpu_thread);
  register_hashpipe_thread(&gpu_cpu_thread);
}

