/* s6_write_etfits.c */
#define _ISOC99_SOURCE  // For long double strtold
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <vector>
#include <hiredis.h>
#include "hashpipe.h"
#include "s6_databuf.h"
#include "s6_obs_data.h"
#include "s6_etfits.h"

//----------------------------------------------------------
int init_etfits(etfits_t *etf, int start_file_num) {
//----------------------------------------------------------

    strcpy(etf->basefilename, "data/serendip6");     // TODO where to get file name?
    etf->file_num              = start_file_num;
    etf->file_cnt              = 0;
    etf->new_run               = 1;
    etf->new_file              = 1;
    etf->multifile             = 1;
    etf->integrations_per_file = 3;    // TODO place holder - should come from status shmem
    etf->integration_cnt       = 0;    
    etf->max_file_size         = 1000000; // 1MB    TODO runtime config

    etf->s6_dir = getenv("S6_DIR");
    if (etf->s6_dir==NULL) {
        etf->s6_dir = (char *)".";
        hashpipe_warn(__FUNCTION__, "S6_DIR environment variable not set, using current directory for ETFITS template");
    }
}

//----------------------------------------------------------
int check_for_file_roll(etfits_t *etf) {
//----------------------------------------------------------
// checks if we need to roll over to a new file
    int * status_p = &(etf->status);
    struct stat st;
    fits_flush_file(etf->fptr, status_p);   // flush to get true size
    if(stat(etf->filename, &st) == -1) { 
        hashpipe_error(__FUNCTION__, "Error getting etfits file size");
    } else {
        fprintf(stderr, "size of %s is %ld\n", etf->filename, st.st_size);
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

    // Create the initial file or change to a new one if needed.
    if (etf->new_run || etf->new_file) {
        etf->new_file = 0;
        if (!etf->new_run) {
            etfits_close(etf);
            etf->integration_cnt = 0;
        }
        etfits_create(etf);
        if(*status_p) {
            hashpipe_error(__FUNCTION__, "Error creating/initializing new etfits file");
            //fprintf(stderr, "Error creating/initializing new etfits file.\n");
            fits_report_error(stderr, *status_p);
            exit(1);
        }    
        // TODO this could be done once per run rather than once per file
        // TODO some (all?) of these will come from iput parms (status shmem)
        // TODO update code versions
        etf->primary_hdr.n_subband = N_COARSE_CHAN;
        etf->primary_hdr.n_chan    = N_FINE_CHAN;
        etf->primary_hdr.n_inputs  = N_BEAMS * N_POLS_PER_BEAM;
        // TODO not yet implemented
        //etf->primary_hdr.bandwidth = ;
        //etf->primary_hdr.chan_bandwidth = ;
        //etf->primary_hdr.freq_res = ;
        write_primary_header(etf);
        etf->file_cnt++;
    }

    // populate hits header data
    // TODO maybe I should do away with this and write directly to the header
    //      from sram in write_hits_header()
    for(int i=0; i < N_BEAMS*N_POLS_PER_BEAM; i++) {
        etf->hits_hdr[i].time    = scram_p->AGCTIME;     // TODO is this right?   
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
int etfits_create(etfits_t * etf) {
//----------------------------------------------------------
    int * status_p = &(etf->status);
    *status_p = 0;

    struct tm tm_now;
    time_t time_now;

    char file_name_str[256];

    // TODO enclose all init code in a do-as-needed block
    // Initialize the key variables if needed
    if (etf->new_run == 1) {  // first time writing to the file
        etf->new_run = 0;
        etf->status = 0;
        etf->tot_rows = 0;
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
    }   // end first time writing to the file

    // Form file name 
    time(&time_now);
    localtime_r(&time_now, &tm_now);
    sprintf(file_name_str, "%04d_%04d%02d%02d_%02d%02d%02d", 
            etf->file_chan,
            1900+tm_now.tm_year, 1+tm_now.tm_mon, tm_now.tm_mday, 
            tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec); 
    sprintf(etf->filename, "%s_%s.fits", etf->basefilename, file_name_str);  

    // Create basic FITS file from our template
    char template_file[1024];
    printf("Opening file '%s'\n", etf->filename);
    sprintf(template_file, "%s/%s", etf->s6_dir, ETFITS_TEMPLATE);
    if(! *status_p) fits_create_template(&(etf->fptr), etf->filename, template_file, status_p);

    // Check to see if file was successfully created
    if (*status_p) {
        hashpipe_error(__FUNCTION__, "Error creating sdfits file from template");
        //fprintf(stderr, "Error creating sdfits file from template.\n");
        fits_report_error(stderr, *status_p);
    }

    return *status_p;
}

//----------------------------------------------------------
int etfits_close(etfits_t *etf) {
//----------------------------------------------------------
    int * status_p = &(etf->status);
    *status_p = 0;

//fprintf(stderr, "in close, status is %d\n", *status_p);
    if (! *status_p) {
        fits_close_file(etf->fptr, status_p);
        printf("Closing file '%s'\n", etf->filename);
    }
    printf("Done.  %s %ld data rows in %d files (status = %d).\n",
            etf->mode=='r' ? "Read" : "Wrote", 
            etf->tot_rows, etf->file_cnt, *status_p);

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

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TTSTIME",  &(scram->TTSTIME),   NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TTTURENC", &(scram->TTTURENC),  NULL, status_p); 
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "TTTURDEG", &(scram->TTTURDEG),  NULL, status_p);
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
int write_hits_header(etfits_t * etf, int beampol, size_t nhits) {
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

    if(! *status_p) fits_update_key(etf->fptr, TINT,    "TIME",    &(etf->hits_hdr[beampol].time),    NULL, status_p);    
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "RA",      &(etf->hits_hdr[beampol].ra),      NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TDOUBLE, "DEC",     &(etf->hits_hdr[beampol].dec),     NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "BEAMPOL", &(etf->hits_hdr[beampol].beampol), NULL, status_p);   
    if(! *status_p) fits_update_key(etf->fptr, TINT,    "NHITS",   &nhits,                            NULL, status_p);   

//fprintf(stderr, "writing hits header. beampol %d nhits : %ld\n", etf->hits_hdr[beampol].beampol, nhits);

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

    size_t nhits = db->block[block_idx].header.nhits;

    int * status_p = &(etf->status);
    *status_p = 0;

    etf->beampol_cnt = 0;

//fprintf(stderr, "writing hits\n");
    //std::vector<hits_t> hits;

    std::vector<float> det_pow;
    std::vector<float> mean_pow;
    std::vector<int>   coarse_chan;
    std::vector<int>   fine_chan;
    float* det_pow_p;
    float* mean_pow_p;
    int* coarse_chan_p;
    int* fine_chan_p;

    firstrow  = 1;
    firstelem = 1;

    hit_i = 0;              
    while(hit_i < nhits) {
//fprintf(stderr, "hit_i %ld nhits %ld\n", hit_i, nhits);
        // init for first / next input
        det_pow.clear();
        mean_pow.clear();
        coarse_chan.clear();
        fine_chan.clear();
        nhits_this_input = 0;

        // calculate hits header fields and populate the header 
        cur_beam    = db->block[block_idx].hits[hit_i].beam;
        cur_input   = db->block[block_idx].hits[hit_i].input;
        cur_beampol = cur_beam * N_POLS_PER_BEAM + cur_input;
//fprintf(stderr, "at hit_i %ld : writing header for beam %d input %d beampol %d\n", hit_i, cur_beam, cur_input, etf->hits_hdr[cur_beampol].beampol);
        etf->hits_hdr[cur_beampol].beampol = cur_beampol;

        // separate the data columns for this input
        // TODO - if this is too slow, we can make hits 4 separate arrays rather
        // than an array of structs
        while(db->block[block_idx].hits[hit_i].input == cur_input && hit_i < nhits) {
            det_pow.push_back       (db->block[block_idx].hits[hit_i].power);
            mean_pow.push_back      (db->block[block_idx].hits[hit_i].baseline);
            coarse_chan.push_back   (db->block[block_idx].hits[hit_i].coarse_chan);
            fine_chan.push_back     (db->block[block_idx].hits[hit_i].fine_chan);
            nhits_this_input++;
            hit_i++;
        }
        // hit_i should now reference next input or one past all inputs
//fprintf(stderr, "det_pow.size %ld nhits_this_input %ld\n", det_pow.size(), nhits_this_input);

        write_hits_header(etf, cur_beampol, nhits_this_input);

        // write the hits for this input
        det_pow_p     = &det_pow[0];
        mean_pow_p    = &mean_pow[0];
        coarse_chan_p = &coarse_chan[0];
        fine_chan_p   = &fine_chan[0];
        colnum      = 1;
        if(! *status_p) fits_write_col(etf->fptr, TFLOAT, colnum, firstrow, firstelem, nhits_this_input, det_pow_p, status_p);
        colnum      = 2;
        if(! *status_p) fits_write_col(etf->fptr, TFLOAT, colnum, firstrow, firstelem, nhits_this_input, mean_pow_p, status_p);
        colnum      = 3;
        if(! *status_p) fits_write_col(etf->fptr, TINT, colnum, firstrow, firstelem, nhits_this_input, coarse_chan_p, status_p);
        colnum      = 4;
        if(! *status_p) fits_write_col(etf->fptr, TINT, colnum, firstrow, firstelem, nhits_this_input, fine_chan_p, status_p);

        etf->beampol_cnt++;
    }  // end while hit_i < nhits

    //etf->rownum += nhits;

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
