/*
 * s6_fake_net_thread.c
 *
 * Routine to write fake data into shared memory blocks.  This allows the
 * processing pipelines to be tested without the network portion of SERENDIP6.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "hashpipe.h"
#include "s6_gen_fake_data.h"
#include "s6_databuf.h"

static void *run(hashpipe_thread_args_t * args)
{
    s6_input_databuf_t *db  = (s6_input_databuf_t *)args->obuf;
    hashpipe_status_t *p_st = &(args->st);

    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

    /* Main loop */
    int i, rv;
    uint64_t mcnt = 0;
    uint64_t *data;
    int block_idx = 0;
    int error_count = 0, max_error_count = 0;
    float error, max_error = 0.0;

    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        //hashpipe_status_lock_safe(p_st);
        hputi4(st.buf, "NETBKOUT", block_idx);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);
        //hashpipe_status_unlock_safe(p_st);
 
#if 1
        // Wait for data
        struct timespec sleep_dur, rem_sleep_dur;
        sleep_dur.tv_sec = 1;
        sleep_dur.tv_nsec = 0;
        fprintf(stderr, "sleeping\n");
        nanosleep(&sleep_dur, &rem_sleep_dur);
        fprintf(stderr, "slept\n");
#endif

	
        /* Wait for new block to be free, then clear it
         * if necessary and fill its header with new values.
         */
        while ((rv=s6_input_databuf_wait_free(db, block_idx)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked_1out");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                pthread_exit(NULL);
                break;
            }
        }

        hashpipe_status_lock_safe(&st);
        hputs(st.buf, status_key, "receiving");
        hashpipe_status_unlock_safe(&st);
 
        // populate block header
        db->block[block_idx].header.mcnt = mcnt;
        db->block[block_idx].header.coarse_chan_id = 321;
        memset(db->block[block_idx].header.missed_pkts, 0, sizeof(uint64_t) * N_BEAMS);

        // For testing, zero out block and set input FAKE_TEST_INPUT, FAKE_TEST_CHAN to
        // all -16 (-1 * 16)
        //data = db->block[block_idx].data;
        //memset(data, 0, N_BYTES_PER_BLOCK);
#if 1
        // gen fake data for all beams   
        // TODO vary data by beam
        static bool first_time = true;
        if(first_time) {
            first_time = false;
            gen_fake_data(&(db->block[block_idx].data[0]));
            //for(int beam_i = 0; beam_i < N_BEAMS; beam_i++) {
            for(int beam_i = 1; beam_i < N_BEAMS; beam_i++) {
                memcpy((void *)&db->block[block_idx].data[beam_i*N_BYTES_PER_BEAM/sizeof(uint64_t)], 
                       (const void *)&db->block[block_idx].data[0], 
                       N_BYTES_PER_BEAM);
                //gen_fake_data(&(db->block[block_idx].data[beam_i*N_BYTES_PER_BEAM]));
            }
        }
#endif

        hashpipe_status_lock_safe(&st);
        hputr4(st.buf, "NETMXERR", max_error);
        hputi4(st.buf, "NETERCNT", error_count);
        hputi4(st.buf, "NETMXECT", max_error_count);
        hashpipe_status_unlock_safe(&st);

        // Mark block as full
        s6_input_databuf_set_filled(db, block_idx);

        // Setup for next block
        block_idx = (block_idx + 1) % db->header.n_block;
        mcnt++;

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

    // Thread success!
    return THREAD_OK;
}

static hashpipe_thread_desc_t fake_net_thread = {
    name: "s6_fake_net_thread",
    skey: "NETSTAT",
    init: NULL,
    run:  run,
    ibuf_desc: {NULL},
    obuf_desc: {s6_input_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&fake_net_thread);
}
