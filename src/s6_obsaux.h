#ifndef _S6OBSAUX_H
#define _S6OBSAUX_H

#include <stdio.h>
#include <math.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define bool int
#define true  1
#define false 0

static const double D2R=(M_PI/180.0);

double s6_seti_ao_timeMS2unixtime(long timeMs, time_t now);
bool s6_BeamOffset(double *Az, double *ZA, int Beam, double AlfaMotorPosition);
void s6_AzZaToRaDec(double Az, double Za, double coord_time, double *Ra, double *Dec);
double wrap(double Val, long LowBound, long HighBound, bool LowInclusive);

#ifdef __cplusplus
}
#endif

#endif  // _S6OBSAUX_H
