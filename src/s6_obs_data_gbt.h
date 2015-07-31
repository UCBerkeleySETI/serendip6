#ifndef _S6_OBS_DATA_GBT_H
#define _S6_OBS_DATA_GBT_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

#define GBTSTATUS_STRING_SIZE 32

typedef struct gbtstatus {

   char LASTUPDT[GBTSTATUS_STRING_SIZE];       // last_update
   char LST[GBTSTATUS_STRING_SIZE];            // lst
   char UTC[GBTSTATUS_STRING_SIZE];            // utc
   double MJD;              // mjd
   char EPOCH[GBTSTATUS_STRING_SIZE];          // epoch
   char MAJTYPE[GBTSTATUS_STRING_SIZE];        // major_type
   char MINTYPE[GBTSTATUS_STRING_SIZE];        // minor_type
   char MAJOR[GBTSTATUS_STRING_SIZE];          // major
   char MINOR[GBTSTATUS_STRING_SIZE];          // minor
   double AZCOMM;           // az_commanded
   double ELCOMM;           // el_commanded
   double AZACTUAL;         // az_actual
   double ELACTUAL;         // el_actual
   double AZERROR;          // az_error
   double ELERROR;          // el_error
   char LPCS[GBTSTATUS_STRING_SIZE];           // lpcs
   char FOCUSOFF[GBTSTATUS_STRING_SIZE];       // focus_offset
   char ANTMOT[GBTSTATUS_STRING_SIZE];         // ant_motion
   char RECEIVER[GBTSTATUS_STRING_SIZE];       // receiver
   double IFFRQ1ST;         // first_if_freq
   double IFFRQRST;         // if_rest_freq
   double DCRSCFRQ;         // dcr_sky_center_freq (should be long?)
   double SPRCSFRQ;         // spectral_processor_sky_freq (should be long?)
   double FREQ;             // freq
   char VELFRAME[GBTSTATUS_STRING_SIZE];       // velocity_frame
   char VELDEF[GBTSTATUS_STRING_SIZE];         // velocity_definition
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
int put_obs_gbt_info_to_redis(char * fits_filename, int instance, char *hostname, int port);

#endif  // _S6_OBS_DATA_GBT_H

