/* s6_write_etfits.c */
#define _ISOC99_SOURCE  // For long double strtold
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <vector>
#include <hiredis/hiredis.h>
#include "hashpipe.h"
#include "s6_databuf.h"
#include "s6_obs_data.h"
#include "s6_obs_data_gbt.h"
#include "s6_etfits.h"
#include "s6_obsaux.h"

//----------------------------------------------------------
int init_etfits(etfits_t *etf, int start_file_num) {
//----------------------------------------------------------

    strcpy(etf->basefilename, "serendip6");     // TODO where to get file name?
    etf->file_num              = start_file_num;
    etf->file_cnt              = 0;
    etf->new_run               = 1;
    etf->new_file              = 1;
    etf->file_open             = 0;
    etf->multifile             = 1;
    etf->integrations_per_file = 3;    // TODO place holder - should come from status shmem
    etf->integration_cnt       = 0;    
    etf->max_file_size         = 1000000000; // 1GB    TODO runtime config

    etf->s6_dir = getenv("S6_DIR");
    if (etf->s6_dir==NULL) {
        etf->s6_dir = (char *)"/usr/local/etc";
        hashpipe_warn(__FUNCTION__, "S6_DIR environment variable not set, using /usr/local/etc for ETFITS template");
    }
}

//----------------------------------------------------------
int check_for_file_roll(etfits_t *etf) {
//----------------------------------------------------------
// checks if we need to roll over to a new file
    int * status_p = &(etf->status);
    struct stat st;
    fits_flush_file(etf->fptr, status_p);   // flush to get true size
    if(stat(etf->filename_working, &st) == -1) { 
        hashpipe_error(__FUNCTION__, "Error getting etfits file size");
    } else {
        //fprintf(stderr, "size of %s is %ld\n", etf->filename, st.st_size);
        if(st.st_size >= etf->max_file_size) {
            etf->file_num++;
            etf->new_file = 1;
        }
    }
    return *status_p;
}

//----------------------------------------------------------
int write_etfits(s6_output_databuf_t *db, int block_idx, etfits_t *etf, scram_t *scram_p) {
//----------------------------------------------------------
    int row, rv;
    int nchan, nivals, nsubband;
    char* temp_str;
    double temp_dbl;
    size_t nhits;

    int * status_p = &(etf->status);
    *status_p = 0;

    scram_t scram;

    extern const char *receiver[];

    // Create the initial file or change to a new one if needed.
    if (etf->new_run || etf->new_file) {
        etf->new_file = 0;
        if (!etf->new_run) {
            if(etf->file_open) {
                etfits_close(etf);
            }
            etf->integration_cnt = 0;
        }
        // TODO update code versions
        etf->primary_hdr.n_subband = db->block[block_idx].header.num_coarse_chan;
        etf->primary_hdr.n_chan    = N_FINE_CHAN;
        //etf->primary_hdr.n_inputs  = N_BEAMS * N_POLS_PER_BEAM;
        etf->primary_hdr.n_inputs  = N_BORS * N_POLS_PER_BEAM;
        strncpy(etf->primary_hdr.receiver, receiver[scram_p->receiver], sizeof(etf->primary_hdr.receiver));
        // TODO not yet implemented
        //etf->primary_hdr.bandwidth = ;
        //etf->primary_hdr.chan_bandwidth = ;
        //etf->primary_hdr.freq_res = ;
        etfits_create(etf);
        if(*status_p) {
            hashpipe_error(__FUNCTION__, "Error creating/initializing new etfits file");
            //fprintf(stderr, "Error creating/initializing new etfits file.\n");
            fits_report_error(stderr, *status_p);
            exit(1);
        }    
        write_primary_header(etf);
        etf->file_cnt++;
    }

    // populate hits header data
    // TODO maybe I should do away with this and write directly to the header
    //      from scram in write_hits_header()
    //for(int i=0; i < N_BEAMS*N_POLS_PER_BEAM; i++) {
    for(int i=0; i < N_BORS*N_POLS_PER_BEAM; i++) {
        etf->hits_hdr[i].time    = (time_t)s6_seti_ao_timeMS2unixtime(scram_p->AGCTIME, scram_p->AGCSTIME);
        etf->hits_hdr[i].ra      = scram_p->ra_by_beam[int(floor(i/N_POLS_PER_BEAM))];       
        etf->hits_hdr[i].dec     = scram_p->dec_by_beam[int(floor(i/N_POLS_PER_BEAM))];  
        etf->hits_hdr[i].beampol = i;       
    }

    if(! *status_p) write_integration_header(etf, scram_p);

    if(! *status_p) nhits = write_hits(db, block_idx, etf);

    etf->integration_cnt++;

    // Now update some key values if no CFITSIO errors
    if (! *status_p) {
        etf->tot_rows += nhits;
        etf->N += 1;
        *status_p = check_for_file_roll(etf);
    }

    if(*status_p) {
        hashpipe_error(__FUNCTION__, "FITS error, exiting");
        //fprintf(stderr, "FITS error, exiting.\n");
        exit(1);
    }

    return *status_p;
}

