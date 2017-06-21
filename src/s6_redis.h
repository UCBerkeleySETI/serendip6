#ifndef _S6_REDIS_H
#define _S6_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif

// In production environments REDISHOST is often set to "redishost" 
// (resolved via name resolution).  In test environments REDISHOST 
// is often set to "localhost". 
#define REDISHOST ("localhost")

#ifdef __cplusplus
}
#endif

#endif  // _S6_REDIS_H
