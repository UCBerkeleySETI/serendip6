#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mysql.h"

#include <hiredis.h>

// file containing redis key/mysql row pairs for which to read from mysql and put into redis
const char *status_fields_config = "./status_fields";

MYSQL* mysql;
MYSQL_ROW row;
MYSQL_RES* resp;
int retval;

int main() {
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

  //### read in status fields (and create mysql "select" command string)
  char *selectcommand = (char *)malloc(sizeof(char*)*lines_allocated*max_line_len);
  char **fitskeys= (char **)malloc(sizeof(char*)*lines_allocated);
  char **mysqlkeys= (char **)malloc(sizeof(char*)*lines_allocated);
  if (fitskeys==NULL || mysqlkeys==NULL) { fprintf(stderr,"Out of memory (while initializing).\n"); exit(1); }

  statusfp = fopen(status_fields_config, "r");
  if (statusfp == NULL) { fprintf(stderr,"Error opening file.\n"); exit(2); }

  for (i=0;1;i++) {
      if (i >= lines_allocated) { fprintf(stderr,"over allocated # of lines (%d lines).\n",lines_allocated); exit(3); }
      fitskeys[i] = malloc(max_line_len);
      mysqlkeys[i] = malloc(max_line_len);
      if (fitskeys[i]==NULL || mysqlkeys[i]==NULL) { fprintf(stderr,"Out of memory (getting next line).\n"); exit(4); }
      if (fscanf(statusfp,"%s %s",fitskeys[i],mysqlkeys[i])!=2) break;
      }
  fclose(statusfp);

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

  //### connect to mysql db
  mysql = mysql_init(0);
  mysql = mysql_real_connect (mysql, "gbtdata.gbt.nrao.edu","gbtstatus","w3bqu3ry","gbt_status",3306,0,CLIENT_FOUND_ROWS);

  //### MAIN LOOP
  while (1) {

    retval = mysql_query(mysql,selectcommand);
    if (retval) {
      fprintf(stderr,"can't connect to the status mysql database - exiting...\n");
      exit(1);
      }
    resp = mysql_store_result(mysql);
    if (!resp) {
      fprintf(stderr,"error storing mysql result - exiting...\n");
      exit(1);
      }
    row = mysql_fetch_row(resp);
    if (!row || !row[0]) {
      fprintf(stderr,"error fetching mysql row - exiting...\n");
      exit(1);
      }
    
    for (i=0;i<numkeys;i++) {
      // fprintf(stderr,"%d %s\n",i,row[i]);
      // sprintf(strbuf,"ROW%d \"%s\"",i,row[i]);
      // fprintf(stderr,"%s\n",strbuf);
      // reply = redisCommand(c,"SET %s",strbuf);
      reply = redisCommand(c,"SET %s \"%s\"",fitskeys[i],row[i]);
      fprintf(stderr, "SET: %s\n", reply->str);

      freeReplyObject(reply); 
      }

    fprintf(stderr,"------\n");
    
    mysql_free_result(resp);

    sleep(1);
  
    } // end main while loop
  
  }


/*
 
MJD = JD - 2400000.5
JD = MJD + 2400000.5
UNIX = (JD - 2440587.5) * 86400
UNIX = (MJD - -40587.0) * 86400

*/
