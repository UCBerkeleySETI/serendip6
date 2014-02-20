#ifndef _S6TIME_H
#define _S6TIME_H

#ifdef __cplusplus
extern "C" {
#endif

double s6_seti_ao_timeMS2unixtime(long timeMs, time_t now);

#ifdef __cplusplus
}
#endif

#endif  // _S6TIME_H