//----------------------------------------------------------
int write_etfits_gbt(s6_output_databuf_t *db, int block_idx, etfits_t *etf, gbtstatus_t *gbtstatus_p) {
//----------------------------------------------------------
    int row, rv;
    int nchan, nivals, nsubband;
    char* temp_str;
    double temp_dbl;
    size_t nhits;

    int * status_p = &(etf->status);
    *status_p = 0;

    // Create the initial file or change to a new one if needed.
    if (etf->new_run || etf->new_file) {
        etf->new_file = 0;
        if (!etf->new_run) {
            if(etf->file_open) {
                etfits_close(etf);
            }
            etf->integration_cnt = 0;
        }
        // TODO update code versions
        etf->primary_hdr.n_subband = db->block[block_idx].header.num_coarse_chan;
        etf->primary_hdr.n_chan    = N_FINE_CHAN;
        //etf->primary_hdr.n_inputs  = N_BEAMS * N_POLS_PER_BEAM;
        etf->primary_hdr.n_inputs  = N_BORS * N_POLS_PER_BEAM;
        // in scram, need to look up receiver from array? but in gbtstatus, it's already a string, so just copy it?...
        // strncpy(etf->primary_hdr.receiver, receiver[gbtstauts_p->receiver], sizeof(etf->primary_hdr.receiver));
        strncpy(etf->primary_hdr.receiver, gbtstatus_p->RECEIVER, sizeof(etf->primary_hdr.receiver));
        // TODO not yet implemented
        //etf->primary_hdr.bandwidth = ;
        //etf->primary_hdr.chan_bandwidth = ;
        //etf->primary_hdr.freq_res = ;
        etfits_create(etf);
        if(*status_p) {
            hashpipe_error(__FUNCTION__, "Error creating/initializing new etfits file");
            //fprintf(stderr, "Error creating/initializing new etfits file.\n");
            fits_report_error(stderr, *status_p);
            exit(1);
        }    
        write_primary_header(etf);
        etf->file_cnt++;
    }

    // populate hits header data
    // TODO maybe I should do away with this and write directly to the header
    //      from scram in write_hits_header()
    //for(int i=0; i < N_BEAMS*N_POLS_PER_BEAM; i++) {
    for(int i=0; i < N_BORS*N_POLS_PER_BEAM; i++) {
        etf->hits_hdr[i].time    = (time_t)((gbtstatus_p->MJD - 40587.0) * 86400);   
        etf->hits_hdr[i].ra      = gbtstatus_p->RADG_DRV;
        etf->hits_hdr[i].dec     = gbtstatus_p->DEC_DRV;
        etf->hits_hdr[i].beampol = i;       
    }

    if(! *status_p) write_integration_header_gbt(etf, gbtstatus_p);

    if(! *status_p) nhits = write_hits(db, block_idx, etf);

    etf->integration_cnt++;

    // Now update some key values if no CFITSIO errors
    if (! *status_p) {
        etf->tot_rows += nhits;
        etf->N += 1;
        *status_p = check_for_file_roll(etf);
    }

    if(*status_p) {
        hashpipe_error(__FUNCTION__, "FITS error, exiting");
        //fprintf(stderr, "FITS error, exiting.\n");
        exit(1);
    }

    return *status_p;
}

