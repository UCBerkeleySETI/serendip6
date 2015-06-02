#include <math.h>
#include "s6_obsaux_gbt.h"

// stuff mostly copied from setilib/seti_coord/seti_time
//////////////////////////////////////////////////////////

static const double D2R=(M_PI/180.0);  /* decimal degrees to radians conversion */

void co_ZenAzToRaDec(double zenith_ang, // = 90 - elevation
        double azimuth,
        double lsthour,
        double *ra,
        double *dec,
        double latitude) {

    double dec_rad, hour_ang_rad;
    double temp;

    zenith_ang *= D2R;      /* decimal degrees to radians */
    azimuth    *= D2R;

    /***** figure and return declination *****/

    dec_rad = asin ((cos(zenith_ang) * sin(latitude*D2R)) +
            (sin(zenith_ang) * cos(latitude*D2R) * cos(azimuth)));

    *dec = dec_rad / D2R;   /* radians to decimal degrees */

    /***** figure and return right ascension *****/

    temp = (cos(zenith_ang) - sin(latitude*D2R) * sin(dec_rad)) /
            (cos(latitude*D2R) * cos(dec_rad));
    if (temp > 1) {
        temp = 1;
    } else if (temp < -1) {
        temp = -1;
    }
    hour_ang_rad = acos (temp);


    if (sin(azimuth) > 0.0) {       /* insure correct quadrant */
        hour_ang_rad = (2 * M_PI) - hour_ang_rad;
    }


    /* to get ra, we convert hour angle to decimal degrees, */
    /* convert degrees to decimal hours, and subtract the   */
    /* result from local sidereal decimal hours.            */
    *ra = lsthour - ((hour_ang_rad / D2R) / 15.0);

    // Take care of wrap situations in RA
    if (*ra < 0.0) {
        *ra += 24.0;
    } else if (*ra >= 24.0) {
        *ra -= 24.0;
    }

}                     /* end zen_az_2_ra_dec */

void            co_EqToXyz(double ra, double dec, double *xyz) {
    double        alpha, delta;           /* Radian equivalents of RA, DEC */
    double        cdelta;

    alpha = M_PI * ra / 12;               /* Convert to radians.*/
    delta = M_PI * dec / 180;
    cdelta = cos(delta);
    xyz[0] = cdelta * cos(alpha);
    xyz[1] = cdelta * sin(alpha);
    xyz[2] = sin(delta);
}


void            co_XyzToEq(double xyz[], double *ra, double *dec) {
    double r,rho,x,y,z;
    x = xyz[0];
    y = xyz[1];
    z = xyz[2];
    rho = sqrt(x * x + y * y);
    r = sqrt(rho * rho + z * z);
    if (r < 1.0e-20) {
        *ra = 0.0;
        *dec = 0.0;
    } else {
        *ra = Atan2 (y,x) * 12.0 / M_PI;
        if (fabs(rho) > fabs(z)) {
            *dec = atan(z/rho) * 180.0 / M_PI;
        } else {
            if (z > 0.0) {
                *dec = 90.0 - atan(rho/z) * 180.0 / M_PI;
            } else {
                *dec = -90.0 - atan(rho/z) * 180.0 / M_PI;
            }
        }
    }
}

void            co_Precess(double e1, double *pos1, double e2, double *pos2) {
    double        tjd1;   /* Epoch1, expressed as a Julian date */
    double        tjd2;   /* Epoch2, expressed as a Julian date */
    double        t0;     /* Lieske's T (time in cents from Epoch1 to J2000) */
    double        t;      /* Lieske's t (time in cents from Epoch1 to Epoch2) */
    double        t02;    /* T0 ** 2 */
    double        t2;     /* T ** 2 */
    double        t3;     /* T ** 3 */
    double        zeta0;  /* Lieske's Zeta[A] */
    double        zee;    /* Lieske's Z[A] */
    double        theta;  /* Lieske's Theta[A] */
    /* The above three angles define the 3 rotations
    required to effect the coordinate transformation. */
    double        czeta0; /* cos (Zeta0) */
    double        szeta0; /* sin (Zeta0) */
    double        czee;   /* cos (Z) */
    double        szee;   /* sin (Z) */
    double        ctheta; /* cos (Theta) */
    double        stheta; /* sin (Theta) */
    double        xx,yx,zx, xy,yy,zy, xz,yz,zz;
    /* Matrix elements for the transformation. */
    double        vin[3]; /* Copy of input vector */
    //double      jetojd();

    vin[0] = pos1[0];
    vin[1] = pos1[1];
    vin[2] = pos1[2];
    tjd1 = tm_JulianEpochToJulianDate(e1);
    tjd2 = tm_JulianEpochToJulianDate(e2);
    t0 = (tjd1 - 2451545) / 36525;
    t = (tjd2 - tjd1) / 36525;
    t02 = t0 * t0;
    t2 = t * t;
    t3 = t2 * t;
    zeta0 = (2306.2181 + 1.39656 * t0 - 0.000139 * t02) * t +
            (0.30188 - 0.000344 * t0) * t2 +
            0.017998 * t3;
    zee = (2306.2181 + 1.39656 * t0 - 0.000139 * t02) * t +
            (1.09468 + 0.000066 * t0) * t2 +
            0.018203 * t3;
    theta = (2004.3109 - 0.85330 * t0 - 0.000217 * t02) * t +
            (-0.42665 - 0.000217 * t0) * t2 -
            0.041833 * t3;
    zeta0 /= SECCON;
    zee /= SECCON;
    theta /= SECCON;
    czeta0 = cos(zeta0);
    szeta0 = sin(zeta0);
    czee = cos(zee);
    szee = sin(zee);
    ctheta = cos(theta);
    stheta = sin(theta);

    /* Precession rotation matrix follows */
    xx = czeta0 * ctheta * czee - szeta0 * szee;
    yx = -szeta0 * ctheta * czee - czeta0 * szee;
    zx = -stheta * czee;
    xy = czeta0 * ctheta * szee + szeta0 * czee;
    yy = -szeta0 * ctheta * szee + czeta0 * czee;
    zy = -stheta * szee;
    xz = czeta0 * stheta;
    yz = -szeta0 * stheta;
    zz = ctheta;

    /* Perform rotation */
    pos2[0] = xx * vin[0] + yx * vin[1] + zx * vin[2];
    pos2[1] = xy * vin[0] + yy * vin[1] + zy * vin[2];
    pos2[2] = xz * vin[0] + yz * vin[1] + zz * vin[2];
}

double tm_JulianEpochToJulianDate(double je) {
    return (365.25 * (je - 2000) + 2451545);
}

double tm_JulianDateToJulianEpoch(double jd) {
    return (2000 + (jd - 2451545) / 365.25);
}
 

double Atan2 (double y, double x) { /* x, y in rectangular coordinates*/

    double  arg;

    if ((fabs(x) == 0.0) && (fabs(y) == 0.0)) {
        arg = 0.0;
    } else {
        if (x > 0) {
            if (x > fabs(y)) {
                arg = atan(y/x);
            } else if (y > 0.0) {
                arg = 0.5 * M_PI - atan(x/y);
            } else {
                arg = -0.5 * M_PI - atan(x/y);
            }
        } else {
            if (fabs(x) > fabs(y)) {
                arg = M_PI + atan (y/x);
            } else if (y > 0.0) {
                arg = 0.5*M_PI - atan (x/y);
            } else {
                arg = -0.5*M_PI - atan (x/y);
            }
        }
    }
    if (arg < 0.0) {
        arg += 2.0*M_PI;
    }
    return(arg);
}

