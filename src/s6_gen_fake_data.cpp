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

void gen_fake_data(uint64_t *data) {
    std::vector<char2>   h_raw_timeseries_gen(N_GPU_ELEMENTS);
    char2 * c2data = (char2 *)data;
    for(int input_i=0; input_i<N_POLS_PER_BEAM; input_i++) {
        gen_sig(2, input_i, h_raw_timeseries_gen);
        for(int j=0; j<N_GPU_ELEMENTS; j++) {
            int src_locator  = j;
            int dst_locator  = j*N_POLS_PER_BEAM+input_i;
            //fprintf(stderr, "src %d of %d  dst %d of %d\n", src_locator, h_raw_timeseries_gen.size(), dst_locator, h_raw_timeseries.size());
            c2data[dst_locator] = h_raw_timeseries_gen[src_locator];
        }
    }
    memcpy((void *)data, (const void *)c2data, N_BYTES_PER_BLOCK);
}
