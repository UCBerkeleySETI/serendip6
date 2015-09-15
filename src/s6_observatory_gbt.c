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

  //### connect to cleo socket

  if (!nocleo) {
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

    // fprintf(stderr, "Connected\n");

    }

  int saved_flags = fcntl(sock, F_GETFL);
  fcntl(sock, F_SETFL, saved_flags | O_NONBLOCK);

  //### any other preparations before main loop?
  
  strcpy(last_last_update,"");

  //### MAIN LOOP
  
  while (1) {

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
        }
      else {
  
        now = time(NULL);
        server_reply[bytes_read] = '\0';
        // fprintf(stderr, "\n#####DEBUG\nServer reply (%d) :\n%s\n#####END DEBUG\n",bytes_read, server_reply);
        byte_at = 0;
        while (sscanf(server_reply+byte_at,"%s %s %[^\n]%n",timestamp,key,value,&bytes_in) == 3) {
          value[strlen(value)-1] = 0;
          // act on timestamp/key/value 
          found = 0;
          // fprintf(stderr,"DEBUG: %s %s %s\n",timestamp,key,value);
          for (i = 0; i < num_cleo_keys; i++) {
            if (strcmp(key,cleokeys[i]) == 0) {
              found = 1;
              if (!nodb) {
                reply = redisCommand(c,"HMSET %s STIME %ld VALUE %s MJD %s",cleofitskeys[i],now,value,timestamp);
                freeReplyObject(reply); 
                }
              if (dostdout) {
                printf("   %8s (%60s) : %s (time: %s)\n",cleofitskeys[i],cleokeys[i],value,timestamp);
                }
              }
            }
          if (found == 0) { fprintf(stderr,"warning: can't look up cleo key: %s\n",key); }
          // fprintf(stderr, "PARSED: timestamp %s key %s value %s (bytes_in %d, byte_at %d)\n", timestamp, key, value,bytes_in,byte_at);
          // fprintf(stderr, "CLEO KEY: %s\n",key);
          byte_at += bytes_in;
          }

        }
  
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


