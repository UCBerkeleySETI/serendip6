#ifndef _S6_OBS_DATA_GBT_H
#define _S6_OBS_DATA_GBT_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

typedef struct gbtstatus {

   char LASTUPDT[32];       // last_update
   char LST[16];            // lst
   char UTC[16];            // utc
   double MJD;              // mjd
   char EPOCH[32];          // epoch
   char MAJTYPE[16];        // major_type
   char MINTYPE[16];        // minor_type
   char MAJOR[32];          // major
   char MINOR[32];          // minor
   double AZCOMM;           // az_commanded
   double ELCOMM;           // el_commanded
   double AZACTUAL;         // az_actual
   double ELACTUAL;         // el_actual
   double AZERROR;          // az_error
   double ELERROR;          // el_error
   char LPCS[32];           // lpcs
   char FOCUSOFF[32];       // focus_offset
   char ANTMOT[16];         // ant_motion
   char RECEIVER[32];       // receiver
   double IFFRQ1ST;         // first_if_freq
   double IFFRQRST;         // if_rest_freq
   double FREQ;             // freq
   char VELFRAME[16];       // velocity_frame
   char VELDEF[16];         // velocity_definition
   double J2000MAJ;         // j2000_major
   double J2000MIN;         // j2000_minor

   // the derived fields below are from s6_observatory_gbt but not from gbtstatus/mysql
    
   double LSTH_DRV;         // lst in decimal hours
   double RA_DRV;           // RA in hours    taken from az/el actual and precessed to J2000
   double RADG_DRV;         // RA in degrees  taken from az/el actual and precessed to J2000
   double DEC_DRV;          // DEC in degrees taken from az/el actual and precessed to J2000

   // the fields below are other scripts and not from gbtstatus/mysql 

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
   int     WEBCNTRL;    // 1 = on, 0 = off - for GBT operator control

} gbtstatus_t;

int get_obs_gbt_info_from_redis(gbtstatus_t *gbtstatus, char *hostname, int port);

#endif  // _S6_OBS_DATA_GBT_H

