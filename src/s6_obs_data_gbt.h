#ifndef _S6_OBS_DATA_GBT_H
#define _S6_OBS_DATA_GBT_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

#define GBTSTATUS_STRING_SIZE 32

typedef struct gbtstatus {

   // actually from gbtstatus, i.e. the mysql database

   char LASTUPDT[GBTSTATUS_STRING_SIZE];       // last_update
   long LASTUPDTSTIME;
   char LST[GBTSTATUS_STRING_SIZE];            // lst
   long LSTSTIME;
   char UTC[GBTSTATUS_STRING_SIZE];            // utc
   long UTCSTIME;
   double MJD;                                 // mjd
   long MJDSTIME;
   char EPOCH[GBTSTATUS_STRING_SIZE];          // epoch
   long EPOCHSTIME;
   char MAJTYPE[GBTSTATUS_STRING_SIZE];        // major_type
   long MAJTYPESTIME;
   char MINTYPE[GBTSTATUS_STRING_SIZE];        // minor_type
   long MINTYPESTIME;
   char MAJOR[GBTSTATUS_STRING_SIZE];          // major
   long MAJORSTIME;
   char MINOR[GBTSTATUS_STRING_SIZE];          // minor
   long MINORSTIME;
   double AZCOMM;                              // az_commanded
   long AZCOMMSTIME;
   double ELCOMM;                              // el_commanded
   long ELCOMMSTIME;
   double AZACTUAL;                            // az_actual
   long AZACTUALSTIME;
   double ELACTUAL;                            // el_actual
   long ELACTUALSTIME;
   double AZERROR;                             // az_error
   long AZERRORSTIME;
   double ELERROR;                             // el_error
   long ELERRORSTIME;
   char LPCS[GBTSTATUS_STRING_SIZE];           // lpcs
   long LPCSSTIME;
   char FOCUSOFF[GBTSTATUS_STRING_SIZE];       // focus_offset
   long FOCUSOFFSTIME;
   char ANTMOT[GBTSTATUS_STRING_SIZE];         // ant_motion
   long ANTMOTSTIME;
   char RECEIVER[GBTSTATUS_STRING_SIZE];       // receiver
   long RECEIVERSTIME;
   double IFFRQ1ST;                            // first_if_freq
   long IFFRQ1STSTIME;
   double IFFRQRST;                            // if_rest_freq
   long IFFRQRSTSTIME;
   double DCRSCFRQ;                            // dcr_sky_center_freq (should be long?)
   long DCRSCFRQSTIME;
   double SPRCSFRQ;                            // spectral_processor_sky_freq (should be long?)
   long SPRCSFRQSTIME;
   double FREQ;                                // freq
   long FREQSTIME;
   char VELFRAME[GBTSTATUS_STRING_SIZE];       // velocity_frame
   long VELFRAMESTIME;
   char VELDEF[GBTSTATUS_STRING_SIZE];         // velocity_definition
   long VELDEFSTIME;
   double J2000MAJ;                            // j2000_major
   long J2000MAJSTIME;
   double J2000MIN;                            // j2000_minor
   long J2000MINSTIME;

   // the derived fields below are from s6_observatory_gbt but not from gbtstatus/mysql
    
   double LSTH_DRV;         // lst in decimal hours
   long LSTH_DRVSTIME;
   double RA_DRV;           // RA in hours    taken from az/el actual and precessed to J2000
   long RA_DRVSTIME;
   double RADG_DRV;         // RA in degrees  taken from az/el actual and precessed to J2000
   long RADG_DRVSTIME;
   double DEC_DRV;          // DEC in degrees taken from az/el actual and precessed to J2000
   long DEC_DRVSTIME;

   // cleo server fields 

   char ATBOT1EO[GBTSTATUS_STRING_SIZE]; 		// Antenna,beamOffsetTable,1,beamElOffset
   long ATBOT1EOSTIME;
   double ATBOT1EOMJD;
   char ATBOT1XO[GBTSTATUS_STRING_SIZE]; 		// Antenna,beamOffsetTable,1,beamXelOffset
   long ATBOT1XOSTIME;
   double ATBOT1XOMJD;
   char ATBOT1NM[GBTSTATUS_STRING_SIZE]; 		// Antenna,beamOffsetTable,1,name
   long ATBOT1NMSTIME;
   double ATBOT1NMMJD;
   char ATBOT1F1[GBTSTATUS_STRING_SIZE]; 		// Antenna,beamOffsetTable,1,SRFeed1
   long ATBOT1F1STIME;
   double ATBOT1F1MJD;
   char ATBOT1F2[GBTSTATUS_STRING_SIZE]; 		// Antenna,beamOffsetTable,1,SRFeed2
   long ATBOT1F2STIME;
   double ATBOT1F2MJD;
   char ATFCTRMD[GBTSTATUS_STRING_SIZE]; 		// Antenna,focusTrackingMode
   long ATFCTRMDSTIME;
   double ATFCTRMDMJD;
   char ATLFCX[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,X
   long ATLFCXSTIME;
   double ATLFCXMJD;
   char ATLFCXT[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,Xt
   long ATLFCXTSTIME;
   double ATLFCXTMJD;
   char ATLFCY[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,Y
   long ATLFCYSTIME;
   double ATLFCYMJD;
   char ATLFCYT[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,Yt
   long ATLFCYTSTIME;
   double ATLFCYTMJD;
   char ATLFCZ[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,Z
   long ATLFCZSTIME;
   double ATLFCZMJD;
   char ATLFCZT[GBTSTATUS_STRING_SIZE]; 		// Antenna,local_focus_correction,Zt
   long ATLFCZTSTIME;
   double ATLFCZTMJD;
   char ATLPOAZ1[GBTSTATUS_STRING_SIZE]; 		// Antenna,localPointingOffsets,azOffset1
   long ATLPOAZ1STIME;
   double ATLPOAZ1MJD;
   char ATLPOAZ2[GBTSTATUS_STRING_SIZE]; 		// Antenna,localPointingOffsets,azOffset2
   long ATLPOAZ2STIME;
   double ATLPOAZ2MJD;
   char ATLPOEL[GBTSTATUS_STRING_SIZE]; 		// Antenna,localPointingOffsets,elOffset
   long ATLPOELSTIME;
   double ATLPOELMJD;
   char ATMCRAJ2[GBTSTATUS_STRING_SIZE]; 		// AntennaManager,ccuData,RaJ2000,indicated
   long ATMCRAJ2STIME;
   double ATMCRAJ2MJD;
   char ATMCDCJ2[GBTSTATUS_STRING_SIZE]; 		// AntennaManager,ccuData,DcJ2000,indicated
   long ATMCDCJ2STIME;
   double ATMCDCJ2MJD;
   char ATOPTMOD[GBTSTATUS_STRING_SIZE]; 		// Antenna,opticsMode
   long ATOPTMODSTIME;
   double ATOPTMODMJD;
   char ATRECVR[GBTSTATUS_STRING_SIZE]; 		// Antenna,receiver
   long ATRECVRSTIME;
   double ATRECVRMJD;
   char ATRXOCTA[GBTSTATUS_STRING_SIZE]; 		// Antenna,rxOpticsConfig,turretAngle
   long ATRXOCTASTIME;
   double ATRXOCTAMJD;
   char BAMMPWR1[GBTSTATUS_STRING_SIZE]; 		// BankAMgr,measpwr1
   long BAMMPWR1STIME;
   double BAMMPWR1MJD;
   char BAMMPWR2[GBTSTATUS_STRING_SIZE]; 		// BankAMgr,measpwr2
   long BAMMPWR2STIME;
   double BAMMPWR2MJD;
   char LO1APSFQ[GBTSTATUS_STRING_SIZE]; 		// LO1A,phaseState,frequency
   long LO1APSFQSTIME;
   double LO1APSFQMJD;
   char LO1BPSFQ[GBTSTATUS_STRING_SIZE]; 		// LO1B,phaseState,frequency
   long LO1BPSFQSTIME;
   double LO1BPSFQMJD;
   char SCPROJID[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,projectId
   long SCPROJIDSTIME;
   double SCPROJIDMJD;
   char SCSNUMBR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,scanNumber
   long SCSNUMBRSTIME;
   double SCSNUMBRMJD;
   char SCSOURCE[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,source
   long SCSOURCESTIME;
   double SCSOURCEMJD;
   char SCSTATE[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,state
   long SCSTATESTIME;
   double SCSTATEMJD;
   char SCSACTSF[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,ActiveSurface
   long SCSACTSFSTIME;
   double SCSACTSFMJD;
   char SCSANAFR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,AnalogFilterRack
   long SCSANAFRSTIME;
   double SCSANAFRMJD;
   char SCSANTEN[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Antenna
   long SCSANTENSTIME;
   double SCSANTENMJD;
   char SCSARCHI[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Archivist
   long SCSARCHISTIME;
   double SCSARCHIMJD;
   char SCSBCPM[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,BCPM
   long SCSBCPMSTIME;
   double SCSBCPMMJD;
   char SCSCCB26[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,CCB26_40
   long SCSCCB26STIME;
   double SCSCCB26MJD;
   char SCSCRACK[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,ConverterRack
   long SCSCRACKSTIME;
   double SCSCRACKMJD;
   char SCSDCR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,DCR
   long SCSDCRSTIME;
   double SCSDCRMJD;
   char SCSGUPPI[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,GUPPI
   long SCSGUPPISTIME;
   double SCSGUPPIMJD;
   char SCSHOLOG[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Holography
   long SCSHOLOGSTIME;
   double SCSHOLOGMJD;
   char SCSIFM[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,IFManager
   long SCSIFMSTIME;
   double SCSIFMMJD;
   char SCSIFR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,IFRack
   long SCSIFRSTIME;
   double SCSIFRMJD;
   char SCSLO1[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,LO1
   long SCSLO1STIME;
   double SCSLO1MJD;
   char SCSMEASUR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Measurements
   long SCSMEASURSTIME;
   double SCSMEASURMJD;
   char SCSQUADRD[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,QuadrantDetector
   long SCSQUADRDSTIME;
   double SCSQUADRDMJD;
   char SCSR1[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr1_2
   long SCSR1STIME;
   double SCSR1MJD;
   char SCSR12[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr12_18
   long SCSR12STIME;
   double SCSR12MJD;
   char SCSR18[GBTSTATUS_STRING_SIZE]; // ScanCoordinator,subsystemSelect,Rcvr18_26
   long SCSR18STIME;
   double SCSR18MJD;
   char SCSR2[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr2_3
   long SCSR2STIME;
   double SCSR2MJD;
   char SCSR26[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr26_40
   long SCSR26STIME;
   double SCSR26MJD;
   char SCSR40[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr40_52
   long SCSR40STIME;
   double SCSR40MJD;
   char SCSR4[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr4_6
   long SCSR4STIME;
   double SCSR4MJD;
   char SCSR68[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr68_92
   long SCSR68STIME;
   double SCSR68MJD;
   char SCSR8[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr8_10
   long SCSR8STIME;
   double SCSR8MJD;
   char SCSRA1[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,RcvrArray1_2
   long SCSRA1STIME;
   double SCSRA1MJD;
   char SCSRA18[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,RcvrArray18_26
   long SCSRA18STIME;
   double SCSRA18MJD;
   char SCSRMBA1[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr_MBA1_5
   long SCSRMBA1STIME;
   double SCSRMBA1MJD;
   char SCSRPAR[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Rcvr_PAR
   long SCSRPARSTIME;
   double SCSRPARMJD;
   char SCSRPF1[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,RcvrPF_1
   long SCSRPF1STIME;
   double SCSRPF1MJD;
   char SCSRPF2[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,RcvrPF_2
   long SCSRPF2STIME;
   double SCSRPF2MJD;
   char SCSSPROC[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,SpectralProcessor
   long SCSSPROCSTIME;
   double SCSSPROCMJD;
   char SCSSPECT[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Spectrometer
   long SCSSPECTSTIME;
   double SCSSPECTMJD;
   char SCSSWSS[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,SwitchingSignalSelector
   long SCSSWSSSTIME;
   double SCSSWSSMJD;
   char SCSVEGAS[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,VEGAS
   long SCSVEGASSTIME;
   double SCSVEGASMJD;
   char SCSZPECT[GBTSTATUS_STRING_SIZE]; 		// ScanCoordinator,subsystemSelect,Zpectrometer
   long SCSZPECTSTIME;
   double SCSZPECTMJD;
   char VEGSFBW1[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,1
   long VEGSFBW1STIME;
   double VEGSFBW1MJD;
   char VEGSFBW2[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,2
   long VEGSFBW2STIME;
   double VEGSFBW2MJD;
   char VEGSFBW3[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,3
   long VEGSFBW3STIME;
   double VEGSFBW3MJD;
   char VEGSFBW4[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,4
   long VEGSFBW4STIME;
   double VEGSFBW4MJD;
   char VEGSFBW5[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,5
   long VEGSFBW5STIME;
   double VEGSFBW5MJD;
   char VEGSFBW6[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,6
   long VEGSFBW6STIME;
   double VEGSFBW6MJD;
   char VEGSFBW7[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,7
   long VEGSFBW7STIME;
   double VEGSFBW7MJD;
   char VEGSFBW8[GBTSTATUS_STRING_SIZE]; 		// VEGAS,filter_bw,8
   long VEGSFBW8STIME;
   double VEGSFBW8MJD;
   char VEGSSBAM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankAMgr
   long VEGSSBAMSTIME;
   double VEGSSBAMMJD;
   char VEGSSBBM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankBMgr
   long VEGSSBBMSTIME;
   double VEGSSBBMMJD;
   char VEGSSBCM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankCMgr
   long VEGSSBCMSTIME;
   double VEGSSBCMMJD;
   char VEGSSBDM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankDMgr
   long VEGSSBDMSTIME;
   double VEGSSBDMMJD;
   char VEGSSBEM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankEMgr
   long VEGSSBEMSTIME;
   double VEGSSBEMMJD;
   char VEGSSBFM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankFMgr
   long VEGSSBFMSTIME;
   double VEGSSBFMMJD;
   char VEGSSBGM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankGMgr
   long VEGSSBGMSTIME;
   double VEGSSBGMMJD;
   char VEGSSBHM[GBTSTATUS_STRING_SIZE]; 		// VEGAS,subsystemSelect,BankHMgr
   long VEGSSBHMSTIME;
   double VEGSSBHMMJD;

   // the fields below are other scripts and not from gbtstatus/mysql/cleo

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

