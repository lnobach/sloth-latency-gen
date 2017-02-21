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

#include <libxml/xmlreader.h>

#define SMALLEST_PKT 64

uint64_t conf_delay_usec = 0;
uint64_t conf_qlen = 0;
uint8_t conf_coremode = 0; // (1: 1core-mode, 2: 2core-mode)
uint8_t conf_ignoremacs = 0; // Set to != 0 if MAC address interface assignment shall be ignored.

void eat_whitespace_left(char* str) {
	char* ptr = str + strlen(str) - 1;
	while (str <= ptr && isspace(*ptr)) {
		//nom nom
		*ptr = 0;
		ptr--;
	}
}

static int64_t si_atoi(const char* str) {
	char str2[50];
	strncpy(str2, str, 50);
	eat_whitespace_left(str2);	
	int64_t mult = 0;
	int len = strlen(str2);
	if (len <= 0) return 0;
	char lastchar = str2[len-1];
	switch (lastchar) {
		case 'k': case 'K':	mult=1024l; break;
		case 'M':		mult=1024l*1024l; break;
		case 'G':		mult=1024l*1024l*1024l; break;
		case 'T':		mult=1024l*1024l*1024l*1024l; break;
	}
	return atoi(str2)*mult;

}


//Returns zero if failure.
uint32_t getQLenFromConf(const char* value_str, const char* qlen_method) {

	int64_t value = si_atoi(value_str);

	if (value <= 0) {
		printf("[CONF] 'qLenValue' must be positive.\n");
		return 0;  
	}

	if (strcmp(qlen_method, "direct") == 0) {
		printf("[CONF] Set qlen value directly to %li.\n", value);
		return value;
	}

	if (strcmp(qlen_method, "maxmem") == 0) {

		int64_t pre = value/MBUF_SIZE/LATGEN_BURSTLEN - ADDL_MBUF_ELEM_CNT;
		if (pre <= 0) {
			printf("[CONF] With the specified maxmem, allowed queue size would be 0. Too small. (%li).\n", pre);
		}
		uint32_t retrn = pre;

		printf("[CONF] Setting qlen value to %u, as by maxmem=%lu.\n", retrn, value);
		return retrn;
	}

	if (strcmp(qlen_method, "maxbw") == 0) {

		if (conf_delay_usec <= 0) {
			printf("[CONF] 'latency' tag must precede any interface definition using 'maxbw'.\n");
			return 0;
		}

		int64_t pre = value*conf_delay_usec*8/1000000/SMALLEST_PKT/LATGEN_BURSTLEN;	//This may be inexact, it only works with smallest packets 
												//if the bursts are completely utilized.

		if (pre <= 0) {
			printf("[CONF] With the specified maxbw, allowed queue size would be 0. Too small. (%li).\n", pre);
			return 0;
		}
		uint32_t retrn = pre;

		printf("[CONF] Set qlen value to %u, as by maxbw=%lu.\n", retrn, value);
		return retrn;
	}
				
}

int slt_load_config() {
    	xmlTextReaderPtr reader;
   	int status;

	reader = xmlReaderForFile("latgen.xml", NULL, 0);
   	if (reader == NULL) {
		printf("[CONF] Unable to open config file.\n");
		return -1;
	}
       	status = xmlTextReaderRead(reader);
        while (status == 1) {


        	const xmlChar *name = xmlTextReaderConstName(reader);
		//printf("Parsing name '%s'\n", name);
		if (name == NULL) continue;

		if (strcmp(name, "ignoremacs") == 0) {
		  conf_ignoremacs = 1;
		}
		
		if (strcmp(name, "interface") == 0) {
			
			const xmlChar* value = xmlTextReaderGetAttribute(reader, "address");
			if (value == NULL) {
				printf("[CONF] An interface must have the address attribute.\n");
				return -2;            	
				//Parse error
			}

			devs_macs[devs_macs_len] = malloc(strlen(value)+1);
			strcpy(devs_macs[devs_macs_len], value);

			printf("[CONF] Found interface: name='%s', value='%s'\n", name, value);

			devs_macs_len++;

		
		}

		if (strcmp(name, "latency") == 0) {
			
			const xmlChar* value = xmlTextReaderGetAttribute(reader, "usec");

			if (value == NULL) {
				printf("[CONF] 'latency' must specify the 'usec' attribute.\n");
				return -3;            	
				//Parse error
			}

			conf_delay_usec = atoi(value);
			if (value <= 0) {
				printf("[CONF]  Must specify a latency greater equal 1 usec.\n");
				return -3;  
			}

			printf("[CONF] Set latency to %lu\n", conf_delay_usec);
		
		}

		if (strcmp(name, "coremode") == 0) {
			
			const xmlChar* value = xmlTextReaderGetAttribute(reader, "value");

			if (value == NULL) {
				printf("[CONF] 'latency' must specify the 'usec' attribute.\n");
				return -3;            	
				//Parse error
			}

			conf_coremode = atoi(value);

			printf("[CONF] Set coremode to %i\n", conf_coremode);
		
		}

		if (strcmp(name, "qlen") == 0) {
			
			const xmlChar* qlen_method = xmlTextReaderGetAttribute(reader, "method");
			const xmlChar* qlen_value_str = xmlTextReaderGetAttribute(reader, "value");
			if (qlen_method == NULL) {
				printf("[CONF]  'qlen' must specify the 'qLenMethod' attribute.\n");
				return -2;            	
				//Parse error
			}

			if (qlen_value_str == NULL) {
				printf("[CONF]  'qlen' must specify the 'qLenValue' attribute.\n");
				return -2;            	
				//Parse error
			}

			conf_qlen = getQLenFromConf(qlen_value_str, qlen_method);
			if (conf_qlen == 0) {
				printf("[CONF]  Error calculating qlen value\n");
				return -3;
			}

			printf("[CONF]  Estimated mempool size required: %lu bytes (must be squeezed into one of the hugepages per interface)\n", 
				getMempoolSizeForInterface(conf_qlen)*MBUF_SIZE);
		
		}

       		status = xmlTextReaderRead(reader);
        }
       	xmlFreeTextReader(reader);
        if (status != 0) {
		printf("[CONF] Parse error in XML document.\n");
		return -2;            	
		//Parse error
    	}

	if (conf_delay_usec <= 0) {
		printf("[CONF] Delay element not specified in XML configuration.\n");
		return -3;
	}

	if (conf_qlen <= 0) {
		printf("[CONF] qlen element not specified in XML configuration.\n");
		return -4;
	}

	if (conf_coremode != 1 && conf_coremode != 2) {
		printf("[CONF] Coremode must be set to either 1 or 2 in the configuration.\n");
		return -4;
	}


	return status;

}



