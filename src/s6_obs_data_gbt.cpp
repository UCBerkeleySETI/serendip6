#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_gbt.h"

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

    reply = (redisReply *)redisCommand(c,"HMGET MJD VALUE"); gbtstatus->MJD = atof(reply->element[0]->str); freeReplyObject(reply); 
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

      // mysql/gbstatus values

      reply = (redisReply *)redisCommand(c,"HMGET LASTUPDT VALUE"); s6_strcpy(gbtstatus->LASTUPDT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LASTUPDT STIME"); gbtstatus->LASTUPDTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LST VALUE"); s6_strcpy(gbtstatus->LST,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LST STIME"); gbtstatus->LSTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET UTC VALUE"); s6_strcpy(gbtstatus->UTC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET UTC STIME"); gbtstatus->UTCSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET EPOCH VALUE"); s6_strcpy(gbtstatus->EPOCH,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET EPOCH STIME"); gbtstatus->EPOCHSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MAJTYPE VALUE"); s6_strcpy(gbtstatus->MAJTYPE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MAJTYPE STIME"); gbtstatus->MAJTYPESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MINTYPE VALUE"); s6_strcpy(gbtstatus->MINTYPE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MINTYPE STIME"); gbtstatus->MINTYPESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MAJOR VALUE"); s6_strcpy(gbtstatus->MAJOR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MAJOR STIME"); gbtstatus->MAJORSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MINOR VALUE"); s6_strcpy(gbtstatus->MINOR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET MINOR STIME"); gbtstatus->MINORSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET AZCOMM VALUE"); gbtstatus->AZCOMM = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET AZCOMM STIME"); gbtstatus->AZCOMMSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELCOMM VALUE"); gbtstatus->ELCOMM = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELCOMM STIME"); gbtstatus->ELCOMMSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET AZACTUAL VALUE"); gbtstatus->AZACTUAL = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET AZACTUAL STIME"); gbtstatus->AZACTUALSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELACTUAL VALUE"); gbtstatus->ELACTUAL = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELACTUAL STIME"); gbtstatus->ELACTUALSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET AZERROR VALUE"); gbtstatus->AZERROR = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET AZERROR STIME"); gbtstatus->AZERRORSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELERROR VALUE"); gbtstatus->ELERROR = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET ELERROR STIME"); gbtstatus->ELERRORSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET LPCS VALUE"); s6_strcpy(gbtstatus->LPCS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LPCS STIME"); gbtstatus->LPCSSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET FOCUSOFF VALUE"); s6_strcpy(gbtstatus->FOCUSOFF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET FOCUSOFF STIME"); gbtstatus->FOCUSOFFSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ANTMOT VALUE"); s6_strcpy(gbtstatus->ANTMOT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ANTMOT STIME"); gbtstatus->ANTMOTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET RECEIVER VALUE"); s6_strcpy(gbtstatus->RECEIVER,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET RECEIVER STIME"); gbtstatus->RECEIVERSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFFRQ1ST VALUE"); gbtstatus->IFFRQ1ST = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET IFFRQ1ST STIME"); gbtstatus->IFFRQ1STSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET IFFRQRST VALUE"); gbtstatus->IFFRQRST = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET IFFRQRST STIME"); gbtstatus->IFFRQRSTSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET DCRSCFRQ VALUE"); gbtstatus->DCRSCFRQ = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET DCRSCFRQ STIME"); gbtstatus->DCRSCFRQSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET SPRCSFRQ VALUE"); gbtstatus->SPRCSFRQ = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET SPRCSFRQ STIME"); gbtstatus->SPRCSFRQSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET FREQ VALUE"); gbtstatus->FREQ = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET FREQ STIME"); gbtstatus->FREQSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET VELFRAME VALUE"); s6_strcpy(gbtstatus->VELFRAME,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VELFRAME STIME"); gbtstatus->VELFRAMESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VELDEF VALUE"); s6_strcpy(gbtstatus->VELDEF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VELDEF STIME"); gbtstatus->VELDEFSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET J2000MAJ VALUE"); gbtstatus->J2000MAJ = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET J2000MAJ STIME"); gbtstatus->J2000MAJSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET J2000MIN VALUE"); gbtstatus->J2000MIN = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET J2000MIN STIME"); gbtstatus->J2000MINSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 

      // values derived from mysql/gbstatus

      reply = (redisReply *)redisCommand(c,"HMGET LSTH_DRV VALUE"); gbtstatus->LSTH_DRV = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET LSTH_DRV STIME"); gbtstatus->LSTH_DRVSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET RA_DRV VALUE"); gbtstatus->RA_DRV = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET RA_DRV STIME"); gbtstatus->RA_DRVSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET RADG_DRV VALUE"); gbtstatus->RADG_DRV = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET RADG_DRV STIME"); gbtstatus->RADG_DRVSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET DEC_DRV VALUE"); gbtstatus->DEC_DRV = atof(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET DEC_DRV STIME"); gbtstatus->DEC_DRVSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 

      // cleo values

#if 0

      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM VALUE"); s6_strcpy(gbtstatus->ATATCOTM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM STIME"); gbtstatus->ATATCOTMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM MJD"); gbtstatus->ATATCOTMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF VALUE"); s6_strcpy(gbtstatus->ATBEELOF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF STIME"); gbtstatus->ATBEELOFSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF MJD"); gbtstatus->ATBEELOFMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO VALUE"); s6_strcpy(gbtstatus->ATBOT1EO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO STIME"); gbtstatus->ATBOT1EOSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO MJD"); gbtstatus->ATBOT1EOMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO VALUE"); s6_strcpy(gbtstatus->ATBOT1XO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO STIME"); gbtstatus->ATBOT1XOSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO MJD"); gbtstatus->ATBOT1XOMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM VALUE"); s6_strcpy(gbtstatus->ATBOT1NM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM STIME"); gbtstatus->ATBOT1NMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM MJD"); gbtstatus->ATBOT1NMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 VALUE"); s6_strcpy(gbtstatus->ATBOT1F1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 STIME"); gbtstatus->ATBOT1F1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 MJD"); gbtstatus->ATBOT1F1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 VALUE"); s6_strcpy(gbtstatus->ATBOT1F2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 STIME"); gbtstatus->ATBOT1F2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 MJD"); gbtstatus->ATBOT1F2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF VALUE"); s6_strcpy(gbtstatus->ATBEXLOF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF STIME"); gbtstatus->ATBEXLOFSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF MJD"); gbtstatus->ATBEXLOFMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD VALUE"); s6_strcpy(gbtstatus->ATFCTRMD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD STIME"); gbtstatus->ATFCTRMDSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD MJD"); gbtstatus->ATFCTRMDMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC VALUE"); s6_strcpy(gbtstatus->ATGREGRC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC STIME"); gbtstatus->ATGREGRCSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC MJD"); gbtstatus->ATGREGRCMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX VALUE"); s6_strcpy(gbtstatus->ATLFCX,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX STIME"); gbtstatus->ATLFCXSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX MJD"); gbtstatus->ATLFCXMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT VALUE"); s6_strcpy(gbtstatus->ATLFCXT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT STIME"); gbtstatus->ATLFCXTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT MJD"); gbtstatus->ATLFCXTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY VALUE"); s6_strcpy(gbtstatus->ATLFCY,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY STIME"); gbtstatus->ATLFCYSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY MJD"); gbtstatus->ATLFCYMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT VALUE"); s6_strcpy(gbtstatus->ATLFCYT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT STIME"); gbtstatus->ATLFCYTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT MJD"); gbtstatus->ATLFCYTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ VALUE"); s6_strcpy(gbtstatus->ATLFCZ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ STIME"); gbtstatus->ATLFCZSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ MJD"); gbtstatus->ATLFCZMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT VALUE"); s6_strcpy(gbtstatus->ATLFCZT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT STIME"); gbtstatus->ATLFCZTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT MJD"); gbtstatus->ATLFCZTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 STIME"); gbtstatus->ATLPOAZ1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 MJD"); gbtstatus->ATLPOAZ1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 STIME"); gbtstatus->ATLPOAZ2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 MJD"); gbtstatus->ATLPOAZ2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL VALUE"); s6_strcpy(gbtstatus->ATLPOEL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL STIME"); gbtstatus->ATLPOELSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL MJD"); gbtstatus->ATLPOELMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA VALUE"); s6_strcpy(gbtstatus->ATMATMCA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA STIME"); gbtstatus->ATMATMCASTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA MJD"); gbtstatus->ATMATMCAMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM VALUE"); s6_strcpy(gbtstatus->ATMATMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM STIME"); gbtstatus->ATMATMCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM MJD"); gbtstatus->ATMATMCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT VALUE"); s6_strcpy(gbtstatus->ATMATMCT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT STIME"); gbtstatus->ATMATMCTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT MJD"); gbtstatus->ATMATMCTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 VALUE"); s6_strcpy(gbtstatus->ATMCDCJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 STIME"); gbtstatus->ATMCDCJ2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 MJD"); gbtstatus->ATMCDCJ2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 VALUE"); s6_strcpy(gbtstatus->ATMCRAJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 STIME"); gbtstatus->ATMCRAJ2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 MJD"); gbtstatus->ATMCRAJ2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS VALUE"); s6_strcpy(gbtstatus->ATMFBS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS STIME"); gbtstatus->ATMFBSSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS MJD"); gbtstatus->ATMFBSMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS VALUE"); s6_strcpy(gbtstatus->ATMTLS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS STIME"); gbtstatus->ATMTLSSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS MJD"); gbtstatus->ATMTLSMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD VALUE"); s6_strcpy(gbtstatus->ATOPTMOD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD STIME"); gbtstatus->ATOPTMODSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD MJD"); gbtstatus->ATOPTMODMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR VALUE"); s6_strcpy(gbtstatus->ATRECVR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR STIME"); gbtstatus->ATRECVRSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR MJD"); gbtstatus->ATRECVRMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA VALUE"); s6_strcpy(gbtstatus->ATRXOCTA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA STIME"); gbtstatus->ATRXOCTASTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA MJD"); gbtstatus->ATRXOCTAMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM VALUE"); s6_strcpy(gbtstatus->ATTRBEAM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM STIME"); gbtstatus->ATTRBEAMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM MJD"); gbtstatus->ATTRBEAMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC VALUE"); s6_strcpy(gbtstatus->ATTURLOC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC STIME"); gbtstatus->ATTURLOCSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC MJD"); gbtstatus->ATTURLOCMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA VALUE"); s6_strcpy(gbtstatus->BAMBAMCA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA STIME"); gbtstatus->BAMBAMCASTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA MJD"); gbtstatus->BAMBAMCAMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM VALUE"); s6_strcpy(gbtstatus->BAMBAMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM STIME"); gbtstatus->BAMBAMCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM MJD"); gbtstatus->BAMBAMCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT VALUE"); s6_strcpy(gbtstatus->BAMBAMCT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT STIME"); gbtstatus->BAMBAMCTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT MJD"); gbtstatus->BAMBAMCTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 VALUE"); s6_strcpy(gbtstatus->BAMMPWR1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 STIME"); gbtstatus->BAMMPWR1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 MJD"); gbtstatus->BAMMPWR1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 VALUE"); s6_strcpy(gbtstatus->BAMMPWR2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 STIME"); gbtstatus->BAMMPWR2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 MJD"); gbtstatus->BAMMPWR2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID VALUE"); s6_strcpy(gbtstatus->CLEOPID,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID STIME"); gbtstatus->CLEOPIDSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID MJD"); gbtstatus->CLEOPIDMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV VALUE"); s6_strcpy(gbtstatus->CLEOREV,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV STIME"); gbtstatus->CLEOREVSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV MJD"); gbtstatus->CLEOREVMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM VALUE"); s6_strcpy(gbtstatus->IFMIFMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM STIME"); gbtstatus->IFMIFMCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM MJD"); gbtstatus->IFMIFMCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW VALUE"); s6_strcpy(gbtstatus->IFV1BW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW STIME"); gbtstatus->IFV1BWSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW MJD"); gbtstatus->IFV1BWMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ VALUE"); s6_strcpy(gbtstatus->IFV1CSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ STIME"); gbtstatus->IFV1CSFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ MJD"); gbtstatus->IFV1CSFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL VALUE"); s6_strcpy(gbtstatus->IFV1LVL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL STIME"); gbtstatus->IFV1LVLSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL MJD"); gbtstatus->IFV1LVLMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ VALUE"); s6_strcpy(gbtstatus->IFV1IFFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ STIME"); gbtstatus->IFV1IFFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ MJD"); gbtstatus->IFV1IFFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB VALUE"); s6_strcpy(gbtstatus->IFV1SSB,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB STIME"); gbtstatus->IFV1SSBSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB MJD"); gbtstatus->IFV1SSBMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ VALUE"); s6_strcpy(gbtstatus->IFV1SKFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ STIME"); gbtstatus->IFV1SKFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ MJD"); gbtstatus->IFV1SKFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH VALUE"); s6_strcpy(gbtstatus->IFV1TH,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH STIME"); gbtstatus->IFV1THSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH MJD"); gbtstatus->IFV1THMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI VALUE"); s6_strcpy(gbtstatus->IFV1TNCI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI STIME"); gbtstatus->IFV1TNCISTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI MJD"); gbtstatus->IFV1TNCIMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO VALUE"); s6_strcpy(gbtstatus->IFV1TNCO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO STIME"); gbtstatus->IFV1TNCOSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO MJD"); gbtstatus->IFV1TNCOMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT VALUE"); s6_strcpy(gbtstatus->IFV1TT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT STIME"); gbtstatus->IFV1TTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT MJD"); gbtstatus->IFV1TTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW VALUE"); s6_strcpy(gbtstatus->IFV2BW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW STIME"); gbtstatus->IFV2BWSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW MJD"); gbtstatus->IFV2BWMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ VALUE"); s6_strcpy(gbtstatus->IFV2CSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ STIME"); gbtstatus->IFV2CSFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ MJD"); gbtstatus->IFV2CSFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL VALUE"); s6_strcpy(gbtstatus->IFV2LVL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL STIME"); gbtstatus->IFV2LVLSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL MJD"); gbtstatus->IFV2LVLMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ VALUE"); s6_strcpy(gbtstatus->IFV2IFFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ STIME"); gbtstatus->IFV2IFFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ MJD"); gbtstatus->IFV2IFFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB VALUE"); s6_strcpy(gbtstatus->IFV2SSB,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB STIME"); gbtstatus->IFV2SSBSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB MJD"); gbtstatus->IFV2SSBMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ VALUE"); s6_strcpy(gbtstatus->IFV2SKFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ STIME"); gbtstatus->IFV2SKFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ MJD"); gbtstatus->IFV2SKFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH VALUE"); s6_strcpy(gbtstatus->IFV2TH,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH STIME"); gbtstatus->IFV2THSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH MJD"); gbtstatus->IFV2THMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI VALUE"); s6_strcpy(gbtstatus->IFV2TNCI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI STIME"); gbtstatus->IFV2TNCISTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI MJD"); gbtstatus->IFV2TNCIMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO VALUE"); s6_strcpy(gbtstatus->IFV2TNCO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO STIME"); gbtstatus->IFV2TNCOSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO MJD"); gbtstatus->IFV2TNCOMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT VALUE"); s6_strcpy(gbtstatus->IFV2TT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT STIME"); gbtstatus->IFV2TTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT MJD"); gbtstatus->IFV2TTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA VALUE"); s6_strcpy(gbtstatus->LO1ACA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA STIME"); gbtstatus->LO1ACASTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA MJD"); gbtstatus->LO1ACAMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM VALUE"); s6_strcpy(gbtstatus->LO1ACM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM STIME"); gbtstatus->LO1ACMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM MJD"); gbtstatus->LO1ACMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT VALUE"); s6_strcpy(gbtstatus->LO1ACT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT STIME"); gbtstatus->LO1ACTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT MJD"); gbtstatus->LO1ACTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ VALUE"); s6_strcpy(gbtstatus->LO1APSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ STIME"); gbtstatus->LO1APSFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ MJD"); gbtstatus->LO1APSFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCA VALUE"); s6_strcpy(gbtstatus->LO1BCA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCA STIME"); gbtstatus->LO1BCASTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCA MJD"); gbtstatus->LO1BCAMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCM VALUE"); s6_strcpy(gbtstatus->LO1BCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCM STIME"); gbtstatus->LO1BCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCM MJD"); gbtstatus->LO1BCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCT VALUE"); s6_strcpy(gbtstatus->LO1BCT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCT STIME"); gbtstatus->LO1BCTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BCT MJD"); gbtstatus->LO1BCTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ VALUE"); s6_strcpy(gbtstatus->LO1BPSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ STIME"); gbtstatus->LO1BPSFQSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ MJD"); gbtstatus->LO1BPSFQMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW VALUE"); s6_strcpy(gbtstatus->LO1FQSW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW STIME"); gbtstatus->LO1FQSWSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW MJD"); gbtstatus->LO1FQSWMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM VALUE"); s6_strcpy(gbtstatus->LO1CM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM STIME"); gbtstatus->LO1CMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM MJD"); gbtstatus->LO1CMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG VALUE"); s6_strcpy(gbtstatus->LO1CFG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG STIME"); gbtstatus->LO1CFGSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG MJD"); gbtstatus->LO1CFGMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL VALUE"); s6_strcpy(gbtstatus->LO1PHCAL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL STIME"); gbtstatus->LO1PHCALSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL MJD"); gbtstatus->LO1PHCALMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG VALUE"); s6_strcpy(gbtstatus->OPTGREG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG STIME"); gbtstatus->OPTGREGSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG MJD"); gbtstatus->OPTGREGMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME VALUE"); s6_strcpy(gbtstatus->OPTPRIME,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME STIME"); gbtstatus->OPTPRIMESTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME MJD"); gbtstatus->OPTPRIMEMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID VALUE"); s6_strcpy(gbtstatus->SCPROJID,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID STIME"); gbtstatus->SCPROJIDSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID MJD"); gbtstatus->SCPROJIDMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM VALUE"); s6_strcpy(gbtstatus->SCSCCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM STIME"); gbtstatus->SCSCCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM MJD"); gbtstatus->SCSCCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR VALUE"); s6_strcpy(gbtstatus->SCSNUMBR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR STIME"); gbtstatus->SCSNUMBRSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR MJD"); gbtstatus->SCSNUMBRMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE VALUE"); s6_strcpy(gbtstatus->SCSOURCE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE STIME"); gbtstatus->SCSOURCESTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE MJD"); gbtstatus->SCSOURCEMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE VALUE"); s6_strcpy(gbtstatus->SCSTATE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE STIME"); gbtstatus->SCSTATESTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE MJD"); gbtstatus->SCSTATEMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF VALUE"); s6_strcpy(gbtstatus->SCSACTSF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF STIME"); gbtstatus->SCSACTSFSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF MJD"); gbtstatus->SCSACTSFMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR VALUE"); s6_strcpy(gbtstatus->SCSANAFR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR STIME"); gbtstatus->SCSANAFRSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR MJD"); gbtstatus->SCSANAFRMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN VALUE"); s6_strcpy(gbtstatus->SCSANTEN,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN STIME"); gbtstatus->SCSANTENSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN MJD"); gbtstatus->SCSANTENMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI VALUE"); s6_strcpy(gbtstatus->SCSARCHI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI STIME"); gbtstatus->SCSARCHISTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI MJD"); gbtstatus->SCSARCHIMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM VALUE"); s6_strcpy(gbtstatus->SCSBCPM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM STIME"); gbtstatus->SCSBCPMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM MJD"); gbtstatus->SCSBCPMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 VALUE"); s6_strcpy(gbtstatus->SCSCCB26,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 STIME"); gbtstatus->SCSCCB26STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 MJD"); gbtstatus->SCSCCB26MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK VALUE"); s6_strcpy(gbtstatus->SCSCRACK,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK STIME"); gbtstatus->SCSCRACKSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK MJD"); gbtstatus->SCSCRACKMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR VALUE"); s6_strcpy(gbtstatus->SCSDCR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR STIME"); gbtstatus->SCSDCRSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR MJD"); gbtstatus->SCSDCRMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI VALUE"); s6_strcpy(gbtstatus->SCSGUPPI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI STIME"); gbtstatus->SCSGUPPISTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI MJD"); gbtstatus->SCSGUPPIMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG VALUE"); s6_strcpy(gbtstatus->SCSHOLOG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG STIME"); gbtstatus->SCSHOLOGSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG MJD"); gbtstatus->SCSHOLOGMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM VALUE"); s6_strcpy(gbtstatus->SCSIFM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM STIME"); gbtstatus->SCSIFMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM MJD"); gbtstatus->SCSIFMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR VALUE"); s6_strcpy(gbtstatus->SCSIFR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR STIME"); gbtstatus->SCSIFRSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR MJD"); gbtstatus->SCSIFRMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 VALUE"); s6_strcpy(gbtstatus->SCSLO1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 STIME"); gbtstatus->SCSLO1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 MJD"); gbtstatus->SCSLO1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR VALUE"); s6_strcpy(gbtstatus->SCSMEASUR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR STIME"); gbtstatus->SCSMEASURSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR MJD"); gbtstatus->SCSMEASURMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD VALUE"); s6_strcpy(gbtstatus->SCSQUADRD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD STIME"); gbtstatus->SCSQUADRDSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD MJD"); gbtstatus->SCSQUADRDMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 VALUE"); s6_strcpy(gbtstatus->SCSR1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 STIME"); gbtstatus->SCSR1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 MJD"); gbtstatus->SCSR1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 VALUE"); s6_strcpy(gbtstatus->SCSR12,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 STIME"); gbtstatus->SCSR12STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 MJD"); gbtstatus->SCSR12MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 VALUE"); s6_strcpy(gbtstatus->SCSR18,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 STIME"); gbtstatus->SCSR18STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 MJD"); gbtstatus->SCSR18MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 VALUE"); s6_strcpy(gbtstatus->SCSR2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 STIME"); gbtstatus->SCSR2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 MJD"); gbtstatus->SCSR2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 VALUE"); s6_strcpy(gbtstatus->SCSR26,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 STIME"); gbtstatus->SCSR26STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 MJD"); gbtstatus->SCSR26MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 VALUE"); s6_strcpy(gbtstatus->SCSR40,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 STIME"); gbtstatus->SCSR40STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 MJD"); gbtstatus->SCSR40MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 VALUE"); s6_strcpy(gbtstatus->SCSR4,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 STIME"); gbtstatus->SCSR4STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 MJD"); gbtstatus->SCSR4MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 VALUE"); s6_strcpy(gbtstatus->SCSR68,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 STIME"); gbtstatus->SCSR68STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 MJD"); gbtstatus->SCSR68MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 VALUE"); s6_strcpy(gbtstatus->SCSR8,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 STIME"); gbtstatus->SCSR8STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 MJD"); gbtstatus->SCSR8MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 VALUE"); s6_strcpy(gbtstatus->SCSRA1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 STIME"); gbtstatus->SCSRA1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 MJD"); gbtstatus->SCSRA1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 VALUE"); s6_strcpy(gbtstatus->SCSRA18,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 STIME"); gbtstatus->SCSRA18STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 MJD"); gbtstatus->SCSRA18MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 VALUE"); s6_strcpy(gbtstatus->SCSRMBA1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 STIME"); gbtstatus->SCSRMBA1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 MJD"); gbtstatus->SCSRMBA1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR VALUE"); s6_strcpy(gbtstatus->SCSRPAR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR STIME"); gbtstatus->SCSRPARSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR MJD"); gbtstatus->SCSRPARMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 VALUE"); s6_strcpy(gbtstatus->SCSRPF1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 STIME"); gbtstatus->SCSRPF1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 MJD"); gbtstatus->SCSRPF1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 VALUE"); s6_strcpy(gbtstatus->SCSRPF2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 STIME"); gbtstatus->SCSRPF2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 MJD"); gbtstatus->SCSRPF2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC VALUE"); s6_strcpy(gbtstatus->SCSSPROC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC STIME"); gbtstatus->SCSSPROCSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC MJD"); gbtstatus->SCSSPROCMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT VALUE"); s6_strcpy(gbtstatus->SCSSPECT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT STIME"); gbtstatus->SCSSPECTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT MJD"); gbtstatus->SCSSPECTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS VALUE"); s6_strcpy(gbtstatus->SCSSWSS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS STIME"); gbtstatus->SCSSWSSSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS MJD"); gbtstatus->SCSSWSSMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS VALUE"); s6_strcpy(gbtstatus->SCSVEGAS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS STIME"); gbtstatus->SCSVEGASSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS MJD"); gbtstatus->SCSVEGASMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT VALUE"); s6_strcpy(gbtstatus->SCSZPECT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT STIME"); gbtstatus->SCSZPECTSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT MJD"); gbtstatus->SCSZPECTMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 VALUE"); s6_strcpy(gbtstatus->VEGSFBW1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 STIME"); gbtstatus->VEGSFBW1STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 MJD"); gbtstatus->VEGSFBW1MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 VALUE"); s6_strcpy(gbtstatus->VEGSFBW2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 STIME"); gbtstatus->VEGSFBW2STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 MJD"); gbtstatus->VEGSFBW2MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 VALUE"); s6_strcpy(gbtstatus->VEGSFBW3,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 STIME"); gbtstatus->VEGSFBW3STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 MJD"); gbtstatus->VEGSFBW3MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 VALUE"); s6_strcpy(gbtstatus->VEGSFBW4,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 STIME"); gbtstatus->VEGSFBW4STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 MJD"); gbtstatus->VEGSFBW4MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 VALUE"); s6_strcpy(gbtstatus->VEGSFBW5,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 STIME"); gbtstatus->VEGSFBW5STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 MJD"); gbtstatus->VEGSFBW5MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 VALUE"); s6_strcpy(gbtstatus->VEGSFBW6,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 STIME"); gbtstatus->VEGSFBW6STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 MJD"); gbtstatus->VEGSFBW6MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 VALUE"); s6_strcpy(gbtstatus->VEGSFBW7,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 STIME"); gbtstatus->VEGSFBW7STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 MJD"); gbtstatus->VEGSFBW7MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 VALUE"); s6_strcpy(gbtstatus->VEGSFBW8,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 STIME"); gbtstatus->VEGSFBW8STIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 MJD"); gbtstatus->VEGSFBW8MJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM VALUE"); s6_strcpy(gbtstatus->VEGSSBAM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM STIME"); gbtstatus->VEGSSBAMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM MJD"); gbtstatus->VEGSSBAMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM VALUE"); s6_strcpy(gbtstatus->VEGSSBBM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM STIME"); gbtstatus->VEGSSBBMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM MJD"); gbtstatus->VEGSSBBMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM VALUE"); s6_strcpy(gbtstatus->VEGSSBCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM STIME"); gbtstatus->VEGSSBCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM MJD"); gbtstatus->VEGSSBCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM VALUE"); s6_strcpy(gbtstatus->VEGSSBDM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM STIME"); gbtstatus->VEGSSBDMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM MJD"); gbtstatus->VEGSSBDMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM VALUE"); s6_strcpy(gbtstatus->VEGSSBEM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM STIME"); gbtstatus->VEGSSBEMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM MJD"); gbtstatus->VEGSSBEMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM VALUE"); s6_strcpy(gbtstatus->VEGSSBFM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM STIME"); gbtstatus->VEGSSBFMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM MJD"); gbtstatus->VEGSSBFMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM VALUE"); s6_strcpy(gbtstatus->VEGSSBGM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM STIME"); gbtstatus->VEGSSBGMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM MJD"); gbtstatus->VEGSSBGMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM VALUE"); s6_strcpy(gbtstatus->VEGSSBHM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM STIME"); gbtstatus->VEGSSBHMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM MJD"); gbtstatus->VEGSSBHMMJD atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM VALUE"); s6_strcpy(gbtstatus->VEGASCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM STIME"); gbtstatus->VEGASCMSTIME atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM MJD"); gbtstatus->VEGASCMMJD atof(reply->element[0]->str); freeReplyObject(reply);
#endif
      
   
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
