/* sdfits.h */
#ifndef _ETFITS_H
#define _ETFITS_H
#include "fitsio.h"

// The following is the max file length in GB
#define ETFITS_MAXFILELEN 1L

// The following is the template file to use to create a ETFITS file.
#define ETFITS_TEMPLATE "s6_ETFITS_template.txt"

typedef struct {
    char date[16];          // Date file was created (dd/mm/yy)
    // TODO ? This would be where to put primary header items that we do not init via a template file.
} etfits_primary_header_t;

typedef struct {

} etfits_integration_header_t;
    
typedef struct {
    time_t time;
    double ra;
    double dec;
    int    beampol;
    

} etfits_hits_header_t;

typedef struct {
    float           detected_power;
    float           mean_power;
    unsigned long   fine_channel_bin;
    unsigned short  coarse_channel_bin;
} etfits_hits_t;
    
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
    etfits_primary_header_t     primary_hdr;
    etfits_integration_header_t integration_hdr;
    etfits_hits_header_t        hits_hdr;       // one hits HDU per beam/pol per integration
    etfits_hits_t               hits;
};

// In write_sdfits.c
int etfits_create(struct etfits *etf);
int etfits_close(struct etfits *etf);
int etftits_write_subint(struct etfits *etf);
int write_hits_header(struct etfits *etf);

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
