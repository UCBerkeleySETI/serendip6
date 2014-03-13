#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <hiredis/hiredis.h>

#include "hashpipe.h"
#include "s6_obs_data.h"

//                          0       1       2
const char *receiver[3] = {"none", "AO ALFA", "AO 327MHz"};

//----------------------------------------------------------
int get_obs_info_from_redis(scram_t *scram,     
                            char    *hostname, 
                            int     port) {
//----------------------------------------------------------

    redisContext *c;
    redisReply *reply;
    int rv = 0;

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

    // TODO factor out all the redis error checking (use hashpipe_error())

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:PNT        PNTSTIME PNTRA PNTDEC PNTMJD PNTAZCOR PNTZACOR");
    if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
    else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
    else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:PNT not set yet!\n"); rv = 1; }
    else {
        scram->PNTSTIME  = atoi(reply->element[0]->str);
        scram->PNTRA     = atof(reply->element[1]->str);
        scram->PNTDEC    = atof(reply->element[2]->str);
        scram->PNTMJD    = atof(reply->element[3]->str);
        scram->PNTAZCOR  = atof(reply->element[4]->str);
        scram->PNTZACOR  = atof(reply->element[5]->str);
    }
    freeReplyObject(reply);
#if 0
    fprintf(stderr, "GET SCRAM:PNTSTIME %d\n", scram->PNTSTIME);
    fprintf(stderr, "GET SCRAM:PNTRA %lf\n", scram->PNTRA);   
    fprintf(stderr, "GET SCRAM:PNTDEC %lf\n", scram->PNTDEC);  
    fprintf(stderr, "GET SCRAM:PNTMJD %lf\n", scram->PNTMJD);  
    fprintf(stderr, "GET SCRAM:PNTAZCOR %lf\n", scram->PNTAZCOR);  
    fprintf(stderr, "GET SCRAM:PNTZACOR %lf\n", scram->PNTZACOR);  
#endif

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:AGC       AGCSTIME AGCTIME AGCAZ AGCZA AGCLST");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:AGC not set yet!\n"); rv = 1; }
      else {
          scram->AGCSTIME  = atoi(reply->element[0]->str);
          scram->AGCTIME   = atoi(reply->element[1]->str);
          scram->AGCAZ     = atof(reply->element[2]->str);
          scram->AGCZA     = atof(reply->element[3]->str);
          scram->AGCLST    = atof(reply->element[4]->str);
      }
      freeReplyObject(reply);
    }

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:ALFASHM       ALFSTIME ALFBIAS1 ALFBIAS2 ALFMOPOS");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:ALFASHM not set yet!\n"); rv = 1; }
      else {
          scram->ALFSTIME  = atoi(reply->element[0]->str);
          scram->ALFBIAS1  = atoi(reply->element[1]->str);
          scram->ALFBIAS2  = atoi(reply->element[2]->str);
          scram->ALFMOPOS  = atof(reply->element[3]->str);
      }
      freeReplyObject(reply);
    }

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:IF1      IF1STIME IF1SYNHZ IF1SYNDB IF1RFFRQ IF1IFFRQ IF1ALFFB");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:IF1 not set yet!\n"); rv = 1; }
      else {
          scram->IF1STIME  = atoi(reply->element[0]->str);
          scram->IF1SYNHZ  = atof(reply->element[1]->str);
          scram->IF1SYNDB  = atoi(reply->element[2]->str);
          scram->IF1RFFRQ  = atof(reply->element[3]->str);
          scram->IF1IFFRQ  = atof(reply->element[4]->str);
          scram->IF1ALFFB  = atoi(reply->element[5]->str);
      }
      freeReplyObject(reply);
    }

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:IF2      IF2STIME IF2ALFON");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:IF2 not set yet!\n"); rv = 1; }
      else {
          scram->IF2STIME  = atoi(reply->element[0]->str);
          scram->IF2ALFON  = atoi(reply->element[1]->str);
      }
      freeReplyObject(reply);
    }

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:TT      TTSTIME TTTURENC TTTURDEG");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:TT not set yet!\n"); rv = 1; }
      else {
          scram->TTSTIME  = atoi(reply->element[0]->str);
          scram->TTTURENC = atoi(reply->element[1]->str);
          scram->TTTURDEG = atof(reply->element[2]->str);
      }
      freeReplyObject(reply);
    }

    if (!rv) {
      reply = (redisReply *)redisCommand(c, "HMGET SCRAM:DERIVED      DERTIME RA0 DEC0 RA1 DEC1 RA2 DEC2 RA3 DEC3 RA4 DEC4 RA5 DEC5 RA6 DEC6");
      if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); rv = 1; }
      else if (reply->type != REDIS_REPLY_ARRAY) { fprintf(stderr, "Unexpected type: %d\n", reply->type); rv = 1; }
      else if (!reply->element[0]->str) { fprintf(stderr,"SCRAM:DERIVED not set yet!\n"); rv = 1; }
      else {
          scram->DERTIME        = atoi(reply->element[0]->str);
          scram->ra_by_beam[0]  = atof(reply->element[1]->str);
          scram->dec_by_beam[0] = atof(reply->element[2]->str);
          scram->ra_by_beam[1]  = atof(reply->element[3]->str);
          scram->dec_by_beam[1] = atof(reply->element[4]->str);
          scram->ra_by_beam[2]  = atof(reply->element[5]->str);
          scram->dec_by_beam[2] = atof(reply->element[6]->str);
          scram->ra_by_beam[3]  = atof(reply->element[7]->str);
          scram->dec_by_beam[3] = atof(reply->element[8]->str);
          scram->ra_by_beam[4]  = atof(reply->element[9]->str);
          scram->dec_by_beam[4] = atof(reply->element[10]->str);
          scram->ra_by_beam[5]  = atof(reply->element[11]->str);
          scram->dec_by_beam[5] = atof(reply->element[12]->str);
          scram->ra_by_beam[6]  = atof(reply->element[13]->str);
          scram->dec_by_beam[6] = atof(reply->element[14]->str);
      }
      freeReplyObject(reply);
    }

    redisFree(c);       // TODO do I really want to free each time?

    if (!rv) {
        // TODO this should be collapsed a bit once testing/debugging is done
        scram->rec_alfa_enabled = is_alfa_enabled(scram);
        scram->rec_327_enabled  = is_327_enabled(scram);
        if(scram->rec_alfa_enabled) {
            scram->receiver = RECEIVER_ALFA;
        } else if(scram->rec_327_enabled) {
            scram->receiver = RECEIVER_327MHZ;
        } else {
            scram->receiver = RECEIVER_NONE;
        }
    }

    return rv;         
}

