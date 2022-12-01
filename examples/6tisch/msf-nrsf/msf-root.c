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

#include "net/routing/routing.h"
#include "net/mac/tsch/sixtop/sixtop.h"
#include "services/msf/msf.h"
#include "services/msf/nrsf/nrsf.h"
#include "services/shell/serial-shell.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/sdn-statis.h"
#include "net/routing/rpl-lite/rpl.h"


#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO


void print_ideal_cell_num(void);


PROCESS(msf_root_process, "MSF root");
AUTOSTART_PROCESSES(&msf_root_process);

static struct simple_udp_connection udp_conn;
static int print_schedule = 0;
//static linkaddr_t ter_dest = { { 0, 4 } };
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

  static uint32_t sent_asn;
  static uint16_t seq_num;
  linkaddr_t src_addr;
  linkaddr_t dest_addr;
  
  src_addr.u8[0] = (uint8_t)data[0];
  src_addr.u8[1] = (uint8_t)data[1];
  
  dest_addr.u8[0] = (uint8_t)data[2];
  dest_addr.u8[1] = (uint8_t)data[3];
  
  seq_num =  (uint8_t)data[4] |
             (uint8_t)data[5] << 8;
  
  sent_asn = (uint8_t)data[6] |
             (uint8_t)data[7] << 8 |
             (uint8_t)data[8] << 16 |
             (uint8_t)data[9] << 24;
  
  LOG_INFO("11111111 |sr %d%d sr|d %d%d d|s %u s|as 0x%lx as|ar 0x%lx ar|c %d c|p %d p|", src_addr.u8[0], src_addr.u8[1], dest_addr.u8[0],
                                                                                   dest_addr.u8[1], seq_num, sent_asn, tsch_current_asn.ls4b,
                                                                                   packetbuf_attr(PACKETBUF_ATTR_CHANNEL), sender_port);
  LOG_INFO_("\n");
/*  if(linkaddr_cmp(&src_addr, &ter_dest) && seq_num > 500 && print_schedule == 0) {
    print_schedule = 1;
    tsch_schedule_print();
  }*/
  if(tsch_current_asn.ls4b > 0x8D9A0 && print_schedule == 0) {
    //tsch_schedule_print();
    sdn_schedule_stat_print();
    print_max_sf_cell_usage();
    print_ideal_cell_num();
    print_schedule = 1;
  }
/*  if(linkaddr_cmp(&src_addr, &ter_dest) && seq_num > 1100){
    printf("TERMINATE_SIMIMULATION\n");
  } */
  //LOG_INFO("Received request '%.*s' from ", datalen, (char *) data);
  //LOG_INFO_6ADDR(sender_addr);
  
}
/*---------------------------------------------------------------------------*/
void
print_ideal_cell_num(void)
{
  //uip_ipaddr_t *parent_ipaddr = rpl_parent_get_ipaddr(curr_instance.dag.preferred_parent);
  //const uip_lladdr_t *parent_lladdr = uip_ds6_nbr_lladdr_from_ipaddr(parent_ipaddr);
  
  LOG_INFO("msf rank to cal. ideal cell: node_id:[%d%d] parent:[%d%d] rank[%u]] \n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], curr_instance.dag.rank);
  //LOG_INFO_LLADDR((linkaddr_t*)parent_lladdr);
  //LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(msf_root_process, ev, data)
{
  PROCESS_BEGIN();

#ifndef BUILD_WITH_RPL_BORDER_ROUTER
  serial_shell_init();
#endif

  if(simple_udp_register(&udp_conn, SERVER_UDP_PORT, NULL, APP1_CLIENT_UDP_PORT, udp_rx_callback) == 0 || 
     simple_udp_register(&udp_conn, SERVER_UDP_PORT, NULL, APP2_CLIENT_UDP_PORT, udp_rx_callback) == 0) {
    printf("CRITICAL ERROR: socket initialization failed\n");
    /*
     * we don't need to process received packets, but we need to have
     * a socket so as to prevent sending back ICMP error packets
     */
  } else {
    NETSTACK_ROUTING.root_start();
    sixtop_add_sf(&msf);
    sixtop_add_sf(&nrsf);
  }

  PROCESS_END();
}
