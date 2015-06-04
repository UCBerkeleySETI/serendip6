#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mysql.h"

#include <hiredis.h>

#include "s6_obsaux_gbt.h"

#define SLEEP_MICROSECONDS 100000

#define ftoa(A,B) sprintf(B,"%lf",A);

// file containing redis key/mysql row pairs for which to read from mysql and put into redis
const char *status_fields_config = "./status_fields";

const char *usage = "Usage: s6_observatory_gbt [-stdout] [-nodb] [-hostname hostname] [-port port]\n  -stdout: output packets to stdout (normally quiet)\n  -nodb: don't update redis db\n  hostname/port: for redis database (default 127.0.0.1:6379)\n\n";

MYSQL mysql;
MYSQL_ROW row;
MYSQL_RES* resp;
int retval;

int main(int argc, char ** argv) {

  int i;

  redisContext *c;
  redisReply *reply;
  const char *hostname = "127.0.0.1";
  int port = 6379;
  char strbuf[1024];

  int lines_allocated = 128; // for reading in from status fields
  int max_line_len = 50;     // for reading in from status fields
  FILE *statusfp;
  int numkeys;

  bool nodb = false;
  bool dostdout = false;

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

  //### read in command line arguments

  for (i = 1; i < argc; i++) {
      if (strcmp(argv[i],"-nodb") == 0) {
          // do not update the redis DB
          nodb = true;
      }
      else if (strcmp(argv[i],"-stdout") == 0) {
          // output values to stdout.  Can be used with or without
          // -nodb but is nearly always specified when -nodb is used.
          dostdout = true;
      }
      else if (strcmp(argv[i],"-hostname") == 0) {
          // the host on which the redis DB resides
          hostname = argv[++i];
      }
      else if (strcmp(argv[i],"-port") == 0) {
          // the port on which the redis DB server is listening
          port = atoi(argv[++i]);
      }
      else {
          fprintf(stderr,"%s",usage); exit (1);
      }
  }

  //### read in status fields (and create mysql "select" command string)
  
  az_actual_index = el_actual_index = last_update_index = lst_index = mjd_index = -1;
  char *selectcommand = (char *)malloc(sizeof(char*)*lines_allocated*max_line_len);
  char **fitskeys= (char **)malloc(sizeof(char*)*lines_allocated);
  char **mysqlkeys= (char **)malloc(sizeof(char*)*lines_allocated);
  if (fitskeys==NULL || mysqlkeys==NULL) { fprintf(stderr,"Out of memory (while initializing).\n"); exit(1); }

  statusfp = fopen(status_fields_config, "r");
  if (statusfp == NULL) { fprintf(stderr,"Error opening file.\n"); exit(1); }

  for (i=0;1;i++) {
      if (i >= lines_allocated) { fprintf(stderr,"over allocated # of lines (%d lines).\n",lines_allocated); exit(1); }
      fitskeys[i] = malloc(max_line_len);
      mysqlkeys[i] = malloc(max_line_len);
      if (fitskeys[i]==NULL || mysqlkeys[i]==NULL) { fprintf(stderr,"Out of memory (getting next line).\n"); exit(1); }
      if (fscanf(statusfp,"%s %s",fitskeys[i],mysqlkeys[i])!=2) break;
      if (strcmp(mysqlkeys[i],"az_actual") == 0) az_actual_index = i;
      if (strcmp(mysqlkeys[i],"el_actual") == 0) el_actual_index = i;
      if (strcmp(mysqlkeys[i],"last_update") == 0) last_update_index = i;
      if (strcmp(mysqlkeys[i],"lst") == 0) lst_index = i;
      if (strcmp(mysqlkeys[i],"mjd") == 0) mjd_index = i;
      }
  fclose(statusfp);

  if (az_actual_index == -1 || el_actual_index == -1 || last_update_index == -1 || lst_index == -1 || mjd_index == -1 ) {
    fprintf(stderr,"az_actual, el_actual, last_update, lst, mjd\n   must all be represented in status fields file: %s\n\n",status_fields_config);
    exit(1);
    }

  numkeys = i;

  *selectcommand = '\0';
  strcat(selectcommand,"select ");

  for(i = 0; i < (numkeys-1); i++) {
    strcat(selectcommand,mysqlkeys[i]);
    strcat(selectcommand,",");
    }
  strcat(selectcommand,mysqlkeys[numkeys-1]);
  strcat(selectcommand," from status;");

  // fprintf(stderr,"command: %s\n",selectcommand);
  
  //### connect to redis db

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
  
  //### connect to mysql db
  
  mysql_init(&mysql);
  if (!mysql_real_connect (&mysql, "gbtdata.gbt.nrao.edu","gbtstatus","w3bqu3ry","gbt_status",3306,0,CLIENT_FOUND_ROWS)) {
    fprintf(stderr, "Failed to connect to database: Error: %s\n", mysql_error(&mysql));
    exit(1);
    }

  //### any other preparations before main loop?
  
  strcpy(last_last_update,"");

  //### MAIN LOOP
  
  while (1) {

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
      for (i=0;i<numkeys;i++) {
        // fprintf(stderr,"%d %s\n",i,row[i]);
        // sprintf(strbuf,"ROW%d \"%s\"",i,row[i]);
        // fprintf(stderr,"%s\n",strbuf);
        // reply = redisCommand(c,"SET %s",strbuf);
        reply = redisCommand(c,"SET %s %s",fitskeys[i],row[i]);
        // fprintf(stderr, "SET: %d\n", reply->type);
        freeReplyObject(reply); 
        }
      ftoa(ra_derived,RAbuf);
      ftoa(ra_derived*15,RADbuf);
      ftoa(dec_derived,Decbuf);
      ftoa(lsthour,lsthourbuf); 
      reply = redisCommand(c,"SET RA_DRV %s",RAbuf);
      freeReplyObject(reply); 
      reply = redisCommand(c,"SET RADG_DRV %s",RADbuf);
      freeReplyObject(reply); 
      reply = redisCommand(c,"SET DEC_DRV %s",Decbuf);
      freeReplyObject(reply); 
      reply = redisCommand(c,"SET LSTH_DRV %s",lsthourbuf);
      freeReplyObject(reply); 
      }
 
    //display values to stdout (if so desired)
     
    if (dostdout) {
      for (i=0;i<numkeys;i++) printf("%2d %8s (%32s) : %s\n",i,fitskeys[i],mysqlkeys[i],row[i]);
      printf("   LSTH_DRV (%32s) : %lf\n","",lsthour);
      printf("     RA_DRV (%32s) : %lf\n","",ra_derived);
      printf("   RADG_DRV (%32s) : %lf\n","",ra_derived*15);
      printf("    DEC_DRV (%32s) : %lf\n","",dec_derived);
      printf("-----------\n");
      }

    mysql_free_result(resp);

    usleep(SLEEP_MICROSECONDS); // sleep one half a second (enough to ensure update within +/- 1 second)
  
    } // end main while loop
  
  }


/*
 
MJD = JD - 2400000.5
JD = MJD + 2400000.5
UNIX = (JD - 2440587.5) * 86400
UNIX = (MJD - -40587.0) * 86400

*/


