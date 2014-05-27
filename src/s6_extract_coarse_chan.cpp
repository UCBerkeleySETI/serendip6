#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "s6_databuf.h"

int main(int argc, char ** argv) {

    int coarse_chan;
    FILE * file_p;
    char * filename;
    char buff[N_FINE_CHAN*2];

    coarse_chan = atoi(argv[1]);
    filename    = argv[2];

    file_p = fopen(filename,"r");

    fseek(file_p, 128 + coarse_chan*N_POLS_PER_BEAM*N_BYTES_PER_SAMPLE, SEEK_SET);
    for(int i=0; i<N_FINE_CHAN*N_BYTES_PER_SAMPLE; i+=N_BYTES_PER_SAMPLE) {
        //fprintf(stderr, "%lx ", ftell(file_p));
        fread(&buff[i],   1, 1, file_p);
        fread(&buff[i+1], 1, 1, file_p);
        //fprintf(stderr, "%02x\n", buff[i]);
        fseek(file_p, (size_t)N_COARSE_CHAN*N_POLS_PER_BEAM*N_BYTES_PER_SAMPLE-N_BYTES_PER_SAMPLE, SEEK_CUR);
    }

    for(int i=0; i<N_FINE_CHAN*2; i=i+N_BYTES_PER_SAMPLE) {
        //fprintf(stderr, "%02x ", buff[i]);
        fprintf(stderr, "%d %d\n", buff[i], buff[i+1]);
    }
    fprintf(stderr, "\n");
}