//----------------------------------------------------------
int etfits_create(etfits_t * etf) {
//----------------------------------------------------------
    int * status_p = &(etf->status);
    *status_p = 0;

    struct tm tm_now;
    time_t time_now;

    char file_name_str[256];

    // TODO enclose all init code in a do-as-needed block
    // Initialize the key variables if needed
    if (etf->new_run == 1) {  // first time writing this run
        etf->new_run = 0;
        etf->status = 0;
        etf->N = 0L;
        etf->T = 0.0;
        etf->mode = 'w';

        // Create the output directory if needed
        char datadir[1024];
        strncpy(datadir, etf->basefilename, 1023);
        char *last_slash = strrchr(datadir, '/');
        if (last_slash!=NULL && last_slash!=datadir) {
            *last_slash = '\0';
            printf("Using directory '%s' for output.\n", datadir);
            char cmd[1024];
            sprintf(cmd, "mkdir -m 1777 -p %s", datadir);
            system(cmd);
        }
    }   // end first time writing this run

    etf->tot_rows = 0;      // count rows per file

    // Form file name 
    time(&time_now);
    localtime_r(&time_now, &tm_now);
    sprintf(file_name_str, "%s_%04d_%04d%02d%02d_%02d%02d%02d", 
            etf->primary_hdr.receiver,
            etf->file_chan,
            1900+tm_now.tm_year, 1+tm_now.tm_mon, tm_now.tm_mday, 
            tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec); 
    sprintf(etf->filename_working, "%s_%s_%s.working", etf->basefilename, etf->hostname, file_name_str);  
    sprintf(etf->filename_fits,    "%s_%s_%s.fits",    etf->basefilename, etf->hostname, file_name_str);  

    // Create basic FITS file from our template
    char template_file[1024];
    //printf("Opening file '%s'\n", etf->filename);
#ifdef SOURCE_S6
    sprintf(template_file, "%s/%s", etf->s6_dir, ETFITS_TEMPLATE);
#elif SOURCE_DIBAS
    sprintf(template_file, "%s/%s", etf->s6_dir, ETFITS_GBT_TEMPLATE);
#endif
    if(! *status_p) fits_create_template(&(etf->fptr), etf->filename_working, template_file, status_p);

    // Check to see if file was successfully created
    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error creating sdfits file from template");
        //fprintf(stderr, "Error creating sdfits file from template.\n");
        fits_report_error(stderr, *status_p);
    } else {
        etf->file_open = 1;     // successful file open
    }

    return *status_p;
}

//----------------------------------------------------------
int etfits_close(etfits_t *etf) {
//----------------------------------------------------------
    int * status_p = &(etf->status);
    *status_p = 0;
    int rv;

    fits_close_file(etf->fptr, status_p);
    // TODO test for successful file close
    etf->file_open = 0;

    rv = rename((const char *)etf->filename_working, (const char *)etf->filename_fits);

    if(rv) {
        hashpipe_error(__FUNCTION__, "file rename error : %d %s", errno, strerror(errno));
    } else {
        hashpipe_info(__FUNCTION__, "Done.  %s %ld data rows into %s (status = %d).\n",
                etf->mode=='r' ? "Read" : "Wrote", 
                etf->tot_rows, etf->filename_fits, *status_p);
    }

    return *status_p;
}

