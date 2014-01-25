/* s6_write_etfits.c */
#define _ISOC99_SOURCE  // For long double strtold
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <hiredis.h>
#include "s6_databuf.h"
#include "s6GPU.h"
#include "etfits.h"

#define DEBUGOUT 0


int etfits_create(struct etfits *etf) {
    int itmp, *status;
    char ctmp[40];
    //etfits_header_t *hdr;

    //hdr = &(etf->hdr);        // dereference the ptr to the header struct
    status = &(etf->status);  // dereference the ptr to the CFITSIO status

    // Initialize the key variables if needed
    if (etf->new_file == 1) {  // first time writing to the file
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
        etf->new_file = 0;
    }   // end first time writing to the file
    etf->filenum++;
    etf->rownum = 1;

    sprintf(etf->filename, "%s_%04d.fits", etf->basefilename, etf->filenum);

    // Create basic FITS file from our template
    char *s6_dir = getenv("S6_DIR");
    char template_file[1024];
    if (s6_dir==NULL) {
        s6_dir = (char *)".";
        fprintf(stderr, 
                "Warning: S6_DIR environment variable not set, using current directory for ETFITS template.\n");
    }
    printf("Opening file '%s'\n", etf->filename);
    sprintf(template_file, "%s/%s", s6_dir, ETFITS_TEMPLATE);
    fits_create_template(&(etf->fptr), etf->filename, template_file, status);

    // Check to see if file was successfully created
    if (*status) {
        fprintf(stderr, "Error creating sdfits file from template.\n");
        fits_report_error(stderr, *status);
        exit(1);
    }

    // Go to the primary HDU
    fits_movabs_hdu(etf->fptr, 1, NULL, status);

    // Update the keywords that need it
    fits_get_system_time(ctmp, &itmp, status);      // date the file was written
    fits_update_key(etf->fptr, TSTRING, "DATE", ctmp, NULL, status);

    // TODO update code versions

    // TODO ? This would be where to init primary header items that we do not init via a template file.

    return *status;
}


int etfits_write_hits(struct etfits *etf) {
    int row, *status, rv;
    int nchan, nivals, nsubband;
    char* temp_str;
    double temp_dbl;

    scram_t scram;
    coordcof_t coordcof;

    status = &(etf->status);         // dereference the ptr to the CFITSIO status

    // Create the initial file or change to a new one if needed.
    if (etf->new_file || (etf->multifile==1 && etf->rownum > etf->rows_per_file))
    {
        if (!etf->new_file) {
            printf("Closing file '%s'\n", etf->filename);
            fits_close_file(etf->fptr, status);
        }
        etfits_create(etf);
    }

    // get scram, etc data
    rv = get_obs_info_from_redis(scram, coordcof, (char *)"redishost", 6379);

    // write integration header - in s6_write_etfits.c for now
    fits_movnam_hdu(etf->fptr, BINARY_TBL, (char *)"AOSCRAM", 0, status);
    fits_report_error(stderr, *status);
    rv = write_integration_header(etf, scram);

    // Go to the first/next ET HDU and populate the header 
    fits_movnam_hdu(etf->fptr, BINARY_TBL, (char *)"ETHITS", 0, status);
    fits_report_error(stderr, *status);
    write_hits_header(etf);

    // write the hits themselves
    //write_hits_to_etfits(etf);

    // Now update some key values if no CFITSIO errors
#if 0
    if (!(*status)) {
        etf->rownum++;
        etf->tot_rows++;
        etf->N += 1;
        etf->T += dcols->exposure;
    }
#endif

fprintf(stderr, "status = %d  fptr = %p\n", *status, etf->fptr);

    return *status;
}

