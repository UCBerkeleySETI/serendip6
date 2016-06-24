#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mysql.h"
#include <fcntl.h>

#include<sys/socket.h>
#include<arpa/inet.h>
#include <netdb.h>

#include <hiredis/hiredis.h>

#include "s6_obsaux_gbt.h"

// #define SLEEP_MICROSECONDS 100000 // tenth of a second
#define SLEEP_MICROSECONDS 1000000   // whole second

#define ftoa(A,B) sprintf(B,"%lf",A);

// file containing redis key/mysql row pairs for which to read from mysql and put into redis
// const char *status_fields_config = "./mysql_status_fields";
const char *mysql_fields_config = "/usr/local/etc/mysql_status_fields";
// const char *cleo_fields_config = "./cleo_status_fields";
const char *cleo_fields_config = "/usr/local/etc/cleo_status_fields";

const char *usage = "Usage: s6_observatory_gbt [-stdout] [-nodb] [-redishost hostname] [-redisport port]\n -[-cleohost hostname] [-cleoport port]\n  [-nomysql] [-nocleo]\n  -stdout: output packets to stdout (normally quiet)\n  -nodb: don't update redis db\n  redishost/redisport: for redis database (default 127.0.0.1:6379)\n  cleohost/cleoport: for cleo socket connection (default euler.gb.nrao.edu/8030)\n  -nomysql/-nocleo : don't attempt to read from either as specified\n\n";

MYSQL mysql;
MYSQL_ROW row;
MYSQL_RES* resp;
int retval;

