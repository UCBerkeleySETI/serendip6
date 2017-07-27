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
#include "hashpipe.h"

#ifdef SOURCE_FAST
// Functors
// -----------------------------------------------------------------------------
char generate_gaussian_real_8b() {
// -----------------------------------------------------------------------------
    float mu    = 0.f;
    float sigma = 32.f;
    float u1 = drand48();					// random... 
    float r = sqrtf(-2*logf(u1)) * sigma;	// ...exponential amplitude 
    float u2 = drand48();					// random frequency
    return char(mu + r*sinf(2*M_PI * u2));
}



// -----------------------------------------------------------------------------
void gen_time_series(int input_i, std::vector<char> &h_raw_timeseries) {
// -----------------------------------------------------------------------------
// First we fill the time domain vector with noise.
// To this we add some easily identifiable signals such that they will show up in a
// specific subset of coarse and fine channels in the frequency domain.

    // generate noise
    //srand48(1234);                // constant seed
    srand48((long int)time(NULL));  // seed with changing time
    std::generate(h_raw_timeseries.begin(), h_raw_timeseries.end(), generate_gaussian_real_8b);

	double zero_phase_offset = 0.0;

    // put signals in the quarter way points in the coarse channel range
    for( size_t cc=0; cc<N_COARSE_CHAN_PER_BORS; cc += ceil((double)N_COARSE_CHAN_PER_BORS/4.0)) {   
        for( size_t t=0; t<N_TIME_SAMPLES; t++ ) {              	// for each time (ie, fine channel in freq domain)
           long locator     	= t*N_COARSE_CHAN_PER_BORS + cc;  	// stay in this "time", ie stride by N_CC * cc offset      
           double amplitude 	= 10.0;
           double f         	= 2*M_PI * t/N_TIME_SAMPLES;	  	// 1 cycle completes every N_TIME_SAMPLES. Final frequencies
																	//   are determined by the multipliers below.  (Without the
																	//   /N_TIME_SAMPLES, f would be 0 for any integer t.) 
            // Put signal frequencies at the quarter way points in the fine channel range.
            // This is data for a real to complex FFT so the number of fine channels is 1/2 the number of time samples.
            h_raw_timeseries[locator] += amplitude * sin(f);
            h_raw_timeseries[locator] += amplitude * sin(N_TIME_SAMPLES/2 * 0.25 * f);
            h_raw_timeseries[locator] += amplitude * sin(N_TIME_SAMPLES/2 * 0.50 * f);
            h_raw_timeseries[locator] += amplitude * sin(N_TIME_SAMPLES/2 * 0.75 * f);
            h_raw_timeseries[locator] += amplitude * sin((N_TIME_SAMPLES/2 - 1)	 * f);
        }
    }
}

#else

// Functors
// -----------------------------------------------------------------------------
char2 generate_gaussian_complex_8b() {
// -----------------------------------------------------------------------------
    float mu    = 0.f;
    float sigma = 32.f;
    float u1 = drand48();					// random... 
    float r = sqrtf(-2*logf(u1)) * sigma;	// ...exponential amplitude 
    float u2 = drand48();					// random frequency
    return make_char2(mu + r*cosf(2*M_PI * u2),
                      mu + r*sinf(2*M_PI * u2));

}



// -----------------------------------------------------------------------------
//void gen_time_series(int f_factor, int input_i, std::vector<char2> &h_raw_timeseries) {
void gen_time_series(int input_i, std::vector<char2> &h_raw_timeseries) {
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
        for( size_t t=0; t<N_TIME_SAMPLES; t++ ) {             	// for each time (ie, fine channel in freq domain)
         	int locator     = t*N_COARSE_CHAN_PER_BORS + cc; 	// stay in this "time", ie stride by N_CC * cc offset      
           	double f       	= 2*M_PI * t/N_TIME_SAMPLES;	  	// 1 cycle completes every N_TIME_SAMPLES. Final frequencies
																//   are determined by the multipliers below.  (Without the
																//   /N_TIME_SAMPLES, f would be 0 for any integer t.) 
            float amplitude = 10.0;
            // Put signals at the quarter way points in the "fine channel" range.
            // This is data for a complex to complex FFT so the number of fine channels is == to the number of time samples.
            h_raw_timeseries[locator].x += amplitude * cos(0.111   + 1                             * f);
            h_raw_timeseries[locator].y += amplitude * sin(0.111   + 1                             * f);
            h_raw_timeseries[locator].x += amplitude * cos(10.111  + (long)(N_TIME_SAMPLES*0.25)   * f);
            h_raw_timeseries[locator].y += amplitude * sin(10.111  + (long)(N_TIME_SAMPLES*0.25)   * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 + (long)(N_TIME_SAMPLES*0.5)    * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 + (long)(N_TIME_SAMPLES*0.5)    * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 + (long)(N_TIME_SAMPLES*0.75)   * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 + (long)(N_TIME_SAMPLES*0.75)   * f);
            h_raw_timeseries[locator].x += amplitude * cos(100.111 +        N_TIME_SAMPLES-1       * f);
            h_raw_timeseries[locator].y += amplitude * sin(100.111 +        N_TIME_SAMPLES-1       * f);
        }
    }
}

#endif

// -----------------------------------------------------------------------------
void gen_fake_data(uint64_t *data) {
// data points to the input buffer
// -----------------------------------------------------------------------------
	uint64_t n_gpu_input_elements = N_TIME_SAMPLES *  N_COARSE_CHAN_PER_BORS;
#ifdef SOURCE_FAST
    std::vector<char>   h_raw_timeseries_gen(n_gpu_input_elements);
    char * c2data = (char *)data;       // input buffer pointer cast to char pointer
#else 
    std::vector<char2>   h_raw_timeseries_gen(n_gpu_input_elements);
    char2 * c2data = (char2 *)data;     // input buffer pointer cast to char2 pointer
#endif
    int bors_i;

	hashpipe_info(__FUNCTION__, "Generating %lu random samples", 
				  (unsigned long)(N_BORS * N_POLS_PER_BEAM * N_COARSE_CHAN_PER_BORS * N_TIME_SAMPLES));

    for(bors_i=0; bors_i<N_BORS; bors_i++) {
        for(int pol=0; pol<N_POLS_PER_BEAM; pol++) {

            gen_time_series(pol, h_raw_timeseries_gen);      // gen data for 1 bors, 1 pol, all channels

            int do_print = 0;           // for debugging, change this to the number read outs you want
            for(long j=0; j<n_gpu_input_elements; j++) {       // properly scatter the data into the input buffer
                long src_locator  = j;
                //int dst_locator  = j*N_POLS_PER_BEAM+pol;
                long dst_locator  = bors_i * (long)N_COARSE_CHAN_PER_BORS * (long)N_TIME_SAMPLES * N_POLS_PER_BEAM +     // start of correct bors
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
