#ifndef bool
#define bool int
#define true  1
#define false 0
#endif

// latitude:   38 deg 25.98727 mins
// longitude: -79 deg 50.39010 mins

#define GBT_LATITUDE   38.433121
#define GBT_LONGITUDE -79.839835

#define SECCON  206264.8062470964

void co_ZenAzToRaDec(double zenith_ang, double azimuth, double lsthour, double *ra, double *dec, double latitude);
void co_EqToXyz(double ra, double dec, double *xyz);
void co_XyzToEq(double xyz[], double *ra, double *dec);
void co_Precess(double e1, double *pos1, double e2, double *pos2);
double tm_JulianEpochToJulianDate(double je);
double tm_JulianDateToJulianEpoch(double jd);
double Atan2 (double y, double x);
