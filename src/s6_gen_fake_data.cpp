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

void gen_fake_data(uint64_t *data) {
    std::vector<char2>   h_raw_timeseries_gen(N_GPU_ELEMENTS);
    char2 * c2data = (char2 *)data;
    for(int input_i=0; input_i<N_POLS_PER_BEAM; input_i++) {
        gen_sig(2+input_i, h_raw_timeseries_gen);
            for(int j=0; j<N_GPU_ELEMENTS; j++) {
                int src_locator  = j;
                int dst_locator  = j*N_POLS_PER_BEAM+input_i;
                //fprintf(stderr, "src %d of %d  dst %d of %d\n", src_locator, h_raw_timeseries_gen.size(), dst_locator, h_raw_timeseries.size());
                c2data[dst_locator] = h_raw_timeseries_gen[src_locator];
            }
        }
}
