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
    unsigned char buff[256];

    coarse_chan = atoi(argv[1]);
    filename    = argv[2];

    file_p = fopen(filename,"r");

    fseek(file_p, coarse_chan, SEEK_SET);
    for(int i=0; i<10; i++) {
        fread(&buff[i], 1, 1, file_p);
        fseek(file_p, 1, SEEK_CUR);
    }

    for(int i=0; i<10; i++) {
        fprintf(stderr, "%02x ", buff[i]);
    }
    fprintf(stderr, "\n");
}

