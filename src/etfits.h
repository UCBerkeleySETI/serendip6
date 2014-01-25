/* sdfits.h */
#ifndef _ETFITS_H
#define _ETFITS_H
#include "fitsio.h"

// The following is the max file length in GB
#define ETFITS_MAXFILELEN 1L

// The following is the template file to use to create a PSRFITS file.
// Path is relative to VEGAS_DIR environment variable.
//#define ETFITS_TEMPLATE "/s6_config/s6_ETFITS_template.txt"
#define ETFITS_TEMPLATE "s6_ETFITS_template.txt"

typedef struct {
    char date[16];          // Date file was created (dd/mm/yy)
} etfits_primary_header_t;

typedef struct {
    char telescope[16];     // Telescope used
    double bandwidth;       // Bandwidth of the entire backend
    double freqres;         // Width of each spectral channel in the file
    char date_obs[16];      // Date of observation (dd/mm/yy)
    double tsys;            // System temperature

    char projid[16];        // The project ID
    char frontend[16];      // Frontend used
    double obsfreq;         // Centre frequency for observation
    double scan;            // Scan number (float)

    char instrument[16];       // Backend or instrument used
    char cal_mode[16];      // Cal mode (OFF, SYNC, EXT1, EXT2
    double cal_freq;        // Cal modulation frequency (Hz)
    double cal_dcyc;        // Cal duty cycle (0-1)
    double cal_phs;         // Cal phase (wrt start time)
    int npol;               // Number of antenna polarisations (normally 2)
    int nchan;              // Number of spectral bins per sub-band
    double chan_bw;         // Width of each spectral bin

    int nsubband;           // Number of sub-bands
    double efsampfr;        // Effective sampling frequency (after decimation)
    double fpgaclk;         // FPGA clock rate [Hz]
    double hwexposr;        // Duration of fixed integration on FPGA/GPU [s]
    double filtnep;         // PFB filter noise-equivalent parameter
    double sttmjd;          // Observation start time [double MJD]
} etfits_header_t;
    
typedef struct {
    double time;            // MJD start of integration (from system time)
    unsigned long int time_counter;       // FPGA time counter at start of integration
    int integ_num;          // The integration number (indicates a specific integ. period)
    float exposure;         // Effective integration time (seconds)
    char object[16];        // Object being viewed
    float azimuth;          // Commanded azimuth
    float elevation;        // Commanded elevation
    float bmaj;             // Beam major axis length (deg)
    float bmin;             // Beam minor axis length (deg)
    float bpa;              // Beam position angle (deg)

    int accumid;            // ID of the accumulator from where the spectrum came
    int sttspec;            // SPECTRUM_COUNT of the first spectrum in the integration
    int stpspec;            // SPECTRUM_COUNT of the last spectrum in the integration

    float centre_freq_idx;  // Index of centre frequency bin
    double centre_freq[128];  // Frequency at centre of each sub-band
    double ra;              // RA mid-integration
    double dec;             // DEC mid-integration

    char data_len[16];      // Length of the data array
    char data_dims[16];     // Data matrix dimensions
    unsigned char *data;    // Ptr to the raw data itself
} etfits_data_columns_t;

struct etfits
{
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
    etfits_primary_header_t primary_hdr;
    etfits_header_t         hdr;
    etfits_data_columns_t   data_columns;
};

// In write_sdfits.c
int etfits_create(struct etfits *etf);
int etfits_close(struct etfits *etf);
int etftits_write_subint(struct etfits *etf);

//=========================================================================

    typedef struct {
        int     PNTSTIME;
        double  PNTRA;   
        double  PNTDEC;  
        double  PNTMJD;  
        int     AGCSTIME;
        int     AGCTIME; 
        double  AGCAZ;   
        double  AGCZA;   
        double  AGCLST;  
        int     ALFSTIME;
        int     ALFBIAS1;
        int     ALFBIAS2;
        double  ALFMOPOS;
        int     IF1STIME;
        double  IF1SYNHZ;
        int     IF1SYNDB;
        double  IF1RFFRQ;
        double  IF1IFFRQ;
        int     IF1ALFFB;
        int     IF2STIME;
        int     IF2ALFON;
        int     TTSTIME; 
        int     TTTURENC;
        double  TTTURDEG;
    } scram_t;

    typedef struct {
        double  ZCORCOF00;
        double  ZCORCOF01;
        double  ZCORCOF02;
        double  ZCORCOF03;
        double  ZCORCOF04;
        double  ZCORCOF05;
        double  ZCORCOF06;
        double  ZCORCOF07;
        double  ZCORCOF08;
        double  ZCORCOF09;
        double  ZCORCOF10;
        double  ZCORCOF11;
        double  ZCORCOF12;
        double  ACORCOF00;
        double  ACORCOF01;
        double  ACORCOF02;
        double  ACORCOF03;
        double  ACORCOF04;
        double  ACORCOF05;
        double  ACORCOF06;
        double  ACORCOF07;
        double  ACORCOF08;
        double  ACORCOF09;
        double  ACORCOF10;
        double  ACORCOF11;
        double  ACORCOF12;
        double  ZELLIPSE0;
        double  ZELLIPSE1;
        double  ZELLIPSE2;
        double  ZELLIPSE3;
        double  ZELLIPSE4;
        double  ZELLIPSE5;
        double  ZELLIPSE6;
        double  AELLIPSE0;
        double  AELLIPSE1;
        double  AELLIPSE2;
        double  AELLIPSE3;
        double  AELLIPSE4;
        double  AELLIPSE5;
        double  AELLIPSE6;
        double  ARRANGLE0;
        double  ARRANGLE1;
        double  ARRANGLE2;
        double  ARRANGLE3;
        double  ARRANGLE4;
        double  ARRANGLE5;
        double  ARRANGLE6;
    } coordcof_t;

int get_obs_info_from_redis(scram_t &scram, coordcof_t &coordcof, char *hostname, int port);
int write_integration_header(struct etfits *etf, scram_t &scram);
int write_header_to_etfits();
int write_hits_to_etfits(struct etfits *etf);
int etfits_write_hits(struct etfits *etf);
//=========================================================================

// In read_psrfits.c
// int psrfits_open(struct psrfits *pf);
// int psrfits_read_subint(struct psrfits *pf);
// int psrfits_read_part_DATA(struct psrfits *pf, int N, char *buffer);

#endif
