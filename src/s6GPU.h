#ifndef _S6GPU_H
#define _S6GPU_H

#include <vector_functions.h>
#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/copy.h>
#include <thrust/scan.h>
#include <thrust/gather.h>
#include <thrust/binary_search.h>
#include <thrust/device_ptr.h>

typedef struct {
    float power;
    float baseline;
    float strength;
    int   beam;
    int   input;    // ie, polariazation
#ifdef COMPUTE_HIT_DENSITY
// This is not needed for production but is kept here for reference
//  int   density;
#endif
    int   coarse_chan;
    int   fine_chan;
} hits_t;

typedef struct {
    thrust::device_vector<char2>  * raw_timeseries_p;       // input time series, correctly ordered
#ifdef TRANSPOSE
    thrust::device_vector<char2>  * raw_timeseries_rowmaj_p;    // temp time series if we are transposing    
#endif
    thrust::device_vector<float2> * fft_data_p;             // 
    thrust::device_vector<float2> * fft_data_out_p;         // 
    thrust::device_vector<float>  * powspec_p;              // power spectra
    thrust::device_vector<float>  * scanned_p;              // mean powers
    thrust::device_vector<float>  * baseline_p;             // running mean by region, using scanned_p
    thrust::device_vector<float>  * normalised_p;           // normalized spectra, using baseline_p
    thrust::device_vector<int>    * hit_indices_p;          // indexes subset of normalized spectrum that exceeds threshold
    thrust::device_vector<float>  * hit_powers_p;           // non-normalized hit powers, reported to caller
    thrust::device_vector<float>  * hit_baselines_p;        // mean power at hit bins, reported to caller. 
                                                            //   hit_power[n]/hit_baseline[n] = normalized hit power, reported to caller
    thrust::device_vector<int>    * hit_indices_high_p;     // used for hit densities
    thrust::device_vector<int>    * hit_indices_low_p;      // used for hit densities
    thrust::device_vector<int>    * hit_densities_p;        // hit densities
} device_vectors_t;

device_vectors_t * init_device_vectors(int n_element, int n_input);

void delete_device_vectors( device_vectors_t * dv_p);

int init_device(int gpu_dev);

void create_fft_plan_1d_c2c(cufftHandle* plan,
                            int          istride,
                            int          idist,
                            int          ostride,
                            int          odist,
                            size_t       nfft_,
                            size_t       nbatch);

int spectroscopy(int n_subband,
                 int n_chan,
                 int n_input,
                 int beam,
                 size_t maxhits,
                 size_t maxgpuhits,
                 float power_thresh,
                 float smooth_scale,
                 uint64_t * input_data,
                 size_t n_input_data_bytes,
                 hits_t *hits_p, 
                 device_vectors_t    *dv_p,
                 cufftHandle *fft_plan);
#if 0
int spectroscopy(int n_subband,
                 int n_chan,
                 int n_input,
                 float power_thresh,
                 float smooth_scale,
                 std::vector<char2>  &h_raw_timeseries, 
                 std::vector<hits_t> &hits, 
                 device_vectors_t    *dv,
                 cufftHandle   *plan);
#endif

#endif  // _S6GPU_TEST_H
