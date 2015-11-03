inline int logger(char * log_message, int line_limit) {
// Keep an in-memory log that gets written out every so often.
// This is currently used to keep track of packet wait, recv,
// and process times.
    static char log[1024*1024*1024];
    static int log_size  = sizeof(log);
    static int log_limit = int(log_size*0.9); // don't exceed 90% of log size
    static char * log_p  = log;
    static int num_lines;
    int retval;

    num_lines++;
    sprintf(log_p, "%05d  %s\n", num_lines, log_message);
    log_p += strlen(log_p);
    retval = num_lines;

    if(num_lines >= line_limit || log_p - log >= log_limit) {
	    hashpipe_info(__FUNCTION__, "\n%s", log);   // write out current log
        log_p  = log;                               // start a new one
        retval = line_limit;                        // in case we are at log_limit
        num_lines = 0;
    }
    return retval;
}

