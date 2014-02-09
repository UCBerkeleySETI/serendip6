#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <hiredis.h>

#include "s6_obs_data.h"

//----------------------------------------------------------
int get_obs_info_from_redis(scram_t *scram,     
                            char *hostname, 
                            int port) {
//----------------------------------------------------------

    redisContext *c;
    redisReply *reply;

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    // TODO make c static?
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }

    // TODO factor out all the redis error checking

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:PNT        PNTSTIME PNTRA PNTDEC PNTMJD");
    if (reply->type == REDIS_REPLY_ERROR)               // TODO error checking does not seem to work
        printf("Error: %s\n", reply->str);              //      check for correct # elements
    else if (reply->type != REDIS_REPLY_ARRAY)          //      move error check to function
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->PNTSTIME  = atoi(reply->element[0]->str);
        scram->PNTRA     = atof(reply->element[1]->str);
        scram->PNTDEC    = atof(reply->element[2]->str);
        scram->PNTMJD    = atof(reply->element[3]->str);
    }
    freeReplyObject(reply);
    printf("GET SCRAM:PNTSTIME %d\n", scram->PNTSTIME);
    printf("GET SCRAM:PNTRA %lf\n", scram->PNTRA);   
    printf("GET SCRAM:PNTDEC %lf\n", scram->PNTDEC);  
    printf("GET SCRAM:PNTMJD %lf\n", scram->PNTMJD);  

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:AGC       AGCSTIME AGCTIME AGCAZ AGCZA AGCLST");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->AGCSTIME  = atoi(reply->element[0]->str);
        scram->AGCTIME   = atoi(reply->element[1]->str);
        scram->AGCAZ     = atof(reply->element[2]->str);
        scram->AGCZA     = atof(reply->element[3]->str);
        scram->AGCLST    = atof(reply->element[4]->str);
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:ALFASHM       ALFSTIME ALFBIAS1 ALFBIAS2 ALFMOPOS");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->ALFSTIME  = atoi(reply->element[0]->str);
        scram->ALFBIAS1  = atoi(reply->element[1]->str);
        scram->ALFBIAS2  = atoi(reply->element[2]->str);
        scram->ALFMOPOS  = atof(reply->element[3]->str);
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:IF1      IF1STIME IF1SYNHZ IF1SYNDB IF1RFFRQ IF1IFFRQ IF1ALFFB");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->IF1STIME  = atoi(reply->element[0]->str);
        scram->IF1SYNHZ  = atof(reply->element[1]->str);
        scram->IF1SYNDB  = atoi(reply->element[2]->str);
        scram->IF1RFFRQ  = atof(reply->element[3]->str);
        scram->IF1IFFRQ  = atof(reply->element[4]->str);
        scram->IF1ALFFB  = atoi(reply->element[5]->str);
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:IF2      IF2STIME IF2ALFON");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->IF2STIME  = atoi(reply->element[0]->str);
        scram->IF2ALFON  = atoi(reply->element[1]->str);
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:TT      TTSTIME TTTURENC TTTURDEG");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
    else {
        scram->TTSTIME  = atoi(reply->element[0]->str);
        scram->TTTURENC = atoi(reply->element[1]->str);
        scram->TTTURDEG = atof(reply->element[2]->str);
    }
    freeReplyObject(reply);

#if 0
// waiting on fix to s6_observatory
    reply = (redisReply *)redisCommand(c, "HMGET SCRAM:DERIVED      DERTIME RA0 DEC0 RA1 DEC1 RA2 DEC2 RA3 DEC3 RA4 DEC4 RA5 DEC5 RA6 DEC6");
    if (reply->type == REDIS_REPLY_ERROR)
        printf( "Error: %s\n", reply->str);
    else if (reply->type != REDIS_REPLY_ARRAY )
        printf("Unexpected type: %d\n", reply->type);
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
#endif

    // TODO get the obs_enabled bool

    redisFree(c);       // TODO do I really want to free each time?

    return 0;           // TODO return something meaningful
}
