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

#include <cuda.h>
#include <cufft.h>

#include <s6GPU.h>
#include "hashpipe.h"
#include "s6_databuf.h"

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

static void *run(hashpipe_thread_args_t * args)
{
    // Local aliases to shorten access to args fields
    s6_input_databuf_t *db_in = (s6_input_databuf_t *)args->ibuf;
    s6_output_databuf_t *db_out = (s6_output_databuf_t *)args->obuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

#ifdef DEBUG_SEMS
    fprintf(stderr, "s/tid %lu/                      GPU/\n", pthread_self());
#endif

    // Init integration control status variables
    // what of this do I need - how do I get the gpu dev?
    //int gpu_dev = 0;
    //hashpipe_status_lock_safe(&st);
    //hputs(st.buf,  "INTSTAT", "off");
    //hputi8(st.buf, "INTSYNC", 0);
    //hputi4(st.buf, "INTCOUNT", N_SUB_BLOCKS_PER_INPUT_BLOCK);
    //hgeti4(st.buf, "GPUDEV", &gpu_dev); // No change if not found
    //hputi4(st.buf, "GPUDEV", gpu_dev);
    //hashpipe_status_unlock_safe(&st);

    int rv;
    uint64_t start_mcount, last_mcount=0;
    int s6gpu_error = 0;
    int curblock_in=0;
    int curblock_out=0;

    struct timespec start, stop;
    uint64_t elapsed_gpu_ns  = 0;
    uint64_t gpu_block_count = 0;

    // init s6GPU
    device_vectors_t *dv_p;
    cufftHandle fft_plan;
    cufftHandle *fft_plan_p = &fft_plan;
    // TODO handle errors

    int     n_subband = N_COARSE_CHAN;
    int     n_chan    = N_FINE_CHAN;
    int     n_input   = N_POLS_PER_BEAM;
    size_t  nfft_     = n_chan;
    size_t  nbatch    = n_subband;
    int     istride   = n_subband*n_input;   // this effectively transposes the input data
    int     ostride   = 1;
    int     idist     = n_input;            // this takes care of the input interleave
    int     odist     = nfft_;
    create_fft_plan_1d_c2c(fft_plan_p, istride, idist, ostride, odist, nfft_, nbatch);

    dv_p = init_device_vectors(N_GPU_ELEMENTS, N_POLS_PER_BEAM);

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
          while ((rv=s6_output_databuf_wait_free(db_out, curblock_out)) != HASHPIPE_OK) {
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

        // pass mcnt and missed_pkts on to the output thread
        db_in->block[curblock_out].header.mcnt = db_in->block[curblock_in].header.mcnt;
        memcpy(&db_out->block[curblock_out].header.missed_pkts, 
               &db_in->block[curblock_in].header.missed_pkts, 
               sizeof(uint64_t) * N_BEAMS);

        // do spectroscopy and hit detection on this block.
        // spectroscopy() writes directly to the output buffer.
        // TODO error checking
        for(int beam_i = 0; beam_i < N_BEAMS; beam_i++) {
            // TODO putting beam into hits_t is kind of ugly.
            db_out->block[curblock_out].header.nhits += 
                spectroscopy(N_COARSE_CHAN,
                             N_FINE_CHAN,
                             N_POLS_PER_BEAM,
                             beam_i,
                             MAXHITS,
                             POWER_THRESH,
                             SMOOTH_SCALE,
                             &(db_in->block[curblock_in].data[beam_i*N_BYTES_PER_BEAM]),
                             N_BYTES_PER_BEAM,
                             (hits_t *) &db_out->block[curblock_out].hits,
                             dv_p,
                             fft_plan_p);

            clock_gettime(CLOCK_MONOTONIC, &stop);
            elapsed_gpu_ns += ELAPSED_NS(start, stop);
            gpu_block_count++;
        }

        // re-init input block
        // TODO

        // Mark output block as full and advance
        s6_output_databuf_set_filled(db_out, curblock_out);
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

static hashpipe_thread_desc_t gpu_thread = {
    name: "s6_gpu_thread",
    skey: "GPUSTAT",
    init: NULL,
    run:  run,
    ibuf_desc: {s6_input_databuf_create},
    obuf_desc: {s6_output_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&gpu_thread);
}
