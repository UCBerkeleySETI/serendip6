/* s6_pktsock_thread.cpp
 *
 * Routine to read packets from network via packet socket and put them into
 * shared memory blocks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <errno.h>

#include <sched.h>

#include <smmintrin.h>
#include <immintrin.h>

#include "hashpipe.h"
#include "s6_databuf.h"

#define DEBUG_NET

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define PKTSOCK_BYTES_PER_FRAME (16384)
#define PKTSOCK_FRAMES_PER_BLOCK (8)
#define PKTSOCK_NBLOCKS (8)
#define PKTSOCK_NFRAMES (PKTSOCK_FRAMES_PER_BLOCK * PKTSOCK_NBLOCKS)

// Number of mcnts per block
static const uint64_t     Nm = N_FINE_CHAN;
static const unsigned int N_PACKETS_PER_BLOCK = Nm * N_BEAMS;
static const unsigned int N_BYTES_PER_CHAN = 4;

typedef struct {
    uint64_t mcnt;
    uint64_t pchan;	// First chan in packet
    uint64_t nchan;	// Number of channels in packet
    uint64_t sid;	// Source ID (aka beam id)
} packet_header_t;

// The fields of a block_info_t structure hold (at least) two different kinds
// of data.  Some fields hold data that persist over many packets while other
// fields hold data that are only applicable to the current packet (or the
// previous packet).
typedef struct {
    int initialized;
    uint64_t  pchan;
    uint64_t  nchan;
    uint64_t mcnt_start;
    uint64_t mcnt_log_late;
    int out_of_seq_cnt;
    int block_i;
    // The m,x,f fields hold three of the five dimensional indices for
    // the first data word of the current packet (i.e. t=0 and c=0).
    int m; // formerly known as sub_block_i
    int f;
    int block_packet_counter[N_INPUT_BLOCKS];
} block_info_t;

static hashpipe_status_t *st_p;

static void print_pkt_header(packet_header_t * pkt_header) {

    static long long prior_mcnt;

    printf("packet header : mcnt %012lx (diff from prior %lld) pchan %lud sid %lu\n",
	   pkt_header->mcnt, pkt_header->mcnt-prior_mcnt, pkt_header->pchan, pkt_header->sid);

    prior_mcnt = pkt_header->mcnt;
}

static void print_block_info(block_info_t * binfo) {
    printf("binfo : mcnt_start %012lx block_i %d\n",
           binfo->mcnt_start, binfo->block_i);
}

static void print_block_packet_counter(block_info_t * binfo) {
    int i;
    for(i=0;i<N_INPUT_BLOCKS;i++) {
	if(i == binfo->block_i) {
		fprintf(stdout, "*%03d ", binfo->block_packet_counter[i]);	
	} else {
		fprintf(stdout, " %03d ", binfo->block_packet_counter[i]);	
	}
    }
    fprintf(stdout, "\n");
}

static void print_ring_mcnts(s6_input_databuf_t *s6_input_databuf_p) {

    int i;

    for(i=0; i < N_INPUT_BLOCKS; i++) {
	printf("block %d mcnt %012lx\n", i, s6_input_databuf_p->block[i].header.mcnt);
    }
}

// Returns physical block number for given mcnt
static inline int block_for_mcnt(uint64_t mcnt)
{
    return (mcnt / Nm) % N_INPUT_BLOCKS;
}

#ifdef LOG_MCNTS
#define MAX_MCNT_LOG (1024*1024)
//static uint64_t mcnt_log[MAX_MCNT_LOG];
//static int mcnt_log_idx = 0;
static int total_packets_counted = 0;
static int expected_packets_counted = 0;
static int late_packets_counted = 0;
static int outofseq_packets_counted = 0;
static int filled_packets_counted = 0;

void dump_mcnt_log(int pchan)
{
    //int i;
    char fname[80];
    FILE *f;
    sprintf(fname, "mcnt.pchan%02d.log", pchan);
    f = fopen(fname,"w");
    fprintf(f, "expected packets counted = %d\n", expected_packets_counted);
    fprintf(f, "late     packets counted = %d\n", late_packets_counted);
    fprintf(f, "outofseq packets counted = %d\n", outofseq_packets_counted);
    fprintf(f, "total    packets counted = %d\n", total_packets_counted);
    fprintf(f, "filled   packets counted = %d\n", filled_packets_counted);
    //for(i=0; i<MAX_MCNT_LOG; i++) {
    //    if(mcnt_log[i] == 0) break;
    //    fprintf(f, "%012lx\n", mcnt_log[i]);
    //}
    fclose(f);
}
#endif

static inline void * s6_memcpy(uint64_t * out, const uint64_t * const in, size_t n_bytes) {
//#define bitload256
  __m128i lo128, hi128;
  __m256i out256         = _mm256_setzero_si256();
#ifdef bitload256
  __m256i *p_in          = (__m256i *)in;
#else
  __m128i *p_in          = (__m128i *)in;
#endif
  __m256i *p_out         = (__m256i *)out;
  size_t n_256_bit_words = n_bytes/32;
  size_t i;

  for(i=0; i<n_256_bit_words; i++) {
#ifdef bitload256
        //out256 = _mm256_stream_load_si256(p_in++);  // Program terminated with signal 4, Illegal instruction.
        out256 = _mm256_load_si256(p_in++);
#else
        // have to load 128, rather than 256, bits at a time
        lo128 = _mm_stream_load_si128(p_in++);
        hi128 = _mm_stream_load_si128(p_in++);

        // Transfer lo128 to lower half of __m256i variable 'out256'
        // Use pragmas to avoid "uninitialized use of out256" warning/error
        out256 = _mm256_insertf128_si256(out256, lo128, 0);
        // Transfer hi128 to upper half of __m256i variable 'out256'
        out256 = _mm256_insertf128_si256(out256, hi128, 1);
#endif
        // now stream to final destination 256 bits at a time
        _mm256_stream_si256(p_out++, out256);

        // both p_in and p_out have now been advanced by 256 bits (32 bytes)
  }

//fprintf(stderr, "In s6_memcpy : end   : p_in = %p p_out = %p\n", p_in, p_out);
  return (void *)out;
}

static inline void get_header(unsigned char *p_frame, packet_header_t * pkt_header)
{
    uint64_t raw_header;
    raw_header = be64toh(*(unsigned long long *)PKT_UDP_DATA(p_frame));
    pkt_header->mcnt        =  raw_header >> 16;
    pkt_header->pchan       = (raw_header >>  4) & 0x0000000000000FFF;
    pkt_header->sid         =  raw_header        & 0x000000000000000F;

    // Compute nchan from packet size (minus UDP header and S6 header and CRC words)
    pkt_header->nchan = (PKT_UDP_SIZE(p_frame) - 3*8)/ N_BYTES_PER_CHAN; // N_BYTES_PER_CHAN==4

#ifdef LOG_MCNTS
    total_packets_counted++;
    //mcnt_log[mcnt_log_idx++] = pkt_header->mcnt;
    //if(mcnt_log_idx == MAX_MCNT_LOG) {
    //    dump_mcnt_log(pkt_header->sid);
    //    abort();
    //}
    if(total_packets_counted == 10*1000*1000) {
	dump_mcnt_log(pkt_header->sid);
	abort();
    }
#endif
}

#ifdef DIE_ON_OUT_OF_SEQ_FILL
static void die(s6_input_databuf_t *s6_input_databuf_p, block_info_t *binfo)
{
    print_block_info(binfo);
    print_block_packet_counter(binfo);
    print_ring_mcnts(s6_input_databuf_p);
#ifdef LOG_MCNTS
    dump_mcnt_log();
#endif
    abort(); // End process and generate core file (if ulimit allows)
}
#endif

// This sets the "current" block to be marked as filled.  The current block is
// the block corresponding to binfo->mcnt_start.  Returns mcnt of the block
// being marked filled.
static uint64_t set_block_filled(s6_input_databuf_t *s6_input_databuf_p, block_info_t *binfo)
{
    static int last_filled = -1;

    uint32_t block_missed_pkt_cnt=N_PACKETS_PER_BLOCK, block_missed_mod_cnt, block_missed_beams, missed_pkt_cnt=0;

    uint32_t block_i = block_for_mcnt(binfo->mcnt_start);

    // Validate that we're filling blocks in the proper sequence
    last_filled = (last_filled+1) % N_INPUT_BLOCKS;
    if(last_filled != block_i) {
	printf("block %d being marked filled, but expected block %d!\n", block_i, last_filled);
#ifdef DIE_ON_OUT_OF_SEQ_FILL
	die(s6_input_databuf_p, binfo);
#endif
    }

    // Validate that block_i matches binfo->block_i
    if(block_i != binfo->block_i) {
	hashpipe_warn(__FUNCTION__,
		"block_i for binfo's mcnt (%d) != binfo's block_i (%d)",
		block_i, binfo->block_i);
    }
#ifdef LOG_MCNTS
    filled_packets_counted += binfo->block_packet_counter[block_i];
#endif

    // Set the block as filled
    if(s6_input_databuf_set_filled(s6_input_databuf_p, block_i) != HASHPIPE_OK) {
	hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
	pthread_exit(NULL);
    }

    // Calculate missing packets.
    block_missed_pkt_cnt = N_PACKETS_PER_BLOCK - binfo->block_packet_counter[block_i];
    // If we missed more than Nm, then assume we are missing one or more beams.
    // Any missed packets beyond an integer multiple of Nm will be considered
    // as dropped packets.
    block_missed_beams   = block_missed_pkt_cnt / Nm;
    block_missed_mod_cnt = block_missed_pkt_cnt % Nm;

    // Update status buffer
    hashpipe_status_lock_busywait_safe(st_p);
    hputu4(st_p->buf, "NETBKOUT", block_i);
    hputu4(st_p->buf, "MISSEDBM", block_missed_beams);
    if(block_missed_mod_cnt) {
	// Increment MISSEDPK by number of missed packets for this block
	hgetu4(st_p->buf, "MISSEDPK", &missed_pkt_cnt);
	missed_pkt_cnt += block_missed_mod_cnt;
	hputu4(st_p->buf, "MISSEDPK", missed_pkt_cnt);
    //  fprintf(stderr, "got %d packets instead of %d\n",
    //	    binfo->block_packet_counter[block_i], Nm);
    }
    // Update SCHAN and NCHAN in status buffer
    hputi4(st_p->buf, "SCHAN", (uint32_t)binfo->pchan);
    hputi4(st_p->buf, "NCHAN", (uint32_t)binfo->nchan);
    hashpipe_status_unlock_safe(st_p);

    return binfo->mcnt_start;
}

static inline int calc_block_indexes(block_info_t *binfo, packet_header_t * pkt_header)
{
    // Reject pacets with bad SID Values
    if(pkt_header->sid >= N_BEAMS) {
	hashpipe_error(__FUNCTION__,
		"current packet SID %u out of range (0-%d)",
		pkt_header->sid, N_BEAMS-1);
	return -1;
    }

    // Lazy init binfo->pchan and binfo->nchan
    if(binfo->nchan == 0) {
	binfo->nchan = pkt_header->nchan;
	binfo->pchan = pkt_header->pchan;
    }

    // Warn on changes to PCHAN and NCHAN
    if(pkt_header->pchan != binfo->pchan) {
	hashpipe_warn(__FUNCTION__,
		"unexpected packet PCHAN %d (expected %d)",
		pkt_header->pchan, binfo->pchan);
    }
    if(pkt_header->nchan != binfo->nchan) {
	hashpipe_warn(__FUNCTION__,
		"unexpected packet NCHAN %d (expected %d)",
		pkt_header->nchan, binfo->nchan);
    }

    return 0;
}

// This allows for 2 out of sequence packets from each beam (in a row)
#define MAX_OUT_OF_SEQ (2*N_BEAMS)

// This allows packets to be two full databufs late without being considered
// out of sequence.
#define LATE_PKT_MCNT_THRESHOLD (2*Nm*N_INPUT_BLOCKS)

// Initialize a block by setting its per-beam missed packet counters to Nm and
// saving the first (i.e. earliest) mcnt of the block.  Note that mcnt does not
// have to be a multiple of Nm.  In theory, the block's data could be cleared
// as well, but that takes time and is largely unnecessary in a properly
// functionong system.  If desired, it could be done by the downstream thread.
static inline void initialize_block(s6_input_databuf_t * s6_input_databuf_p,
	uint64_t mcnt, uint64_t pchan, uint64_t nchan)
{
    int i;
    int block_i = block_for_mcnt(mcnt);

    for(i=0; i<N_BEAM_SLOTS; i++) {
	s6_input_databuf_p->block[block_i].header.missed_pkts[i] = (i < N_BEAMS ? Nm : 0);
    }
    // Round pkt_mcnt down to nearest multiple of Nm
    s6_input_databuf_p->block[block_i].header.mcnt = mcnt - (mcnt % Nm);

    // Save pchan and nchan
    s6_input_databuf_p->block[block_i].header.coarse_chan_id = pchan;
    s6_input_databuf_p->block[block_i].header.num_coarse_chan = nchan;
}

// This function must be called once and only once per block_info structure!
// Subsequent calls are no-ops.
static inline void initialize_block_info(block_info_t * binfo)
{
    int i;

    // If this block_info structure has already been initialized
    if(binfo->initialized) {
	return;
    }

    for(i = 0; i < N_INPUT_BLOCKS; i++) {
	binfo->block_packet_counter[i] = 0;
    }

    // Initialize nchan and pchan to 0 (unknown until read from packet)
    binfo->pchan = 0;
    binfo->nchan = 0;

    // On startup mcnt_start will be zero and mcnt_log_late will be Nm.
    binfo->mcnt_start = 0;
    binfo->mcnt_log_late = Nm;
    binfo->block_i = 0;

    binfo->out_of_seq_cnt = 0;
    binfo->initialized = 1;
}

// This function returns -1 unless the given packet causes a block to be marked
// as filled in which case this function returns the marked block's first mcnt.
// Any return value other than -1 will be stored in the status memory as
// NETMCNT, so it is important that values other than -1 are returned rarely
// (i.e. when marking a block as filled)!!!
static inline uint64_t process_packet(
	s6_input_databuf_t *s6_input_databuf_p, unsigned char *p_frame)
{

    static block_info_t binfo;
    packet_header_t pkt_header;
    const uint64_t *payload_p;
    int pkt_block_i;
    int new_block_i;
    uint64_t *dest_p;
    int64_t pkt_mcnt_dist;
    uint64_t pkt_mcnt;
    uint64_t cur_mcnt;
    uint64_t netmcnt = -1; // Value to return (!=-1 is stored in status memory)
    int i = 0;
#if N_DEBUG_INPUT_BLOCKS == 1
    static uint64_t debug_remaining = -1ULL;
    static off_t debug_offset = 0;
    uint64_t * debug_ptr;
#endif

    // Lazy init binfo
    if(!binfo.initialized) {
	initialize_block_info(&binfo);
    }

    // Parse packet header
    get_header(p_frame, &pkt_header);
    pkt_mcnt = pkt_header.mcnt;
    pkt_block_i = block_for_mcnt(pkt_mcnt);
    cur_mcnt = binfo.mcnt_start;

    // Packet mcnt distance (how far away is this packet's mcnt from the
    // current mcnt).  Positive distance for pkt mcnt > current mcnt.
    pkt_mcnt_dist = pkt_mcnt - cur_mcnt;

#if N_DEBUG_INPUT_BLOCKS == 1
    debug_ptr = (uint64_t *)&s6_input_databuf_p->block[N_INPUT_BLOCKS];
    debug_ptr[debug_offset++] = be64toh(*(uint64_t *)(p->data));
    if(--debug_remaining == 0) {
	exit(1);
    }
    if(debug_offset >= sizeof(s6_input_block_t)/sizeof(uint64_t)) {
	debug_offset = 0;
    }
#endif

    // We expect packets for the current block and the next block.
    if(0 <= pkt_mcnt_dist && pkt_mcnt_dist < 2*Nm) {
	// If the packet is for the second half of the next block (i.e. current
	// block + 3/2 blocks), then "current" block is done.
	if(pkt_mcnt_dist >= 3*(Nm/2)) {

	    for(i=0; i<N_BEAMS; i++) {
		if(s6_input_databuf_p->block[binfo.block_i].header.missed_pkts[i] != 0)
		    printf("missed %lu packets for beam %d\n",
			s6_input_databuf_p->block[binfo.block_i].header.missed_pkts[i], i);
	    }

	    // Mark the current block as filled
	    netmcnt = set_block_filled(s6_input_databuf_p, &binfo);

	    // Advance mcnt_start to next block
	    cur_mcnt += Nm;
	    binfo.mcnt_start += Nm;
	    binfo.block_i = (binfo.block_i + 1) % N_INPUT_BLOCKS;
	    // Reset binfo's pchan and nchan fields
	    binfo.pchan = 0;
	    binfo.nchan = 0;

	    // Wait (hopefully not long!) to acquire the block after the new
	    // "current" block (i.e. plan ahead).
	    new_block_i = (binfo.block_i + 1) % N_INPUT_BLOCKS;
	    if(s6_input_databuf_busywait_free(s6_input_databuf_p, new_block_i) != HASHPIPE_OK) {
		if (errno == EINTR) {
		    // Interrupted by signal, return -1
		    hashpipe_error(__FUNCTION__, "interrupted by signal waiting for free databuf");
		    pthread_exit(NULL);
		    return -1; // We're exiting so return value is kind of moot
		} else {
		    hashpipe_error(__FUNCTION__, "error waiting for free databuf");
		    pthread_exit(NULL);
		    return -1; // We're exiting so return value is kind of moot
		}
	    }

	    // Initialize the newly acquired block
	    initialize_block(s6_input_databuf_p, pkt_mcnt+Nm, pkt_header.pchan, pkt_header.nchan);
	    // Reset binfo's packet counter for this packet's block
	    binfo.block_packet_counter[new_block_i] = 0;
	}

	// Reset out-of-seq counter
	binfo.out_of_seq_cnt = 0;

	// Increment packet count for block
	binfo.block_packet_counter[pkt_block_i]++;
#ifdef LOG_MCNTS
	expected_packets_counted++;
#endif

	// Validate header SID, PCHAN, and NCHAN.
	if(calc_block_indexes(&binfo, &pkt_header)) {
	    // Bad packet, error already reported
	    return -1;
	}

	// Decrement missed packet counter
	s6_input_databuf_p->block[pkt_block_i].header.missed_pkts[pkt_header.sid]--;

	// Calculate starting points for unpacking this packet into block's data buffer.
	dest_p = s6_input_databuf_p->block[pkt_block_i].data
	    + (pkt_header.sid*Nm + (pkt_mcnt%Nm)) * N_COARSE_CHAN * N_BYTES_PER_CHAN / sizeof(uint64_t);

	// Copy data into buffer

#if 1
	// Point to payload (after S6 header)
	payload_p        = (uint64_t *)(PKT_UDP_DATA(p_frame)+8);
	// Use length from packet (minus UDP header and minus HEADER word and minus CRC word)
	memcpy(dest_p, payload_p, PKT_UDP_SIZE(p_frame) - 8 - 8 - 8);
#else
	// Copy "extra" packet preface data to maintain alignment
	payload_p        = (uint64_t *)(PKT_NET(p_frame)+16);
	s6_memcpy(dest_p, payload_p, PKT_UDP_SIZE(p_frame));
#endif

	//memset(dest_p, 0, N_COARSE_CHAN*N_BYTES_PER_CHAN);
	//dest_p[0] = be64toh(*(uint64_t *)(p->data));

	return netmcnt;
    }
    // Else, if packet is late, but not too late (so we can handle gateware
    // restarts and MCNT rollover), then ignore it
    else if(pkt_mcnt_dist < 0 && pkt_mcnt_dist > -(int64_t)LATE_PKT_MCNT_THRESHOLD) {
	// If not just after an mcnt reset, issue warning.
	if(cur_mcnt >= binfo.mcnt_log_late) {
	    hashpipe_warn("s6_pktsock_thread",
		    "Ignoring late packet (%d mcnts late)",
		    cur_mcnt - pkt_mcnt);
	}
#ifdef LOG_MCNTS
	late_packets_counted++;
#endif
	return -1;
    }
    // Else, it is an "out-of-order" packet.
    else {
	// If not at start-up, issue warning.
	if(cur_mcnt != 0) {
	    hashpipe_warn("s6_pktsock_thread",
		    "out of seq mcnt %012lx (expected: %012lx <= mcnt < %012x)",
		    pkt_mcnt, cur_mcnt, cur_mcnt+2*Nm);
	}

	// Increment out-of-seq packet counter
	binfo.out_of_seq_cnt++;
#ifdef LOG_MCNTS
	outofseq_packets_counted++;
#endif

	// If too may out-of-seq packets
	if(binfo.out_of_seq_cnt > MAX_OUT_OF_SEQ) {
	    // Reset current mcnt.  The value to reset to must be the first
	    // value greater than or equal to pkt_mcnt that corresponds to the
	    // same databuf block as the old current mcnt.
	    if(binfo.block_i > pkt_block_i) {
		// Advance pkt_mcnt to correspond to binfo.block_i
		pkt_mcnt += Nm*(binfo.block_i - pkt_block_i);
	    } else if(binfo.block_i < pkt_block_i) {
		// Advance pkt_mcnt to binfo.block_i + N_INPUT_BLOCKS blocks
		pkt_mcnt += Nm*(binfo.block_i + N_INPUT_BLOCKS - pkt_block_i);
	    }
	    // Round pkt_mcnt down to nearest multiple of Nm
	    binfo.mcnt_start = pkt_mcnt - (pkt_mcnt%Nm);
	    binfo.mcnt_log_late = binfo.mcnt_start + Nm;
	    binfo.block_i = block_for_mcnt(binfo.mcnt_start);
	    hashpipe_warn("s6_pktsock_thread",
		    "resetting to mcnt %012lx block %d based on packet mcnt %012lx",
		    binfo.mcnt_start, block_for_mcnt(binfo.mcnt_start), pkt_mcnt);
	    // Reset binfo's pchan and nchan fields
	    binfo.pchan = 0;
	    binfo.nchan = 0;
	    // Reinitialize/recycle our two already acquired blocks with new
	    // mcnt values.
	    initialize_block(s6_input_databuf_p, binfo.mcnt_start,
		    pkt_header.pchan, pkt_header.nchan);
	    initialize_block(s6_input_databuf_p, binfo.mcnt_start+Nm,
		    pkt_header.pchan, pkt_header.nchan);
	    // Reset binfo's packet counters for these blocks.
	    binfo.block_packet_counter[binfo.block_i] = 0;
	    binfo.block_packet_counter[(binfo.block_i+1)%N_INPUT_BLOCKS] = 0;
	}
	return -1;
    }

    return netmcnt;
}

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

static void *run(hashpipe_thread_args_t * args)
{
    // Local aliases to shorten access to args fields
    // Our output buffer happens to be a s6_input_databuf
    s6_input_databuf_t *db = (s6_input_databuf_t *)args->obuf;
    hashpipe_status_t st = args->st;
    const char * status_key = args->thread_desc->skey;

    st_p = &st;	// allow global (this source file) access to the status buffer

    // Flag that holds off the net thread
    int holdoff = 1;

#if 0
    // raise this thread to maximum scheduling priority
    // This was found not to help with peformance, but try it again anyway...
    struct sched_param SchedParam;
    int retval;
    SchedParam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    hashpipe_info(__FUNCTION__, "Setting scheduling priority to %d\n", SchedParam.sched_priority);
    retval = sched_setscheduler(0, SCHED_FIFO, &SchedParam);
    if(retval) {
        perror("sched_setscheduler :");
    }
#endif

    // Force ourself into the hold off state
    hashpipe_status_lock_safe(&st);
    hputi4(st.buf, "NETHOLD", 1);
    hashpipe_status_unlock_safe(&st);

    while(holdoff) {
	// We're not in any hurry to startup
	sleep(1);
	hashpipe_status_lock_safe(&st);
	// Look for NETHOLD value
	hgeti4(st.buf, "NETHOLD", &holdoff);
	if(!holdoff) {
	    // Done holding, so delete the key
	    hdel(st.buf, "NETHOLD");
	}
	hashpipe_status_unlock_safe(&st);
    }

#ifdef DEBUG_SEMS
    fprintf(stderr, "s/tid %lu/NET/' <<.\n", pthread_self());
#endif

#if 0
    /* Copy status buffer */
    char status_buf[HASHPIPE_STATUS_SIZE];
    hashpipe_status_lock_busywait_safe(st_p);
    memcpy(status_buf, st_p->buf, HASHPIPE_STATUS_SIZE);
    hashpipe_status_unlock_safe(st_p);
