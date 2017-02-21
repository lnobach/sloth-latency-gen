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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_config.h>
#include <rte_common.h>
//#include <rte_byteorder.h>
//#include <rte_log.h>
//#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
//#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
//#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
//#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
//#include <rte_string_fns.h>
#include "sloth_main.h"


uint8_t lcores_count;	//Maximum index of available lcores.
uint8_t available_lcores[RTE_MAX_LCORE];	//Map: available-lcore-id -> general lcore_id
uint8_t lcores_to_availables[RTE_MAX_LCORE];	//Map: general lcore_id -> available-lcore-id
latgen_lcore_info_t* latgen_lcore_info[RTE_MAX_LCORE];	//Struct: available-lcore-id -> some infos.
latgen_interface_info_t* latgen_interfaces[RTE_MAX_IFACE];

uint8_t latgen_if_to_portid[255];	//Maps the insm device number (according to devs_macs) to the port_id

char* devs_macs[255];		//All the mac addresses to look up in the available intfs.
uint8_t devs_macs_len = 0;
uint8_t num_devs = 0;

uint64_t conf_delay_cycles;


struct rte_ring* waitbufs[MAX_RINGS];
latgen_stat_t stats[MAX_RINGS];


void slt_printHexArray(char* array, uint16_t array_length) {
	printf("[");
	int i;
	for (i=0;i < array_length;i++) {
    		if (i == array_length-1) printf("%02X", (uint8_t)array[i]);
		else printf("%02X ", (uint8_t)array[i]);
		
	}
	printf("]");
}

void slt_printIntArray(uint8_t* array, uint8_t array_length) {
	printf("[");
	int i;
	for (i=0;i < array_length;i++) {
    		if (i == array_length-1) printf("%i",array[i]);
		else printf("%i,",(uint8_t)array[i]);
		
	}
	printf("]");
}

int slt_ethaddr_to_string(char* str2write, const struct ether_addr* eth_addr) {

	return sprintf (str2write, "%02x:%02x:%02x:%02x:%02x:%02x",
		eth_addr->addr_bytes[0],
		eth_addr->addr_bytes[1],
		eth_addr->addr_bytes[2],
		eth_addr->addr_bytes[3],
		eth_addr->addr_bytes[4],
		eth_addr->addr_bytes[5]);
}

int slt_ethertype_to_string(char* str2write, const uint16_t ether_type) {

	return sprintf (str2write, "0x%02x%02x", ether_type & 0x00ff, ether_type >> 8);
}

uint64_t getMempoolSizeForInterface(uint64_t waitq_len) {

	//For future extension
	return waitq_len*LATGEN_BURSTLEN + ADDL_MBUF_ELEM_CNT;

}

uint64_t nextpow2(uint64_t val) {
	if (val <= 1) return 0;
	uint8_t i=0;
	val--;
	while (val >>= 1 != 0) i++;
	return 1 << i+1;

}


#include "sloth_config.c"
#include "sloth_dataplane.c"

