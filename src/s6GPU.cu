#include <iostream>
using std::cout;
using std::endl;
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <cuda.h>
#include <cufft.h>

#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/copy.h>
#include <thrust/scan.h>
#include <thrust/gather.h>
#include <thrust/binary_search.h>
#include <thrust/device_ptr.h>
#include <thrust/for_each.h>
#include <thrust/functional.h>

#include "s6GPU.h"
#include "stopwatch.hpp"

#include "hashpipe.h"
#include <time.h>

#define USE_TIMER
#define USE_TOTAL_GPU_TIMER
#ifdef USE_TIMER
    bool use_timer=true;
#else
    bool use_timer=false;
#endif
#ifdef USE_TOTAL_GPU_TIMER
    bool use_total_gpu_timer=true;
#else
    bool use_total_gpu_timer=false;
#endif
#define TRACK_GPU_MEMORY
#ifdef TRACK_GPU_MEMORY
    bool track_gpu_memory=true;
#else
    bool track_gpu_memory=false;
#endif

//int init_device_vectors(int n_element, int n_input, device_vectors_t &dv) {
device_vectors_t * init_device_vectors(int n_element_max, int n_element_utilized, int n_input) {

    device_vectors_t * dv_p  = new device_vectors_t;

#ifdef TRANSPOSE
    dv_p->raw_timeseries_rowmaj_p   = new thrust::device_vector<char2>(n_element_max*n_input);
#endif
    dv_p->hit_indices_p      = new thrust::device_vector<int>();
    dv_p->hit_powers_p       = new thrust::device_vector<float>;
    dv_p->hit_baselines_p    = new thrust::device_vector<float>;

    return dv_p;
}

int init_device(int gpu_dev) {
    int rv = cudaSetDevice(gpu_dev);
    // TODO error checking
    return rv;
}

void delete_device_vectors( device_vectors_t * dv_p) {
// TODO - is the right way to deallocate thrust vectors?
#ifdef TRANSPOSE
    delete(dv_p->raw_timeseries_rowmaj_p);
#endif
    delete(dv_p->hit_indices_p);      
    delete(dv_p->hit_powers_p);       
    delete(dv_p->hit_baselines_p);    

    delete(dv_p);
}

void create_fft_plan_1d_c2c(cufftHandle* plan,
                            int          istride,
                            int          idist,
                            int          ostride,
                            int          odist,
                            size_t       nfft_,
                            size_t       nbatch) {
    int rank      = 1;
    int nfft[]    = {nfft_};
    int inembed[] = {nfft[0]};
    //int idist     = inembed[0];
    int onembed[] = {nfft[0]};
    //int odist     = onembed[0];
    cufftResult fft_ret = cufftPlanMany(plan,
                                        rank, nfft,
                                        inembed, istride, idist,
                                        onembed, ostride, odist,
                                        CUFFT_C2C, nbatch);
    if( fft_ret != CUFFT_SUCCESS ) {
        throw std::runtime_error("cufftPlanMany failed");
    }
}

inline void get_gpu_mem_info(const char * comment) {
    int rv;
    size_t free, total;
    double free_gb, total_gb, allocated_gb;
    rv = cudaMemGetInfo(&free, &total);
    if(rv) {
        fprintf(stderr, "Error from cudaMemGetInfo() : %d\n", rv);
    } else {
        total_gb = (double)total/(1024*1024*1024);
        free_gb =  (double)free/(1024*1024*1024);
        allocated_gb = total_gb - free_gb;
        fprintf(stderr, "GPU memory total : %2.2lf GB    allocated : %2.2lf GB (%2.2f%%)    free : %2.2lf GB    (%s)\n", 
                total_gb, allocated_gb, (allocated_gb/total_gb)*100, free_gb, comment);
    }
} 

// Note: input == output is ok
void execute_fft_plan_c2c(cufftHandle   *plan,
                          const float2* input,
                          float2*       output) {
    cufftResult fft_ret = cufftExecC2C(*plan,
                                       (cufftComplex*)input,
                                       (cufftComplex*)output,
                                       CUFFT_FORWARD);
    if( fft_ret != CUFFT_SUCCESS ) {
        throw std::runtime_error("cufftExecC2C failed");
    }
}

