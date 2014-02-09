/* s6_etfits.h */
#ifndef _ETFITS_H
#define _ETFITS_H

#include "fitsio.h"

#include "s6_databuf.h"

// The following is the max file length in GB
#define ETFITS_MAXFILELEN 1L

// The following is the template file to use to create a ETFITS file.
#define ETFITS_TEMPLATE "s6_ETFITS_template.txt"

typedef struct etfits_primary_header {
    char date[16];          // Date file was created (dd/mm/yy)  TODO does this need to be populated?
    int n_subband;
    int n_chan;
    int n_inputs;
    double bandwidth;
    double chan_bandwidth;
    double freq_res;     // redundant w/ chan_band_width?
} etfits_primary_header_t;

typedef struct etfits_integration_header {
    // TODO - do we reaslly need this struct?  All data come from scram
} etfits_integration_header_t;
    
typedef struct etfits_hits_header {
    time_t time;
    double ra;
    double dec;
    int    beampol;
    uint64_t missed_pkts;
    uint64_t nhits;
} etfits_hits_header_t;

typedef struct etfits_hits {
    float           detected_power;
    float           mean_power;
    unsigned long   fine_channel_bin;
    unsigned short  coarse_channel_bin;
} etfits_hits_t;
    
typedef struct etfits {
    char basefilename[200]; // The base filename from which to build the true filename
    char filename[200];     // Filename of the current PSRFITs file
    long long N;            // Current number of spectra written
    double T;               // Current duration of the observation written
    int filenum;            // The current number of the file in the scan (1-offset)
    int new_file;           // Indicates that a new file must be created.    
    int rownum;             // The current data row number to be written (1-offset)
    int tot_rows;           // The total number of data rows written so far
    int rows_per_file;      // The maximum number of data rows per file
    int status;             // The CFITSIO status value
    fitsfile *fptr;         // The CFITSIO file structure
    int multifile;          // Write multiple output files
    int quiet;              // Be quiet about writing each subint
    char mode;              // Read (r) or write (w).
    etfits_primary_header_t     primary_hdr;
    etfits_integration_header_t integration_hdr;
    etfits_hits_header_t        hits_hdr[N_BEAMS*N_POLS_PER_BEAM];       // one hits HDU per beam/pol per integration
    etfits_hits_t               hits;
} etfits_t;

int write_etfits(s6_output_databuf_t *db, int block_idx, etfits_t *etf, scram_t *scram_p);
int etfits_create(etfits_t *etf);
int etfits_close(etfits_t *etf);
int write_primary_header(etfits_t *etf);
int write_integration_header(etfits_t *etf, scram_t *scram);
int write_hits_header(etfits_t *etf, uint64_t nhits);
int write_hits(s6_output_databuf_t *db, int block_idx, etfits_t *etf);

#endif  // _ETFITS_H
