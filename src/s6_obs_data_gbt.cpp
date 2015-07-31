#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_gbt.h"

#define redis_get_string(key,struct,rediscontext) reply = (redisReply *)redisCommand(rediscontext,"GET key"); strcpy(struct->key,reply->str); freeReplyObject(reply);

//----------------------------------------------------------
redisContext * redis_connect(char *hostname, int port) {
//----------------------------------------------------------
    redisContext *c;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

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

    return(c);

}

//----------------------------------------------------------
int s6_strcpy(char * dest, char * src) {
//----------------------------------------------------------

    strncpy(dest, src, GBTSTATUS_STRING_SIZE);
    if(dest[GBTSTATUS_STRING_SIZE-1] != '\0') {
        dest[GBTSTATUS_STRING_SIZE-1] = '\0';
        hashpipe_error(__FUNCTION__, "GBT status string exceeded buffer size, truncated");
    }
}

//----------------------------------------------------------
int put_obs_gbt_info_to_redis(char * fits_filename, int instance, char *hostname, int port) {
//----------------------------------------------------------
    redisContext *c;
    redisReply *reply;
    char key[200];
    int rv;

    // TODO - sane rv

    // TODO make c static?
    c = redis_connect(hostname, port);

    // update current filename
    char my_hostname[200];
    // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
    rv =  gethostname(my_hostname, sizeof(my_hostname));
    sprintf(key, "FN%s_%02d", my_hostname, instance);
    //fprintf(stderr, "redis SET: %s %s %ld\n", key, fits_filename, strlen(fits_filename));
    reply = (redisReply *)redisCommand(c,"SET %s %s", key, fits_filename);
    freeReplyObject(reply);

    redisFree(c);       // TODO do I really want to free each time?

    return(rv);
}

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

    // TODO make c static?
    c = redis_connect(hostname, port);

    reply = (redisReply *)redisCommand(c,"GET MJD"); gbtstatus->MJD = atof(reply->str); freeReplyObject(reply); 
// TODO - re-enable this
#if 0
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
#endif
     
    if (!rv) {

      reply = (redisReply *)redisCommand(c,"GET LASTUPDT"); s6_strcpy(gbtstatus->LASTUPDT,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET LST"); s6_strcpy(gbtstatus->LST,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET UTC"); s6_strcpy(gbtstatus->UTC,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET EPOCH"); s6_strcpy(gbtstatus->EPOCH,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MAJTYPE"); s6_strcpy(gbtstatus->MAJTYPE,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MINTYPE"); s6_strcpy(gbtstatus->MINTYPE,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MAJOR"); s6_strcpy(gbtstatus->MAJOR,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET MINOR"); s6_strcpy(gbtstatus->MINOR,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET AZCOMM"); gbtstatus->AZCOMM = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELCOMM"); gbtstatus->ELCOMM = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET AZACTUAL"); gbtstatus->AZACTUAL = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELACTUAL"); gbtstatus->ELACTUAL = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET AZERROR"); gbtstatus->AZERROR = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET ELERROR"); gbtstatus->ELERROR = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET LPCS"); s6_strcpy(gbtstatus->LPCS,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET FOCUSOFF"); s6_strcpy(gbtstatus->FOCUSOFF,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET ANTMOT"); s6_strcpy(gbtstatus->ANTMOT,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET RECEIVER"); s6_strcpy(gbtstatus->RECEIVER,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET IFFRQ1ST"); gbtstatus->IFFRQ1ST = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET IFFRQRST"); gbtstatus->IFFRQRST = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET DCRSCFRQ"); gbtstatus->DCRSCFRQ = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET SPRCSFRQ"); gbtstatus->SPRCSFRQ = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET FREQ"); gbtstatus->FREQ = atof(reply->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"GET VELFRAME"); s6_strcpy(gbtstatus->VELFRAME,reply->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"GET VELDEF"); s6_strcpy(gbtstatus->VELDEF,reply->str); freeReplyObject(reply);
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

    // Web based operator off/on switch
    if (!rv) {
      gbtstatus->WEBCNTRL = 0;  // default to off
      reply = (redisReply *)redisCommand(c,"GET WEBCNTRL"); gbtstatus->WEBCNTRL = atoi(reply->str); freeReplyObject(reply); 
    }



    // ADC RMS values (we get the time from the first set only)
// TODO - re-enable?
#if 0
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
#endif

    redisFree(c);       // TODO do I really want to free each time?

    return rv;         

    }