// Functors
// --------
struct convert_complex_8b_to_float
    : public thrust::unary_function<char2,float2> {
    inline __host__ __device__
    float2 operator()(char2 a) const {
        return make_float2(a.x, a.y);
    }
};
struct compute_complex_power
    : public thrust::unary_function<float2,float> {
    inline __host__ __device__
    float operator()(float2 a) const {
        return a.x*a.x + a.y*a.y;
    }
};
struct advance_within_region
    : public thrust::unary_function<int,int> {
    int  delta;
    uint region_size;
    advance_within_region(int delta_, uint region_size_)
        : delta(delta_), region_size(region_size_) {}
    inline __host__ __device__
    int operator()(int i) const {
        int region = i / region_size;
        int idx    = i % region_size;
        idx += delta;
        idx = max(0, idx);
        idx = min(region_size-1, idx);
        return idx + region_size*region;
    }
};
struct running_mean_by_region
    : public thrust::unary_function<int, float> {
    uint         radius;
    uint         region_size;
    const float* d_scanned;
    running_mean_by_region(uint radius_,
                           uint region_size_,
                           const float* d_scanned_)
        : radius(radius_),
          region_size(region_size_),
          d_scanned(d_scanned_) {}
    inline __host__ __device__
    float operator()(uint i) const {
        uint region = i / region_size;
        uint offset = region * region_size;
        uint idx    = i % region_size;

        float sum;
        if( idx < radius ) {
            sum = (d_scanned[2*radius + offset] -
                   d_scanned[0 + offset]);
        }
        else if( idx > region_size-1-radius ) {
            sum = (d_scanned[region_size-1 + offset] -
                   d_scanned[region_size-1-2*radius + offset]);
        }
        else {
            sum = (d_scanned[idx + radius + offset] -
                   d_scanned[idx - radius + offset]);
        }
        return sum / (2*radius);
    }
};
struct transpose_index : public thrust::unary_function<size_t,size_t> {
// convert a linear index to a linear index in the transpose 
  size_t m, n;

  __host__ __device__
  transpose_index(size_t _m, size_t _n) : m(_m), n(_n) {}

  __host__ __device__
  size_t operator()(size_t linear_index)
  {
      size_t i = linear_index / n;
      size_t j = linear_index % n;

      return m * j + i;
  }
};
// --------
template<typename T>
struct divide_by : public thrust::unary_function<T,T> {
    T val;
    divide_by(T val_) : val(val_) {}
    inline __host__ __device__
    T operator()(T x) const {
        return x / val;
    }
};
template<typename T>
struct greater_than_val : public thrust::unary_function<T,bool> {
    T val;
    greater_than_val(T val_) : val(val_) {}
    inline __host__ __device__
    bool operator()(T x) const {
        return x > val;
    }
};
template <typename T>
void transpose(size_t m, size_t n, thrust::device_vector<T> *src, thrust::device_vector<T> *dst) {
// transpose an m-by-n array
  thrust::counting_iterator<size_t> indices(0);
  
  thrust::gather
    (thrust::make_transform_iterator(indices, transpose_index(n, m)),
     thrust::make_transform_iterator(indices, transpose_index(n, m)) + dst->size(),
     src->begin(),
     dst->begin());
}

// convert a linear index to a row index
template <typename T>
struct linear_index_to_row_index : public thrust::unary_function<T,T>
{
  T C; // number of columns
  
  __host__ __device__
  linear_index_to_row_index(T C) : C(C) {}

  __host__ __device__
  T operator()(T i)
  {
    return i / C;
  }
};

template <typename Iterator>
class strided_range
{
    public:

    typedef typename thrust::iterator_difference<Iterator>::type difference_type;

    struct stride_functor : public thrust::unary_function<difference_type,difference_type>
    {
        difference_type stride;

        stride_functor(difference_type stride)
            : stride(stride) {}

