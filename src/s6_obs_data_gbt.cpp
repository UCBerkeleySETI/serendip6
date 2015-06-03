#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// #include <hiredis/hiredis.h>
#include <hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_gbt.h"

#define redis_get_string(key,struct,rediscontext) reply = redisCommand(rediscontext,"GET key"); struct->key = reply->str; freeReplyObject(reply);
#define redis_get_double(key,struct,rediscontext) reply = redisCommand(rediscontext,"GET key"); struct->key = atof(reply->str); freeReplyObject(reply);

//----------------------------------------------------------
int get_obs_info_from_redis(gbtstatus_t *gbtstatus,     
                            char    *hostname, 
                            int     port) {
//----------------------------------------------------------

    redisContext *c;
    redisReply *reply;
    int rv = 0;

    static double prior_mjd=0;
    static int no_time_change_count=0;
    int no_time_change_limit=2;

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    // TODO make c static?
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            hashpipe_error(__FUNCTION__, c->errstr);
            redisFree(c);
        } else {
            hashpipe_error(__FUNCTION__, "Connection error: can't allocate redis context");
        }
        exit(1);
    }

    redis_get_double(MJD,gbtstatus,c); 
    if (gbtstatus->MJD == prior_mjd) {
      no_time_change_count++;
      hashpipe_warn(__FUNCTION__, "mjd in redis databse has not been updated over %d queries", no_time_change_count);
      if(no_time_change_count >= no_time_change_limit) {
        hashpipe_error(__FUNCTION__, "redis databse is static!");
        rv = 1;
        }
      } 
    else {
      no_time_change_count = 0;
      prior_agc_time = gbtstatus->MJD;
      }
     
    if (!rv) {

      redis_get_string(LASTUPDT,gbtstatus,c);
      redis_get_string(LST,gbtstatus,c);
      redis_get_string(UTC,gbtstatus,c);
      redis_get_string(EPOCH,gbtstatus,c);
      redis_get_string(MAJTYPE,gbtstatus,c);
      redis_get_string(MINTYPE,gbtstatus,c);
      redis_get_string(MAJOR,gbtstatus,c);
      redis_get_string(MINOR,gbtstatus,c);
      redis_get_double(AZCOMM,gbtstatus,c);
      redis_get_double(ELCOMM,gbtstatus,c);
      redis_get_double(AZACTUAL,gbtstatus,c);
      redis_get_double(ELACTUAL,gbtstatus,c);
      redis_get_double(AZERROR,gbtstatus,c);
      redis_get_double(ELERROR,gbtstatus,c);
      redis_get_string(LPCS,gbtstatus,c);
      redis_get_string(FOCUSOFF,gbtstatus,c);
      redis_get_string(ANTMOT,gbtstatus,c);
      redis_get_string(RECEIVER,gbtstatus,c);
      redis_get_double(IFFRQ1ST,gbtstatus,c);
      redis_get_double(IFFRQRST,gbtstatus,c);
      redis_get_double(FREQ,gbtstatus,c);
      redis_get_string(VELFRAME,gbtstatus,c);
      redis_get_string(VELDEF,gbtstatus,c);
      redis_get_double(J2000MAJ,gbtstatus,c);
      redis_get_double(J2000MIN,gbtstatus,c);

      redis_get_double(LSTH_DRV,gbtstatus,c);
      redis_get_double(RA_DRV,gbtstatus,c);
      redis_get_double(RADG_DRV,gbtstatus,c);
      redis_get_double(DEC_DRV,gbtstatus,c);
   
      }

    // Sample clock rate parameters
    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET CLOCKSYN      CLOCKTIM CLOCKFRQ CLOCKDBM CLOCKLOC");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"CLOCKSYN not set yet!\n"); rv = 1; }
      else {
          gbtstatus->CLOCKTIM = atoi(reply->element[0]->str);
          gbtstatus->CLOCKFRQ = atof(reply->element[1]->str);
          gbtstatus->CLOCKDBM = atof(reply->element[2]->str);
          gbtstatus->CLOCKLOC = atoi(reply->element[3]->str);
      }
      freeReplyObject(reply);
    }

    // Birdie frequency parameters
    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET BIRDISYN      BIRDITIM BIRDIFRQ BIRDIDBM BIRDILOC");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"BIRDISYN not set yet!\n"); rv = 1; }
      else {
          gbtstatus->BIRDITIM = atoi(reply->element[0]->str);
          gbtstatus->BIRDIFRQ = atof(reply->element[1]->str);
          gbtstatus->BIRDIDBM = atof(reply->element[2]->str);
          gbtstatus->BIRDILOC = atoi(reply->element[3]->str);
      }
      freeReplyObject(reply);
    }


    // ADC RMS values (we get the time from the first set only)
    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET ADC1RMS      ADCRMSTM ADCRMS1 ADCRMS2 ADCRMS3 ADCRMS4 ADCRMS5 ADCRMS6 ADCRMS7 ADCRMS8");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"ADC1RMS not set yet!\n"); rv = 1; }
      else {
          gbtstatus->ADCRMSTM   = atof(reply->element[0]->str);
          gbtstatus->ADC1RMS[0] = atof(reply->element[1]->str);
          gbtstatus->ADC1RMS[1] = atof(reply->element[2]->str);
          gbtstatus->ADC1RMS[2] = atof(reply->element[3]->str);
          gbtstatus->ADC1RMS[3] = atof(reply->element[4]->str);
          gbtstatus->ADC1RMS[4] = atof(reply->element[5]->str);
          gbtstatus->ADC1RMS[5] = atof(reply->element[6]->str);
          gbtstatus->ADC1RMS[6] = atof(reply->element[7]->str);
          gbtstatus->ADC1RMS[7] = atof(reply->element[8]->str);
      }
      freeReplyObject(reply);
    }
    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET ADC2RMS      ADCRMS1 ADCRMS2 ADCRMS3 ADCRMS4 ADCRMS5 ADCRMS6 ADCRMS7 ADCRMS8");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"ADC2RMS not set yet!\n"); rv = 1; }
      else {
          gbtstatus->ADC2RMS[0] = atof(reply->element[0]->str);
          gbtstatus->ADC2RMS[1] = atof(reply->element[1]->str);
          gbtstatus->ADC2RMS[2] = atof(reply->element[2]->str);
          gbtstatus->ADC2RMS[3] = atof(reply->element[3]->str);
          gbtstatus->ADC2RMS[4] = atof(reply->element[4]->str);
          gbtstatus->ADC2RMS[5] = atof(reply->element[5]->str);
          gbtstatus->ADC2RMS[6] = atof(reply->element[6]->str);
          gbtstatus->ADC2RMS[7] = atof(reply->element[7]->str);
      }
      freeReplyObject(reply);
    }

    redisFree(c);       // TODO do I really want to free each time?

    return rv;         

    }