int main(int argc, char** argv) {

	int status;

	status = slt_load_config();

	if (status != 0) {
		printf("[CONF] Error while loading config file.\n");
		exit(status);
	}


	static struct rte_eth_conf default_ethconf = {
		.link_speeds = ETH_LINK_SPEED_AUTONEG,
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,
			.max_rx_pkt_len = ETHER_MAX_VLAN_FRAME_LEN,
			.split_hdr_size = 0,
			.header_split = 0,
			.hw_ip_checksum = 0,
			.hw_vlan_filter = 0,
			.hw_vlan_strip = 0,
			.hw_vlan_extend = 0,
			.jumbo_frame = 0,
			.hw_strip_crc = 0,
			.enable_scatter = 0,
			.enable_lro = 0,
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
			.hw_vlan_reject_tagged = 0,
			.hw_vlan_reject_untagged = 0,
			.hw_vlan_insert_pvid = 0,
		},
		.lpbk_mode = 0,
		.rx_adv_conf = {
			.rss_conf = {		//Receive Side Scaling.
				.rss_key = NULL,
				.rss_key_len = 0,
				.rss_hf = 0,
			},
		},
	};

	/*
	static struct rte_eth_conf default_ethconf = {
		.link_speed = 0,
		.link_duplex = 0,
		.rxmode = {
			.mq_mode = ETH_MQ_RX_NONE,
			.max_rx_pkt_len = ETHER_MAX_VLAN_FRAME_LEN,
			.split_hdr_size = 0,
			.header_split = 0,
			.hw_ip_checksum = 0,
			.hw_vlan_filter = 0,
			.hw_vlan_strip = 0,
			.hw_vlan_extend = 0,
			.jumbo_frame = 0,
			.hw_strip_crc = 0,
			.enable_scatter = 0,
			.enable_lro = 0,
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
			.hw_vlan_reject_tagged = 0,
			.hw_vlan_reject_untagged = 0,
			.hw_vlan_insert_pvid = 0,
		},
		.lpbk_mode = 0,
		.rx_adv_conf = {
			.rss_conf = {		//Receive Side Scaling.
				.rss_key = NULL,
				.rss_key_len = 0,
				.rss_hf = 0,
			},
		},
	};
	*/
		
	static const struct rte_eth_rxconf rx_conf = {
		.rx_thresh = {
			.pthresh = 8,	//prefetch
			.hthresh = 8,	//host
			.wthresh = 4	//write-back
		},
		.rx_free_thresh = 32,
	};	

	static struct rte_eth_txconf tx_conf = {
		.tx_thresh = {
			.pthresh = 32,
			.hthresh = 0,
			.wthresh = 0
		},
		.tx_free_thresh = 0,
		.tx_rs_thresh = 32,
		.txq_flags = (ETH_TXQ_FLAGS_NOMULTSEGS |
			ETH_TXQ_FLAGS_NOVLANOFFL |
			ETH_TXQ_FLAGS_NOXSUMSCTP |
			ETH_TXQ_FLAGS_NOXSUMUDP |
			ETH_TXQ_FLAGS_NOXSUMTCP)

	};


	//===== END CONFIG

	printf("[[I]] Starting DPDK EAL...\n");
	
	status = rte_eal_init(argc, argv);
	if (status < 0) {
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	}

	printf("[[I]] Checking for ports...\n");

	uint8_t rte_devcount = rte_eth_dev_count();

	if (rte_devcount == 0) {
		rte_exit(EXIT_FAILURE, "No probed ethernet devices\n");
		printf("[[I]] No devs, exiting\n");
	}

	printf("[[I]] Found %i net devices.\n", rte_devcount);
	



	//Get the CPU IDs

	
	uint8_t i;
	for (i=0; i < RTE_MAX_LCORE; i++) {
		
		//printf("Core: %i, isEnabled: %i\n", i, rte_lcore_is_enabled(i)); 
		if (rte_lcore_is_enabled(i) && !(rte_get_master_lcore() == i)) {
			//printf("  Adding.\n");
			available_lcores[lcores_count] = i;
			lcores_to_availables[i] = lcores_count++;
		}
	}

	slt_printIntArray(available_lcores, 30);
	slt_printIntArray(lcores_to_availables, 30);

	printf("[[I]] Forwarding CPUs: ");
	slt_printIntArray(available_lcores, lcores_count);
	printf(". Master Core (not forwarding): %i\n", rte_get_master_lcore());

	if (lcores_count < rte_devcount) {
		rte_exit(EXIT_FAILURE, "slt requires at least as many cores as it has interfaces to do the job. Cores: %i, Devices: %i\n", lcores_count, rte_devcount);
	}

	argc -= status;
	argv += status;
	
	//Here, command parsing would come.

	conf_delay_cycles = conf_delay_usec * rte_get_timer_hz() / 1E6;


	//We initialize the lcore struct array, except the mempool

	for (i=0; i < lcores_count; i++) {
		latgen_lcore_info[i] = rte_zmalloc("slt: struct latgen_lcore_info", sizeof(latgen_lcore_info_t), CACHE_LINE_SIZE);
		if (latgen_lcore_info[i] == NULL) rte_exit(EXIT_FAILURE, "Could not alloc mem for latgen_lcore_info_t %i", i);
		latgen_lcore_info[i]->lcore_id = i;
	}


	//Find the interior and exterior interface by MAC address...
	printf("[[I]] Associating devices...\n");

	for(i=0; i<devs_macs_len; i++) {
		latgen_if_to_portid[i] = UINT8_UNDEF;	//FIXME: this seems to be a dupe: latgen_interfaces[j] might be the same!
		latgen_interfaces[i] = rte_zmalloc("slt: struct latgen_interface_info", sizeof(latgen_interface_info_t), CACHE_LINE_SIZE);
		latgen_interfaces[i]->latgen_intf_id = i;
	}
	
	printf("[[I]] Ignoremacs set to %u\n", conf_ignoremacs);
	
	num_devs = (conf_ignoremacs == 0)?devs_macs_len:rte_devcount;

	for(i=0; i<rte_devcount; i++) {
		struct ether_addr found_dev_macaddr;
		rte_eth_macaddr_get(i, &found_dev_macaddr);
		char ether_addr_string[20]; 
		slt_ethaddr_to_string(ether_addr_string, &found_dev_macaddr);
		printf("  MAC Address found: %s\n", ether_addr_string);
		int j;
		if (conf_ignoremacs == 0) {
		  for (j=0; j < devs_macs_len; j++) {
			  if (strcmp(ether_addr_string, devs_macs[j]) == 0) {
				  latgen_if_to_portid[j] = i;
				  latgen_interfaces[j]->rte_port_id = i;
				  memcpy(&latgen_interfaces[j]->intf_mac, &found_dev_macaddr, sizeof(struct ether_addr)); //The space is already alloced.
				  //printf("Size of ether_addr struct is: %lu", sizeof(struct ether_addr));
				  printf(" (insm interface id: %i)\n", j);
				  break;
			  }
		  }
		} else {
		  latgen_interfaces[i]->rte_port_id = i;
		  latgen_if_to_portid[i] = i;
		  printf(" (insm interface id: %i, 1-to-1 assignment, MACS ignored.)\n", i);
		}
	}

	for (i=0; i < num_devs; i++) {
		if (latgen_if_to_portid[i] == UINT8_UNDEF)
			rte_exit(EXIT_FAILURE, "Specified device (with corresponding MAC address) was not found for interface ID %i.\n", i);
		//Configure each device with one Tx/Rx queue and a default conf.
		status = rte_eth_dev_configure(latgen_if_to_portid[i], 1, 1, &default_ethconf);
		if (status < 0) {
			rte_exit(EXIT_FAILURE, "Could not configure ethernet device %i.\n", i);
		}
	}


	//Set up a mempool for packets

	printf("[[I]] Configuring mempool...\n");

	for (i=0; i < num_devs; i++) {

		//We map interface i's queue (regarding insm's devs_macs index) to lcore i's mempool.
		//The mempools of the other cores (if we have more cores than IFs) remain NULL!

		uint8_t lcore = available_lcores[i];
		uint8_t socket = rte_lcore_to_socket_id(lcore);
		uint8_t portid = latgen_if_to_portid[i];

		printf("[[I]] Configuring interface %i, lcore=%u, socket=%u, portid=%u", i, lcore, socket, portid);

		char mem_label[64];
		snprintf(mem_label, sizeof(mem_label), "mbuf_pool_%i", i);

		uint64_t mempool_sz = getMempoolSizeForInterface(conf_qlen);

		printf("[[I]] MBUF_SIZE: %lu, ETHER_MAX_VLAN_FRAME_LEN: %u\n", 
			MBUF_SIZE, 
			ETHER_MAX_VLAN_FRAME_LEN);

		latgen_lcore_info[i]->mempool = rte_mempool_create(mem_label,
			mempool_sz, //The number of elements in the mempool. n = (2^q - 1).
			MBUF_SIZE,	//Size of each element
			MEMPOOL_CACHE_SIZE,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL,
			rte_pktmbuf_init, NULL,
			socket, 0);				//TODO: Optimize for NUMA.

		if (latgen_lcore_info[i]->mempool == NULL)
			rte_exit(EXIT_FAILURE, "MBuf creation failed for interface %i\n", i);

		printf("[[I]] Preparing queues...\n");

		status = rte_eth_rx_queue_setup(portid, 
			0, 
			SLT_RX_DESC_DEFAULT,
			socket,
			&rx_conf,
			latgen_lcore_info[i]->mempool);

		if (status < 0) rte_exit(EXIT_FAILURE, "Failed to set up interior TX queue\n");
	
		status = rte_eth_tx_queue_setup(portid, 
			0, 
			SLT_TX_DESC_DEFAULT,
			socket,
			&tx_conf);

		if (status < 0) rte_exit(EXIT_FAILURE, "Failed to set up interior RX queue\n");

		status = rte_eth_dev_start(portid);
		if (status < 0) rte_exit(EXIT_FAILURE, "Failed to fire up interface\n");

		rte_eth_promiscuous_enable(portid);

	}

	printf("[[I]] Creating waitbufs...\n");

	uint8_t waitbufNo = 8; //TODO: adapt this to number of interfaces, and check if num of interfaces are even, and if there are enough lcores.

	uint64_t conf_qlen_aligned = nextpow2(conf_qlen);	//FIXME: Optimistic, this might not be safe for the mbuf if you send a lot of small pkts (<256 bytes)!
	printf("[[I]] Aligning waitbuf from %lu to %lu\n", conf_qlen, conf_qlen_aligned);

	for (int i=0; i<waitbufNo; i++) {

		char waitbuf_descr[20];
		snprintf(waitbuf_descr, 20, "WAITBUF_RING_%u", i);

		waitbufs[i] = rte_ring_create(waitbuf_descr, conf_qlen_aligned, 0, RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (waitbufs[i] == NULL) {
			printf("Ring could not be created, errno=%i", rte_errno);
			exit(123);
		}		

	}

	memset(&stats, 0, sizeof(latgen_stat_t)*MAX_RINGS);	//Initialize the stats array

	//Wait for ports up
	printf("[[I]] Waiting for ports up...\n");

	struct rte_eth_link link;
	int allUp = 0;
	while (!allUp) {
		int j;
		for(j=0; j<num_devs; j++) {
			memset(&link, 0, sizeof(struct rte_eth_link));
			rte_eth_link_get_nowait(latgen_if_to_portid[j], &link);
			printf("  Link %i (rte id %i) ", j, latgen_if_to_portid[j]);
			if (link.link_status) {
				printf("up\n");
				if (j == num_devs-1) {
					allUp = 1;
					break;
				}
			} else {
				printf("down\n");
				printf(" Still waiting for interface %i\n", j);
				break;
			}
		}
		rte_delay_ms(200);
	}

	printf("[[I]] Launching data plane cores...\n");
	rte_eal_mp_remote_launch(do_dataplane_job, NULL, CALL_MASTER);
	uint8_t my_lcore;
	RTE_LCORE_FOREACH_SLAVE(my_lcore) {
		if (rte_eal_wait_lcore(my_lcore) < 0)
			return -1;
	}


	printf("[[I]] Go on.\n");


	return 0;

}

latgen_interface_info_t* slt_getInterfaceInfo(uint8_t latgen_interface_id) {
	return latgen_interfaces[latgen_interface_id];
}

latgen_lcore_info_t* slt_getCurrentLCoreInfo() {
	return latgen_lcore_info[lcores_to_availables[rte_lcore_id()]];
}






