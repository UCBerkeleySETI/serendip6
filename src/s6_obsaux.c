#include <stdlib.h>
#include "s6_obsaux.h"
#include <math.h>
#include "azzaToRaDec.h"

/// all this code is from setilib, which is in cpp so can't link against it.
/// NOTE: the call the phils code is set to precess which is DIFFERENT than
///       the setilib code, which is set to NOT precess.

#if 0
double s6_seti_ao_timeMS2unixtime(long timeMs, time_t now) {
//=======================================================
// converts millisecs past midnight to a unix time_t.
// Time is often given as millisecs past midnight in AO
// scramnet structures.
// The "now" variable needs to be unix time ahead of
// timeMs by less that 24 hours.  The realtime code can
// just pass the result of time().  Backend code code
// typically will use the SysTime included with each scramnet
// structure.

    static const double AST_TO_UTC_HOURS=4;
    double secs_past_midnight, encoder_read_secs_past_midnight,fnow;
    struct tm *now_tm;

    // craft a tm structure with values corresponding to encoder
    // read time don't use localtime() because it will mess up
    // when data is read on pacific time computers
    // by correcting for the PST-UT difference
    fnow=now;
    fnow-=AST_TO_UTC_HOURS*3600;                 // translate UTC to AST
    now=(time_t)floor(fnow);
    now_tm = gmtime(&now);
    secs_past_midnight = now_tm->tm_hour*3600 + now_tm->tm_min*60 + now_tm->tm_sec;
    encoder_read_secs_past_midnight = (timeMs * 0.001);
    fnow -= secs_past_midnight;              // just back up to midnight
    fnow += encoder_read_secs_past_midnight;     // now now as the unix time of the encoder reading
    fnow+=AST_TO_UTC_HOURS*3600;                // translate AST to UTC
    return(fnow);
}
#endif

bool s6_BeamOffset(double *Az, double *ZA, int Beam, double AlfaMotorPosition) {

  double array_angle[7] = {0.0, 120.0, 180.0, -120.0, -60.0, 0.0, 60.0};
  double array_az_ellipse;
  double array_za_ellipse;

  if(Beam != 0) {
          array_az_ellipse = 329.06;  //taken from seti_boinc/db/tools/s4_receivers.xml
          array_za_ellipse = 384.005;
   } else {
          array_az_ellipse = 0;
          array_za_ellipse = 0;
   }

  double AzCorrection = *Az;
  double ZenCorrection = *ZA;

  if(Beam != 0) {
    double posrot=(array_angle[Beam] - AlfaMotorPosition)*D2R; 
    AzCorrection+=array_az_ellipse*cos(posrot);
    ZenCorrection+=array_za_ellipse*sin(posrot);

    *ZA -= ZenCorrection / 3600.0;                 // Correction is in arcsec.
    *Az -= (AzCorrection  / 3600.0) / sin(*ZA * D2R);
  }

  // Correction is in arcsec.

  // Sometimes the azimuth is not where the telescope is
  // pointing, but rather where it is physically positioned.
  *Az    += 180.0; //taken from seti_boinc/db/tools/s4_receivers.xml

  if(*ZA < 0.0)                                // Have we corrected zen over
    {                                           //  the zenith?
    *ZA = 0.0 - *ZA;                          // If so, correct zen and
    *Az += 180.0;                               //   swing azimuth around to
    }

  *Az = wrap(*Az, 0, 360, 1);              // az to 0 - 360 degrees

  return true;
}

void s6_AzZaToRaDec(double Az, double Za, double coord_time, double *Ra, double *Dec) {
//=======================================================
// This calls AO Phil's code.
// Any desired model correction must be done prior to calling this routine.

    AZZA_TO_RADEC_INFO  azzaI; /* info for aza to ra dec*/

    const double d2r =  0.017453292;
    int dayNum, i_mjd;
    int ofDate = 0;     // if 1, tells azzaToRaDec() to return coords of the date, ie not precessed
    double utcFrac;
    struct tm *coord_tm;
    time_t lcoord_time=(time_t)floor(coord_time);
    double fcoord_time=coord_time-floor(coord_time);

    coord_tm = gmtime(&lcoord_time);

    // arithmetic needed by AO functions
    coord_tm->tm_mon += 1;
    coord_tm->tm_year += 1900;
    dayNum = dmToDayNo(coord_tm->tm_mday,coord_tm->tm_mon,coord_tm->tm_year);
    i_mjd=gregToMjd(coord_tm->tm_mday, coord_tm->tm_mon, coord_tm->tm_year);
    utcFrac=(coord_tm->tm_hour*3600 + coord_tm->tm_min*60 + coord_tm->tm_sec + fcoord_time)/86400.;
    if (utcFrac >= 1.) {
        i_mjd++;
        utcFrac-=1.;
    }

    // call the AO functions
    if (azzaToRaDecInit(dayNum, coord_tm->tm_year, &azzaI) == -1) {
        exit(1);
    }
    // subtract 180 from Az because Phil wants dome azimuth
    azzaToRaDec(Az-180.0, Za, i_mjd, utcFrac, ofDate, &azzaI, Ra, Dec);

    // Ra to hours and Dec to degrees
    *Ra = (*Ra / d2r) / 15;
    *Dec = *Dec / d2r;
//fprintf(stderr, "azzaToRaDec Az %lf Za %lf i_mjd %d utc1Frac %lf ofdate %d Ra %6.3lf Dec %6.3lf\n", Az+180.0, Za, i_mjd, utcFrac, ofDate, *Ra, *Dec);

    // Take care of wrap situations in RA
    while (*Ra < 0) {
        *Ra += 24;
    }
    *Ra = fmod(*Ra,24);
}

//__________________________________________________________________________
double wrap(double Val, long LowBound, long HighBound, bool LowInclusive) {
//__________________________________________________________________________

    if (LowInclusive) {
        while (Val >= HighBound) {
            Val -= HighBound;
        }
        while (Val <   LowBound) {
            Val += HighBound;
        }
    } else {
        while (Val >  HighBound) {
            Val -= HighBound;
        }
        while (Val <=  LowBound) {
            Val += HighBound;
        }
    }
    return Val;
}

