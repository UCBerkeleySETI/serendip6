#ifndef _S6_OBS_DATA_H
#define _S6_OBS_DATA_H

#include "s6_databuf.h"

// these values were pulled from SETI@home datarecorder2 
#define ScramLagTolerance           10     // in seconds
#define TT_TurretDegreesAlfa        26.64
#define TT_TurretDegreesTolerance   1.0

// receivers that s6 uses
#define RECEIVER_NONE    0
#define RECEIVER_ALFA    1
#define RECEIVER_327MHZ  2

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

typedef struct scram {
    int     PNTSTIME;
    double  PNTRA;   
    double  PNTDEC;  
    double  PNTMJD;  
    double  PNTAZCOR;  
    double  PNTZACOR;  
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
    double  IF2SYNHZ;   
    int     IF2ALFON;
    int     TTSTIME; 
    int     TTTURENC;   
    double  TTTURDEG;  
    int     DERTIME;
    double  ra_by_beam[N_BEAMS];   
    double  dec_by_beam[N_BEAMS];
    int     rec_alfa_enabled;
    int     rec_327_enabled;
    int     receiver;
    // the fields below are not from scram
    int     coarse_chan_id;     
    int     ADCRMSTM;
    double  ADC1RMS[N_ADCS_PER_ROACH2];
    double  ADC2RMS[N_ADCS_PER_ROACH2];
    int     CLOCKTIM;
    double  CLOCKFRQ;
    double  CLOCKDBM;
    int     CLOCKLOC;       
    int     BIRDITIM;
    double  BIRDIFRQ;
    double  BIRDIDBM;
    int     BIRDILOC;       
} scram_t;

int get_obs_info_from_redis(scram_t *scram, char *hostname, int port);
int is_alfa_enabled (scram_t *scram);
int is_327_enabled (scram_t *scram);

#endif  // _S6_OBS_DATA_H
