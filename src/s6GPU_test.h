#ifndef _S6GPU_TEST_H
#define _S6GPU_TEST_H

#define TRANSPOSE_DATA
#ifdef TRANSPOSE_DATA
#define NTIME N_FINE_CHAN
#define NCHAN N_COARSE_CHAN
//#define NTIME (128*1024)    // from the point of view of the caller of s6GPU
//#define NCHAN 342           //   ie, NTIME time samples, each with NCHAN channels
//          #define NTIME 16  
//          #define NCHAN 4    
#else
//          #define NTIME 8         
//          #define NCHAN 4    
#define NTIME N_COARSE_CHAN   
#define NCHAN N_FINE_CHAN    
//#define NTIME 342           // from the point of view of the caller of s6GPU
//#define NCHAN (128*1024)    //   ie, NTIME time samples, each with NCHAN channels
#endif

#define NELEMENT (NCHAN*NTIME)
#define NINPUT N_POLS_PER_BEAM

#define GPU_DEV 0

#endif  // _S6GPU_TEST_H