//int write_hits_to_etfits(s6_output_databuf_t *db, struct etfits * etf_p) {      // use typedef for etfits
int write_hits_to_etfits(struct etfits * etf_p) {      // use typedef for etfits
#define TFIELDS 3
    int fits_status;
    long nrows, firstrow, firstelem, nelements, colnum;
    int i, nhits;
    fits_status = 0; 
    static int first_time=1;

    std::vector<hits_t> hits;

    int tbltype                = BINARY_TBL;
    long long naxis2           = 0;
    //const int tfields          = 3;
    const char *ttype[TFIELDS] = {"det_pow", "mean_pow", "chan"};
    const char *tform[TFIELDS] = {"E",      "E",       "J"};     // cfitsio datatype codes 

    std::vector<float> det_pow;
    std::vector<float> mean_pow;
    std::vector<int>   chan;
    float* det_pow_p  = &det_pow[0];
    float* mean_pow_p = &mean_pow[0];
    int* chan_p       = &chan[0];

    nhits = hits.size();
    // why do I need to do this?
    for(i=0; i<nhits; i++) {
        det_pow.push_back(hits[i].power);
        mean_pow.push_back(hits[i].baseline);
        chan.push_back(hits[i].chan);
    }

    firstrow  = 1;
    firstelem = 1;

    // write primary header

    // for each input
        // write hits header
        // output detected power for all hits
        colnum    = 1;
        fits_write_col(etf_p->fptr, TFLOAT, colnum, firstrow, firstelem, nhits, det_pow_p, &fits_status);
        fits_report_error(stderr, fits_status);

        // output mean power for all hits
        colnum    = 2;
        fits_write_col(etf_p->fptr, TFLOAT, colnum, firstrow, firstelem, nhits, mean_pow_p, &fits_status);
        fits_report_error(stderr, fits_status);

        // output channel (freq?) for all hits
        colnum    = 3;
        fits_write_col(etf_p->fptr, TINT, colnum, firstrow, firstelem, nhits, chan_p, &fits_status);
        fits_report_error(stderr, fits_status);

        //fprintf(stdout, "close file\n");
        fits_close_file(etf_p->fptr, &fits_status);
        fits_report_error(stderr, fits_status);

    // commit this etFITS HDU ?

}

int write_primary_header() {

}

int write_integration_header(struct etfits *etf, scram_t &scram) {
    
    int status=0;


    // observatory (scram) data 
    fits_update_key(etf->fptr, TINT,    "PNTSTIME",  &(scram.PNTSTIME), NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "PNTRA",     &(scram.PNTRA),    NULL, &status);      
    fits_update_key(etf->fptr, TDOUBLE, "PNTDEC",    &(scram.PNTDEC),   NULL, &status);      
    fits_update_key(etf->fptr, TDOUBLE, "PNTMJD",    &(scram.PNTMJD),   NULL, &status);      

    fits_update_key(etf->fptr, TINT,    "AGCSTIME", &(scram.AGCSTIME),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "AGCAZ",    &(scram.AGCAZ),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "AGCZA",    &(scram.AGCZA),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "AGCTIME",  &(scram.AGCTIME),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "AGCLST",   &(scram.AGCLST),    NULL, &status); 

    fits_update_key(etf->fptr, TINT,    "ALFSTIME", &(scram.ALFSTIME),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "ALFBIAS1", &(scram.ALFBIAS1),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "ALFBIAS2", &(scram.ALFBIAS2),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "ALFMOPOS", &(scram.ALFMOPOS),    NULL, &status);

    fits_update_key(etf->fptr, TINT,    "IF1STIME", &(scram.IF1STIME),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "IF1SYNHZ", &(scram.IF1SYNHZ),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "IF1SYNDB", &(scram.IF1SYNDB),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "IF1RFFRQ", &(scram.IF1RFFRQ),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "IF1IFFRQ", &(scram.IF1IFFRQ),    NULL, &status);
    fits_update_key(etf->fptr, TINT,    "IF1ALFFB", &(scram.IF1ALFFB),    NULL, &status);

    fits_update_key(etf->fptr, TINT,    "IF2STIME", &(scram.IF2STIME),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "IF2ALFON", &(scram.IF2ALFON),    NULL, &status);

    fits_update_key(etf->fptr, TINT,    "TTSTIME",  &(scram.TTSTIME),    NULL, &status); 
    fits_update_key(etf->fptr, TINT,    "TTTURENC", &(scram.TTTURENC),    NULL, &status); 
    fits_update_key(etf->fptr, TDOUBLE, "TTTURDEG", &(scram.TTTURDEG),    NULL, &status);
#if 0
    fits_update_key(etf->fptr, TINT,    "MISSEDPK", &(scram.MISSEDPK),    NULL, &status);    // missed packets per input per second 
#endif

    fits_report_error(stderr, status);
}