//----------------------------------------------------------
int write_primary_header(etfits_t * etf) {
//----------------------------------------------------------
    int * status_p = &(etf->status);
    *status_p = 0;

    int itmp;
    char ctmp[40];

//fprintf(stderr, "writing primary header\n");

    if(! *status_p) fits_get_system_time(ctmp, &itmp, status_p);      // date the file was written

    if(! *status_p) fits_movabs_hdu(etf->fptr, 1, NULL, status_p);    // go to primary HDU

    if(! *status_p) fits_update_key(etf->fptr, TSTRING, "DATE",     ctmp,                            NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TSTRING, "TELESCOP", &(etf->primary_hdr.receiver),    NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "NSUBBAND", &(etf->primary_hdr.n_subband),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "NCHAN",    &(etf->primary_hdr.n_chan),      NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "NINPUTS",  &(etf->primary_hdr.n_inputs),    NULL, status_p); 
    // TODO not yet implemented
    //if(! *status_p) fits_update_key(etf->fptr, TINT,    "BANDWID",  &(etf->primary_hdr.bandwidth),       NULL, status_p); 
    //if(! *status_p) fits_update_key(etf->fptr, TINT,    "CHAN_BW",  &(etf->primary_hdr.chan_bandwidth),  NULL, status_p); 
    //if(! *status_p) its_update_key(etf->fptr, TINT,    "FREQRES",  &(etf->primary_hdr.freq_res),        NULL, status_p);    // redundant w/ CHAN_BW? 

    //if(! *status_p) fits_flush_file(etf->fptr, status_p);

    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error updating primary header");
        //fprintf(stderr, "Error updating primary header.\n");
        fits_report_error(stderr, *status_p);
    }

    return *status_p;
}


