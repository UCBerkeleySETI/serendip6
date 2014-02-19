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

    //s6_input_block_t fake_data_block;

    /* Main loop */
    int i, rv;
    uint64_t mcnt = 0;
    uint64_t *data;
    int block_idx = 0;
    int error_count = 0, max_error_count = 0;
    float error, max_error = 0.0;
    int gen_fake = 0;

    hashpipe_status_lock_safe(&st);
    //hashpipe_status_lock_safe(p_st);
    hputi4(st.buf, "NUMCCHAN", N_COARSE_CHAN);
    hputi4(st.buf, "NUMFCHAN", N_FINE_CHAN);
    hputi4(st.buf, "NUMBBEAM", N_BYTES_PER_BEAM);
    hputi4(st.buf, "NUMBBLOC", sizeof(s6_input_block_t));
    hputi4(st.buf, "THRESHLD", POWER_THRESH);
    hgeti4(st.buf, "GENFAKE", &gen_fake);
    hashpipe_status_unlock_safe(&st);
    //hashpipe_status_unlock_safe(p_st);

    time_t t, prior_t;
    prior_t = time(&prior_t);

    while (run_threads()) {

        hashpipe_status_lock_safe(&st);
        //hashpipe_status_lock_safe(p_st);
        hputi4(st.buf, "NETBKOUT", block_idx);
        hputs(st.buf, status_key, "waiting");
        hashpipe_status_unlock_safe(&st);
        //hashpipe_status_unlock_safe(p_st);
 
        t = time(&t);
        fprintf(stderr, "elapsed seconds for block %d : %ld\n", block_idx, t - prior_t);
        prior_t = t;
        // Wait for data
        struct timespec sleep_dur, rem_sleep_dur;
        sleep_dur.tv_sec = 1;
        sleep_dur.tv_nsec = 0;
        fprintf(stderr, "fake net thread sleeping for %7.5f seconds\n", 
                sleep_dur.tv_sec + (double)sleep_dur.tv_nsec/1000000000.0);
        nanosleep(&sleep_dur, &rem_sleep_dur);
	
        /* Wait for new block to be free, then clear it
         * if necessary and fill its header with new values.
         */
        while ((rv=s6_input_databuf_wait_free(db, block_idx)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked");
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
        memset(db->block[block_idx].header.missed_pkts, 0, sizeof(uint64_t) * N_BEAM_SLOTS);

    if(gen_fake) {
        gen_fake = 0;
        // gen fake data for all beams, all blocks   
        // TODO vary data by beam
//fprintf(stderr, "db->block[0]                                             addr is %p\n", (void *)&db->block[0]);
//fprintf(stderr, "db->block[1]                                             addr is %p\n", (void *)&db->block[1]);
//fprintf(stderr, "db->block[2]                                             addr is %p\n", (void *)&db->block[2]);
//fprintf(stderr, "db->block[0].data[0]                                     addr is %p\n", (void *)&db->block[0].data[0]);
//fprintf(stderr, "db->block[0].data[1 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[1 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[0].data[2 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[2 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[0].data[3 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[3 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[0].data[4 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[4 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[0].data[5 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[5 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[0].data[6 * N_BYTES_PER_BEAM/sizeof(uint64_t)] addr is %p\n", (void *)&db->block[0].data[6 * N_BYTES_PER_BEAM/sizeof(uint64_t)]);
//fprintf(stderr, "db->block[1].data[0]                                     addr is %p\n", (void *)&db->block[1].data[0]);
//fprintf(stderr, "db->block[2].data[0]                                     addr is %p\n", (void *)&db->block[2].data[0]);
            fprintf(stderr, "generating fake data to block 0 beam 0...");
//fprintf(stderr, "generating data to %p\n", &db->block[0].data[0]);
            gen_fake_data(&(db->block[0].data[0]));
            fprintf(stderr, " done\n");
            fprintf(stderr, "copying to block 0 beam");
            for(int beam_i = 1; beam_i < N_BEAMS; beam_i++) {
                fprintf(stderr, " %d", beam_i);
//fprintf(stderr, "beam  %d copying %ld bytes from %p to %p\n", beam_i, N_BYTES_PER_BEAM, &db->block[0].data[0], &db->block[0].data[beam_i*N_BYTES_PER_BEAM/sizeof(uint64_t)]);
                memcpy((void *)&db->block[0].data[beam_i*N_BYTES_PER_BEAM/sizeof(uint64_t)], 
                       (void *)&db->block[0].data[0], 
                       N_BYTES_PER_BEAM);
                //gen_fake_data(&(db->block[block_idx].data[beam_i*N_BYTES_PER_BEAM]));
            }
            fprintf(stderr, " done\n");
            fprintf(stderr, "copying to block");
            for(int block_i = 1; block_i < N_INPUT_BLOCKS; block_i++) {
                fprintf(stderr, " %d", block_i);
//fprintf(stderr, "block %d copying %ld bytes from %p to %p\n", block_i, N_DATA_BYTES_PER_BLOCK, &db->block[0].data[0], &db->block[block_i].data[0]);
                memcpy((void *)&db->block[block_i].data[0], 
                       (void *)&db->block[0].data[0], 
                       N_DATA_BYTES_PER_BLOCK);
            }
            fprintf(stderr, " done\n");
        }

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
