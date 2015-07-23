#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data_gbt.h"

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

      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO VALUE"); s6_strcpy(gbtstatus->ATBOT1EO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO STIME"); gbtstatus->ATBOT1EOSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1EO MJD"); gbtstatus->ATBOT1EOSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO VALUE"); s6_strcpy(gbtstatus->ATBOT1XO,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO STIME"); gbtstatus->ATBOT1XOSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1XO MJD"); gbtstatus->ATBOT1XOSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM VALUE"); s6_strcpy(gbtstatus->ATBOT1NM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM STIME"); gbtstatus->ATBOT1NMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1NM MJD"); gbtstatus->ATBOT1NMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 VALUE"); s6_strcpy(gbtstatus->ATBOT1F1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 STIME"); gbtstatus->ATBOT1F1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F1 MJD"); gbtstatus->ATBOT1F1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 VALUE"); s6_strcpy(gbtstatus->ATBOT1F2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 STIME"); gbtstatus->ATBOT1F2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATBOT1F2 MJD"); gbtstatus->ATBOT1F2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD VALUE"); s6_strcpy(gbtstatus->ATFCTRMD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD STIME"); gbtstatus->ATFCTRMDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD MJD"); gbtstatus->ATFCTRMDSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX VALUE"); s6_strcpy(gbtstatus->ATLFCX,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX STIME"); gbtstatus->ATLFCXSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCX MJD"); gbtstatus->ATLFCXSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT VALUE"); s6_strcpy(gbtstatus->ATLFCXT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT STIME"); gbtstatus->ATLFCXTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCXT MJD"); gbtstatus->ATLFCXTSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY VALUE"); s6_strcpy(gbtstatus->ATLFCY,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY STIME"); gbtstatus->ATLFCYSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCY MJD"); gbtstatus->ATLFCYSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT VALUE"); s6_strcpy(gbtstatus->ATLFCYT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT STIME"); gbtstatus->ATLFCYTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCYT MJD"); gbtstatus->ATLFCYTSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ VALUE"); s6_strcpy(gbtstatus->ATLFCZ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ STIME"); gbtstatus->ATLFCZSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZ MJD"); gbtstatus->ATLFCZSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT VALUE"); s6_strcpy(gbtstatus->ATLFCZT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT STIME"); gbtstatus->ATLFCZTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLFCZT MJD"); gbtstatus->ATLFCZTSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 STIME"); gbtstatus->ATLPOAZ1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ1 MJD"); gbtstatus->ATLPOAZ1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 VALUE"); s6_strcpy(gbtstatus->ATLPOAZ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 STIME"); gbtstatus->ATLPOAZ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOAZ2 MJD"); gbtstatus->ATLPOAZ2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL VALUE"); s6_strcpy(gbtstatus->ATLPOEL,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL STIME"); gbtstatus->ATLPOELSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATLPOEL MJD"); gbtstatus->ATLPOELSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 VALUE"); s6_strcpy(gbtstatus->ATMCRAJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 STIME"); gbtstatus->ATMCRAJ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCRAJ2 MJD"); gbtstatus->ATMCRAJ2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 VALUE"); s6_strcpy(gbtstatus->ATMCDCJ2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 STIME"); gbtstatus->ATMCDCJ2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATMCDCJ2 MJD"); gbtstatus->ATMCDCJ2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD VALUE"); s6_strcpy(gbtstatus->ATOPTMOD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD STIME"); gbtstatus->ATOPTMODSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATOPTMOD MJD"); gbtstatus->ATOPTMODSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR VALUE"); s6_strcpy(gbtstatus->ATRECVR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR STIME"); gbtstatus->ATRECVRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRECVR MJD"); gbtstatus->ATRECVRSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA VALUE"); s6_strcpy(gbtstatus->ATRXOCTA,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA STIME"); gbtstatus->ATRXOCTASTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET ATRXOCTA MJD"); gbtstatus->ATRXOCTASTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 VALUE"); s6_strcpy(gbtstatus->BAMMPWR1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 STIME"); gbtstatus->BAMMPWR1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR1 MJD"); gbtstatus->BAMMPWR1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 VALUE"); s6_strcpy(gbtstatus->BAMMPWR2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 STIME"); gbtstatus->BAMMPWR2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET BAMMPWR2 MJD"); gbtstatus->BAMMPWR2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ VALUE"); s6_strcpy(gbtstatus->LO1APSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ STIME"); gbtstatus->LO1APSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1APSFQ MJD"); gbtstatus->LO1APSFQSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ VALUE"); s6_strcpy(gbtstatus->LO1BPSFQ,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ STIME"); gbtstatus->LO1BPSFQSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET LO1BPSFQ MJD"); gbtstatus->LO1BPSFQSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID VALUE"); s6_strcpy(gbtstatus->SCPROJID,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID STIME"); gbtstatus->SCPROJIDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCPROJID MJD"); gbtstatus->SCPROJIDSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR VALUE"); s6_strcpy(gbtstatus->SCSNUMBR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR STIME"); gbtstatus->SCSNUMBRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSNUMBR MJD"); gbtstatus->SCSNUMBRSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE VALUE"); s6_strcpy(gbtstatus->SCSOURCE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE STIME"); gbtstatus->SCSOURCESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSOURCE MJD"); gbtstatus->SCSOURCESTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE VALUE"); s6_strcpy(gbtstatus->SCSTATE,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE STIME"); gbtstatus->SCSTATESTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSTATE MJD"); gbtstatus->SCSTATESTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF VALUE"); s6_strcpy(gbtstatus->SCSACTSF,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF STIME"); gbtstatus->SCSACTSFSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSACTSF MJD"); gbtstatus->SCSACTSFSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR VALUE"); s6_strcpy(gbtstatus->SCSANAFR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR STIME"); gbtstatus->SCSANAFRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANAFR MJD"); gbtstatus->SCSANAFRSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN VALUE"); s6_strcpy(gbtstatus->SCSANTEN,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN STIME"); gbtstatus->SCSANTENSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSANTEN MJD"); gbtstatus->SCSANTENSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI VALUE"); s6_strcpy(gbtstatus->SCSARCHI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI STIME"); gbtstatus->SCSARCHISTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSARCHI MJD"); gbtstatus->SCSARCHISTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM VALUE"); s6_strcpy(gbtstatus->SCSBCPM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM STIME"); gbtstatus->SCSBCPMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSBCPM MJD"); gbtstatus->SCSBCPMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 VALUE"); s6_strcpy(gbtstatus->SCSCCB26,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 STIME"); gbtstatus->SCSCCB26STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCCB26 MJD"); gbtstatus->SCSCCB26STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK VALUE"); s6_strcpy(gbtstatus->SCSCRACK,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK STIME"); gbtstatus->SCSCRACKSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSCRACK MJD"); gbtstatus->SCSCRACKSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR VALUE"); s6_strcpy(gbtstatus->SCSDCR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR STIME"); gbtstatus->SCSDCRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSDCR MJD"); gbtstatus->SCSDCRSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI VALUE"); s6_strcpy(gbtstatus->SCSGUPPI,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI STIME"); gbtstatus->SCSGUPPISTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSGUPPI MJD"); gbtstatus->SCSGUPPISTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG VALUE"); s6_strcpy(gbtstatus->SCSHOLOG,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG STIME"); gbtstatus->SCSHOLOGSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSHOLOG MJD"); gbtstatus->SCSHOLOGSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM VALUE"); s6_strcpy(gbtstatus->SCSIFM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM STIME"); gbtstatus->SCSIFMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFM MJD"); gbtstatus->SCSIFMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR VALUE"); s6_strcpy(gbtstatus->SCSIFR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR STIME"); gbtstatus->SCSIFRSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSIFR MJD"); gbtstatus->SCSIFRSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 VALUE"); s6_strcpy(gbtstatus->SCSLO1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 STIME"); gbtstatus->SCSLO1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSLO1 MJD"); gbtstatus->SCSLO1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR VALUE"); s6_strcpy(gbtstatus->SCSMEASUR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR STIME"); gbtstatus->SCSMEASURSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSMEASUR MJD"); gbtstatus->SCSMEASURSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD VALUE"); s6_strcpy(gbtstatus->SCSQUADRD,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD STIME"); gbtstatus->SCSQUADRDSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSQUADRD MJD"); gbtstatus->SCSQUADRDSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 VALUE"); s6_strcpy(gbtstatus->SCSR1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 STIME"); gbtstatus->SCSR1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR1 MJD"); gbtstatus->SCSR1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 VALUE"); s6_strcpy(gbtstatus->SCSR12,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 STIME"); gbtstatus->SCSR12STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR12 MJD"); gbtstatus->SCSR12STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 VALUE"); s6_strcpy(gbtstatus->SCSR18,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 STIME"); gbtstatus->SCSR18STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR18 MJD"); gbtstatus->SCSR18STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 VALUE"); s6_strcpy(gbtstatus->SCSR2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 STIME"); gbtstatus->SCSR2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR2 MJD"); gbtstatus->SCSR2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 VALUE"); s6_strcpy(gbtstatus->SCSR26,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 STIME"); gbtstatus->SCSR26STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR26 MJD"); gbtstatus->SCSR26STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 VALUE"); s6_strcpy(gbtstatus->SCSR40,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 STIME"); gbtstatus->SCSR40STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR40 MJD"); gbtstatus->SCSR40STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 VALUE"); s6_strcpy(gbtstatus->SCSR4,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 STIME"); gbtstatus->SCSR4STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR4 MJD"); gbtstatus->SCSR4STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 VALUE"); s6_strcpy(gbtstatus->SCSR68,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 STIME"); gbtstatus->SCSR68STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR68 MJD"); gbtstatus->SCSR68STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 VALUE"); s6_strcpy(gbtstatus->SCSR8,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 STIME"); gbtstatus->SCSR8STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSR8 MJD"); gbtstatus->SCSR8STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 VALUE"); s6_strcpy(gbtstatus->SCSRA1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 STIME"); gbtstatus->SCSRA1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA1 MJD"); gbtstatus->SCSRA1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 VALUE"); s6_strcpy(gbtstatus->SCSRA18,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 STIME"); gbtstatus->SCSRA18STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRA18 MJD"); gbtstatus->SCSRA18STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 VALUE"); s6_strcpy(gbtstatus->SCSRMBA1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 STIME"); gbtstatus->SCSRMBA1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRMBA1 MJD"); gbtstatus->SCSRMBA1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR VALUE"); s6_strcpy(gbtstatus->SCSRPAR,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR STIME"); gbtstatus->SCSRPARSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPAR MJD"); gbtstatus->SCSRPARSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 VALUE"); s6_strcpy(gbtstatus->SCSRPF1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 STIME"); gbtstatus->SCSRPF1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF1 MJD"); gbtstatus->SCSRPF1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 VALUE"); s6_strcpy(gbtstatus->SCSRPF2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 STIME"); gbtstatus->SCSRPF2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSRPF2 MJD"); gbtstatus->SCSRPF2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC VALUE"); s6_strcpy(gbtstatus->SCSSPROC,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC STIME"); gbtstatus->SCSSPROCSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPROC MJD"); gbtstatus->SCSSPROCSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT VALUE"); s6_strcpy(gbtstatus->SCSSPECT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT STIME"); gbtstatus->SCSSPECTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSPECT MJD"); gbtstatus->SCSSPECTSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS VALUE"); s6_strcpy(gbtstatus->SCSSWSS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS STIME"); gbtstatus->SCSSWSSSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSSWSS MJD"); gbtstatus->SCSSWSSSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS VALUE"); s6_strcpy(gbtstatus->SCSVEGAS,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS STIME"); gbtstatus->SCSVEGASSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSVEGAS MJD"); gbtstatus->SCSVEGASSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT VALUE"); s6_strcpy(gbtstatus->SCSZPECT,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT STIME"); gbtstatus->SCSZPECTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET SCSZPECT MJD"); gbtstatus->SCSZPECTSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 VALUE"); s6_strcpy(gbtstatus->VEGSFBW1,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 STIME"); gbtstatus->VEGSFBW1STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW1 MJD"); gbtstatus->VEGSFBW1STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 VALUE"); s6_strcpy(gbtstatus->VEGSFBW2,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 STIME"); gbtstatus->VEGSFBW2STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW2 MJD"); gbtstatus->VEGSFBW2STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 VALUE"); s6_strcpy(gbtstatus->VEGSFBW3,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 STIME"); gbtstatus->VEGSFBW3STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW3 MJD"); gbtstatus->VEGSFBW3STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 VALUE"); s6_strcpy(gbtstatus->VEGSFBW4,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 STIME"); gbtstatus->VEGSFBW4STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW4 MJD"); gbtstatus->VEGSFBW4STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 VALUE"); s6_strcpy(gbtstatus->VEGSFBW5,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 STIME"); gbtstatus->VEGSFBW5STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW5 MJD"); gbtstatus->VEGSFBW5STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 VALUE"); s6_strcpy(gbtstatus->VEGSFBW6,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 STIME"); gbtstatus->VEGSFBW6STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW6 MJD"); gbtstatus->VEGSFBW6STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 VALUE"); s6_strcpy(gbtstatus->VEGSFBW7,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 STIME"); gbtstatus->VEGSFBW7STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW7 MJD"); gbtstatus->VEGSFBW7STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 VALUE"); s6_strcpy(gbtstatus->VEGSFBW8,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 STIME"); gbtstatus->VEGSFBW8STIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSFBW8 MJD"); gbtstatus->VEGSFBW8STIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM VALUE"); s6_strcpy(gbtstatus->VEGSSBAM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM STIME"); gbtstatus->VEGSSBAMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBAM MJD"); gbtstatus->VEGSSBAMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM VALUE"); s6_strcpy(gbtstatus->VEGSSBBM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM STIME"); gbtstatus->VEGSSBBMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBBM MJD"); gbtstatus->VEGSSBBMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM VALUE"); s6_strcpy(gbtstatus->VEGSSBCM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM STIME"); gbtstatus->VEGSSBCMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBCM MJD"); gbtstatus->VEGSSBCMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM VALUE"); s6_strcpy(gbtstatus->VEGSSBDM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM STIME"); gbtstatus->VEGSSBDMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBDM MJD"); gbtstatus->VEGSSBDMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM VALUE"); s6_strcpy(gbtstatus->VEGSSBEM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM STIME"); gbtstatus->VEGSSBEMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBEM MJD"); gbtstatus->VEGSSBEMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM VALUE"); s6_strcpy(gbtstatus->VEGSSBFM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM STIME"); gbtstatus->VEGSSBFMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBFM MJD"); gbtstatus->VEGSSBFMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM VALUE"); s6_strcpy(gbtstatus->VEGSSBGM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM STIME"); gbtstatus->VEGSSBGMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBGM MJD"); gbtstatus->VEGSSBGMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM VALUE"); s6_strcpy(gbtstatus->VEGSSBHM,reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM STIME"); gbtstatus->VEGSSBHMSTIME = atol(reply->element[0]->str); freeReplyObject(reply);
      reply = (redisReply *)redisCommand(c,"HMGET VEGSSBHM MJD"); gbtstatus->VEGSSBHMSTIME = atof(reply->element[0]->str); freeReplyObject(reply);
      
   
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