int main(int argc, char ** argv) {

  int i;
  int found;
  long now;
  
  redisContext *c;
  redisReply *reply;
  const char *redis_server_hostname = "127.0.0.1";
  int redis_port = 6379;
  char strbuf[1024];

  int lines_allocated = 256; // for reading in from status fields
  int max_line_len = 256;     // for reading in from status fields
  FILE *statusfp;
  int num_mysql_keys;
  int num_cleo_keys;
  char *comment; // dummy storage for comments

  bool nodb = false;
  bool dostdout = false;
  bool nomysql = false;
  bool nocleo = false;

  int az_actual_index, el_actual_index, last_update_index, lst_index, mjd_index; // make sure we have these

  double az_actual, el_actual, ra_derived, dec_derived;
  double za;
  double lsthour,lstminute,lstsecond;

  char RAbuf[24];  // for annoying hiredis problem with floats
  char RADbuf[24];
  char Decbuf[24];
  char lsthourbuf[24];

  double xyz[3];
  double xyz_precessed[3];

  char last_last_update[32];

  int sock;
  int bytes_read;
  int bytes_in, byte_at;          // for parsing multiline reply
  struct sockaddr_in server;      // arg to connect
  struct hostent *server_info;    // for address resolution
  char message[10000] , server_reply[20000];

  char * cleo_server_hostname = "euler.gb.nrao.edu";
  int cleo_port = 8030;
  char timestamp[256], key[256], value[256];

  long lcudsecs, lcudwhen; // for calculating last cleo update seconds

  bool found_any_key;
  bool cleo_connected = false;

  int64_t idlestatus;
  long tmplong;
  double tmpdouble;
  char tmpstring[256];

  //### read in command line arguments

  for (i = 1; i < argc; i++) {
      if (strcmp(argv[i],"-nodb") == 0) {
          // do not update the redis DB
          nodb = true;
      }
      else if (strcmp(argv[i],"-nomysql") == 0) {
          // do not try to connect to mysql status db
          nomysql = true;
      }
      else if (strcmp(argv[i],"-nocleo") == 0) {
          // do not try to connect to cleo socket
          nocleo = true;
      }
      else if (strcmp(argv[i],"-stdout") == 0) {
          // output values to stdout.  Can be used with or without
          // -nodb but is nearly always specified when -nodb is used.
          dostdout = true;
      }
      else if (strcmp(argv[i],"-redishost") == 0) {
          // the host on which the redis DB resides
          redis_server_hostname = argv[++i];
      }
      else if (strcmp(argv[i],"-redisport") == 0) {
          // the port on which the redis DB server is listening
          redis_port = atoi(argv[++i]);
      }
      else if (strcmp(argv[i],"-cleohost") == 0) {
          // the host from which the cleo socket is served
          cleo_server_hostname = argv[++i];
      }
      else if (strcmp(argv[i],"-cleoport") == 0) {
          // the port from which the cleo socket is served
          cleo_port = atoi(argv[++i]);
      }
      else {
          fprintf(stderr,"%s",usage); exit (1);
      }
  }

  // resolve cleo server host name
  if (!nocleo) {
    server_info = gethostbyname(cleo_server_hostname);
    if (server_info == NULL) {
        fprintf(stderr,"ERROR - no such cleo host: %s\n",cleo_server_hostname);
        exit(0);
      }
    }

  comment = (char *)malloc(max_line_len);

  //### read in mysql status fields (and create mysql "select" command string)
  
  az_actual_index = el_actual_index = last_update_index = lst_index = mjd_index = -1;
  char *selectcommand = (char *)malloc(sizeof(char*)*lines_allocated*max_line_len);
  char **mysqlfitskeys= (char **)malloc(sizeof(char*)*lines_allocated);
  char **mysqlkeys= (char **)malloc(sizeof(char*)*lines_allocated);
  if (mysqlfitskeys==NULL || mysqlkeys==NULL) { fprintf(stderr,"Out of memory (while initializing).\n"); exit(1); }

  statusfp = fopen(mysql_fields_config, "r");
  if (statusfp == NULL) { fprintf(stderr,"Error opening mysql status fields file.\n"); exit(1); }

  for (i=0;1;i++) {
      if (i >= lines_allocated) { fprintf(stderr,"over allocated # of lines (%d lines).\n",lines_allocated); exit(1); }
      mysqlfitskeys[i] = malloc(max_line_len);
      mysqlkeys[i] = malloc(max_line_len);
      if (mysqlfitskeys[i]==NULL || mysqlkeys[i]==NULL) { fprintf(stderr,"Out of memory (getting next line).\n"); exit(1); }
      if (fscanf(statusfp,"%s %s%[^\n]\n",mysqlfitskeys[i],mysqlkeys[i],comment)<2) break;
      if (strcmp(mysqlkeys[i],"az_actual") == 0) az_actual_index = i;
      if (strcmp(mysqlkeys[i],"el_actual") == 0) el_actual_index = i;
      if (strcmp(mysqlkeys[i],"last_update") == 0) last_update_index = i;
      if (strcmp(mysqlkeys[i],"lst") == 0) lst_index = i;
      if (strcmp(mysqlkeys[i],"mjd") == 0) mjd_index = i;
      }
  fclose(statusfp);

  if (az_actual_index == -1 || el_actual_index == -1 || last_update_index == -1 || lst_index == -1 || mjd_index == -1 ) {
    fprintf(stderr,"az_actual, el_actual, last_update, lst, mjd\n   must all be represented in status fields file: %s\n\n",mysql_fields_config);
    exit(1);
    }

  num_mysql_keys = i;

  *selectcommand = '\0';
  strcat(selectcommand,"select ");

  for(i = 0; i < (num_mysql_keys-1); i++) {
    strcat(selectcommand,mysqlkeys[i]);
    strcat(selectcommand,",");
    }
  strcat(selectcommand,mysqlkeys[num_mysql_keys-1]);
  strcat(selectcommand," from status;");

  // read in cleo status fields

  char **cleofitskeys= (char **)malloc(sizeof(char*)*lines_allocated);
  char **cleokeys= (char **)malloc(sizeof(char*)*lines_allocated);
  
  statusfp = fopen(cleo_fields_config, "r");
  if (statusfp == NULL) { fprintf(stderr,"Error opening cleo status fields file.\n"); exit(1); }

  for (i=0;1;i++) {
      if (i >= lines_allocated) { fprintf(stderr,"over allocated # of lines (%d lines).\n",lines_allocated); exit(1); }
      cleofitskeys[i] = malloc(max_line_len);
      cleokeys[i] = malloc(max_line_len);
      if (cleofitskeys[i]==NULL || cleokeys[i]==NULL) { fprintf(stderr,"Out of memory (getting next line).\n"); exit(1); }
      if (fscanf(statusfp,"%s %s%[^\n]\n",cleofitskeys[i],cleokeys[i],comment)<2) break;
      // fprintf(stderr,"DEBUG: fits %s key %s comment %s EOL\n",cleofitskeys[i],cleokeys[i],comment);
      }

  num_cleo_keys = i;

  //### connect to redis db

  if (!nodb) {
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    c = redisConnectWithTimeout(redis_server_hostname, redis_port, timeout);
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
  
  //### connect to mysql db
  
  if (!nomysql) {
    mysql_init(&mysql);
    if (!mysql_real_connect (&mysql, "gbtdata.gbt.nrao.edu","gbtstatus","w3bqu3ry","gbt_status",3306,0,CLIENT_FOUND_ROWS)) {
      fprintf(stderr, "Failed to connect to database: Error: %s\n", mysql_error(&mysql));
      exit(1);
      }
    }

  int saved_flags = fcntl(sock, F_GETFL);
  fcntl(sock, F_SETFL, saved_flags | O_NONBLOCK);

  //### any other preparations before main loop?
  
  strcpy(last_last_update,"");
  lcudsecs = 0;
  lcudwhen = time(NULL);

  //### MAIN LOOP
  
  while (1) {

    //### if not already, connect to cleo socket

    if (!nocleo && !cleo_connected) {

      //Create socket
      sock = socket(AF_INET , SOCK_STREAM , 0);
      if (sock == -1) { fprintf(stderr, "Could not create socket\n"); exit(1); }
      fprintf(stderr, "Socket created\n");
    
      // populate server struct
      memset((char *) &server, 0, sizeof(server));
      server.sin_family = AF_INET;
      memcpy((char *)&server.sin_addr.s_addr,
             (char *)server_info->h_addr,
             server_info->h_length);
      server.sin_port = htons(cleo_port);            // network byte order
    
      //Connect to remote server
      if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
          fprintf(stderr,"Connect to cleo socket failed.\n");
          exit(1);
          }

      cleo_connected = true;
  
      // fprintf(stderr, "Connected\n");
  
      }

    //### okay, now onto the various data collections

    if (!nomysql) {

      // read from mysql database

      retval = mysql_query(&mysql,selectcommand);
      if (retval) {
        fprintf(stderr,"can't connect to the status mysql database - exiting...\n");
        exit(1);
        }
      resp = mysql_store_result(&mysql);
      if (!resp) {
        fprintf(stderr,"error storing mysql result - exiting...\n");
        exit(1);
        }
      row = mysql_fetch_row(resp);
      now = time(NULL);
      if (!row || !row[0]) {
        fprintf(stderr,"error fetching mysql row - exiting...\n");
        exit(1);
        }
   
  
      // calculations
  
      if (strcmp(last_last_update,row[last_update_index]) == 0) {
        // fprintf(stderr,"not updated.. skipping..\n");
        usleep(SLEEP_MICROSECONDS); // sleep one half a second (enough to ensure update within +/- 1 second)
        continue;  
        }
      strcpy(last_last_update,row[last_update_index]);
  
      az_actual = atof(row[az_actual_index]);
      el_actual = atof(row[el_actual_index]);
  
      // testing
      // az_actual = 198.5284;
      // el_actual = 45.6437;
    
      za = 90-el_actual;
  
      sscanf(row[lst_index],"%lf:%lf:%lf",&lsthour,&lstminute,&lstsecond);
      lsthour += (lstminute/60) + (lstsecond/3600);
  
      //testing
      // lsthour = 9.64055552;
   
      co_ZenAzToRaDec(za, az_actual, lsthour, &ra_derived, &dec_derived, GBT_LATITUDE);
      co_EqToXyz(ra_derived, dec_derived, xyz);
      // DEBUG: // printf("before precess: ra %lf dec %lf\n",ra_derived,dec_derived);
      co_Precess(tm_JulianDateToJulianEpoch(atof(row[mjd_index])+2400000.5), xyz, 2000, xyz_precessed);
      // testing
      // co_Precess(tm_JulianDateToJulianEpoch(57170.9397344+2400000.5), xyz, 2000, xyz_precessed);
      // co_Precess(2015, xyz, 2000, xyz_precessed);
      co_XyzToEq(xyz_precessed, &ra_derived, &dec_derived);
  
      // update redis db (if so desired)
  
      if (!nodb) {
        for (i=0;i<num_mysql_keys;i++) {
          // fprintf(stderr,"%d %s\n",i,row[i]);
          // sprintf(strbuf,"ROW%d \"%s\"",i,row[i]);
          // fprintf(stderr,"%s\n",strbuf);
          // reply = redisCommand(c,"SET %s",strbuf);
          reply = redisCommand(c,"HMSET %s STIME %ld VALUE %s",mysqlfitskeys[i],now,row[i]);
          // fprintf(stderr, "SET: %d\n", reply->type);
          freeReplyObject(reply); 
          }
        ftoa(ra_derived,RAbuf);
        ftoa(ra_derived*15,RADbuf);
        ftoa(dec_derived,Decbuf);
        ftoa(lsthour,lsthourbuf); 
        reply = redisCommand(c,"HMSET RA_DRV STIME %ld VALUE %s",now,RAbuf);
        freeReplyObject(reply); 
        reply = redisCommand(c,"HMSET RADG_DRV STIME %ld VALUE %s",now,RADbuf);
        freeReplyObject(reply); 
        reply = redisCommand(c,"HMSET DEC_DRV STIME %ld VALUE %s",now,Decbuf);
        freeReplyObject(reply); 
        reply = redisCommand(c,"HMSET LSTH_DRV STIME %ld VALUE %s",now,lsthourbuf);
        freeReplyObject(reply); 
        }
   
      //display values to stdout (if so desired)
       
      if (dostdout) {
        for (i=0;i<num_mysql_keys;i++) printf("%2d %8s (%32s) : %s\n",i,mysqlfitskeys[i],mysqlkeys[i],row[i]);
        printf("   LSTH_DRV (%32s) : %lf\n","",lsthour);
        printf("     RA_DRV (%32s) : %lf\n","",ra_derived);
        printf("   RADG_DRV (%32s) : %lf\n","",ra_derived*15);
        printf("    DEC_DRV (%32s) : %lf\n","",dec_derived);
        printf("----------- (end mysql)\n");
        }
  
      mysql_free_result(resp);

      }

    if (!nocleo) {

      // read from cleo socket

      //Receive a reply from the server
      // fprintf(stderr,"#####DEBUG: receiving from cleo...\n");
      if( (bytes_read = recv(sock , server_reply , 20000 , 0)) < 0) { 
        if (dostdout) printf("(nothing right now)\n"); 
        now = time(NULL);
        lcudsecs = now-lcudwhen;
        }
      else {
        now = time(NULL);
        server_reply[bytes_read] = '\0';
        // fprintf(stderr, "\n#####DEBUG\nServer reply (%d) :\n%s\n#####END DEBUG\n",bytes_read, server_reply);
        byte_at = 0;
        found_any_key = false;
        while (sscanf(server_reply+byte_at,"%s %s %[^\n]%n",timestamp,key,value,&bytes_in) == 3) {
          value[strlen(value)-1] = 0;
          // act on timestamp/key/value 
          found = 0;
          // fprintf(stderr,"DEBUG: %s %s %s\n",timestamp,key,value);
          for (i = 0; i < num_cleo_keys; i++) {
            if (strcmp(key,cleokeys[i]) == 0) {
              found = 1;
              found_any_key = true;
              if (!nodb) {
                reply = redisCommand(c,"HMSET %s STIME %ld VALUE %s MJD %s",cleofitskeys[i],now,value,timestamp);
                freeReplyObject(reply); 
                }
              if (dostdout) {
                printf("   %8s (%60s) : %s (time: %s)\n",cleofitskeys[i],cleokeys[i],value,timestamp);
                }
              }
            } // end for each key
          if (found == 0) { fprintf(stderr,"warning: can't look up cleo key: %s\n",key); }
          // fprintf(stderr, "PARSED: timestamp %s key %s value %s (bytes_in %d, byte_at %d)\n", timestamp, key, value,bytes_in,byte_at);
          // fprintf(stderr, "CLEO KEY: %s\n",key);
          byte_at += bytes_in;
          } // end while
        if (found_any_key) { lcudwhen = now; lcudsecs = 0; }
        else {
          //fprintf(stderr, "\n#####DEBUG (found_any_key == false)\nServer reply (%d) :\n%s\n#####END DEBUG\n",bytes_read, server_reply);
          fprintf(stderr, "warning: weird cleo state - will attempt to restart socket\n");
          close(sock);
          cleo_connected = false; 
          }     
        }

      // cleo derived values

      if (!nodb) {
        reply = redisCommand(c,"HMSET LCUDSECS STIME %ld VALUE %ld",now,lcudsecs);
        freeReplyObject(reply); 
        }
      if (dostdout) {
        printf("   %8s (%60s) : %ld\n","LCUDSECS","derived",lcudsecs);
        }

      // general derived values

      idlestatus = 0;

 // examples: to delete
 //     reply = (redisReply *)redisCommand(c,"HMGET LASTUPDT VALUE"); s6_strcpy(gbtstatus->LASTUPDT,reply->element[0]->str); freeReplyObject(reply);
 //     reply = (redisReply *)redisCommand(c,"HMGET LASTUPDT STIME"); gbtstatus->LASTUPDTSTIME = atol(reply->element[0]->str); freeReplyObject(reply);

/*
#define idle_atlpoaz1_too_large                 0x000000000000020; // GB - ATLPOAZ1 Antenna,localPointingOffsets,azOffset1 # radians - ignore data if too large
#define idle_atlpoaz2_too_large                 0x000000000000040; // GB - ATLPOAZ2 Antenna,localPointingOffsets,azOffset1 # radians - ignore data if too large
#define idle_atlpoel_too_large                  0x000000000000080; // GB - ATLPOEL Antenna,localPointingOffsets,elOffset # radians - ignore data if too large

#define idle_atlfcxt_non_zero                   0x000000000000200; // GB - ATLFCXT Antenna,local_focus_correction,Xt # subreflector tilt degrees - ignore if non-zero
#define idle_atlfcy_too_large                   0x000000000000400; // GB - ATLFCY Antenna,local_focus_correction,Y # mm - ignore if too large
#define idle_atlfctr_too_large                  0x000000000000800; // GB - ATLFCYT Antenna,local_focus_correction,Yt # subreflector tilt degrees- ignore if too large

#define idle_atlfcz_non_zero                    0x000000000001000; // GB - ATLFCZ Antenna,local_focus_correction,X # mm - ignore if non-zero
#define idle_atlfczt_non_zero                   0x000000000002000; // GB - ATLFCZT Antenna,local_focus_correction,Xt # subreflector tilt degrees - ignore if non-zero
#define idle_atoptmod_not_matched               0x000000000004000; // GB - ATOPTMOD Antenna,opticsMode # should match what IF manager reports
#define idle_atrecvr_not_matched                0x000000000008000; // GB - ATRECVR Antenna,receiver # should match IF manager, opticsMode, and GregorianReceiver

#define idle_atrxocta_wrong_degrees             0x000000000010000; // GB - ATRXOCTA Antenna,rxOpticsConfig,turretAngle # current rot angle of turrent in degrees (should match what's in TurretLocations)
#define idle_attrbeam_not_1                     0x000000000020000; // GB - ATTRBEAM Antenna,trackBeam  # if != 1 then using non central beam and data should be ignored
#define idle_atmfbs_wrong_state                 0x000000000040000; // GB - ATMFBS AntennaManager,feedBoomState # PF receiver && BOOM_EXTENDED || Gregorian && BOOM_RETRACTED
#define idle_atmtls_not_locked                  0x000000000080000; // GB - ATMTLS AntennaManager,turretLockState # If TURRET_LOCK_LOCKED, otherwise ignore data

#define idle_optgreg_not_true                   0x000000000100000; // GB - OPTGREG OpticsOK,Gregorian # if != TRUE then optics offset or tilted and sky pos and gain may be wrong
#define idle_optprime_not_true                  0x000000000200000; // GB - OPTPRIME OpticsOK,PrimeFocus # if != TRUE then optics offset or tilted and sky pos and gain may be wrong
#define idle_lo1fqsw_true                       0x000000000400000; // GB - LO1FQSW LO1,FrequencySwitching # if TRUE then data should probably be ignored
#define idle_lo1cfg_test_tone                   0x000000000800000; // GB - LO1CFG LO1,loConfig # if != (TrackA_BNotUsed || TrackB_ANotUsed) then good chance test tone injected

#define idle_lo1phcal_on                        0x000000001000000; // GB - LO1PHCAL LO1,phaseCalCtl # if ON the VLB phase cal is on and data should have "rail of lines"
#define idle_bammpwr1_bad_power                 0x000000002000000; // GB - BAMMPWR1 BankAMgr,measpwr1  # power levels in (dBn) of VEGAS samplers (polarization 1) - too different than -20 system may be non-linear
#define idle_bammpwr2_bad_power                 0x000000004000000; // GB - BAMMPWR2 BankAMgr,measpwr2  # power levels in (dBn) of VEGAS samplers (polarization 2) - too different than -20 system may be non-linear
#define idle_lastupdt_old                       0x000000008000000; // GB - last update to gbstatus is > 60 then something is wrong
*/

      // #define idle_atfctrmd_not_1                     0x000000000000010; // GB - ATFCTRMD Antenna,focusTrackingMode # if not 1 then data should be ignored
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD VALUE");
      if(reply != NULL && reply->type == 0) {
        tmplong = atol(reply->element[0]->str); 
        freeReplyObject(reply);
//      if (tmplong != 1) idlestatus |= idle_atfctrmd_not_1;
      }

      // #define idle_atlfcx_non_zero                    0x000000000000100; // GB - ATLFCX Antenna,local_focus_correction,X # mm - ignore if non-zero
      reply = (redisReply *)redisCommand(c,"HMGET ATFCTRMD VALUE");
      if(reply != NULL && reply->type == 0) {
        tmpdouble = atof(reply->element[0]->str); 
        freeReplyObject(reply);
      }
      

      // web control  
      reply = (redisReply *)redisCommand(c,"GET WEBCNTRL");
      if(reply != NULL && reply->type == 0) {
        tmplong = atol(reply->str); 
        freeReplyObject(reply);
 //     if (tmplong == 0) idlestatus |= idle_webcntrl_off;
      }


      // end derived values

      if (dostdout) { printf("----------- (end cleo)\n"); }
 
      }

    usleep(SLEEP_MICROSECONDS); // sleep (hopefully enough to ensure update within +/- 1 second
                                //        but not too much as to upset server admins)
  
    } // end main while loop
  
  }


/*
 
MJD = JD - 2400000.5
JD = MJD + 2400000.5
UNIX = (JD - 2440587.5) * 86400
UNIX = (MJD - -40587.0) * 86400

*/