//----------------------------------------------------------
int write_integration_header_gbt(etfits_t * etf, gbtstatus_t *gbtstatus) {
//----------------------------------------------------------

    int * status_p = &(etf->status);
    *status_p = 0;

//fprintf(stderr, "writing integration header\n");
    if(etf->integration_cnt == 0) {
        // go to the template created HDU
        if(! *status_p) fits_movnam_hdu(etf->fptr, BINARY_TBL, (char *)"GBTSTATUS", 0, status_p);
    } else {
        // create new HDU
        if(! *status_p) fits_create_tbl(etf->fptr, BINARY_TBL, 0, 0, NULL, NULL, NULL, (char *)"GBTSTATUS", status_p);
    }

    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "EXTNAME",  (char *)"GBTSTATUS",  NULL, status_p); 
    // is there really an analogous/worthwhile similar value in gbtstatus?
    if(! *status_p) fits_update_key(etf->fptr, TINT,     "COARCHID", &(gbtstatus->coarse_chan_id),   NULL, status_p); 

    // observatory (gbt) data
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "LASTUPDT",  &(gbtstatus->LASTUPDT), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "LST",  &(gbtstatus->LST), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "UTC",  &(gbtstatus->UTC), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "MJD",  &(gbtstatus->MJD), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "EPOCH",  &(gbtstatus->EPOCH), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "MAJTYPE",  &(gbtstatus->MAJTYPE), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "MINTYPE",  &(gbtstatus->MINTYPE), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "MAJOR",  &(gbtstatus->MAJOR), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "MINOR",  &(gbtstatus->MINOR), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "AZCOMM",  &(gbtstatus->AZCOMM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "ELCOMM",  &(gbtstatus->ELCOMM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "AZACTUAL",  &(gbtstatus->AZACTUAL), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "ELACTUAL",  &(gbtstatus->ELACTUAL), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "AZERROR",  &(gbtstatus->AZERROR), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "ELERROR",  &(gbtstatus->ELERROR), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "LPCS",  &(gbtstatus->LPCS), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "FOCUSOFF",  &(gbtstatus->FOCUSOFF), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "ANTMOT",  &(gbtstatus->ANTMOT), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "RECEIVER",  &(gbtstatus->RECEIVER), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "IFFRQ1ST",  &(gbtstatus->IFFRQ1ST), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "IFFRQRST",  &(gbtstatus->IFFRQRST), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "FREQ",  &(gbtstatus->FREQ), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "VELFRAME",  &(gbtstatus->VELFRAME), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "VELDEF",  &(gbtstatus->VELDEF), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "J2000MAJ",  &(gbtstatus->J2000MAJ), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "J2000MIN",  &(gbtstatus->J2000MIN), NULL, status_p); 

    // the derived fields below are from s6_observatory_gbt but not from gbtstatus/mysql
    
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "LSTH_DRV",  &(gbtstatus->LSTH_DRV), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "RA_DRV",  &(gbtstatus->RA_DRV), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "RADG_DRV",  &(gbtstatus->RADG_DRV), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE,  "DEC_DRV",  &(gbtstatus->DEC_DRV), NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "CLOCKTIM",  &(gbtstatus->CLOCKTIM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "CLOCKFRQ",  &(gbtstatus->CLOCKFRQ), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "CLOCKDBM",  &(gbtstatus->CLOCKDBM), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "CLOCKLOC",  &(gbtstatus->CLOCKLOC), NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BIRDITIM",  &(gbtstatus->BIRDITIM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "BIRDIFRQ",  &(gbtstatus->BIRDIFRQ), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "BIRDIDBM",  &(gbtstatus->BIRDIDBM), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BIRDILOC",  &(gbtstatus->BIRDILOC), NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "ADCRMSTM",  &(gbtstatus->ADCRMSTM),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS01",  &(gbtstatus->ADC1RMS[0]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS02",  &(gbtstatus->ADC1RMS[1]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS03",  &(gbtstatus->ADC1RMS[2]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS04",  &(gbtstatus->ADC1RMS[3]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS05",  &(gbtstatus->ADC1RMS[4]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS06",  &(gbtstatus->ADC1RMS[5]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS07",  &(gbtstatus->ADC1RMS[6]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS08",  &(gbtstatus->ADC1RMS[7]), NULL, status_p);

    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error writing integration header");
        //fprintf(stderr, "Error writing integration header.\n");
        fits_report_error(stderr, *status_p);
    }

    return *status_p;
}



//----------------------------------------------------------
int write_integration_header(etfits_t * etf, scram_t *scram) {
//----------------------------------------------------------
    
    int * status_p = &(etf->status);
    *status_p = 0;

//fprintf(stderr, "writing integration header\n");
    if(etf->integration_cnt == 0) {
        // go to the template created HDU
        if(! *status_p) fits_movnam_hdu(etf->fptr, BINARY_TBL, (char *)"AOSCRAM", 0, status_p);
    } else {
        // create new HDU
        if(! *status_p) fits_create_tbl(etf->fptr, BINARY_TBL, 0, 0, NULL, NULL, NULL, (char *)"AOSCRAM", status_p);
    }

    if(! *status_p) fits_update_key(etf->fptr, TSTRING,  "EXTNAME",  (char *)"AOSCRAM",  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,     "COARCHID", &scram->coarse_chan_id,   NULL, status_p); 

    // observatory (scram) data 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "PNTSTIME",  &(scram->PNTSTIME), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "PNTRA",     &(scram->PNTRA),    NULL, status_p);      
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "PNTDEC",    &(scram->PNTDEC),   NULL, status_p);      
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "PNTMJD",    &(scram->PNTMJD),   NULL, status_p);      
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "PNTAZCOR",  &(scram->PNTAZCOR), NULL, status_p);      
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "PNTZACOR",  &(scram->PNTZACOR), NULL, status_p);      

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "AGCSTIME", &(scram->AGCSTIME),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "AGCAZ",    &(scram->AGCAZ),     NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "AGCZA",    &(scram->AGCZA),     NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "AGCTIME",  &(scram->AGCTIME),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "AGCLST",   &(scram->AGCLST),    NULL, status_p);     // TODO

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "ALFSTIME", &(scram->ALFSTIME),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "ALFBIAS1", &(scram->ALFBIAS1),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "ALFBIAS2", &(scram->ALFBIAS2),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ALFMOPOS", &(scram->ALFMOPOS),  NULL, status_p);

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF1STIME", &(scram->IF1STIME),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "IF1SYNHZ", &(scram->IF1SYNHZ),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF1SYNDB", &(scram->IF1SYNDB),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "IF1RFFRQ", &(scram->IF1RFFRQ),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "IF1IFFRQ", &(scram->IF1IFFRQ),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF1ALFFB", &(scram->IF1ALFFB),  NULL, status_p);

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF2STIME", &(scram->IF2STIME),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF2ALFON", &(scram->IF2ALFON),  NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "IF2SYNHZ", &(scram->IF2SYNHZ),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "IF2SIGSR", &(scram->IF2SIGSR),  NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TTSTIME",  &(scram->TTSTIME),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TTTURENC", &(scram->TTTURENC),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "TTTURDEG", &(scram->TTTURDEG),  NULL, status_p);

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "CLOCKTIM",  &(scram->CLOCKTIM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "CLOCKFRQ",  &(scram->CLOCKFRQ), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "CLOCKDBM",  &(scram->CLOCKDBM), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "CLOCKLOC",  &(scram->CLOCKLOC), NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BIRDITIM",  &(scram->BIRDITIM), NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "BIRDIFRQ",  &(scram->BIRDIFRQ), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "BIRDIDBM",  &(scram->BIRDIDBM), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BIRDILOC",  &(scram->BIRDILOC), NULL, status_p); 

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "ADCRMSTM",  &(scram->ADCRMSTM),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS01",  &(scram->ADC1RMS[0]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS02",  &(scram->ADC1RMS[1]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS03",  &(scram->ADC1RMS[2]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS04",  &(scram->ADC1RMS[3]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS05",  &(scram->ADC1RMS[4]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS06",  &(scram->ADC1RMS[5]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS07",  &(scram->ADC1RMS[6]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS08",  &(scram->ADC1RMS[7]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS09",  &(scram->ADC2RMS[0]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS10",  &(scram->ADC2RMS[1]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS11",  &(scram->ADC2RMS[2]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS12",  &(scram->ADC2RMS[3]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS13",  &(scram->ADC2RMS[4]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS14",  &(scram->ADC2RMS[5]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS15",  &(scram->ADC2RMS[6]), NULL, status_p);
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "ADCRMS16",  &(scram->ADC2RMS[7]), NULL, status_p);
#if 0
    fits_update_key(etf->fptr, TINT,    "MISSEDPK", &(scram.MISSEDPK),    NULL, status_p);    // missed packets per input per second 
#endif

    //if(! *status_p) fits_flush_file(etf->fptr, status_p);

    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error writing integration header");
        //fprintf(stderr, "Error writing integration header.\n");
        fits_report_error(stderr, *status_p);
    }

    return *status_p;
}

//----------------------------------------------------------
int write_hits_header(etfits_t * etf, int borspol, size_t nhits, size_t missed_pkts) {
//----------------------------------------------------------

#define TFIELDS 4
    int * status_p = &(etf->status);
    *status_p = 0;

    int tbltype                = BINARY_TBL;
    long long naxis2           = 0;
    //const int tfields          = 3;
    // TODO check chan types!
    const char *ttype[TFIELDS] = {"DETPOW  ", "MEANPOW ",  "COARCHAN", "FINECHAN"};
    const char *tform[TFIELDS] = {"1E",       "1E",        "1U",        "1V"};     // cfitsio format codes 
                             //     32-bit floats       16-bit unint   32-bit uint

    if(etf->integration_cnt == 0 && etf->beampol_cnt == 0) {
        // at start of file go to the template created HDU for this set of beampols
        if(! *status_p) fits_movnam_hdu(etf->fptr, BINARY_TBL, (char *)"ETHITS", 0, status_p);
    } else {
        // otherwise create new HDU for this set of beampols
        if(! *status_p) fits_create_tbl(etf->fptr, BINARY_TBL, 0, TFIELDS, (char **)&ttype, (char **)&tform, NULL, (char *)"ETHITS", status_p);
    }

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TIME",    &(etf->hits_hdr[borspol].time),    NULL, status_p);    
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "RA",      &(etf->hits_hdr[borspol].ra),      NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "DEC",     &(etf->hits_hdr[borspol].dec),     NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BORSPOL", &(etf->hits_hdr[borspol].beampol), NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "NHITS",   &nhits,                            NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "MISSEDPK",&missed_pkts,                      NULL, status_p);   

    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error writing hits header");
        fits_report_error(stderr, *status_p);
    }
}

//----------------------------------------------------------
int write_hits(s6_output_databuf_t *db, int block_idx, etfits_t *etf) {      
//----------------------------------------------------------

    long firstrow, firstelem, colnum;
    size_t nrows, hit_i, nhits_this_input;  
    int cur_beam, cur_input, cur_beampol;  

    size_t nhits = 0;

    int * status_p = &(etf->status);
    *status_p = 0;

    etf->beampol_cnt = 0;

//fprintf(stderr, "writing hits\n");
    //std::vector<hits_t> hits;

    float  det_pow[MAXHITS];
    float  mean_pow[MAXHITS];
    int    coarse_chan[MAXHITS];
    int    fine_chan[MAXHITS];

    firstrow  = 1;
    firstelem = 1;

    //for(int beam=0; beam < N_BEAMS; beam++) {
    for(int bors=0; bors < N_BORS; bors++) {
        // TODO - this goes through the output block for each beam twice. We could cut
        // this in half if we have a set of arrays for each pol (consuming twice the
        // memory).
        for(int input=0; input < N_POLS_PER_BEAM; input++) {        
            int borspol = bors * N_POLS_PER_BEAM + input;
            int hit_j=0;
            nhits_this_input=0;
            for(int hit_i=0; hit_i < (size_t)db->block[block_idx].header.nhits[bors]; hit_i++) {
                if(db->block[block_idx].pol[bors][hit_i] == input) {
                    int this_fine_chan =  db->block[block_idx].fine_chan[bors][hit_i];
                    if(this_fine_chan != 0) {   // ignore the DC bin
                        nhits_this_input++;
                        det_pow[hit_j]     = db->block[block_idx].power[bors][hit_i];
                        mean_pow[hit_j]    = db->block[block_idx].baseline[bors][hit_i];
                        coarse_chan[hit_j] = db->block[block_idx].coarse_chan[bors][hit_i];
                        fine_chan[hit_j]   = this_fine_chan;
                        hit_j++;
                    }
                }
            }

            write_hits_header(etf, 
                              borspol, 
                              nhits_this_input, 
                              (size_t)db->block[block_idx].header.missed_pkts[bors]);
            nhits += nhits_this_input;

            // write the hits for this input
            colnum      = 1;
            if(! *status_p) fits_write_col(etf->fptr, TFLOAT, colnum, firstrow, firstelem, nhits_this_input, det_pow, status_p);
            colnum      = 2;
            if(! *status_p) fits_write_col(etf->fptr, TFLOAT, colnum, firstrow, firstelem, nhits_this_input, mean_pow, status_p);
            colnum      = 3;
            if(! *status_p) fits_write_col(etf->fptr, TINT, colnum, firstrow, firstelem, nhits_this_input, coarse_chan, status_p);
            colnum      = 4;
            if(! *status_p) fits_write_col(etf->fptr, TINT, colnum, firstrow, firstelem, nhits_this_input, fine_chan, status_p);

            etf->beampol_cnt++;
        } 
    }

    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error writing hits");
        //fprintf(stderr, "Error writing hits.\n");
        fits_report_error(stderr, *status_p);
    }

    // non-standard return
    if(*status_p) {
        return -1;
    } else {
        return nhits;
    }
}
