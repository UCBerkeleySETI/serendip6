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

      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM VALUE"); s6_strcpy(gbtstatus->ATATCOTM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM STIME"); gbtstatus->ATATCOTMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATATCOTM MJD"); gbtstatus->ATATCOTMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF VALUE"); s6_strcpy(gbtstatus->ATBEELOF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF STIME"); gbtstatus->ATBEELOFSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEELOF MJD"); gbtstatus->ATBEELOFMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF VALUE"); s6_strcpy(gbtstatus->ATBEXLOF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF STIME"); gbtstatus->ATBEXLOFSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBEXLOF MJD"); gbtstatus->ATBEXLOFMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD VALUE"); s6_strcpy(gbtstatus->ATFCTRMD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD STIME"); gbtstatus->ATFCTRMDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD MJD"); gbtstatus->ATFCTRMDMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC VALUE"); s6_strcpy(gbtstatus->ATGREGRC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC STIME"); gbtstatus->ATGREGRCSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATGREGRC MJD"); gbtstatus->ATGREGRCMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX VALUE"); s6_strcpy(gbtstatus->ATLFCX,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX STIME"); gbtstatus->ATLFCXSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX MJD"); gbtstatus->ATLFCXMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT VALUE"); s6_strcpy(gbtstatus->ATLFCXT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT STIME"); gbtstatus->ATLFCXTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT MJD"); gbtstatus->ATLFCXTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY VALUE"); s6_strcpy(gbtstatus->ATLFCY,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY STIME"); gbtstatus->ATLFCYSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY MJD"); gbtstatus->ATLFCYMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT VALUE"); s6_strcpy(gbtstatus->ATLFCYT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT STIME"); gbtstatus->ATLFCYTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT MJD"); gbtstatus->ATLFCYTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ VALUE"); s6_strcpy(gbtstatus->ATLFCZ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ STIME"); gbtstatus->ATLFCZSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ MJD"); gbtstatus->ATLFCZMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT VALUE"); s6_strcpy(gbtstatus->ATLFCZT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT STIME"); gbtstatus->ATLFCZTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT MJD"); gbtstatus->ATLFCZTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 STIME"); gbtstatus->ATLPOAZ1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 MJD"); gbtstatus->ATLPOAZ1MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 STIME"); gbtstatus->ATLPOAZ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 MJD"); gbtstatus->ATLPOAZ2MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL VALUE"); s6_strcpy(gbtstatus->ATLPOEL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL STIME"); gbtstatus->ATLPOELSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL MJD"); gbtstatus->ATLPOELMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA VALUE"); s6_strcpy(gbtstatus->ATMATMCA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA STIME"); gbtstatus->ATMATMCASTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCA MJD"); gbtstatus->ATMATMCAMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM VALUE"); s6_strcpy(gbtstatus->ATMATMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM STIME"); gbtstatus->ATMATMCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCM MJD"); gbtstatus->ATMATMCMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT VALUE"); s6_strcpy(gbtstatus->ATMATMCT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT STIME"); gbtstatus->ATMATMCTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMATMCT MJD"); gbtstatus->ATMATMCTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 VALUE"); s6_strcpy(gbtstatus->ATMCDCJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 STIME"); gbtstatus->ATMCDCJ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 MJD"); gbtstatus->ATMCDCJ2MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 VALUE"); s6_strcpy(gbtstatus->ATMCRAJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 STIME"); gbtstatus->ATMCRAJ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 MJD"); gbtstatus->ATMCRAJ2MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS VALUE"); s6_strcpy(gbtstatus->ATMFBS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS STIME"); gbtstatus->ATMFBSSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMFBS MJD"); gbtstatus->ATMFBSMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS VALUE"); s6_strcpy(gbtstatus->ATMTLS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS STIME"); gbtstatus->ATMTLSSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMTLS MJD"); gbtstatus->ATMTLSMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD VALUE"); s6_strcpy(gbtstatus->ATOPTMOD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD STIME"); gbtstatus->ATOPTMODSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD MJD"); gbtstatus->ATOPTMODMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR VALUE"); s6_strcpy(gbtstatus->ATRECVR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR STIME"); gbtstatus->ATRECVRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR MJD"); gbtstatus->ATRECVRMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA VALUE"); s6_strcpy(gbtstatus->ATRXOCTA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA STIME"); gbtstatus->ATRXOCTASTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA MJD"); gbtstatus->ATRXOCTAMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM VALUE"); s6_strcpy(gbtstatus->ATTRBEAM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM STIME"); gbtstatus->ATTRBEAMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTRBEAM MJD"); gbtstatus->ATTRBEAMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC VALUE"); s6_strcpy(gbtstatus->ATTURLOC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC STIME"); gbtstatus->ATTURLOCSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATTURLOC MJD"); gbtstatus->ATTURLOCMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA VALUE"); s6_strcpy(gbtstatus->BAMBAMCA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA STIME"); gbtstatus->BAMBAMCASTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCA MJD"); gbtstatus->BAMBAMCAMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM VALUE"); s6_strcpy(gbtstatus->BAMBAMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM STIME"); gbtstatus->BAMBAMCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCM MJD"); gbtstatus->BAMBAMCMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT VALUE"); s6_strcpy(gbtstatus->BAMBAMCT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT STIME"); gbtstatus->BAMBAMCTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMBAMCT MJD"); gbtstatus->BAMBAMCTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 VALUE"); s6_strcpy(gbtstatus->BAMMPWR1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 STIME"); gbtstatus->BAMMPWR1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 MJD"); gbtstatus->BAMMPWR1MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 VALUE"); s6_strcpy(gbtstatus->BAMMPWR2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 STIME"); gbtstatus->BAMMPWR2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 MJD"); gbtstatus->BAMMPWR2MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID VALUE"); s6_strcpy(gbtstatus->CLEOPID,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID STIME"); gbtstatus->CLEOPIDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOPID MJD"); gbtstatus->CLEOPIDMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV VALUE"); s6_strcpy(gbtstatus->CLEOREV,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV STIME"); gbtstatus->CLEOREVSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET CLEOREV MJD"); gbtstatus->CLEOREVMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM VALUE"); s6_strcpy(gbtstatus->IFMIFMCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM STIME"); gbtstatus->IFMIFMCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFMIFMCM MJD"); gbtstatus->IFMIFMCMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW VALUE"); s6_strcpy(gbtstatus->IFV1BW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW STIME"); gbtstatus->IFV1BWSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1BW MJD"); gbtstatus->IFV1BWMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ VALUE"); s6_strcpy(gbtstatus->IFV1CSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ STIME"); gbtstatus->IFV1CSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1CSFQ MJD"); gbtstatus->IFV1CSFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL VALUE"); s6_strcpy(gbtstatus->IFV1LVL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL STIME"); gbtstatus->IFV1LVLSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1LVL MJD"); gbtstatus->IFV1LVLMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ VALUE"); s6_strcpy(gbtstatus->IFV1IFFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ STIME"); gbtstatus->IFV1IFFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1IFFQ MJD"); gbtstatus->IFV1IFFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB VALUE"); s6_strcpy(gbtstatus->IFV1SSB,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB STIME"); gbtstatus->IFV1SSBSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SSB MJD"); gbtstatus->IFV1SSBMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ VALUE"); s6_strcpy(gbtstatus->IFV1SKFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ STIME"); gbtstatus->IFV1SKFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1SKFQ MJD"); gbtstatus->IFV1SKFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH VALUE"); s6_strcpy(gbtstatus->IFV1TH,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH STIME"); gbtstatus->IFV1THSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TH MJD"); gbtstatus->IFV1THMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI VALUE"); s6_strcpy(gbtstatus->IFV1TNCI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI STIME"); gbtstatus->IFV1TNCISTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCI MJD"); gbtstatus->IFV1TNCIMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO VALUE"); s6_strcpy(gbtstatus->IFV1TNCO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO STIME"); gbtstatus->IFV1TNCOSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TNCO MJD"); gbtstatus->IFV1TNCOMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT VALUE"); s6_strcpy(gbtstatus->IFV1TT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT STIME"); gbtstatus->IFV1TTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV1TT MJD"); gbtstatus->IFV1TTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW VALUE"); s6_strcpy(gbtstatus->IFV2BW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW STIME"); gbtstatus->IFV2BWSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2BW MJD"); gbtstatus->IFV2BWMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ VALUE"); s6_strcpy(gbtstatus->IFV2CSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ STIME"); gbtstatus->IFV2CSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2CSFQ MJD"); gbtstatus->IFV2CSFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL VALUE"); s6_strcpy(gbtstatus->IFV2LVL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL STIME"); gbtstatus->IFV2LVLSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2LVL MJD"); gbtstatus->IFV2LVLMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ VALUE"); s6_strcpy(gbtstatus->IFV2IFFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ STIME"); gbtstatus->IFV2IFFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2IFFQ MJD"); gbtstatus->IFV2IFFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB VALUE"); s6_strcpy(gbtstatus->IFV2SSB,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB STIME"); gbtstatus->IFV2SSBSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SSB MJD"); gbtstatus->IFV2SSBMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ VALUE"); s6_strcpy(gbtstatus->IFV2SKFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ STIME"); gbtstatus->IFV2SKFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2SKFQ MJD"); gbtstatus->IFV2SKFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH VALUE"); s6_strcpy(gbtstatus->IFV2TH,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH STIME"); gbtstatus->IFV2THSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TH MJD"); gbtstatus->IFV2THMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI VALUE"); s6_strcpy(gbtstatus->IFV2TNCI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI STIME"); gbtstatus->IFV2TNCISTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCI MJD"); gbtstatus->IFV2TNCIMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO VALUE"); s6_strcpy(gbtstatus->IFV2TNCO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO STIME"); gbtstatus->IFV2TNCOSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TNCO MJD"); gbtstatus->IFV2TNCOMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT VALUE"); s6_strcpy(gbtstatus->IFV2TT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT STIME"); gbtstatus->IFV2TTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET IFV2TT MJD"); gbtstatus->IFV2TTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA VALUE"); s6_strcpy(gbtstatus->LO1ACA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA STIME"); gbtstatus->LO1ACASTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACA MJD"); gbtstatus->LO1ACAMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM VALUE"); s6_strcpy(gbtstatus->LO1ACM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM STIME"); gbtstatus->LO1ACMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACM MJD"); gbtstatus->LO1ACMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT VALUE"); s6_strcpy(gbtstatus->LO1ACT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT STIME"); gbtstatus->LO1ACTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1ACT MJD"); gbtstatus->LO1ACTMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ VALUE"); s6_strcpy(gbtstatus->LO1APSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ STIME"); gbtstatus->LO1APSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ MJD"); gbtstatus->LO1APSFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ VALUE"); s6_strcpy(gbtstatus->LO1BPSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ STIME"); gbtstatus->LO1BPSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ MJD"); gbtstatus->LO1BPSFQMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW VALUE"); s6_strcpy(gbtstatus->LO1FQSW,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW STIME"); gbtstatus->LO1FQSWSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1FQSW MJD"); gbtstatus->LO1FQSWMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM VALUE"); s6_strcpy(gbtstatus->LO1CM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM STIME"); gbtstatus->LO1CMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CM MJD"); gbtstatus->LO1CMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG VALUE"); s6_strcpy(gbtstatus->LO1CFG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG STIME"); gbtstatus->LO1CFGSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1CFG MJD"); gbtstatus->LO1CFGMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL VALUE"); s6_strcpy(gbtstatus->LO1PHCAL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL STIME"); gbtstatus->LO1PHCALSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1PHCAL MJD"); gbtstatus->LO1PHCALMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG VALUE"); s6_strcpy(gbtstatus->OPTGREG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG STIME"); gbtstatus->OPTGREGSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTGREG MJD"); gbtstatus->OPTGREGMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME VALUE"); s6_strcpy(gbtstatus->OPTPRIME,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME STIME"); gbtstatus->OPTPRIMESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET OPTPRIME MJD"); gbtstatus->OPTPRIMEMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID VALUE"); s6_strcpy(gbtstatus->SCPROJID,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID STIME"); gbtstatus->SCPROJIDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID MJD"); gbtstatus->SCPROJIDMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM VALUE"); s6_strcpy(gbtstatus->SCSCCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM STIME"); gbtstatus->SCSCCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCM MJD"); gbtstatus->SCSCCMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR VALUE"); s6_strcpy(gbtstatus->SCSNUMBR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR STIME"); gbtstatus->SCSNUMBRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR MJD"); gbtstatus->SCSNUMBRMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE VALUE"); s6_strcpy(gbtstatus->SCSOURCE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE STIME"); gbtstatus->SCSOURCESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE MJD"); gbtstatus->SCSOURCEMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE VALUE"); s6_strcpy(gbtstatus->SCSTATE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE STIME"); gbtstatus->SCSTATESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE MJD"); gbtstatus->SCSTATEMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 VALUE"); s6_strcpy(gbtstatus->VEGSFBW1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 STIME"); gbtstatus->VEGSFBW1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 MJD"); gbtstatus->VEGSFBW1MJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM VALUE"); s6_strcpy(gbtstatus->VEGSSBAM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM STIME"); gbtstatus->VEGSSBAMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM MJD"); gbtstatus->VEGSSBAMMJD = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM VALUE"); s6_strcpy(gbtstatus->VEGASCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM STIME"); gbtstatus->VEGASCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGASCM MJD"); gbtstatus->VEGASCMMJD = atof(reply->element[0]->str); freeReplyObject(reply);

      // values derived from cleo

      reply = (redisReply *)redisCommand(c,"HMGET LCUDSECS VALUE"); gbtstatus->LCUDSECS = atol(reply->element[0]->str); freeReplyObject(reply); 
      reply = (redisReply *)redisCommand(c,"HMGET LCUDSECS STIME"); gbtstatus->LCUDSECSSTIME = atol(reply->element[0]->str); freeReplyObject(reply); 

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