        __host__ __device__
        difference_type operator()(const difference_type& i) const
        { 
            return stride * i;
        }
    };

    typedef typename thrust::counting_iterator<difference_type>                   CountingIterator;
    typedef typename thrust::transform_iterator<stride_functor, CountingIterator> TransformIterator;
    typedef typename thrust::permutation_iterator<Iterator,TransformIterator>     PermutationIterator;

    // type of the strided_range iterator
    typedef PermutationIterator iterator;

    // construct strided_range for the range [first,last)
    strided_range(Iterator first, Iterator last, difference_type stride)
        : first(first), last(last), stride(stride) {}
   
    iterator begin(void) const
    {
        return PermutationIterator(first, TransformIterator(CountingIterator(0), stride_functor(stride)));
    }

    iterator end(void) const
    {
        return begin() + ((last - first) + (stride - 1)) / stride;
    }
    
    protected:
    Iterator first;
    Iterator last;
    difference_type stride;
};

void do_fft(cufftHandle *fft_plan, float2* &fft_input_ptr, float2* &fft_output_ptr) {
    Stopwatch timer;
    if(use_timer) timer.start();
    execute_fft_plan_c2c(fft_plan, fft_input_ptr, fft_output_ptr);
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "FFT execution time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
}

void compute_power_spectrum(device_vectors_t *dv_p) {
//fprintf(stderr, "In compute_power_spectrum 1\n");
    Stopwatch timer;
    if(use_timer) timer.start();
//fprintf(stderr, "In compute_power_spectrum 2 %p %p\n", thrust::raw_pointer_cast(dv_p->fft_data_out_p), thrust::raw_pointer_cast(dv_p->powspec_p));
//fprintf(stderr, "In compute_power_spectrum 2 %p %lu %p %lu\n", dv_p->fft_data_out_p, dv_p->fft_data_out_p->size() * sizeof(float2), dv_p->powspec_p, dv_p->powspec_p->size() * sizeof(float));
    thrust::transform(dv_p->fft_data_out_p->begin(), dv_p->fft_data_out_p->end(),
                      dv_p->powspec_p->begin(),
                      compute_complex_power());
//fprintf(stderr, "In compute_power_spectrum 3\n");
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Power spectrum time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
}

struct printf_functor {
    __host__ __device__
    void operator()(float x)
    {
      // note that using printf in a __device__ function requires
      // code compiled for a GPU with compute capability 2.0 or
      // higher (nvcc --arch=sm_20)
      printf("%f\n", x);
    }
};

using namespace thrust::placeholders;
void reduce_power_spectra(device_vectors_t *dv_p, int n_subband_pols, int n_chan) {

    // first, sum all of the fine (time) channels for each coarse channel (subband_pol)
    thrust::reduce_by_key(thrust::make_transform_iterator(thrust::counting_iterator<int>(0),    // beginning of the input key range 
                            linear_index_to_row_index<int>(n_chan)),                            //  (keyed by row (spectra) index)
                          thrust::make_transform_iterator(thrust::counting_iterator<int>(0),    // end of the input key range
                            linear_index_to_row_index<int>(n_chan)) + (n_subband_pols*n_chan),      
                          dv_p->powspec_p->begin(),                                             // beginning of the input (spectra) value range
                          dv_p->spectra_indices_p->begin(),                                     // beginning of the output (power sums) key range
                          dv_p->spectra_sums_p->begin(),                                        // beginning of the output (power sums) value range
                          thrust::equal_to<int>(),                                              // binary predicate used to determine equality of key
                          thrust::plus<float>());                                               // binary function used to accumulate values
    //thrust::for_each(dv_p->spectra_sums_p->begin(), dv_p->spectra_sums_p->end(), printf_functor());
    // now find the mean of each (TODO why won't the divide_by functor work here?)
    thrust::for_each(dv_p->spectra_sums_p->begin(), dv_p->spectra_sums_p->end(), _1 /= n_chan);
    //thrust::for_each(dv_p->spectra_sums_p->begin(), dv_p->spectra_sums_p->end(), printf_functor());
}