int write_hits_header(struct etfits *etf) {

    int * status_p = &(etf->status);

    fits_update_key(etf->fptr, TINT,    "TIME",    &(etf->hits_hdr.time),    NULL, status_p);   
    fits_update_key(etf->fptr, TDOUBLE, "RA",      &(etf->hits_hdr.ra),      NULL, status_p);   
    fits_update_key(etf->fptr, TDOUBLE, "DEC",     &(etf->hits_hdr.dec),     NULL, status_p);   
    fits_update_key(etf->fptr, TINT,    "BEAMPOL", &(etf->hits_hdr.beampol), NULL, status_p);   
    fits_report_error(stderr, *status_p);
}

int etfits_close(struct etfits *etf) {
    if (!etf->status) {
        fits_close_file(etf->fptr, &(etf->status));
        printf("Closing file '%s'\n", etf->filename);
    }
    printf("Done.  %s %d data rows (%f sec) in %d files (status = %d).\n",
            etf->mode=='r' ? "Read" : "Wrote", 
            etf->tot_rows, etf->T, etf->filenum, etf->status);
    return etf->status;
}

int get_obs_info_from_redis(scram_t &scram, coordcof_t &coordcof, char *hostname, int port) {

    unsigned int j;
    redisContext *c;
    redisReply *reply;

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:PNT        PNTSTIME PNTRA PNTDEC PNTMJD");
    if ( reply->type == REDIS_REPLY_ERROR )
        printf( "Error: %s\n", reply->str );
    else if ( reply->type != REDIS_REPLY_ARRAY )
        printf( "Unexpected type: %d\n", reply->type );
    else {
    scram.PNTSTIME  = atoi(reply->element[0]->str);
    scram.PNTRA     = atof(reply->element[1]->str);
    scram.PNTDEC    = atof(reply->element[2]->str);
    scram.PNTMJD    = atof(reply->element[3]->str);
    }
    freeReplyObject(reply);
    printf("GET SCRAM:PNTSTIME %d\n", scram.PNTSTIME);
    printf("GET SCRAM:PNTRA %lf\n", scram.PNTRA);   
    printf("GET SCRAM:PNTDEC %lf\n", scram.PNTDEC);  
    printf("GET SCRAM:PNTMJD %lf\n", scram.PNTMJD);  

#if 0

    reply = redisCommand(c,"GET SCRAM:AGCSTIME");
    scram.AGCSTIME = atoi(reply->str);
    printf("GET SCRAM:AGCSTIME %s %d\n", reply->str, scram.AGCSTIME);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:AGCTIME"); 
    scram.AGCTIME = atoi(reply->str);
    printf("GET SCRAM:AGCTIME %s %d\n", reply->str, scram.AGCTIME); 
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:AGCAZ");   
    scram.AGCAZ = atof(reply->str);
    printf("GET SCRAM:AGCAZ %s %lf\n", reply->str, scram.AGCAZ);   
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:AGCZA");   
    scram.AGCZA = atof(reply->str);
    printf("GET SCRAM:AGCZA %s %lf\n", reply->str, scram.AGCZA);   
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:AGCLST");  
    scram.AGCLST = atof(reply->str);
    printf("GET SCRAM:AGCLST %s %lf\n", reply->str, scram.AGCLST);  
    //freeReplyObject(reply);


    reply = redisCommand(c,"GET SCRAM:ALFSTIME");
    scram.ALFSTIME = atoi(reply->str);
    printf("GET SCRAM:ALFSTIME %s %d\n", reply->str, scram.ALFSTIME);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:ALFBIAS1");
    scram.ALFBIAS1 = atoi(reply->str);
    printf("GET SCRAM:ALFBIAS1 %s %d\n", reply->str, scram.ALFBIAS1);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:ALFBIAS2");
    scram.ALFBIAS2 = atoi(reply->str);
    printf("GET SCRAM:ALFBIAS2 %s %d\n", reply->str, scram.ALFBIAS2);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:ALFMOPOS");
    scram.ALFMOPOS = atof(reply->str);
    printf("GET SCRAM:ALFMOPOS %s %lf\n", reply->str, scram.ALFMOPOS);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF1STIME");
    scram.IF1STIME = atoi(reply->str);
    printf("GET SCRAM:IF1STIME %s %d\n", reply->str, scram.IF1STIME);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF1SYNHZ");
    scram.IF1SYNHZ = atof(reply->str);
    printf("GET SCRAM:IF1SYNHZ %s %lf\n", reply->str, scram.IF1SYNHZ);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF1SYNDB");
    scram.IF1SYNDB = atoi(reply->str);
    printf("GET SCRAM:IF1SYNDB %s %d\n", reply->str, scram.IF1SYNDB);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF1RFFRQ");
    scram.IF1RFFRQ = atof(reply->str);
    printf("GET SCRAM:IF1IFFRQ %s %lf\n", reply->str, scram.IF1RFFRQ);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF1ALFFB");
    scram.IF1ALFFB = atoi(reply->str);
    printf("GET SCRAM:IF1ALFFB %s %d\n", reply->str, scram.IF1ALFFB);
    //freeReplyObject(reply);


    reply = redisCommand(c,"GET SCRAM:IF2STIME");
    scram.IF2STIME = atoi(reply->str);
    printf("GET SCRAM:IF2STIME %s %d\n", reply->str, scram.IF2STIME);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:IF2ALFON");
    scram.IF2ALFON = atoi(reply->str);
    printf("GET SCRAM:IF2ALFON %s %d\n", reply->str, scram.IF2ALFON);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:TTSTIME"); 
    scram.TTSTIME = atoi(reply->str);
    printf("GET SCRAM:TTSTIME  %s %d\n", reply->str, scram.TTSTIME);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:TTTURENC");
    scram.TTTURENC = atoi(reply->str);
    printf("GET SCRAM:TTTURENC %s %d\n", reply->str, scram.TTTURENC);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET SCRAM:TTTURDEG");
    scram.TTTURDEG = atof(reply->str);
    printf("GET SCRAM:TTTURDEG %s %lf\n", reply->str, scram.TTTURDEG);
    //freeReplyObject(reply);


    reply = redisCommand(c,"GET ZCORCOF00");
    coordcof.ZCORCOF00 = atof(reply->str);
    printf("GET ZCORCOF00 %s %lf\n", reply->str, coordcof.ZCORCOF00);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF01");
    coordcof.ZCORCOF01 = atof(reply->str);
    printf("GET ZCORCOF01 %s %lf\n", reply->str, coordcof.ZCORCOF01);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF02");
    coordcof.ZCORCOF02 = atof(reply->str);
    printf("GET ZCORCOF02 %s %lf\n", reply->str, coordcof.ZCORCOF02);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF03");
    coordcof.ZCORCOF03 = atof(reply->str);
    printf("GET ZCORCOF03 %s %lf\n", reply->str, coordcof.ZCORCOF03);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF04");
    coordcof.ZCORCOF04 = atof(reply->str);
    printf("GET ZCORCOF04 %s %lf\n", reply->str, coordcof.ZCORCOF04);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF05");
    coordcof.ZCORCOF05 = atof(reply->str);
    printf("GET ZCORCOF05 %s %lf\n", reply->str, coordcof.ZCORCOF05);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF06");
    coordcof.ZCORCOF06 = atof(reply->str);
    printf("GET ZCORCOF06 %s %lf\n", reply->str, coordcof.ZCORCOF06);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF07");
    coordcof.ZCORCOF07 = atof(reply->str);
    printf("GET ZCORCOF07 %s %lf\n", reply->str, coordcof.ZCORCOF07);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF08");
    coordcof.ZCORCOF08 = atof(reply->str);
    printf("GET ZCORCOF08 %s %lf\n", reply->str, coordcof.ZCORCOF08);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF09");
    coordcof.ZCORCOF09 = atof(reply->str);
    printf("GET ZCORCOF09 %s %lf\n", reply->str, coordcof.ZCORCOF09);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF10");
    coordcof.ZCORCOF10 = atof(reply->str);
    printf("GET ZCORCOF10 %s %lf\n", reply->str, coordcof.ZCORCOF10);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF11");
    coordcof.ZCORCOF11 = atof(reply->str);
    printf("GET ZCORCOF11 %s %lf\n", reply->str, coordcof.ZCORCOF11);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZCORCOF12");
    coordcof.ZCORCOF12 = atof(reply->str);
    printf("GET ZCORCOF12 %s %lf\n", reply->str, coordcof.ZCORCOF12);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET ACORCOF00");
    coordcof.ACORCOF00 = atof(reply->str);
    printf("GET ACORCOF00 %s %lf\n", reply->str, coordcof.ACORCOF00);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF01");
    coordcof.ACORCOF01 = atof(reply->str);
    printf("GET ACORCOF01 %s %lf\n", reply->str, coordcof.ACORCOF01);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF02");
    coordcof.ACORCOF02 = atof(reply->str);
    printf("GET ACORCOF02 %s %lf\n", reply->str, coordcof.ACORCOF02);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF03");
    coordcof.ACORCOF03 = atof(reply->str);
    printf("GET ACORCOF03 %s %lf\n", reply->str, coordcof.ACORCOF03);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF04");
    coordcof.ACORCOF04 = atof(reply->str);
    printf("GET ACORCOF04 %s %lf\n", reply->str, coordcof.ACORCOF04);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF05");
    coordcof.ACORCOF05 = atof(reply->str);
    printf("GET ACORCOF05 %s %lf\n", reply->str, coordcof.ACORCOF05);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF06");
    coordcof.ACORCOF06 = atof(reply->str);
    printf("GET ACORCOF06 %s %lf\n", reply->str, coordcof.ACORCOF06);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF07");
    coordcof.ACORCOF07 = atof(reply->str);
    printf("GET ACORCOF07 %s %lf\n", reply->str, coordcof.ACORCOF07);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF08");
    coordcof.ACORCOF08 = atof(reply->str);
    printf("GET ACORCOF08 %s %lf\n", reply->str, coordcof.ACORCOF08);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF09");
    coordcof.ACORCOF09 = atof(reply->str);
    printf("GET ACORCOF09 %s %lf\n", reply->str, coordcof.ACORCOF09);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF10");
    coordcof.ACORCOF10 = atof(reply->str);
    printf("GET ACORCOF10 %s %lf\n", reply->str, coordcof.ACORCOF10);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF11");
    coordcof.ACORCOF11 = atof(reply->str);
    printf("GET ACORCOF11 %s %lf\n", reply->str, coordcof.ACORCOF11);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ACORCOF12");
    coordcof.ACORCOF12 = atof(reply->str);
    printf("GET ACORCOF12 %s %lf\n", reply->str, coordcof.ACORCOF12);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET ZELLIPSE0");
    coordcof.ZELLIPSE0 = atof(reply->str);
    printf("GET ZELLIPSE0 %s %lf\n", reply->str, coordcof.ZELLIPSE0);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE1");
    coordcof.ZELLIPSE1 = atof(reply->str);
    printf("GET ZELLIPSE1 %s %lf\n", reply->str, coordcof.ZELLIPSE1);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE2");
    coordcof.ZELLIPSE2 = atof(reply->str);
    printf("GET ZELLIPSE2 %s %lf\n", reply->str, coordcof.ZELLIPSE2);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE3");
    coordcof.ZELLIPSE3 = atof(reply->str);
    printf("GET ZELLIPSE3 %s %lf\n", reply->str, coordcof.ZELLIPSE3);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE4");
    coordcof.ZELLIPSE4 = atof(reply->str);
    printf("GET ZELLIPSE4 %s %lf\n", reply->str, coordcof.ZELLIPSE4);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE5");
    coordcof.ZELLIPSE5 = atof(reply->str);
    printf("GET ZELLIPSE5 %s %lf\n", reply->str, coordcof.ZELLIPSE5);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ZELLIPSE6");
    coordcof.ZELLIPSE6 = atof(reply->str);
    printf("GET ZELLIPSE6 %s %lf\n", reply->str, coordcof.ZELLIPSE6);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET AELLIPSE0");
    coordcof.AELLIPSE0 = atof(reply->str);
    printf("GET AELLIPSE0 %s %lf\n", reply->str, coordcof.AELLIPSE0);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE1");
    coordcof.AELLIPSE1 = atof(reply->str);
    printf("GET AELLIPSE1 %s %lf\n", reply->str, coordcof.AELLIPSE1);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE2");
    coordcof.AELLIPSE2 = atof(reply->str);
    printf("GET AELLIPSE2 %s %lf\n", reply->str, coordcof.AELLIPSE2);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE3");
    coordcof.AELLIPSE3 = atof(reply->str);
    printf("GET AELLIPSE3 %s %lf\n", reply->str, coordcof.AELLIPSE3);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE4");
    coordcof.AELLIPSE4 = atof(reply->str);
    printf("GET AELLIPSE4 %s %lf\n", reply->str, coordcof.AELLIPSE4);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE5");
    coordcof.AELLIPSE5 = atof(reply->str);
    printf("GET AELLIPSE5 %s %lf\n", reply->str, coordcof.AELLIPSE5);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET AELLIPSE6");
    coordcof.AELLIPSE6 = atof(reply->str);
    printf("GET AELLIPSE6 %s %lf\n", reply->str, coordcof.AELLIPSE6);
    //freeReplyObject(reply);

    reply = redisCommand(c,"GET ARRANGLE0");
    coordcof.ARRANGLE0 = atof(reply->str);
    printf("GET ARRANGLE0 %s %lf\n", reply->str, coordcof.ARRANGLE0);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE1");
    coordcof.ARRANGLE1 = atof(reply->str);
    printf("GET ARRANGLE1 %s %lf\n", reply->str, coordcof.ARRANGLE1);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE2");
    coordcof.ARRANGLE2 = atof(reply->str);
    printf("GET ARRANGLE2 %s %lf\n", reply->str, coordcof.ARRANGLE2);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE3");
    coordcof.ARRANGLE3 = atof(reply->str);
    printf("GET ARRANGLE3 %s %lf\n", reply->str, coordcof.ARRANGLE3);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE4");
    coordcof.ARRANGLE4 = atof(reply->str);
    printf("GET ARRANGLE4 %s %lf\n", reply->str, coordcof.ARRANGLE4);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE5");
    coordcof.ARRANGLE5 = atof(reply->str);
    printf("GET ARRANGLE5 %s %lf\n", reply->str, coordcof.ARRANGLE5);
    //freeReplyObject(reply);
    reply = redisCommand(c,"GET ARRANGLE6");
    coordcof.ARRANGLE6 = atof(reply->str);
    printf("GET ARRANGLE6 %s %lf\n", reply->str, coordcof.ARRANGLE6);

    freeReplyObject(reply);

    /* Disconnects and frees the context */
    redisFree(c);

#endif
    return 0;
}
