/*
 * s6_fake_net_thread.c
 *
 * Routine to write fake data into shared memory blocks.  This allows the
 * processing pipelines to be tested without the network portion of SERENDIP6.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
extern "C" {
#include <pthread.h>
}
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <vector_functions.h>
#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/copy.h>
#include <thrust/scan.h>
#include <thrust/gather.h>
#include <thrust/binary_search.h>
#include <thrust/device_ptr.h>
#include <vector>
#include <algorithm>

#include <s6GPU.h>

extern "C" {
#include "hashpipe.h"
}
#include "s6_databuf.h"

// Functors
// --------
char2 generate_gaussian_complex_8b() {
    float mu    = 0.f;
    float sigma = 32.f;
    float u1 = drand48();
    float u2 = drand48();
    float r = sqrtf(-2*logf(u1)) * sigma;
    return make_char2(mu + r*cosf(2*3.14159265*u2),
                      mu + r*sinf(2*3.14159265*u2));

}
// --------

void gen_sig(int f_factor, std::vector<char2> &h_raw_timeseries) {
    // Generate noise
    srand48(1234);
    std::generate(h_raw_timeseries.begin(), h_raw_timeseries.end(),
                  generate_gaussian_complex_8b);

    int locator;

#ifdef TRANSPOSE_DATA
    // dirty and not-so-quick transpose of the noise  
    std::vector<char2>   x_raw_timeseries(N_GPU_ELEMENTS);
    for(int i=0; i < N_GPU_ELEMENTS; i++) x_raw_timeseries[i] =  h_raw_timeseries[i];
    int j=0;
    for( size_t c=0; c<N_COARSE_CHAN; ++c ) {
        for( size_t t=0; t<N_FINE_CHAN; ++t ) {
            locator = t*N_COARSE_CHAN + c;
            h_raw_timeseries[locator] = x_raw_timeseries[j];
            j++;
        }
    }   // end noise transpose

    // Add some alien signals 
    for( size_t c=0; c<N_COARSE_CHAN; ++c ) {
        for( size_t t=0; t<N_FINE_CHAN; ++t ) {
            locator = t*N_COARSE_CHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * t / N_FINE_CHAN;
            //fprintf(stderr, "c %d t %d location %d f %f\n", c, t, locator, f);
            
#else
    // Add some alien signals 
    for( size_t t=0; t<N_FINE_CHAN; ++t ) {
        for( size_t c=0; c<N_COARSE_CHAN; ++c ) {
            locator = t*N_COARSE_CHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * c / N_COARSE_CHAN;
            //fprintf(stderr, "c %d t %d location %d f %f\n", c, t, locator, f);
#endif
            h_raw_timeseries[locator].x += 10. * cos(0.111   + 1   * f);
            h_raw_timeseries[locator].y += 10. * sin(0.111   + 1   * f);
            h_raw_timeseries[locator].x += 10. * cos(10.111  + 10  * f);
            h_raw_timeseries[locator].y += 10. * sin(10.111  + 10  * f);
            h_raw_timeseries[locator].x += 10. * cos(100.111 + 100 * f);
            h_raw_timeseries[locator].y += 10. * sin(100.111 + 100 * f);
            h_raw_timeseries[locator].x += 10. * cos(100.111 + 1000 * f);
            h_raw_timeseries[locator].y += 10. * sin(100.111 + 1000 * f);
        }
    }
}

static void *run(hashpipe_thread_args_t * args)
{
    std::vector<char2>   h_raw_timeseries_gen(N_GPU_ELEMENTS);
    char2 h_raw_timeseries[N_GPU_ELEMENTS*N_POLS_PER_BEAM];

    s6_input_databuf_t *db = (s6_input_databuf_t *)args->obuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

    /* Main loop */
    int i, rv;
    uint64_t mcnt = 0;
    uint64_t *data;
    int m,f,t,c;
    int block_idx = 0;
    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);
 
#if 0
        // xgpuRandomComplex is super-slow so no need to sleep
        // Wait for data
        //struct timespec sleep_dur, rem_sleep_dur;
        //sleep_dur.tv_sec = 0;
        //sleep_dur.tv_nsec = 0;
        //nanosleep(&sleep_dur, &rem_sleep_dur);
#endif

	
        /* Wait for new block to be free, then clear it
         * if necessary and fill its header with new values.
         */
        while ((rv=s6_input_databuf_wait_free(db, block_idx)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                pthread_exit(NULL);
                break;
            }
        }

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "receiving");
        hputi4(st.buf, "NETBKOUT", block_idx);
        hashpipe_status_unlock_safe(&st);
 
#if 0
        // Fill in sub-block headers
        for(i=0; i<N_SUB_BLOCKS_PER_INPUT_BLOCK; i++) {
          //db->block[block_idx].header.good_data = 1;
          db->block[block_idx].header.mcnt = mcnt;
          mcnt+=Nm;
        }
#endif

        // For testing, zero out block and set input FAKE_TEST_INPUT, FAKE_TEST_CHAN to
        // all -16 (-1 * 16)
        //data = db->block[block_idx].data;
        //memset(data, 0, N_BYTES_PER_BLOCK);

        // gen fake data    
        char2 * h_raw_timeseries = (char2 *)&(db->block[block_idx].data[0]);
        for(int input_i=0; input_i<N_POLS_PER_BEAM; input_i++) {
            gen_sig(2+input_i, h_raw_timeseries_gen);
            for(int j=0; j<N_GPU_ELEMENTS; j++) {
                int src_locator  = j;
                int dst_locator  = j*N_POLS_PER_BEAM+input_i;
                //fprintf(stderr, "src %d of %d  dst %d of %d\n", src_locator, h_raw_timeseries_gen.size(), dst_locator, h_raw_timeseries.size());
                h_raw_timeseries[dst_locator] = h_raw_timeseries_gen[src_locator];
            }
        }

        // Mark block as full
        s6_input_databuf_set_filled(db, block_idx);

        // Setup for next block
        block_idx = (block_idx + 1) % db->header.n_block;

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    // Thread success!
    return THREAD_OK;
}

static hashpipe_thread_desc_t fake_net_thread = {
    name: "s6_fake_net_thread",
    skey: "NETSTAT",
    init: NULL,
    run:  run,
    ibuf_desc: {NULL},
    obuf_desc: {s6_input_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&fake_net_thread);
}
