/*
 *   ==========================================
 *   
 *   Sloth - DPDK-Based, High-Performance Latency Generator
 *   
 *   Author: Leonhard Nobach
 *
 *   ==========================================
 *   
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Technische Universität Darmstadt, Fachgebiet PS
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Technische Universität Darmstadt nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *   ==========================================
 */

#include <rte_ether.h>

//======= Pre-Definitions

#define SLT_RX_DESC_DEFAULT 256
#define SLT_TX_DESC_DEFAULT 512
#define MAX_SIZE_BURST 32
#define MEMPOOL_CACHE_SIZE 256

//======= Definitions

#define CACHE_LINE_SIZE		64
#define UINT8_UNDEF		255
#define SLT_CORE_IDS_MAX	255
#define RTE_MAX_IFACE		255

#define MBUF_SIZE (ETHER_MAX_VLAN_FRAME_LEN + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

#define ADDL_MBUF_ELEM_CNT (SLT_RX_DESC_DEFAULT + SLT_TX_DESC_DEFAULT + MAX_SIZE_BURST + MEMPOOL_CACHE_SIZE) //Per-interface

#define MAX_RINGS	128
#define LATGEN_BURSTLEN	16

//#define EXTENDED_STATS_RINGCOUNT
#define EXTENDED_STATS_GT1BURSTLEN
//#define TRAFFICSHAPING

typedef struct {
	char* args[1];
	int delayms;
	int qlen_type; //0: qlen(direct), 1: maxmem, 2: bandwidth
	int qlen_value;
} args_t;

typedef struct {
	struct rte_mempool* mempool; /**< Mempool to be used */
	uint8_t  lcore_id;       /**< Our own lcore ID. Redundant to info in fwd_available_lcores[..] */
} latgen_lcore_info_t;

typedef struct {
	uint8_t rte_port_id;
	uint8_t latgen_intf_id;
	struct ether_addr intf_mac;
} latgen_interface_info_t;

typedef struct {
	uint64_t packets;
	uint64_t drops_q;
	uint64_t drops_out;
#ifdef EXTENDED_STATS_RINGCOUNT
	uint64_t max_qlen;
#endif
#ifdef EXTENDED_STATS_GT1BURSTLEN
	uint64_t gt1burstlen;
#endif
} latgen_stat_t;






