#endif

    /* Read network params */
    struct hashpipe_udp_params up;
    strcpy(up.bindhost, "eth2");
    up.bindport = 21302;
    up.packet_size = 8 + (N_COARSE_CHAN * N_BYTES_PER_CHAN) + 8;

    hashpipe_status_lock_safe(&st);
    // Get info from status buffer if present (no change if not present)
    hgets(st.buf, "BINDHOST", 80, up.bindhost);
    hgeti4(st.buf, "BINDPORT", &up.bindport);
    // Store bind host/port info etc in status buffer
    hputs(st.buf, "BINDHOST", up.bindhost);
    hputi4(st.buf, "BINDPORT", up.bindport);
    hputu4(st.buf, "MISSEDBM", 0);
    hputu4(st.buf, "MISSEDPK", 0);
    hputs(st.buf, status_key, "running");
    hashpipe_status_unlock_safe(&st);

    unsigned char *p_frame;

    /* Give all the threads a chance to start before opening network socket */
    sleep(1);

#ifndef TIMING_TEST
    /* Set up pktsock */
    struct hashpipe_pktsock s_ps;
    // Make frame_size be a divisor of block size so that frames will be
    // contiguous in mapped mempory.  block_size must also be a multiple of
    // page_size.  Easiest way is to oversize the frames to be 16384 bytes, which
    // is bigger than we need, but keeps things easy.
    s_ps.frame_size = PKTSOCK_BYTES_PER_FRAME;
    // total number of frames
    s_ps.nframes = PKTSOCK_NFRAMES;
    // number of blocks
    s_ps.nblocks = PKTSOCK_NBLOCKS;

    int rv = hashpipe_pktsock_open(&s_ps, up.bindhost, PACKET_RX_RING);
    if (rv!=HASHPIPE_OK) {
        hashpipe_error("s6_pktsock_thread",
                "Error opening pktsock.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void (*)(void *))hashpipe_pktsock_close, &s_ps);
#endif

    // Acquire first two blocks to start
    if(s6_input_databuf_busywait_free(db, 0) != HASHPIPE_OK) {
	if (errno == EINTR) {
	    // Interrupted by signal, return -1
	    hashpipe_error(__FUNCTION__, "interrupted by signal waiting for free databuf");
	    pthread_exit(NULL);
	} else {
	    hashpipe_error(__FUNCTION__, "error waiting for free databuf");
	    pthread_exit(NULL);
	}
    }
    if(s6_input_databuf_busywait_free(db, 1) != HASHPIPE_OK) {
	if (errno == EINTR) {
	    // Interrupted by signal, return -1
	    hashpipe_error(__FUNCTION__, "interrupted by signal waiting for free databuf");
	    pthread_exit(NULL);
	} else {
	    hashpipe_error(__FUNCTION__, "error waiting for free databuf");
	    pthread_exit(NULL);
	}
    }

    // Initialize the newly acquired block
    initialize_block(db,  0, 0, 0);
    initialize_block(db, Nm, 0, 0);

    /* Main loop */
    uint64_t packet_count = 0;
    uint64_t wait_ns = 0; // ns for most recent wait
    uint64_t recv_ns = 0; // ns for most recent recv
    uint64_t proc_ns = 0; // ns for most recent proc
    uint64_t min_wait_ns = 99999; // min ns per single wait
    uint64_t min_recv_ns = 99999; // min ns per single recv
    uint64_t min_proc_ns = 99999; // min ns per single proc
    uint64_t max_wait_ns = 0;     // max ns per single wait
    uint64_t max_recv_ns = 0;     // max ns per single recv
    uint64_t max_proc_ns = 0;     // max ns per single proc
    uint64_t elapsed_wait_ns = 0; // cumulative wait time per block
    uint64_t elapsed_recv_ns = 0; // cumulative recv time per block
    uint64_t elapsed_proc_ns = 0; // cumulative proc time per block
    uint64_t status_ns = 0; // User to fetch ns values from status buffer
    float ns_per_wait = 0.0; // Average ns per wait over 1 block
    float ns_per_recv = 0.0; // Average ns per recv over 1 block
    float ns_per_proc = 0.0; // Average ns per proc over 1 block
    struct timespec start, stop;
    struct timespec recv_start, recv_stop;

    while (run_threads()) {

#ifndef TIMING_TEST
        /* Read packet */
	clock_gettime(CLOCK_MONOTONIC, &recv_start);
	do {
	    clock_gettime(CLOCK_MONOTONIC, &start);
	    //p_frame = hashpipe_pktsock_recv_udp_frame(&s_ps, up.bindport, 10);
	    p_frame = hashpipe_pktsock_recv_udp_frame_nonblock(&s_ps, up.bindport);
	    clock_gettime(CLOCK_MONOTONIC, &recv_stop);
	} while (!p_frame && run_threads());
	if(!run_threads()) break;
	// Handle variable packet size!
        int packet_size = PKT_UDP_SIZE(p_frame) - 8;
        if (packet_size < 16 || up.packet_size < packet_size || packet_size % 8 != 0) {
	    // Log warning and ignore wrongly sized packet
	    #ifdef DEBUG_NET
	    hashpipe_warn("s6_pktsock_thread", "Invalid pkt size (%d)", packet_size);
	    #endif
	    continue;
	}
#endif
	packet_count++;

        // Copy packet into any blocks where it belongs.
        const uint64_t mcnt = process_packet((s6_input_databuf_t *)db, p_frame);
	// Release frame back to kernel
	hashpipe_pktsock_release_frame(p_frame);

	clock_gettime(CLOCK_MONOTONIC, &stop);
	wait_ns = ELAPSED_NS(recv_start, start);
	recv_ns = ELAPSED_NS(start, recv_stop);
	proc_ns = ELAPSED_NS(recv_stop, stop);
	elapsed_wait_ns += wait_ns;
	elapsed_recv_ns += recv_ns;
	elapsed_proc_ns += proc_ns;
	// Update min max values
	min_wait_ns = MIN(wait_ns, min_wait_ns);
	min_recv_ns = MIN(recv_ns, min_recv_ns);
	min_proc_ns = MIN(proc_ns, min_proc_ns);
	max_wait_ns = MAX(wait_ns, max_wait_ns);
	max_recv_ns = MAX(recv_ns, max_recv_ns);
	max_proc_ns = MAX(proc_ns, max_proc_ns);

        if(mcnt != -1) {
            // Update status
            ns_per_wait = (float)elapsed_wait_ns / packet_count;
            ns_per_recv = (float)elapsed_recv_ns / packet_count;
            ns_per_proc = (float)elapsed_proc_ns / packet_count;

            hashpipe_status_lock_busywait_safe(&st);

            hputu8(st.buf, "NETMCNT", mcnt);
	    // Gbps = bits_per_packet / ns_per_packet
	    // (N_BYTES_PER_PACKET excludes header, so +8 for the header)
            hputr4(st.buf, "NETGBPS", 8*(packet_size)/(ns_per_recv+ns_per_proc));
            hputr4(st.buf, "NETWATNS", ns_per_wait);
            hputr4(st.buf, "NETRECNS", ns_per_recv);
            hputr4(st.buf, "NETPRCNS", ns_per_proc);

            hputi8(st.buf, "NETWATMN", min_wait_ns);
            hputi8(st.buf, "NETRECMN", min_recv_ns);
            hputi8(st.buf, "NETPRCMN", min_proc_ns);
            hputi8(st.buf, "NETWATMX", max_wait_ns);
            hputi8(st.buf, "NETRECMX", max_recv_ns);
            hputi8(st.buf, "NETPRCMX", max_proc_ns);

            hashpipe_status_unlock_safe(&st);

	    // Reset mins and maxs to make them be per-block rather than for
	    // all time.
	    min_wait_ns = 99999;
	    min_recv_ns = 99999;
	    min_proc_ns = 99999;
	    max_wait_ns = 0;
	    max_recv_ns = 0;
	    max_proc_ns = 0;

	    // Start new average
	    elapsed_wait_ns = 0;
	    elapsed_recv_ns = 0;
	    elapsed_proc_ns = 0;
	    packet_count = 0;
        }

#if defined TIMING_TEST || defined NET_TIMING_TEST

#define END_LOOP_COUNT (1*1000*1000)
	static int loop_count=0;
	static struct timespec tt_start, tt_stop;
	if(loop_count == 0) {
	    clock_gettime(CLOCK_MONOTONIC, &tt_start);
	}
	//if(loop_count == 1000000) pthread_exit(NULL);
	if(loop_count == END_LOOP_COUNT) {
	    clock_gettime(CLOCK_MONOTONIC, &tt_stop);
	    int64_t elapsed = ELAPSED_NS(tt_start, tt_stop);
	    printf("processed %d packets in %.6f ms (%.3f us per packet)\n",
		    END_LOOP_COUNT, elapsed/1e6, elapsed/1e3/END_LOOP_COUNT);
	    exit(0);
	}
	loop_count++;
#endif

        /* Will exit if thread has been cancelled */
        pthread_testcancel();
    }

#ifndef TIMING_TEST
    /* Have to close all push's */
    pthread_cleanup_pop(1); /* Closes push(hashpipe_pktsock_close) */
#endif

    return NULL;
}

static hashpipe_thread_desc_t pktsock_thread = {
    name: "s6_pktsock_thread",
    skey: "NETSTAT",
    init: NULL,
    run:  run,
    ibuf_desc: {NULL},
    obuf_desc: {s6_input_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&pktsock_thread);
}

// vi: set ts=8 sw=4 noet :
