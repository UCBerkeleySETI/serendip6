#include <stdlib.h>
#include <math.h>
#include <time.h>

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
// NOTE : This is AO specific code, due to the AST offset!

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