void compute_baseline(device_vectors_t *dv_p, int n_chan, int n_element, float smooth_scale) {
// Compute smoothed power spectrum baseline

    using thrust::make_transform_iterator;
    using thrust::make_counting_iterator;

    if(track_gpu_memory) get_gpu_mem_info("in compute_baseline(), right after making iterators");
    Stopwatch timer;
    if(use_timer) timer.start();
    thrust::exclusive_scan_by_key(make_transform_iterator(make_counting_iterator<int>(0),
                                                          //_1 / n_chan),
                                                          divide_by<int>(n_chan)),
                                  make_transform_iterator(make_counting_iterator<int>(n_element),
                                                          //_1 / n_chan),
                                                          divide_by<int>(n_chan)),
                                  dv_p->powspec_p->begin(),
                                  dv_p->scanned_p->begin());
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Scan time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
    if(track_gpu_memory) get_gpu_mem_info("in compute_baseline(), right after scan");
    
    if(use_timer) timer.start();
    const float* d_scanned_ptr = thrust::raw_pointer_cast(&(*dv_p->scanned_p)[0]);
  //const float* d_scanned_ptr = thrust::raw_pointer_cast(&(*dv.scanned_p   )[0]);
    thrust::transform(make_counting_iterator<uint>(0),
                      make_counting_iterator<uint>(n_element),
                      dv_p->baseline_p->begin(),
                      running_mean_by_region(smooth_scale,
                                             n_chan,
                                             d_scanned_ptr));
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Running mean time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
    if(track_gpu_memory) get_gpu_mem_info("in compute_baseline(), right after running mean by region");
}

void normalize_power_spectrum(device_vectors_t *dv_p) {

    Stopwatch timer;
    if(use_timer) timer.start();
    thrust::transform(dv_p->powspec_p->begin(), dv_p->powspec_p->end(),
                      dv_p->baseline_p->begin(),
                      dv_p->normalised_p->begin(),
                      //_1 / _2);
                      thrust::divides<float>());
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Normalisation time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
}

size_t find_hits(device_vectors_t *dv_p, int n_element, size_t maxhits, float power_thresh) {
// Extract and retrieve values exceeding the threshold

    using thrust::make_counting_iterator;

    size_t nhits;

    Stopwatch timer;
    if(use_timer) timer.start();
    dv_p->hit_indices_p->resize(n_element); // Note: Upper limit on required storage TODO - is n_element being set right?

    // Find normalised powers (S/N) over threshold.
    // The hit_indices vector will then index the powspec (detected powers) and baseline (mean powers) as well
    // as the normalized power (S/N) vector.
    nhits = thrust::copy_if(make_counting_iterator<int>(0),
                                   make_counting_iterator<int>(n_element),
                                   dv_p->normalised_p->begin(),  // stencil
                                   dv_p->hit_indices_p->begin(), // result
                                   //_1 > power_thresh) - dv_p->hit_indices_p->begin();
                                   greater_than_val<float>(power_thresh))
                                                          - dv_p->hit_indices_p->begin();

    nhits = nhits > maxhits ? maxhits : nhits;       // overrun protection - hits beyond maxgpuhits are thrown away
    dv_p->hit_indices_p->resize(nhits);                 // this will only be resized downwards
                                            
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Hit extraction time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
    
    if(use_timer) timer.start();
    // Retrieve (hit) detected and mean powers into their own vectors for ease of outputting.
    dv_p->hit_powers_p->resize(nhits);
    thrust::gather(dv_p->hit_indices_p->begin(), dv_p->hit_indices_p->end(),
                   dv_p->powspec_p->begin(),
                   dv_p->hit_powers_p->begin());
    dv_p->hit_baselines_p->resize(nhits);
    thrust::gather(dv_p->hit_indices_p->begin(), dv_p->hit_indices_p->end(),
                   dv_p->baseline_p->begin(),
                   dv_p->hit_baselines_p->begin());
    cudaThreadSynchronize();
    if(use_timer) timer.stop();
    if(use_timer) cout << "Hit info gather time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();

    return nhits;
}    

