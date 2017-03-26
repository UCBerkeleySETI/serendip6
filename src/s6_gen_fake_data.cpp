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

#include "s6_databuf.h"
#include "s6_fake_data.h"

// Functors
// -----------------------------------------------------------------------------
char2 generate_gaussian_complex_8b() {
// -----------------------------------------------------------------------------
    float mu    = 0.f;
    float sigma = 32.f;
    float u1 = drand48();
    float u2 = drand48();
    float r = sqrtf(-2*logf(u1)) * sigma;
    return make_char2(mu + r*cosf(2*3.14159265*u2),
                      mu + r*sinf(2*3.14159265*u2));

}

// -----------------------------------------------------------------------------
void gen_time_series(int f_factor, int input_i, std::vector<char2> &h_raw_timeseries) {
// -----------------------------------------------------------------------------

    // generate noise
    //srand48(1234);                // constant seed
    srand48((long int)time(NULL));  // seed with changing time
    std::generate(h_raw_timeseries.begin(), h_raw_timeseries.end(),
                  generate_gaussian_complex_8b);

    int locator;

//fprintf(stderr, "N_BYTES_PER_BEAM %d N_GPU_ELEMENTS %d N_FINE_CHAN %d N_COARSE_CHAN_PER_BORS %d\n", N_BYTES_PER_BEAM, N_GPU_ELEMENTS, N_FINE_CHAN, N_COARSE_CHAN_PER_BORS);
//fprintf(stderr, "in gen_sig : N_COARSE_CHAN %d  N_BORS %d  N_COARSE_CHAN_PER_BORS %d\n",                                     N_COARSE_CHAN, N_BORS, N_COARSE_CHAN_PER_BORS); 

#ifdef TRANSPOSE_DATA
#if 0
    fprintf(stderr, "Transposing synthetic data...\n");
    // dirty and not-so-quick transpose of the noise  
    std::vector<char2>   x_raw_timeseries(N_GPU_ELEMENTS);
    for(int jeffc=0; jeffc < N_GPU_ELEMENTS; jeffc++) x_raw_timeseries[jeffc] =  h_raw_timeseries[jeffc];
    int jeffc2=0;
    for( size_t c=0; c<N_FINE_CHAN; ++c ) {
        for( size_t t=0; t<N_COARSE_CHAN_PER_BORS; ++t ) {
            locator = t*N_FINE_CHAN + c;
            h_raw_timeseries[locator] = x_raw_timeseries[jeffc2];
            jeffc2++;
        }
    }   // end noise transpose
#endif

    // Add some alien signals 
    for( size_t c=0; c<N_FINE_CHAN; ++c ) {
        for( size_t t=0; t<N_COARSE_CHAN_PER_BORS; ++t ) {
            locator = t*N_FINE_CHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * t / N_COARSE_CHAN_PER_BORS;
            //fprintf(stderr, "c %d t %d location %d f %f\n", c, t, locator, f);

#else
fprintf(stderr, "not transposing\n");
    // Add some alien signals 
    for( size_t t=0; t<N_COARSE_CHAN_PER_BORS; ++t ) {       // ie for each coarse channel
        for( size_t c=0; c<N_FINE_CHAN; ++c ) {   // ie for each fine channel
            locator = t*N_FINE_CHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * c / N_FINE_CHAN;
            //fprintf(stderr, "c %d t %d location %d f %f\n", c, t, locator, f);
#endif
            h_raw_timeseries[locator].x += 10. * cos(0.111   + (c + input_i)   * f);  // predictable variability
            h_raw_timeseries[locator].y += 10. * sin(0.111   + (c + input_i)   * f);
            h_raw_timeseries[locator].x += 10. * cos(10.111  + 10  * f);
            h_raw_timeseries[locator].y += 10. * sin(10.111  + 10  * f);
            h_raw_timeseries[locator].x += 10. * cos(100.111 + 100 * f);
            h_raw_timeseries[locator].y += 10. * sin(100.111 + 100 * f);
            h_raw_timeseries[locator].x += 10. * cos(100.111 + 1000 * f);
            h_raw_timeseries[locator].y += 10. * sin(100.111 + 1000 * f);
        }
    }
}

// -----------------------------------------------------------------------------
void gen_fake_data(uint64_t *data) {
// data points to the input buffer
// -----------------------------------------------------------------------------
    std::vector<char2>   h_raw_timeseries_gen(N_GPU_ELEMENTS);
    char2 * c2data = (char2 *)data;     // input buffer pointer cast to char2 pointer
    int bors_i;

    for(bors_i=0; bors_i<N_BORS; bors_i++) {
    //for(bors_i=0; bors_i<1; bors_i++) {
        for(int pol=0; pol<N_POLS_PER_BEAM; pol++) {

            gen_time_series(2, pol, h_raw_timeseries_gen);      // gen data for 1 bors, 1 pol, all channels

            int do_print = 0;
            for(long j=0; j<N_GPU_ELEMENTS; j++) {       // properly scatter the data into the input buffer
                long src_locator  = j;
                //int dst_locator  = j*N_POLS_PER_BEAM+pol;
                long dst_locator  = bors_i * (long)N_COARSE_CHAN_PER_BORS * (long)N_FINE_CHAN * N_POLS_PER_BEAM +     // start of correct bors
                                    j*N_POLS_PER_BEAM+pol;                                  // offset w/in bors
                if(do_print) {
                    fprintf(stderr, "bors_i %d src %ld of %ld  dst %ld of %ld\n", bors_i, src_locator, h_raw_timeseries_gen.size(), dst_locator, N_DATA_BYTES_PER_BLOCK/sizeof(char2)); 
                    do_print--;
                }
                c2data[dst_locator] = h_raw_timeseries_gen[src_locator];
            }

            //fprintf(stderr, "bors %d pol %d ++++++++++++++++++++++++++++++++++++++++++++\n", bors_i, pol);
            //for(int jeffc=0; jeffc<20; jeffc++) {
            //    fprintf(stderr, ">>>> %d %d %d %d\n", c2data[jeffc].x, c2data[jeffc].y, c2data[jeffc+1].x, c2data[jeffc+1].y);
           // }
    }
}
    //memcpy((void *)data, (const void *)c2data, N_BYTES_PER_BLOCK);
    // memcpy((void *)data, (const void *)c2data, (size_t)N_BYTES_PER_BEAM);    // not needed! 3/14/17

}
