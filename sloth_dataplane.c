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

#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_errno.h>
#include <unistd.h>

//#define DEBUG_SINGLE_PACKETS

#define STATS_INTERVAL_SEC 5



typedef struct {
	struct rte_mbuf* brst[LATGEN_BURSTLEN]; /**< Mbuf of pkt */
	int len;
	uint64_t ts;       /**< Software timestamp of pkt */
} brst_ts;

static int do_dataplane_job(__attribute__((unused)) void *dummy) {

	uint8_t lcore_id = rte_lcore_id();
	//RTE_LOG(INFO, USER1, "Called do-dataplane-job, lcore=%i\n", lcore_id);
	uint8_t slt_core_index = lcores_to_availables[lcore_id];
	int is_consumer = slt_core_index % 2;	// 1 if consumer, 0 if producer, ONLY needed for 2core-mode.
	uint8_t slt_dev_id = slt_core_index/conf_coremode;
	uint8_t rte_port_id = latgen_if_to_portid[slt_dev_id];

	if (!rte_lcore_is_enabled(lcore_id)) {
		RTE_LOG(INFO, USER1, "Lcore %u is not enabled. It has nothing to do here. Exiting thread.\n", lcore_id);
		return 1;
	}

	if (slt_core_index >= num_devs*conf_coremode) {
		//We only let lcores with an interface do something here.
		RTE_LOG(INFO, USER1, "Lcore %u is not assigned to an interface, it has nothing to do, exiting in this thread.\n", lcore_id);
		return 0;
	}

	


	if (rte_get_master_lcore() == lcore_id) {

		while(1) {

			printf("\n===== STATS ======\n");
			
			int i;
			struct ether_addr found_dev_macaddr;
			for (i=0; i < num_devs; i++) {

				uint8_t i_src;
				if (i%2 == 1)	i_src=i-1;
				else i_src=i+1;


				rte_eth_macaddr_get(latgen_if_to_portid[i_src], &found_dev_macaddr);
				char src_ether_addr_string[20]; 
				slt_ethaddr_to_string(src_ether_addr_string, &found_dev_macaddr);

				rte_eth_macaddr_get(latgen_if_to_portid[i], &found_dev_macaddr);
				char ether_addr_string[20]; 
				slt_ethaddr_to_string(ether_addr_string, &found_dev_macaddr);

				latgen_stat_t* stat = &stats[i];

				printf(" %u(%s) -> %u(%s), pkts=%lu, drops_q=%lu drops_out=%lu, qlen=%u"
#ifdef EXTENDED_STATS_RINGCOUNT
					", max_qlen=%lu" 
#endif
#ifdef EXTENDED_STATS_GT1BURSTLEN
					", gt1burstlen=%lu" 
#endif
					"\n", 
					i_src,
					src_ether_addr_string,
					i,
					ether_addr_string,
					stat->packets,
					stat->drops_q,
					stat->drops_out,
					rte_ring_count(waitbufs[i])
#ifdef EXTENDED_STATS_RINGCOUNT
					, stat->max_qlen
#endif
#ifdef EXTENDED_STATS_GT1BURSTLEN
					, stat->gt1burstlen
#endif
				);
			}

			sleep(STATS_INTERVAL_SEC);
		}

	} else {
		printf("Started lcore %u, slt_core %u, is_consumer=%i, slt_dev_id=%u, rte_port_id=%u \n", 
			lcore_id, slt_core_index, is_consumer, slt_dev_id, rte_port_id);

		uint8_t slt_target_dev_id;
		if (slt_dev_id%2 == 1)	slt_target_dev_id=slt_dev_id-1;
		else slt_target_dev_id=slt_dev_id+1;

		if (conf_coremode == 2) {

			if (is_consumer == 1) {
				//printf("Initializing consumer thread...\n");
				brst_ts* last_burst_dequeued = NULL;

				while(1) {
					if (last_burst_dequeued != NULL) {
						while (rte_get_timer_cycles() < last_burst_dequeued->ts + conf_delay_cycles);
						int retrn = rte_eth_tx_burst(rte_port_id, 0, last_burst_dequeued->brst, last_burst_dequeued->len);
#ifdef DEBUG_SINGLE_PACKETS
						printf("%u -> (RTE: %u) %u bytes\n",  slt_dev_id, rte_port_id, last_burst_dequeued->brst[0]->pkt_len);
#endif
						rte_free(last_burst_dequeued);

						if (unlikely(retrn < last_burst_dequeued->len)) {

							stats[slt_dev_id].drops_out += last_burst_dequeued->len - retrn;

							do {
								rte_pktmbuf_free(last_burst_dequeued->brst[retrn]);
							} while (++retrn < last_burst_dequeued->len);

						}

						stats[slt_dev_id].packets += retrn;

					}
			
					int status = rte_ring_sc_dequeue(waitbufs[slt_dev_id], (void**)&last_burst_dequeued);
					if (status == -ENOENT) {
						last_burst_dequeued = NULL;
					}

				}

			} else {

				//printf("Initializing producer thread...\n");

				//printf("SLT target dev id=%u\n", slt_target_dev_id);

				while(1) {

					brst_ts* brst2Enq = rte_malloc("brst_ts", sizeof(brst_ts), 0);
					brst2Enq->len = rte_eth_rx_burst(rte_port_id, 0, brst2Enq->brst, LATGEN_BURSTLEN);					
					brst2Enq->ts = rte_get_timer_cycles();
					if (brst2Enq->len > 0) {
#ifdef DEBUG_SINGLE_PACKETS
						printf("(RTE %u, SLT %u) -> %u\n", rte_port_id, slt_dev_id, slt_target_dev_id);
#endif
						int status = rte_ring_sp_enqueue(waitbufs[slt_target_dev_id], brst2Enq);
						if (unlikely(status == -ENOBUFS)) {
							for (int i=0; i<brst2Enq->len; i++) {
								rte_pktmbuf_free(brst2Enq->brst[i]);
							}
							rte_free(brst2Enq);
							stats[slt_target_dev_id].drops_q += brst2Enq->len;
						}
#ifdef EXTENDED_STATS_GT1BURSTLEN
						if (brst2Enq->len > 1) stats[slt_target_dev_id].gt1burstlen++;
#endif
#ifdef EXTENDED_STATS_RINGCOUNT
						uint64_t current_qlen = rte_ring_count(waitbufs[slt_target_dev_id]);
						if (unlikely(current_qlen > stats[slt_target_dev_id].max_qlen)) stats[slt_target_dev_id].max_qlen = current_qlen;
#endif
					} else {
						rte_free(brst2Enq);
					}

				}
			}
		} else {	//1core mode

			uint8_t rte_target_port_id = latgen_if_to_portid[slt_target_dev_id];

			brst_ts* last_burst_dequeued = NULL;
#ifdef TRAFFICSHAPING
			uint64_t waitUntil = -1;
#endif

			while(1) {
#ifdef TRAFFICSHAPING
				while (rte_get_timer_cycles() < waitUntil);
#endif				
				brst_ts* brst2Enq = rte_malloc("brst_ts", sizeof(brst_ts), 0);
				brst2Enq->ts = rte_get_timer_cycles();
				brst2Enq->len = rte_eth_rx_burst(rte_port_id, 0, brst2Enq->brst, LATGEN_BURSTLEN);
				
#ifdef TRAFFICSHAPING
				uint64_t bytes = 0;
				int i;
				for (i=0; i<brst2Enq->len; i++) bytes += rte_pktmbuf_pkt_len(brst2Enq->brst[i]);
				waitUntil = brst2Enq->ts+conf_microcycles_per_byte*bytes/1000000;
#endif
				if (brst2Enq->len > 0) {
#ifdef DEBUG_SINGLE_PACKETS
					printf("(RTE %u, SLT %u) -> %u\n", rte_port_id, slt_dev_id, slt_target_dev_id);
#endif
					int status = rte_ring_sp_enqueue(waitbufs[slt_target_dev_id], brst2Enq);
					if (unlikely(status == -ENOBUFS)) {
						for (int i=0; i<brst2Enq->len; i++) {
							rte_pktmbuf_free(brst2Enq->brst[i]);
						}
						rte_free(brst2Enq);
						stats[slt_target_dev_id].drops_q += brst2Enq->len;
					}
#ifdef EXTENDED_STATS_RINGCOUNT
					uint64_t current_qlen = rte_ring_count(waitbufs[slt_target_dev_id]);
					if (unlikely(current_qlen > stats[slt_target_dev_id].max_qlen)) stats[slt_target_dev_id].max_qlen = current_qlen;
#endif
#ifdef EXTENDED_STATS_GT1BURSTLEN
					if (brst2Enq->len > 1) stats[slt_target_dev_id].gt1burstlen++;
#endif
				} else {
					rte_free(brst2Enq);
				}

				if (last_burst_dequeued == NULL) {
					int status = rte_ring_sc_dequeue(waitbufs[slt_target_dev_id], (void**)&last_burst_dequeued);
					/*if (status == -ENOENT) {
						last_burst_dequeued = NULL;
					}*/
				}

				while (last_burst_dequeued != NULL && rte_get_timer_cycles() >= last_burst_dequeued->ts + conf_delay_cycles) {
					int retrn = rte_eth_tx_burst(rte_target_port_id, 0, last_burst_dequeued->brst, last_burst_dequeued->len);
#ifdef DEBUG_SINGLE_PACKETS
					printf("%u -> (RTE: %u)...\n",  slt_dev_id, rte_port_id);
#endif
					rte_free(last_burst_dequeued);

					if (unlikely(retrn < last_burst_dequeued->len)) {

						stats[slt_target_dev_id].drops_out += last_burst_dequeued->len - retrn;

						do {
							rte_pktmbuf_free(last_burst_dequeued->brst[retrn]);
						} while (++retrn < last_burst_dequeued->len);

					}

					stats[slt_target_dev_id].packets += retrn;
					int status = rte_ring_sc_dequeue(waitbufs[slt_target_dev_id], (void**)&last_burst_dequeued);
					if (status == -ENOENT) {
						last_burst_dequeued = NULL;
					}
				}

			}

		}
	}
	
	return 0;


}