int is_alfa_enabled (scram_t *scram) {

  time_t now;
  
  now = time(NULL);
  if ((now - scram->AGCSTIME) > ScramLagTolerance) return false;
  if ((now - scram->ALFSTIME) > ScramLagTolerance) return false;
  if ((now - scram->IF1STIME) > ScramLagTolerance) return false;
  if ((now - scram->IF2STIME) > ScramLagTolerance) return false;
  if (!scram->ALFBIAS1 && !scram->ALFBIAS2) return false;
  if (!scram->IF2ALFON) return false;
  if ((now - scram->TTSTIME) > ScramLagTolerance) return false;
  if (fabs(scram->TTTURDEG - TT_TurretDegreesAlfa) > TT_TurretDegreesTolerance) return false;

  return true;

}

int is_327_enabled (scram_t *scram) {
#define TUR_DEG_TO_ENC_UNITS  ( (4096. * 210. / 5.0) / (360.) )
#define RCV_POS_327 339.90

    int    rcv327Active = 0;

    double turDeg     = scram->TTTURDEG;
    double syn1Mhz    = scram->IF1SYNHZ * 1e-6;
    double syn2Mhz    = scram->IF2SYNHZ * 1e-6;

    double epsPosDeg  = 0.5;      // epsilon, the error we allow
    double epsFrqMhz  = 1e-6;     // epsilon, the error we allow

    double rfCfr      = 327.0;
    double if1Cfr     = 750.0;
    double if2Cfr     = 325.0;

    rcv327Active=((fabs(turDeg  - RCV_POS_327)      < epsPosDeg)  &&   
                  (fabs(syn1Mhz -(rfCfr + if1Cfr))  < epsFrqMhz)  &&
                  (fabs(syn2Mhz -(if1Cfr + if2Cfr)) < epsFrqMhz));

    return(rcv327Active);
}
