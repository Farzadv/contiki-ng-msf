/*
 * Copyright (c) 2019, Inria.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "contiki.h"
#include "contiki-net.h"

#include "net/mac/tsch/sixtop/sixtop.h"
#include "services/msf/msf.h"
#include "services/shell/serial-shell.h"
#include "net/ipv6/simple-udp.h"

#include "lib/sensors.h"

#include "sys/log.h"
#define LOG_MODULE "APP"
#define LOG_LEVEL LOG_LEVEL_DBG



static struct simple_udp_connection udp_conn1;
static struct simple_udp_connection udp_conn2;
int j = 0;
int k = 0;
static uint16_t count_app1 = 1;
static uint16_t count_app2 = 300;
static int first_rpl_join = 0;
/*---------------------------------------------------------------------------*/
PROCESS(msf_node_process_app1, "MSF node app 1");
PROCESS(msf_node_process_app2, "MSF node app 2");
AUTOSTART_PROCESSES(&msf_node_process_app1, &msf_node_process_app2);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(msf_node_process_app1, ev, data)
{
  static struct etimer et;
  static uint8_t msg[2*LINKADDR_SIZE+6];
  uip_ipaddr_t root_ipaddr;

  PROCESS_BEGIN();

  serial_shell_init();
  sixtop_add_sf(&msf);
  LOG_INFO("APP1_SEND_INTERVAL: %u\n", APP1_SEND_INTERVAL);
  if(APP1_SEND_INTERVAL > 0) {
    etimer_set(&et, APP1_SEND_INTERVAL);

    if(simple_udp_register(&udp_conn1, APP1_CLIENT_UDP_PORT, NULL,
                      SERVER_UDP_PORT, udp_rx_callback) == 0) {
      LOG_ERR("CRITICAL ERROR: socket initialization failed\n");
    } else {
      while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);

        
	for(j = 0; j< LINKADDR_SIZE; ++j){
	  msg[j] = linkaddr_node_addr.u8[j];
        }
        
      
        msg[4] =  count_app1 & 0xFF;
        msg[5] = (count_app1 >> 8)& 0xFF;
        msg[6] =  tsch_current_asn.ls4b & 0xFF;
        msg[7] = (tsch_current_asn.ls4b >> 8)& 0xFF;
        msg[8] = (tsch_current_asn.ls4b >> 16)& 0xFF;
        msg[9] = (tsch_current_asn.ls4b >> 24)& 0xFF;
        
        
        
        if(NETSTACK_ROUTING.node_is_reachable() &&
           NETSTACK_ROUTING.get_root_ipaddr(&root_ipaddr) &&
           msf_is_ready()) {
           
           if(first_rpl_join == 0) {          
             first_rpl_join = 1;
           }
           
           msg[2] =  root_ipaddr.u8[14];
           msg[3] =  root_ipaddr.u8[15];
           
           simple_udp_sendto(&udp_conn1, msg, sizeof(msg), &root_ipaddr);
           //LOG_DBG("send app data\n");
           
        }
        
        if(first_rpl_join) {
          count_app1++;
        }
      }
    }
  } else {
    /* nothing to do */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(msf_node_process_app2, ev, data)
{
  static struct etimer et;
  static uint8_t msg[2*LINKADDR_SIZE+6];
  uip_ipaddr_t root_ipaddr;

  PROCESS_BEGIN();

  //serial_shell_init();
  //sixtop_add_sf(&msf);
  LOG_INFO("APP2_SEND_INTERVAL: %u\n", APP2_SEND_INTERVAL);
  if(APP2_SEND_INTERVAL > 0) {
    etimer_set(&et, APP2_SEND_INTERVAL);

    if(simple_udp_register(&udp_conn2, APP2_CLIENT_UDP_PORT, NULL,
                      SERVER_UDP_PORT, udp_rx_callback) == 0) {
      LOG_ERR("CRITICAL ERROR: socket initialization failed\n");
    } else {
      while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);

        
	for(k = 0; k< LINKADDR_SIZE; ++k){
	  msg[k] = linkaddr_node_addr.u8[k];
        }
      
      
        msg[4] =  count_app2 & 0xFF;
        msg[5] = (count_app2 >> 8)& 0xFF;
        msg[6] =  tsch_current_asn.ls4b & 0xFF;
        msg[7] = (tsch_current_asn.ls4b >> 8)& 0xFF;
        msg[8] = (tsch_current_asn.ls4b >> 16)& 0xFF;
        msg[9] = (tsch_current_asn.ls4b >> 24)& 0xFF;
        
        
        
        if(NETSTACK_ROUTING.node_is_reachable() &&
           NETSTACK_ROUTING.get_root_ipaddr(&root_ipaddr) &&
           msf_is_ready() && (count_app1>300)) {
           
           msg[2] =  root_ipaddr.u8[14];
           msg[3] =  root_ipaddr.u8[15];
           
           simple_udp_sendto(&udp_conn2, msg, sizeof(msg), &root_ipaddr);
           //LOG_DBG("send app data\n");
           
        }
        
        if(count_app1>300) {
          count_app2++;
        }
      }
    }
  } else {
    /* nothing to do */
  }

  PROCESS_END();
}

