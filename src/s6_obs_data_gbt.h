#ifndef _S6_OBS_DATA_GBT_H
#define _S6_OBS_DATA_GBT_H

#include "s6_databuf.h"

#define N_ADCS_PER_ROACH2 8     // should this be N_BEAM_SLOTS/2 ?

typedef struct gbtstatus {

   // actually from gbtstatus, i.e. the mysql database

   char LASTUPDT[32];       // last_update
   long LASTUPDTSTIME;
   char LST[16];            // lst
   long LSTSTIME;
   char UTC[16];            // utc
   long UTCSTIME;
   double MJD;              // mjd
   long MJDSTIME;
   char EPOCH[32];          // epoch
   long EPOCHSTIME;
   char MAJTYPE[16];        // major_type
   long MAJTYPESTIME;
   char MINTYPE[16];        // minor_type
   long MINTYPESTIME;
   char MAJOR[32];          // major
   long MAJORSTIME;
   char MINOR[32];          // minor
   long MINORSTIME;
   double AZCOMM;           // az_commanded
   long AZCOMMSTIME;
   double ELCOMM;           // el_commanded
   long ELCOMMSTIME;
   double AZACTUAL;         // az_actual
   long AZACTUALSTIME;
   double ELACTUAL;         // el_actual
   long ELACTUALSTIME;
   double AZERROR;          // az_error
   long AZERRORSTIME;
   double ELERROR;          // el_error
   long ELERRORSTIME;
   char LPCS[32];           // lpcs
   long LPCSSTIME;
   char FOCUSOFF[32];       // focus_offset
   long FOCUSOFFSTIME;
   char ANTMOT[16];         // ant_motion
   long ANTMOTSTIME;
   char RECEIVER[32];       // receiver
   long RECEIVERSTIME;
   double IFFRQ1ST;         // first_if_freq
   long IFFRQ1STSTIME;
   double IFFRQRST;         // if_rest_freq
   long IFFRQRSTSTIME;
   double DCRSCFRQ;         // dcr_sky_center_freq (should be long?)
   long DCRSCFRQSTIME;
   double SPRCSFRQ;         // spectral_processor_sky_freq (should be long?)
   long SPRCSFRQSTIME;
   double FREQ;             // freq
   long FREQSTIME;
   char VELFRAME[16];       // velocity_frame
   long VELFRAMESTIME;
   char VELDEF[16];         // velocity_definition
   long VELDEFSTIME;
   double J2000MAJ;         // j2000_major
   long J2000MAJSTIME;
   double J2000MIN;         // j2000_minor
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

   char ATBOT1EO[32]; 		// Antenna,beamOffsetTable,1,beamElOffset
   long ATBOT1EOSTIME;
   double ATBOT1EOMJD;
   char ATBOT1XO[32]; 		// Antenna,beamOffsetTable,1,beamXelOffset
   long ATBOT1XOSTIME;
   double ATBOT1XOMJD;
   char ATBOT1NM[32]; 		// Antenna,beamOffsetTable,1,name
   long ATBOT1NMSTIME;
   double ATBOT1NMMJD;
   char ATBOT1F1[32]; 		// Antenna,beamOffsetTable,1,SRFeed1
   long ATBOT1F1STIME;
   double ATBOT1F1MJD;
   char ATBOT1F2[32]; 		// Antenna,beamOffsetTable,1,SRFeed2
   long ATBOT1F2STIME;
   double ATBOT1F2MJD;
   char ATFCTRMD[32]; 		// Antenna,focusTrackingMode
   long ATFCTRMDSTIME;
   double ATFCTRMDMJD;
   char ATLFCX[32]; 		// Antenna,local_focus_correction,X
   long ATLFCXSTIME;
   double ATLFCXMJD;
   char ATLFCXT[32]; 		// Antenna,local_focus_correction,Xt
   long ATLFCXTSTIME;
   double ATLFCXTMJD;
   char ATLFCY[32]; 		// Antenna,local_focus_correction,Y
   long ATLFCYSTIME;
   double ATLFCYMJD;
   char ATLFCYT[32]; 		// Antenna,local_focus_correction,Yt
   long ATLFCYTSTIME;
   double ATLFCYTMJD;
   char ATLFCZ[32]; 		// Antenna,local_focus_correction,Z
   long ATLFCZSTIME;
   double ATLFCZMJD;
   char ATLFCZT[32]; 		// Antenna,local_focus_correction,Zt
   long ATLFCZTSTIME;
   double ATLFCZTMJD;
   char ATLPOAZ1[32]; 		// Antenna,localPointingOffsets,azOffset1
   long ATLPOAZ1STIME;
   double ATLPOAZ1MJD;
   char ATLPOAZ2[32]; 		// Antenna,localPointingOffsets,azOffset2
   long ATLPOAZ2STIME;
   double ATLPOAZ2MJD;
   char ATLPOEL[32]; 		// Antenna,localPointingOffsets,elOffset
   long ATLPOELSTIME;
   double ATLPOELMJD;
   char ATMCRAJ2[32]; 		// AntennaManager,ccuData,RaJ2000,indicated
   long ATMCRAJ2STIME;
   double ATMCRAJ2MJD;
   char ATMCDCJ2[32]; 		// AntennaManager,ccuData,DcJ2000,indicated
   long ATMCDCJ2STIME;
   double ATMCDCJ2MJD;
   char ATOPTMOD[32]; 		// Antenna,opticsMode
   long ATOPTMODSTIME;
   double ATOPTMODMJD;
   char ATRECVR[32]; 		// Antenna,receiver
   long ATRECVRSTIME;
   double ATRECVRMJD;
   char ATRXOCTA[32]; 		// Antenna,rxOpticsConfig,turretAngle
   long ATRXOCTASTIME;
   double ATRXOCTAMJD;
   char BAMMPWR1[32]; 		// BankAMgr,measpwr1
   long BAMMPWR1STIME;
   double BAMMPWR1MJD;
   char BAMMPWR2[32]; 		// BankAMgr,measpwr2
   long BAMMPWR2STIME;
   double BAMMPWR2MJD;
   char LO1APSFQ[32]; 		// LO1A,phaseState,frequency
   long LO1APSFQSTIME;
   double LO1APSFQMJD;
   char LO1BPSFQ[32]; 		// LO1B,phaseState,frequency
   long LO1BPSFQSTIME;
   double LO1BPSFQMJD;
   char SCPROJID[32]; 		// ScanCoordinator,projectId
   long SCPROJIDSTIME;
   double SCPROJIDMJD;
   char SCSNUMBR[32]; 		// ScanCoordinator,scanNumber
   long SCSNUMBRSTIME;
   double SCSNUMBRMJD;
   char SCSOURCE[32]; 		// ScanCoordinator,source
   long SCSOURCESTIME;
   double SCSOURCEMJD;
   char SCSTATE[32]; 		// ScanCoordinator,state
   long SCSTATESTIME;
   double SCSTATEMJD;
   char SCSACTSF[32]; 		// ScanCoordinator,subsystemSelect,ActiveSurface
   long SCSACTSFSTIME;
   double SCSACTSFMJD;
   char SCSANAFR[32]; 		// ScanCoordinator,subsystemSelect,AnalogFilterRack
   long SCSANAFRSTIME;
   double SCSANAFRMJD;
   char SCSANTEN[32]; 		// ScanCoordinator,subsystemSelect,Antenna
   long SCSANTENSTIME;
   double SCSANTENMJD;
   char SCSARCHI[32]; 		// ScanCoordinator,subsystemSelect,Archivist
   long SCSARCHISTIME;
   double SCSARCHIMJD;
   char SCSBCPM[32]; 		// ScanCoordinator,subsystemSelect,BCPM
   long SCSBCPMSTIME;
   double SCSBCPMMJD;
   char SCSCCB26[32]; 		// ScanCoordinator,subsystemSelect,CCB26_40
   long SCSCCB26STIME;
   double SCSCCB26MJD;
   char SCSCRACK[32]; 		// ScanCoordinator,subsystemSelect,ConverterRack
   long SCSCRACKSTIME;
   double SCSCRACKMJD;
   char SCSDCR[32]; 		// ScanCoordinator,subsystemSelect,DCR
   long SCSDCRSTIME;
   double SCSDCRMJD;
   char SCSGUPPI[32]; 		// ScanCoordinator,subsystemSelect,GUPPI
   long SCSGUPPISTIME;
   double SCSGUPPIMJD;
   char SCSHOLOG[32]; 		// ScanCoordinator,subsystemSelect,Holography
   long SCSHOLOGSTIME;
   double SCSHOLOGMJD;
   char SCSIFM[32]; 		// ScanCoordinator,subsystemSelect,IFManager
   long SCSIFMSTIME;
   double SCSIFMMJD;
   char SCSIFR[32]; 		// ScanCoordinator,subsystemSelect,IFRack
   long SCSIFRSTIME;
   double SCSIFRMJD;
   char SCSLO1[32]; 		// ScanCoordinator,subsystemSelect,LO1
   long SCSLO1STIME;
   double SCSLO1MJD;
   char SCSMEASUR[32]; 		// ScanCoordinator,subsystemSelect,Measurements
   long SCSMEASURSTIME;
   double SCSMEASURMJD;
   char SCSQUADRD[32]; 		// ScanCoordinator,subsystemSelect,QuadrantDetector
   long SCSQUADRDSTIME;
   double SCSQUADRDMJD;
   char SCSR1[32]; 		// ScanCoordinator,subsystemSelect,Rcvr1_2
   long SCSR1STIME;
   double SCSR1MJD;
   char SCSR12[32]; 		// ScanCoordinator,subsystemSelect,Rcvr12_18
   long SCSR12STIME;
   double SCSR12MJD;
   char SCSR18[32]; // ScanCoordinator,subsystemSelect,Rcvr18_26
   long SCSR18STIME;
   double SCSR18MJD;
   char SCSR2[32]; 		// ScanCoordinator,subsystemSelect,Rcvr2_3
   long SCSR2STIME;
   double SCSR2MJD;
   char SCSR26[32]; 		// ScanCoordinator,subsystemSelect,Rcvr26_40
   long SCSR26STIME;
   double SCSR26MJD;
   char SCSR40[32]; 		// ScanCoordinator,subsystemSelect,Rcvr40_52
   long SCSR40STIME;
   double SCSR40MJD;
   char SCSR4[32]; 		// ScanCoordinator,subsystemSelect,Rcvr4_6
   long SCSR4STIME;
   double SCSR4MJD;
   char SCSR68[32]; 		// ScanCoordinator,subsystemSelect,Rcvr68_92
   long SCSR68STIME;
   double SCSR68MJD;
   char SCSR8[32]; 		// ScanCoordinator,subsystemSelect,Rcvr8_10
   long SCSR8STIME;
   double SCSR8MJD;
   char SCSRA1[32]; 		// ScanCoordinator,subsystemSelect,RcvrArray1_2
   long SCSRA1STIME;
   double SCSRA1MJD;
   char SCSRA18[32]; 		// ScanCoordinator,subsystemSelect,RcvrArray18_26
   long SCSRA18STIME;
   double SCSRA18MJD;
   char SCSRMBA1[32]; 		// ScanCoordinator,subsystemSelect,Rcvr_MBA1_5
   long SCSRMBA1STIME;
   double SCSRMBA1MJD;
   char SCSRPAR[32]; 		// ScanCoordinator,subsystemSelect,Rcvr_PAR
   long SCSRPARSTIME;
   double SCSRPARMJD;
   char SCSRPF1[32]; 		// ScanCoordinator,subsystemSelect,RcvrPF_1
   long SCSRPF1STIME;
   double SCSRPF1MJD;
   char SCSRPF2[32]; 		// ScanCoordinator,subsystemSelect,RcvrPF_2
   long SCSRPF2STIME;
   double SCSRPF2MJD;
   char SCSSPROC[32]; 		// ScanCoordinator,subsystemSelect,SpectralProcessor
   long SCSSPROCSTIME;
   double SCSSPROCMJD;
   char SCSSPECT[32]; 		// ScanCoordinator,subsystemSelect,Spectrometer
   long SCSSPECTSTIME;
   double SCSSPECTMJD;
   char SCSSWSS[32]; 		// ScanCoordinator,subsystemSelect,SwitchingSignalSelector
   long SCSSWSSSTIME;
   double SCSSWSSMJD;
   char SCSVEGAS[32]; 		// ScanCoordinator,subsystemSelect,VEGAS
   long SCSVEGASSTIME;
   double SCSVEGASMJD;
   char SCSZPECT[32]; 		// ScanCoordinator,subsystemSelect,Zpectrometer
   long SCSZPECTSTIME;
   double SCSZPECTMJD;
   char VEGSFBW1[32]; 		// VEGAS,filter_bw,1
   long VEGSFBW1STIME;
   double VEGSFBW1MJD;
   char VEGSFBW2[32]; 		// VEGAS,filter_bw,2
   long VEGSFBW2STIME;
   double VEGSFBW2MJD;
   char VEGSFBW3[32]; 		// VEGAS,filter_bw,3
   long VEGSFBW3STIME;
   double VEGSFBW3MJD;
   char VEGSFBW4[32]; 		// VEGAS,filter_bw,4
   long VEGSFBW4STIME;
   double VEGSFBW4MJD;
   char VEGSFBW5[32]; 		// VEGAS,filter_bw,5
   long VEGSFBW5STIME;
   double VEGSFBW5MJD;
   char VEGSFBW6[32]; 		// VEGAS,filter_bw,6
   long VEGSFBW6STIME;
   double VEGSFBW6MJD;
   char VEGSFBW7[32]; 		// VEGAS,filter_bw,7
   long VEGSFBW7STIME;
   double VEGSFBW7MJD;
   char VEGSFBW8[32]; 		// VEGAS,filter_bw,8
   long VEGSFBW8STIME;
   double VEGSFBW8MJD;
   char VEGSSBAM[32]; 		// VEGAS,subsystemSelect,BankAMgr
   long VEGSSBAMSTIME;
   double VEGSSBAMMJD;
   char VEGSSBBM[32]; 		// VEGAS,subsystemSelect,BankBMgr
   long VEGSSBBMSTIME;
   double VEGSSBBMMJD;
   char VEGSSBCM[32]; 		// VEGAS,subsystemSelect,BankCMgr
   long VEGSSBCMSTIME;
   double VEGSSBCMMJD;
   char VEGSSBDM[32]; 		// VEGAS,subsystemSelect,BankDMgr
   long VEGSSBDMSTIME;
   double VEGSSBDMMJD;
   char VEGSSBEM[32]; 		// VEGAS,subsystemSelect,BankEMgr
   long VEGSSBEMSTIME;
   double VEGSSBEMMJD;
   char VEGSSBFM[32]; 		// VEGAS,subsystemSelect,BankFMgr
   long VEGSSBFMSTIME;
   double VEGSSBFMMJD;
   char VEGSSBGM[32]; 		// VEGAS,subsystemSelect,BankGMgr
   long VEGSSBGMSTIME;
   double VEGSSBGMMJD;
   char VEGSSBHM[32]; 		// VEGAS,subsystemSelect,BankHMgr
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

#endif  // _S6_OBS_DATA_GBT_H

