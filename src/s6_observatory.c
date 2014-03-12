#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <fmtmsg.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

//extern "C"
//{
#include "mshmLib.h"
#include "execshm.h"
#include "wappshm.h"
#include "wapplib.h"
#include "alfashm.h"
#include "vtxLib.h"
#include "scram.h"

#include <hiredis/hiredis.h>

//}

#include "s6_time.h"
#include "s6_obsaux.h"

#define ftoa(A,B) sprintf(B,"%0.10lf",A);

int main(int argc, char ** argv) {
    struct SCRAMNET * scram;
    char    name[256];
    int     i;
    bool    got_pnt, got_agc, got_alfashm;
    time_t  time_pnt, time_agc, time_if1, time_if2, time_tt, time_alfashm, time_fix;
    double Enc2Deg = 1./ ( (4096. * 210. / 5.0) / (360.) );
    const double D2R=(M_PI/180.0);

    char strbuf[1024];
    redisContext *c;
    redisReply *reply;
    const char *hostname = "127.0.0.1";
    int port = 6379;

    const char *usage = "Usage: s6_observatory [-test] [-stdout] [-nodb] [-nottl] [-hostname hostname] [-port port]\n  -test: don't read scram, put in dummy values\n  -stdout: output packets to stdout (normally quiet)\n  -nodb: don't update redis db\n  -nottl: don't expire any of the scram keys in the redis db\n  hostname/port: for redis database (default 127.0.0.1:6379)\n\n";

    bool dotest = false;
    bool dostdout = false;
    bool nodb = false;
    bool nottl = false;

    double RA, Dec, MJD, azfix, zafix; // PNT vars
    int mlasttck, Az, ZA, agctime; double Azdeg, ZAdeg, timesecs, Azerrdeg, ZAerrdeg; // AGC vars
    int synIDB_0, fltrbank; double synIHz_0, rfFreq, FrqMhz; // IF1 vars
    bool useAlfa; // IF2 vars
    int encoder; double degrees; // TT vars
    int fstbias, sndbias; double motorpos; // ALFASHM vars
    double synIHz_1, lo2Hz;

    double beamAz, beamZA; // ra/dec conversion per beam vars
    double coord_unixtime;
    double fixedRA[7]; // for fixed values
    double fixedDec[7];

    // totally annoying: string buffers for doubles above, since hiredis/hmset demands we need these
    char RAbuf[24];
    char Decbuf[24];
    char MJDbuf[24];
    char azfixbuf[24];
    char zafixbuf[24];
    char Azdegbuf[24];
    char ZAdegbuf[24];
    char synIHz_0buf[24];
    char synIHz_1buf[24];
    char rfFreqbuf[24];
    char FrqMhzbuf[24];
    char degreesbuf[24];
    char motorposbuf[24];
    char *fixedRAbuf[7];
    char *fixedDecbuf[7];
    for (i = 0; i < 7; i++) { fixedRAbuf[i] = malloc(24); fixedDecbuf[i] = malloc(24); }

    for (i = 1; i < argc; i++) {
      if (strcmp(argv[i],"-test") == 0) { dotest = true; }
      else if (strcmp(argv[i],"-stdout") == 0) { dostdout = true; }
      else if (strcmp(argv[i],"-nodb") == 0) { nodb = true; }
      else if (strcmp(argv[i],"-nottl") == 0) { nottl = true; }
      else if (strcmp(argv[i],"-hostname") == 0) { hostname = argv[++i]; }
      else if (strcmp(argv[i],"-port") == 0) { port = atoi(argv[++i]); }
      else { fprintf(stderr,"%s",usage); exit (1); }
      }

    if (!nodb) {
      struct timeval timeout = { 1, 500000 }; // 1.5 seconds
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
      }

    if (!dotest) scram = init_scramread(NULL);

    got_pnt = got_agc = got_alfashm = false;
    time_pnt = time_agc = time_if1 = time_if2 = time_tt = time_alfashm = time_fix = 0;

    while (1) {

      if (!dotest) {

        if (read_scram(scram) == -1) {
            fprintf(stderr, "GetScramData : bad scram read\n");
            exit(1);
          }

        getnameinfo((const struct sockaddr *)&scram->from, sizeof(struct sockaddr_in), name, 256, NULL, 0, 0);

        if (strcmp(scram->in.magic, "PNT") == 0) {
            got_pnt = true;
            time_pnt = time(NULL);
            RA  = scram->pntData.st.x.pl.curP.raJ;
            Dec = scram->pntData.st.x.pl.curP.decJ;
            RA  *= 24.0 / C_2PI;
            Dec *= 360.0 / C_2PI;
            MJD  = scram->pntData.st.x.pl.tm.mjd + scram->pntData.st.x.pl.tm.ut1Frac;
            azfix = scram->pntData.st.x.modelCorEncAzZa[0] / D2R;   // go to degrees
            zafix = scram->pntData.st.x.modelCorEncAzZa[1] / D2R;

            ftoa(RA,RAbuf); ftoa(Dec,Decbuf); ftoa(MJD,MJDbuf); ftoa(azfix,azfixbuf); ftoa(zafix,zafixbuf);
            sprintf(strbuf,"SCRAM:PNT PNTSTIME %ld PNTRA %0.10lf PNTDEC %0.10lf PNTMJD %0.10lf PNTAZCOR %0.10lf PNTZACOR %0.10lf",time_pnt,RA,Dec,MJD,azfix,zafix);
            if (dostdout) fprintf(stderr,"%s\n",strbuf);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %s %s %s %s %s %s %s %s %s","SCRAM:PNT","PNTSTIME",time_pnt,"PNTRA",RAbuf,"PNTDEC",Decbuf,"PNTMJD",MJDbuf,"PNTAZCOR",azfixbuf,"PNTZACOR",zafixbuf); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);

          } else if (strcmp(scram->in.magic, "AGC") == 0) {
            got_agc = true;
            time_agc = time(NULL);
            mlasttck = scram->agcData.st.secMLastTick;
            Az = scram->agcData.st.cblkMCur.dat.posAz;
            Azdeg = scram->agcData.st.cblkMCur.dat.posAz * 0.0001;
            ZA = scram->agcData.st.cblkMCur.dat.posGr; 
            ZAdeg = scram->agcData.st.cblkMCur.dat.posGr * 0.0001;
            agctime = scram->agcData.st.cblkMCur.dat.timeMs;
            timesecs = scram->agcData.st.cblkMCur.dat.timeMs * 0.001; 
            Azerrdeg = scram->agcData.st.posErr.reqPosDifRd[0];
            ZAerrdeg = scram->agcData.st.posErr.reqPosDifRd[1];
            ftoa(Azdeg,Azdegbuf); ftoa(ZAdeg,ZAdegbuf);
            sprintf(strbuf,"SCRAM:AGC AGCSTIME %ld AGCTIME %d AGCAZ %0.10lf AGCZA %0.10lf",time_agc,agctime,Azdeg,ZAdeg);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %d %s %s %s %s","SCRAM:AGC","AGCSTIME",time_agc,"AGCTIME",agctime,"AGCAZ",Azdegbuf,"AGCZA",ZAdegbuf); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);

          } else if (strcmp(scram->in.magic, "IF1") == 0) {
            time_if1 = time(NULL); 
            synIHz_0 = scram->if1Data.st.synI.freqHz[0];        // TODO label/name as 1st LO, right?
            synIDB_0 = scram->if1Data.st.synI.ampDb[0];
            rfFreq = scram->if1Data.st.rfFreq;
            FrqMhz = scram->if1Data.st.if1FrqMhz;
            fltrbank = scram->if1Data.st.stat2.alfaFb;
            ftoa(synIHz_0,synIHz_0buf); ftoa(rfFreq,rfFreqbuf); ftoa(FrqMhz,FrqMhzbuf);
            sprintf(strbuf,"SCRAM:IF1 IF1STIME %ld IF1SYNHZ %0.10lf IF1SYNDB %d IF1RFFRQ %0.10lf IF1IFFRQ %0.10lf IF1ALFFB %d",time_if1,synIHz_0,synIDB_0,rfFreq,FrqMhz,fltrbank);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %s %s %d %s %s %s %s %s %d","SCRAM:IF1","IF1STIME",time_if1,"IF1SYNHZ",synIHz_0buf,"IF1SYNDB",synIDB_0,"IF1RFFRQ",rfFreqbuf,"IF1IFFRQ",FrqMhzbuf,"IF1ALFFB",fltrbank); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);

          } else if (strcmp(scram->in.magic, "IF2") == 0) {
            time_if2 = time(NULL); 
            if(scram->if2Data.st.stat1.useAlfa) { useAlfa = true; } else { useAlfa = false; }
            synIHz_1 = scram->if2Data.st.synI.freqHz[0];        // TODO label/name as 2nd LO, right?
            ftoa(synIHz_1,synIHz_1buf);
            sprintf(strbuf,"SCRAM:IF2 IF2STIME %ld IF2ALFON %d",time_if2,useAlfa);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %d","SCRAM:IF2","IF2STIME",time_if2, "IF2SYNHZ", synIHz_1buf, "IF2ALFON",useAlfa); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);

          } else if (strcmp(scram->in.magic, "TT") == 0) {
            time_tt = time(NULL);
            encoder = scram->ttData.st.slv[0].inpMsg.position;
            degrees = (double)(scram->ttData.st.slv[0].inpMsg.position) * Enc2Deg;
double turDeg=scram->ttData.st.slv[0].tickMsg.position/ TUR_DEG_TO_ENC_UNITS;

            ftoa(degrees,degreesbuf);
            sprintf(strbuf,"SCRAM:TT TTSTIME %ld TTTURENC %d TTTURDEG %0.10lf",time_tt,encoder,degrees);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %d %s %s","SCRAM:TT","TTSTIME",time_tt,"TTTURENC",encoder,"TTTURDEG",degreesbuf); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);

          } else if (strcmp(scram->in.magic, "ALFASHM") == 0) {
            got_alfashm = true; 
            time_alfashm = time(NULL);
            fstbias = (int)scram->alfa.first_bias;
            sndbias = (int)scram->alfa.second_bias;
            motorpos = scram->alfa.motor_position; 
            ftoa(motorpos,motorposbuf)
            sprintf(strbuf,"SCRAM:ALFASHM ALFSTIME %ld ALFBIAS1 %d ALFBIAS2 %d ALFMOPOS %0.10lf",time_alfashm,fstbias,sndbias,motorpos);
            if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %d %s %d %s %s","SCRAM:ALFASHM","ALFSTIME",time_alfashm,"ALFBIAS1",fstbias,"ALFBIAS2",sndbias,"ALFMOPOS",motorposbuf); freeReplyObject(reply); }
            if (dostdout) fprintf(stderr,"%s\n",strbuf);
          } else {
            // fprintf(stderr, "UNKNOWN SCRAM: %ld %s from %s\n", current, scram->in.magic, name);
          }

        if (got_alfashm && got_agc && got_pnt) {
          time_fix = time_alfashm;
          if (time_agc > time_fix) time_fix = time_agc;
          if (time_pnt > time_fix) time_fix = time_pnt;
          coord_unixtime = s6_seti_ao_timeMS2unixtime(agctime,time_fix);
          for (i=0;i<7;i++) {
            beamAz = Azdeg; beamZA = ZAdeg; // in degrees
            beamAz -= azfix; beamZA -= zafix; // fix also in degrees
            s6_BeamOffset(&beamAz, &beamZA, i, motorpos); // i is beam
            s6_AzZaToRaDec(beamAz, beamZA, coord_unixtime, &fixedRA[i], &fixedDec[i]);
            ftoa(fixedRA[i],fixedRAbuf[i]); 
            ftoa(fixedDec[i],fixedDecbuf[i]); 
            }
          sprintf(strbuf,"SCRAM:DERIVED DERTIME %ld RA0 %0.10lf DEC0 %0.10lf RA1 %0.10lf DEC1 %0.10lf RA2 %0.10lf DEC2 %0.10lf RA3 %0.10lf DEC3 %0.10lf RA4 %0.10lf DEC4 %0.10lf RA5 %0.10lf DEC5 %0.10lf RA6 %0.10lf DEC6 %0.10lf",time_fix,fixedRA[0],fixedDec[0],fixedRA[1],fixedDec[1],fixedRA[2],fixedDec[2],fixedRA[3],fixedDec[3],fixedRA[4],fixedDec[4],fixedRA[5],fixedDec[5],fixedRA[6],fixedDec[6]);
          if (!nodb) { reply = redisCommand(c,"HMSET %s %s %d %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s","SCRAM:DERIVED","DERTIME",time_fix,"RA0",fixedRAbuf[0],"DEC0",fixedDecbuf[0],"RA1",fixedRAbuf[1],"DEC1",fixedDecbuf[1],"RA2",fixedRAbuf[2],"DEC2",fixedDecbuf[2],"RA3",fixedRAbuf[3],"DEC3",fixedDecbuf[3],"RA4",fixedRAbuf[4],"DEC4",fixedDecbuf[4],"RA5",fixedRAbuf[5],"DEC5",fixedDecbuf[5],"RA6",fixedRAbuf[6],"DEC6",fixedDecbuf[6]); freeReplyObject(reply); }
          if (dostdout) fprintf(stderr,"%s\n",strbuf);
          }
  
        } // end if !nodotest

      else { // test mode

        fprintf(stderr, "..test mode..\n");  // indicate that we are running

        sprintf(strbuf,"SCRAM:PNT PNTSTIME %ld PNTRA 1.1 PNTDEC 2.2 PNTMJD 3.3 PNTAZCOR 4.4 PNTZACOR 5.5",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %s %s %s %s %s %s %s %s %s","SCRAM:PNT","PNTSTIME",time(NULL),"PNTRA","1.1","PNTDEC","2.2","PNTMJD","3.3","PNTAZCOR","4.4","PNTZACOR","5.5"); 
            if (reply->type == REDIS_REPLY_ERROR) {
                fprintf(stderr, "Error: %s\n", reply->str);
            }
            freeReplyObject(reply);
        }

        sprintf(strbuf,"SCRAM:AGC AGCSTIME %ld AGCTIME 1 AGCAZ 2.2 AGCZA 3.3",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %d %s %s %s %s","SCRAM:AGC","AGCSTIME",time(NULL),"AGCTIME",1,"AGCAZ","2.2","AGCZA","3.3"); 
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }
        sprintf(strbuf,"SCRAM:IF1 IF1STIME %ld IF1SYNHZ 1.1 IF1SYNDB 2 IF1RFFRQ 3.3 IF1IFFRQ 4.4 IF1ALFFB 5",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %s %s %d %s %s %s %s %s %d","SCRAM:IF1","IF1STIME",time(NULL),"IF1SYNHZ","1.1","IF1SYNDB",2,"IF1RFFRQ","3.3","IF1IFFRQ","4.4","IF1ALFFB",5); 
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }

        sprintf(strbuf,"SCRAM:IF2 IF2STIME %ld IF2ALFON 1",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %d","SCRAM:IF2","IF2STIME",time(NULL),"IF2ALFON",1); 
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }

        sprintf(strbuf,"SCRAM:TT TTSTIME %ld TTTURENC 1 TTTURDEG 2.2",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %d %s %s","SCRAM:TT","TTSTIME",time(NULL),"TTTURENC",1,"TTTURDEG","2.2"); 
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }

        sprintf(strbuf,"SCRAM:ALFASHM ALFSTIME %ld ALFBIAS1 1 ALFBIAS2 2 ALFMOPOS 3.3",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %d %s %d %s %s","SCRAM:ALFASHM","ALFSTIME",time(NULL),"ALFBIAS1",1,"ALFBIAS2",2,"ALFMOPOS","3.3"); 
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }

        sprintf(strbuf,"SCRAM:DERIVED DERTIME %ld RA0 0.1 DEC0 0.1 RA1 1.2 DEC1 1.2 RA2 2.3 DEC2 2.3 RA3 3.4 DEC3 3.4 RA4 4.4 DEC4 4.5 RA5 5.6 DEC5 5.6 RA6 6.7 DEC6 6.7",time(NULL));
        if (dostdout) fprintf(stderr,"%s\n",strbuf);
        if (!nodb) { 
            reply = redisCommand(c,"HMSET %s %s %d %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s","SCRAM:DERIVED","DERTIME",time(NULL),"RA0","0.1","DEC0","0.1","RA1","1.2","DEC1","1.2","RA2","2.3","DEC2","2.3","RA3","3.4","DEC3","3.4","RA4","4.5","DEC4","4.5","RA5","5.6","DEC5","5.6","RA6","6.7","DEC6","6.7");
            if (reply->type == REDIS_REPLY_ERROR) { fprintf(stderr, "Error: %s\n", reply->str); }
            freeReplyObject(reply);
        }
        
        sleep(1); // just so we make it look more like scram and don't hammer redis db

        } // end if dotest

      } // end while (1)

    exit(0);
}

