#ifndef _S6_OBS_DATA_H
#define _S6_OBS_DATA_H

#include "s6_databuf.h"

// these values were pulled from SETI@home datarecorder2 
#define ScramLagTolerance           10     // in seconds
#define TT_TurretDegreesAlfa        26.64
#define TT_TurretDegreesTolerance   1.0

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
    double  IF1SYNHZ;   // for 327 receiver : ___ 
    int     IF1SYNDB;
    double  IF1RFFRQ;
    double  IF1IFFRQ;   // for 327 receiver : ___
    int     IF1ALFFB;
    int     IF2STIME;
    int     IF2ALFON;
    int     TTSTIME; 
    int     TTTURENC;   // for 327 receiver : ___
    double  TTTURDEG;   // for 327 receiver : ___
    int     DERTIME;
    double  ra_by_beam[N_BEAMS];   
    double  dec_by_beam[N_BEAMS];
    int     alfa_enabled;
    int     coarse_chan_id;     // not from scram
} scram_t;

int get_obs_info_from_redis(scram_t *scram, char *hostname, int port);

int is_alfa_enabled (scram_t *scram);

#endif  // _S6_OBS_DATA_H
