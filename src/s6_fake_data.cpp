#include <stdio.h>
#include <cufft.h>
#include <vector>
#include <algorithm>
#include "s6_databuf.h"
#include "s6GPU.h"
#include "s6GPU_test.h"

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

void gen_sig(int f_factor, int input_i, std::vector<char2> &h_raw_timeseries) {

    // Generate noise
    srand48(1234);
    std::generate(h_raw_timeseries.begin(), h_raw_timeseries.end(),
                  generate_gaussian_complex_8b);

    int locator;

#ifdef TRANSPOSE_DATA
    // dirty and not-so-quick transpose of the noise  
    std::vector<char2>   x_raw_timeseries(NELEMENT);
    for(int jeffc=0; jeffc < NELEMENT; jeffc++) x_raw_timeseries[jeffc] =  h_raw_timeseries[jeffc];
    int jeffc2=0;
    for( size_t c=0; c<NCHAN; ++c ) {
        for( size_t t=0; t<NTIME; ++t ) {
            locator = t*NCHAN + c;
            h_raw_timeseries[locator] = x_raw_timeseries[jeffc2];
            jeffc2++;
        }
    }   // end noise transpose

    // Add some alien signals 
    for( size_t c=0; c<NCHAN; ++c ) {
        for( size_t t=0; t<NTIME; ++t ) {
            locator = t*NCHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * t / NTIME;
            //fprintf(stderr, "c %d t %d location %d f %f\n", c, t, locator, f);
            
#else
    // Add some alien signals 
    for( size_t t=0; t<NTIME; ++t ) {
        for( size_t c=0; c<NCHAN; ++c ) {
            locator = t*NCHAN + c;          // same for transpose and no transpose
            float f = f_factor*3.14159265 * c / NCHAN;
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

