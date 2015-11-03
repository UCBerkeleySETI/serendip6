#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "s6_obs_data_gbt.h"

int main() {

  gbtstatus_t gbtstatus;
  gbtstatus_t * gbtstatus_p = &gbtstatus;
  // const char *hostname = "127.0.0.1";
  const char *hostname = "10.0.1.1";
  int port = 6379;

  get_obs_gbt_info_from_redis(gbtstatus_p, (char *)hostname, port);

  fprintf(stderr,"MJD: %lf LASTUPDT: %s\n",gbtstatus_p->MJD,gbtstatus_p->LASTUPDT);

  } 

