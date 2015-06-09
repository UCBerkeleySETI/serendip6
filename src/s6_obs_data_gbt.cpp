#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_gbt.h"

#define redis_get_string(key,struct,rediscontext) reply = (redisReply *)redisCommand(rediscontext,"GET key"); strcpy(struct->key,reply->str); freeReplyObject(reply);

//----------------------------------------------------------
int get_obs_gbt_info_from_redis(gbtstatus_t * gbtstatus,     
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

    reply = (redisReply *)redisCommand(c,"GET MJD"); gbtstatus->MJD = atof(reply->str); freeReplyObject(reply); 
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
      prior_mjd = gbtstatus->MJD;
      }
     
    if (!rv) {

      reply = (redisReply *)redisCommand(c,"GET LASTUPDT"); strcpy(gbtstatus->LASTUPDT,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET LST"); strcpy(gbtstatus->LST,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET UTC"); strcpy(gbtstatus->UTC,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET EPOCH"); strcpy(gbtstatus->EPOCH,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MAJTYPE"); strcpy(gbtstatus->MAJTYPE,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MINTYPE"); strcpy(gbtstatus->MINTYPE,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MAJOR"); strcpy(gbtstatus->MAJOR,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MINOR"); strcpy(gbtstatus->MINOR,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET AZCOMM"); gbtstatus->AZCOMM = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELCOMM"); gbtstatus->ELCOMM = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET AZACTUAL"); gbtstatus->AZACTUAL = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELACTUAL"); gbtstatus->ELACTUAL = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET AZERROR"); gbtstatus->AZERROR = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELERROR"); gbtstatus->ELERROR = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET LPCS"); strcpy(gbtstatus->LPCS,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET FOCUSOFF"); strcpy(gbtstatus->FOCUSOFF,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET ANTMOT"); strcpy(gbtstatus->ANTMOT,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET RECEIVER"); strcpy(gbtstatus->RECEIVER,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET IFFRQ1ST"); gbtstatus->IFFRQ1ST = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET IFFRQRST"); gbtstatus->IFFRQRST = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET FREQ"); gbtstatus->FREQ = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET VELFRAME"); strcpy(gbtstatus->VELFRAME,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET VELDEF"); strcpy(gbtstatus->VELDEF,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET J2000MAJ"); gbtstatus->J2000MAJ = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET J2000MIN"); gbtstatus->J2000MIN = atof(reply->str); freeReplyObject(reply); 

      reply = (redisReply *)redisCommand(c,"GET LSTH_DRV"); gbtstatus->LSTH_DRV = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET RA_DRV"); gbtstatus->RA_DRV = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET RADG_DRV"); gbtstatus->RADG_DRV = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET DEC_DRV"); gbtstatus->DEC_DRV = atof(reply->str); freeReplyObject(reply); 
   
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

    // ONLY ONE ROACH AT GBT (not like AO, which has 2)
    //
    // if (!rv) {
    //   reply = (redisReply *)redisCommand(c, "HMGET ADC2RMS      ADCRMS1 ADCRMS2 ADCRMS3 ADCRMS4 ADCRMS5 ADCRMS6 ADCRMS7 ADCRMS8");
    //   if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
    //   else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
    //   else if (!reply->element[0]->str) { fprintf(stderr,"ADC2RMS not set yet!\n"); rv = 1; }
    //   else {
    //       gbtstatus->ADC2RMS[0] = atof(reply->element[0]->str);
    //       gbtstatus->ADC2RMS[1] = atof(reply->element[1]->str);
    //       gbtstatus->ADC2RMS[2] = atof(reply->element[2]->str);
    //       gbtstatus->ADC2RMS[3] = atof(reply->element[3]->str);
    //       gbtstatus->ADC2RMS[4] = atof(reply->element[4]->str);
    //       gbtstatus->ADC2RMS[5] = atof(reply->element[5]->str);
    //       gbtstatus->ADC2RMS[6] = atof(reply->element[6]->str);
    //       gbtstatus->ADC2RMS[7] = atof(reply->element[7]->str);
    //   }
    //   freeReplyObject(reply);
    // }

    redisFree(c);       // TODO do I really want to free each time?

    return rv;         

    }
