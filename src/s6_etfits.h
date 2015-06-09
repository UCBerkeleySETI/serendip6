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
    char receiver[200];     // receiver used
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
    size_t nhits;
} etfits_hits_header_t;

typedef struct etfits_hits {
    float           detected_power;
    float           mean_power;
    unsigned long   fine_channel_bin;
    unsigned short  coarse_channel_bin;
} etfits_hits_t;
    
typedef struct etfits {
    char hostname[200];         // The hostname becomes part of the filename
    char basefilename[200];     // The base filename from which to build the true filename
    char filename_working[200]; // Filename of the current ETFITs file
    char filename_fits[200];    // We rename to mark the file as ready for post-processing or transfer
    char * s6_dir;
    long long N;            // Current number of spectra written TODO obsolete?
    double T;               // Current duration of the observation written TODO obsolete?
    int new_run;            // Indicates that this is a new s6 run   
    int new_file;           // indicates that we need a new file
    int file_num;           // used in naming files
    int file_chan;          // used in naming files
    int file_cnt;           // The current file count
    int integration_cnt;    // The current integration count
    int beampol_cnt;        // The current beampol count for this integration
    size_t tot_rows;        // The total number of data rows written this integration
    int integrations_per_file;      // The maximum number of data rows per file
    int max_file_size;
    int status;             // The CFITSIO status value
    fitsfile *fptr;         // The CFITSIO file structure
    int multifile;          // Write multiple output files TODO obsolete?
    int quiet;              // Be quiet about writing each subint
    char mode;              // Read (r) or write (w).
    int file_open;          // boolean indicating file open or not      TODO can I use fitsfile * for this?
    etfits_primary_header_t     primary_hdr;
    etfits_integration_header_t integration_hdr;
    etfits_hits_header_t        hits_hdr[N_BEAMS*N_POLS_PER_BEAM];       // one hits HDU per beam/pol per integration
    etfits_hits_t               hits;
} etfits_t;

int init_etfits(etfits_t *etf, int file_num_start);
int check_for_file_roll(etfits_t *etf);
int write_etfits(s6_output_databuf_t *db, int block_idx, etfits_t *etf, scram_t *scram_p);
int write_etfits_gbt(s6_output_databuf_t *db, int block_idx, etfits_t *etf, gbtstatus_t *gbtstatus_p);
int etfits_create(etfits_t *etf);
int etfits_close(etfits_t *etf);
int write_primary_header(etfits_t *etf);
int write_integration_header(etfits_t *etf, scram_t *scram);
int write_integration_header_gbt(etfits_t *etf, gbtstatus_t *gbtstatus);
int write_hits_header(etfits_t *etf, size_t nhits);
int write_hits(s6_output_databuf_t *db, int block_idx, etfits_t *etf);

#endif  // _ETFITS_H
