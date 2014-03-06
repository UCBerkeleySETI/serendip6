#include <stdio.h>
#include <cufft.h>
#include <vector>
#include <algorithm>
#include "s6GPU.h"
#include "s6GPU_test.h"
#include "s6_fake_data.h"

#include "s6_databuf.h"
#include "s6_obs_data.h"
#include "s6_etfits.h"
#include "s6_obsaux.h"

using std::cout;
using std::endl;

s6_output_databuf_t   db;   // has to be here - too big for stack w/o using setrlimit()

void stage_hits(hits_t *hits_p, size_t nhits) {
    int block_idx = 0;
    static size_t ophits_i=0;

    s6_output_databuf_t * db_p = &db;

    for(size_t i=0; i<nhits; i++, ophits_i++) {
        db_p->block[block_idx].hits[ophits_i] = hits_p[i];
    }
    db_p->block[block_idx].header.nhits += nhits;
}

void write_efits(hits_t *hits_p, etfits_t *etf_p) {
    int rv;
    int block_idx = 0;

    scram_t   scram;
    scram_t * scram_p = &scram;

    s6_output_databuf_t * db_p = &db;

    // TODO uncommnet after solving redis segfault
    rv = get_obs_info_from_redis(scram_p, (char *)"redishost", 6379);
    fprintf(stderr, "rv from get_obs_info_from_redis() is %d\n", rv);

    rv = write_etfits(db_p, block_idx, etf_p, scram_p);
    fprintf(stderr, "rv from write_etfits is %d\n", rv);
}

void print_hits(hits_t *hits_p, size_t nhits) {
    //int nhits = hits.size();
    cout << "#Found " << nhits << " hits" << endl;
    cout << "#coarse_chan\tfine_chan\tpower\t\tbaseline\tstrength\tinput\tbeam" << endl;
    for( size_t i=0; i<nhits; ++i ) {
        cout << hits_p[i].coarse_chan << "\t\t"
             << hits_p[i].fine_chan << "\t\t"
             << hits_p[i].power << "\t"
             << hits_p[i].baseline << "\t"
             << hits_p[i].strength << "\t\t"
             << hits_p[i].input << "\t"
             << hits_p[i].beam << endl;
    }
}

int main(int argc, char* argv[]) {

    int retval;
    size_t nhits;
    bool write_fits=false;
    int beam_start = 0;
    int beams = 1;

    device_vectors_t * dv_p;
    cufftHandle fft_plan;
    cufftHandle *fft_plan_p = &fft_plan;

    std::vector<char2>   h_raw_timeseries_gen(NELEMENT);        // for generating data for 1 input
    std::vector<char2>   h_raw_timeseries(NELEMENT*NINPUT);     // holds all inputs

    uint64_t * input_data = (uint64_t *)&h_raw_timeseries[0];   // make it look like s6 data
    size_t n_input_data_bytes = NELEMENT*NINPUT*sizeof(char2);  

    hits_t hits[MAXHITS*NINPUT];
    hits_t * hits_p = &hits[0];

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--fits")) {
             write_fits = true;
        } else if (!strcmp(argv[i], "--beamstart")) {
             beam_start = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--beams")) {
             beams = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized arg: %s\n", argv[i]);
            exit(1);
        }
    }

    fprintf(stderr, "maxhits is %d Starting at beam %d and going for %d beams\n", MAXHITS, beam_start, beams);

    init_device(GPU_DEV);

    int     n_subband = NCHAN;
    int     n_chan    = NTIME;
    int     n_input   = NINPUT;
    size_t  nfft_      = n_chan;
    size_t  nbatch    = n_subband;
    int     istride   = n_subband*n_input;   // this effectively transposes the input data
    int     ostride   = 1;
    int     idist     = n_input;            // this takes care of the input interleave
    int     odist     = nfft_;
    create_fft_plan_1d_c2c(&fft_plan, istride, idist, ostride, odist, nfft_, nbatch);

    dv_p = init_device_vectors(NELEMENT, NINPUT);

    for(int input_i=0; input_i<NINPUT; input_i++) {
        gen_sig(2, input_i, h_raw_timeseries_gen);
        for(int j=0; j<NELEMENT; j++) {
            int src_locator  = j;
            int dst_locator  = j*NINPUT+input_i;
            h_raw_timeseries[dst_locator] = h_raw_timeseries_gen[src_locator];
        }
    }

    etfits_t etf;       
    init_etfits(&etf, 0);

    for(int beam=beam_start; beam < beam_start+beams; beam++) {
        nhits = spectroscopy(NCHAN, NTIME, NINPUT, beam, MAXHITS, MAXGPUHITS, POWER_THRESH, SMOOTH_SCALE, 
                             input_data, n_input_data_bytes, 
                             hits_p, dv_p, 
                             fft_plan_p);
        if(write_fits) {
            stage_hits(hits_p, nhits);
        }
    }

    if(write_fits) {
        write_efits(hits_p, &etf);
        etfits_close(&etf);
    } else {
        print_hits(hits_p, nhits);
    }

    delete_device_vectors(dv_p);
}
