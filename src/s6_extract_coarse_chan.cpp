#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "s6_databuf.h"

int main(int argc, char ** argv) {

    int coarse_chan;
    int n_fine_chan;
    int beam;
    int pol;
    FILE * file_p;
    char * filename;
    char buff[N_FINE_CHAN*2];       // size for max

    coarse_chan = atoi(argv[1]);
    n_fine_chan = atoi(argv[2]);
    beam        = atoi(argv[3]);
    pol         = atoi(argv[4]);
    filename    = argv[5];

    // 0 indicates to use max
    if(n_fine_chan == 0) {
        n_fine_chan = N_FINE_CHAN;
    }

    file_p = fopen(filename,"r");

    fprintf(stderr, "cchan %d", coarse_chan);

    // postion file pointer...
    fprintf(stderr, " file pointer at %lx", ftell(file_p));
    // ...just beyond the buffer header
    fseek(file_p, 128, SEEK_SET);
    fprintf(stderr, " ...%lx ", ftell(file_p));
    // ...at start of the chosen beam
    fseek(file_p, beam * N_BYTES_PER_BEAM, SEEK_CUR);
    fprintf(stderr, " ... %lx", ftell(file_p));
    // ...at the start of the chosen pol of the chosen coarse channel 
    fseek(file_p, coarse_chan*N_POLS_PER_BEAM*N_BYTES_PER_SAMPLE + pol*N_BYTES_PER_SAMPLE, SEEK_CUR);
    fprintf(stderr, " ... %lx\n", ftell(file_p));
    
    // each iteration extracts 1 fine channel of the chosen coarse channel
    for(int i=0; i<n_fine_chan*N_BYTES_PER_SAMPLE; i+=N_BYTES_PER_SAMPLE) {
        // read real, imag
        fread(&buff[i],   1, 1, file_p);
        fread(&buff[i+1], 1, 1, file_p);

        //fprintf(stderr, "%02x\n", buff[i]);

        // we have to de-interlace the pols and de-stride the coarse channels
        fseek(file_p, (size_t)N_COARSE_CHAN*N_POLS_PER_BEAM*N_BYTES_PER_SAMPLE-N_BYTES_PER_SAMPLE, SEEK_CUR);
    }

    // OK, now output all fine channels of the chosen coarse channel
    for(int i=0; i<n_fine_chan*2; i=i+N_BYTES_PER_SAMPLE) {
        //fprintf(stderr, "%02x ", buff[i]);
        fprintf(stdout, "%d %d\n", buff[i], buff[i+1]);
    }
}