// AO spectra order goes as pol0chan0 pol0chan1    pol1chan0 pol1chan1    pol0chan2 pol0chan3    pol1chan2 pol1chan3... 
// (S0-C0-P0-Re), (S0-C0-P0-Im), (S0-C1-P0-Re), (S0-C1-P0-Im), (S0-C0-P1-Re), (S0-C0-P1-Im), (S0-C1-P1-Re), (S0-C1-P1-Im)
// foreach spectra
// 	foreach pair of channels
// 		for each pol
// 			8 bits Re, 8 bits Im
inline int ao_pol(long spectrum_index) {
    return((long)floor((double)spectrum_index/2) % 2);
}
inline int ao_coarse_chan(long spectrum_index) {
    return((long)floor((double)spectrum_index/4) * 2 + spectrum_index % 2);
}
// DiBAS (GBT) spectra order goes as pol0chan0 pol1chan0    pol0chan1 pol1chan1    pol0chan2 pol1chan3    pol0chan3 pol1chan3... 
// (S0-C0-P0-Re ), (S0-C0-P0-Im), (S0-C0-P1-Re), (S0-C0-P1-Im), (S0-C1-P0-Re), (S0-C1-P0-Im), (S0-C1-P1-Re), (S0-C1-P1-Im)
// foreach spectra
// 	foreach channel
// 		for each pol
// 			8 bits Re, 8 bits Im
inline int dibas_pol(long spectrum_index) {
    return ((long)(double)spectrum_index % 2);
}
inline int dibas_coarse_chan(long spectrum_index, int sub_spectrum_i) {
    return((long)floor((double)spectrum_index/2) + sub_spectrum_i * N_COARSE_CHAN / N_SUBSPECTRA_PER_SPECTRUM);
}

    
int spectroscopy(int n_subband,         // N coarse chan
                 int n_chan,            // N fine chan
                 int n_input,           // N pols
                 int bors,              // beam or subspectrum
                 size_t maxhits,
                 size_t maxgpuhits,
                 float power_thresh,
                 float smooth_scale,
                 uint64_t * input_data,
                 size_t n_input_data_bytes,
                 s6_output_block_t *s6_output_block,
                 device_vectors_t    *dv_p,
                 cufftHandle *fft_plan) {

// Note - beam or subspectra. Sometimes we are passed a beam's worth of coarse 
// channels (eg, at AO). At other times we are passed a subspectrum of channels  
// (eg, at GBT). In both cases, each course channel runs the full length of fine
// channels.
 
// Note - GPU memory allocation.  Our total memory needs are larger than the
// capcity of our current GPU (GeForce GTX 780 Ti with 3071MB). So we allocate 
// as needed and delete memory as soon as it is no longer needed.

    Stopwatch timer; 
    Stopwatch total_gpu_timer;
    int n_element = n_subband*n_chan*n_input;
    size_t nhits;
    //size_t prior_nhits=0;
    size_t total_nhits=0;

    if(track_gpu_memory) {
        char comment[256];
        sprintf(comment, "on entry to spectroscopy() : n_element = %d n_input_data_bytes = %d raw_timeseries_length in char2 = %d", 
                n_element, n_input_data_bytes, N_COARSE_CHAN / N_SUBSPECTRA_PER_SPECTRUM * N_FINE_CHAN * N_POLS_PER_BEAM);
        get_gpu_mem_info((const char *)comment);
    }

    char2 * h_raw_timeseries = (char2 *)input_data;

    if(use_total_gpu_timer) total_gpu_timer.start();

    // allocate GPU memory for the timeseries, FFTs and power spectra
    dv_p->fft_data_p         = new thrust::device_vector<float2>(n_element);
    dv_p->fft_data_out_p     = new thrust::device_vector<float2>(n_element);
    dv_p->powspec_p          = new thrust::device_vector<float>(n_element);
    dv_p->raw_timeseries_p   = new thrust::device_vector<char2>(N_COARSE_CHAN / N_SUBSPECTRA_PER_SPECTRUM * N_FINE_CHAN * N_POLS_PER_BEAM);

    // Copy to the device
    if(use_timer) timer.start();

//fprintf(stderr, "HERE 1 %p %p %p %p\n", dv_p->fft_data_p, dv_p->fft_data_out_p, dv_p->powspec_p, dv_p->raw_timeseries_p);

    thrust::copy(h_raw_timeseries, h_raw_timeseries + n_input_data_bytes / sizeof(char2),
                 //d_raw_timeseries.begin());
                 dv_p->raw_timeseries_p->begin());
    if(track_gpu_memory) get_gpu_mem_info("right after time series copy");
    if(use_timer) timer.stop();
    if(use_timer) cout << "H2D time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();

    if(use_timer) timer.start();
    // Unpack from 8-bit to floats
    thrust::transform(dv_p->raw_timeseries_p->begin(), 
                      dv_p->raw_timeseries_p->end(),
                      dv_p->fft_data_p->begin(),
                      convert_complex_8b_to_float());
    cudaThreadSynchronize();
    if(track_gpu_memory) get_gpu_mem_info("right after 8bit to float transform");
    if(use_timer) timer.stop();
    if(use_timer) cout << "Unpack time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
    
    // Input pointer varies with input.
    // Output pointer is constant - we reuse the output area for each input.
    // This is not true anymore - we analyze all inputs in one go. These
    // comments and this way of assigning fft_input_ptr and fft_output_ptr
    // are left as is in case we need to go back to one-input-at-a-time.
    float2* fft_input_ptr  = thrust::raw_pointer_cast(&((*dv_p->fft_data_p)[0]));
    float2* fft_output_ptr = thrust::raw_pointer_cast(&((*dv_p->fft_data_out_p)[0]));

    do_fft                      (fft_plan, fft_input_ptr, fft_output_ptr);
    if(track_gpu_memory) get_gpu_mem_info("right after FFT");
    compute_power_spectrum      (dv_p);

    // done with the timeseries and FFTs - delete the associated GPU memory
    if(track_gpu_memory) get_gpu_mem_info("right after compute power spectrum");
    delete(dv_p->raw_timeseries_p);         
    delete(dv_p->fft_data_p);         
    delete(dv_p->fft_data_out_p);     
    if(track_gpu_memory) get_gpu_mem_info("right after post power spectrum deletes");

    // reduce coarse channels to mean power...
    if(use_timer) timer.start();
    // allocate working vectors to accomodate all power spectra for this block :
    // all coarse channels (n_subbands) x both pols (n_input)
    dv_p->spectra_sums_p      = new thrust::device_vector<float>(n_subband*n_input);
    dv_p->spectra_indices_p   = new thrust::device_vector<int>(n_subband*n_input);
    if(track_gpu_memory) get_gpu_mem_info("right after vector allocation for coarse channel reduction");
    // do the reduce
    reduce_power_spectra(dv_p, n_subband*n_input, n_chan);
    if(track_gpu_memory) get_gpu_mem_info("right after coarse channel reduction");
    // copy the result to the output buffer, separating the pols. First, create the 
    // strided ranges (2 pols, so a stride of 2) then copy to the output block area
    // for this bors. Note: the "begin() + 1" is to get to the Y pol. 
    typedef thrust::device_vector<float>::iterator Iterator;
    strided_range<Iterator> polX(dv_p->spectra_sums_p->begin(),     dv_p->spectra_sums_p->end(), 2);
    strided_range<Iterator> polY(dv_p->spectra_sums_p->begin() + 1, dv_p->spectra_sums_p->end(), 2);
    thrust::copy(polX.begin(), polX.end(), &(s6_output_block->cc_pwrs_x[bors][0]));      
    thrust::copy(polY.begin(), polY.end(), &(s6_output_block->cc_pwrs_y[bors][0]));      
    // delete working vectors
    delete(dv_p->spectra_sums_p);
    delete(dv_p->spectra_indices_p);
    if(track_gpu_memory) get_gpu_mem_info("right after vector deletion for coarse channel reduction");
    if(use_timer) timer.stop();
    if(use_timer) cout << "Reduce coarse channels time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();
    // ...end reduce coarse channels to mean power

    // done with the timeseries and FFTs - delete the associated GPU memory
    //delete(dv_p->raw_timeseries_p);         
    //delete(dv_p->fft_data_p);         
    //delete(dv_p->fft_data_out_p);     
    // and allocate GPU memory for power normalization
    dv_p->baseline_p         = new thrust::device_vector<float>(n_element);
    if(track_gpu_memory) get_gpu_mem_info("right after baseline vector allocation");
    dv_p->normalised_p       = new thrust::device_vector<float>(n_element);
    if(track_gpu_memory) get_gpu_mem_info("right after normalized vector allocation");
    dv_p->scanned_p          = new thrust::device_vector<float>(n_element);
    if(track_gpu_memory) get_gpu_mem_info("right after scanned vector allocation");
    compute_baseline            (dv_p, n_chan, n_element, smooth_scale);        // not enough mem for this with 128m pt fft
    if(track_gpu_memory) get_gpu_mem_info("right after baseline computation");
    delete(dv_p->scanned_p);          
    if(track_gpu_memory) get_gpu_mem_info("right after scanned vector deletion");
    normalize_power_spectrum    (dv_p);
    if(track_gpu_memory) get_gpu_mem_info("right after spectrum normalization");
    nhits = find_hits           (dv_p, n_element, maxhits, power_thresh);
    if(track_gpu_memory) get_gpu_mem_info("right after find hits");
    // TODO should probably report if nhits == maxgpuhits, ie overflow
    
    // copy to return vector
    nhits = nhits > maxhits ? maxhits : nhits;
    if(use_timer) timer.start();

    total_nhits += nhits;
    s6_output_block->header.nhits[bors] = nhits;
    // We output both detected and mean powers (not S/N).
    thrust::copy(dv_p->hit_powers_p->begin(),    dv_p->hit_powers_p->end(),    &s6_output_block->power[bors][0]);      
    thrust::copy(dv_p->hit_baselines_p->begin(), dv_p->hit_baselines_p->end(), &s6_output_block->baseline[bors][0]);
    thrust::copy(dv_p->hit_indices_p->begin(),   dv_p->hit_indices_p->end(),   &s6_output_block->hit_indices[bors][0]);
    for(size_t i=0; i<nhits; ++i) {
        long hit_index                        = s6_output_block->hit_indices[bors][i]; 
        long spectrum_index                   = (long)floor((double)hit_index/n_chan);
#ifdef SOURCE_S6
        s6_output_block->pol[bors][i]         = ao_pol(spectrum_index);
        s6_output_block->coarse_chan[bors][i] = ao_coarse_chan(spectrum_index);
#elif SOURCE_DIBAS
        s6_output_block->pol[bors][i]         = dibas_pol(spectrum_index);    
        s6_output_block->coarse_chan[bors][i] = dibas_coarse_chan(spectrum_index, bors);
#endif
        s6_output_block->fine_chan[bors][i]   = hit_index % n_chan;
        //fprintf(stderr, "hit_index %ld spectrum_index %ld pol %d cchan %d fchan %d power %f\n", 
        //        hit_index, spectrum_index, s6_output_block->pol[bors][i], s6_output_block->coarse_chan[bors][i], 
        //        s6_output_block->fine_chan[bors][i], s6_output_block->power[bors][i]);
    }
        
    // delete remaining GPU memory
    delete(dv_p->powspec_p);          
    //delete(dv_p->scanned_p);          
    delete(dv_p->baseline_p);         
    delete(dv_p->normalised_p);       
       
    if(use_timer) timer.stop();
    if(use_timer) cout << "Copy to return vector time:\t" << timer.getTime() << endl;
    if(use_timer) timer.reset();

    if(use_total_gpu_timer) total_gpu_timer.stop();
    if(use_total_gpu_timer) cout << "Total GPU time:\t" << total_gpu_timer.getTime() << endl;
    if(use_total_gpu_timer) total_gpu_timer.reset();
    
    return total_nhits;
}
