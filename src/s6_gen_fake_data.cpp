/*
 * s6_gen_fake_data.c
 *
 * Data generation for use with the fake net thread.
 * This allows the processing pipelines to be tested without the 
 * network portion of SERENDIP6.
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
#include "hashpipe.h"

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
// First we fill the time domain vector with noise.
// To this we add some easily identifiable signals such that they will show up in a
// specific subset of coarse and fine channels in the frequency domain.

    // generate noise
    //srand48(1234);                // constant seed
    srand48((long int)time(NULL));  // seed with changing time
    std::generate(h_raw_timeseries.begin(), h_raw_timeseries.end(), generate_gaussian_complex_8b);

    // put signals in the quarter way points in the coarse channel range
    for( size_t cc=0; cc<N_COARSE_CHAN_PER_BORS; cc += ceil((double)N_COARSE_CHAN_PER_BORS/4.0)) {   
        for( size_t t=0; t<N_FINE_CHAN; t++ ) {               // for each time (ie, fine channel in freq domain)
            int locator     = t*N_COARSE_CHAN_PER_BORS + cc;  // stay in this "time", ie stride by N_CC * cc offset      
            float f         = f_factor*3.14159265 * t/N_FINE_CHAN;
            float amplitude = 10.0;
            // put signals at the quarter way points in the "fine channel" range
            h_raw_timeseries[locator].x += amplitude * cos(0.111   + 1                          * f);
            h_raw_timeseries[locator].y += amplitude * sin(0.111   + 1                          * f);
            h_raw_timeseries[locator].x += amplitude * cos(10.111  + (long)(N_FINE_CHAN*0.25)   * f);
            h_raw_timeseries[locator].y += amplitude * sin(10.111  + (long)(N_FINE_CHAN*0.25)   * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 + (long)(N_FINE_CHAN*0.5)    * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 + (long)(N_FINE_CHAN*0.5)    * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 + (long)(N_FINE_CHAN*0.75)   * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 + (long)(N_FINE_CHAN*0.75)   * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 +        N_FINE_CHAN-1       * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 +        N_FINE_CHAN-1       * f);
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

	hashpipe_info(__FUNCTION__, "Generating %lu random samples", 
				  (unsigned long)(N_BORS * N_POLS_PER_BEAM * N_COARSE_CHAN_PER_BORS * N_FINE_CHAN));

    for(bors_i=0; bors_i<N_BORS; bors_i++) {
        for(int pol=0; pol<N_POLS_PER_BEAM; pol++) {

            gen_time_series(2, pol, h_raw_timeseries_gen);      // gen data for 1 bors, 1 pol, all channels

            int do_print = 0;           // for debugging, change this to the number read outs you want
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
        }
    }
}
