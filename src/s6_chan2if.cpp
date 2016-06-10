#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main (int argc, char ** argv) {

	char * observatory;
	double clock;	     	// MHz
	double band_width;		// MHz
	double cc_per_sys;		// number of coarse channels for the entire system
	int    cc_per_instance;	// number of coarse channesl per compute instance 
    int32_t fc_per_cc;    	// numnber of fine channels per coarse channel
	int	   start_cc;
	double cc_bin_width, fc_bin_width;
	double ifreq;			// IF
	int    cc, sys_cc;
	int    sys_fc;
	int32_t  unsigned_fc, signed_fc;	// make sure we have 32bit ints for sign extention below
    double resolution;
	int instance;

	if(argc != 5 || !strcmp(argv[1], "-h")) {
		fprintf(stderr, "Usage: %s ao|gbt instance coarse_channel fine_channel\n", argv[0]);
		exit(0);
	}
	
	// get cmd line paramters
	observatory		= argv[1];			// "ao" or "gbt"
	instance	 	= atoi(argv[2]);	// the compute instance, generally 0-7                  
	cc       	 	= atoi(argv[3]);	// the course channel as it appears in an ETFITS file              
	unsigned_fc	 	= atoi(argv[4]);	// the fine channel as it appears in an ETFITS file                  
	
	if(strcmp(observatory, "gbt") == 0) {
		clock               = 3000;	       		// an instrument configurable (CLOCKFRQ in FITS header)
		cc_per_sys	        = 4096;
		cc_per_instance     = 512;
		start_cc			= 0;				// an instrument configurable (COARCHID in FITS header)
    	fc_per_cc           = pow(2.0, 19);    	// 512K, 19 bits, 
												//  all bits 1 = 0x0007FFFF, high bit = 0x00040000
	} else if (strcmp(observatory, "ao") == 0) {
		clock               = 896;	       		// an instrument configurable (CLOCKFRQ in FITS header)
		cc_per_sys	        = 4096;
		cc_per_instance     = 320;
		start_cc			= 1006;				// an instrument configurable (COARCHID in FITS header)
    	fc_per_cc           = pow(2.0, 17);    	// 128K, 17 bits, 
												//  all bits 1 = 0x0001FFFF, high bit = 0x00010000
	} else {
		fprintf(stderr, "Bad observatory (must be gbt or ao)\n");
		exit(1);
	}

	// MHz
	band_width      = clock/2;			
	cc_bin_width 	= band_width/cc_per_sys;	
    fc_bin_width    = band_width/(cc_per_sys * fc_per_cc);
	// Hz
    resolution = fc_bin_width * 1000000;    

	sys_cc		= start_cc + instance * cc_per_instance + cc;		// Calc system wide coarse channel.	
																	// We could also use the instance 
																	// cc from the FITS header item 
																	// COARCHID, in which case we would:
																	// sys_cc = COARCHID + cc; 
																	// We could also use the cc token 
																	// in the file name, which would give us:
																	// sys_cc = token_value + cc;

	// sign extend to get positive and negative (two's compliment) fc's from cc center
	if(strcmp(observatory, "gbt") == 0) {
		// assumes 2**19 fc's 												   signed neg   pos
		signed_fc = (unsigned_fc & 0x0007FFFF) | ((unsigned_fc & 0x00040000) ? 0xFFF80000 : 0);	
	}
	if (strcmp(observatory, "ao") == 0) {
		// assumes 2**17 fc's												   signed neg   pos
		signed_fc = (unsigned_fc & 0x0001FFFF) | ((unsigned_fc & 0x00010000) ? 0xFFFE0000 : 0);	
	}

	sys_fc  =	fc_per_cc * sys_cc + signed_fc;	
	ifreq  	= ((sys_fc + fc_per_cc) * resolution) / 1000000;	// MHz

	fprintf(stderr, "System coarse channel : %u\n", sys_cc);
	fprintf(stderr, "System fine channel   : %u\n", sys_fc);
	fprintf(stderr, "IF (MHz)              : %f\n", ifreq);
	fprintf(stderr, "freq resolution (Hz)  : %f\n", resolution);
	fprintf(stderr, "time resolution (sec) : %f\n", 1/resolution);

}
