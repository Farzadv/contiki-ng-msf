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

/*
points to simulate the MSF:
  - set EB and DIO period correctly to avoid collision in minimal cell
  - set slotframe len long enough to accommodate all the cells
*/

/*
   The value of MAX_NUMCELLS is chosen according to the traffic type of
   the network.  Generally speaking, the larger the value MAX_NUMCELLS
   is, the more accurate the cell usage is calculated.  The 6P traffic
   overhead using a larger value of MAX_NUMCELLS could be reduced as
   well.  Meanwhile, the latency won't increase much by using a larger
   value of MAX_NUMCELLS for periodic traffic type.  For burst traffic
   type, larger value of MAX_NUMCELLS indeed introduces higher latency.
   The latency caused by slight changes of traffic load can be absolved
   by the additional scheduled cells.  In this sense, MSF is a
   scheduling function trading latency with energy by scheduling more
   cells than needed.  It is recommended to set MAX_NUMCELLS value at
   least 4 times than the maximum link traffic load of the network in
   packets per slotframe.  For example, a 2 packets/slotframe traffic
   load results an average 4 cells scheduled, using the value of double
   number of scheduled cells (which is 8) as MAX_NUMCELLS gives a good
   resolution on cell usage calculation
   
   
   Remove an AutoTxCell when:

      *  there is no frame to transmit on that cell, or
      *  there is at least one 6P negotiated Tx cell in the schedule for
         the frames to transmit.
         
  6P PACKETS: Once the preferred parent has been selected, the node uses6P to request from the parent one negotiated cell among 5 proposed candidates. 
  Thisnegotiated cell can be used only for unicast transmission to the preferred parent. Thisinitial 6P request occurs over autonomous cells which
  are removed after transmission.Subsequent 6P requests will occur on any of the negotiated cells to the preferred parent




=====> there is no collision in the 6p packets because they are sent through negotiated cells

=====> simululation is crashs under the the harsh traffic rate
*/

/*
  way to calculate ppm: ((EF-MF) * 1 million) / EF
  EF: expected frequency
  MF: measured frequency
*/


#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define SIXTOP_CONF_MAX_TRANSACTIONS 8
#define TSCH_SCHEDULE_CONF_MAX_LINKS 1024

#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 101
//#define MSF_CONF_SLOTFRAME_LENGTH         501
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_16_16   // MODIFIED
#define TSCH_CONF_WITH_SIXTOP 1
#define TSCH_CONF_BURST_MAX_LEN 0
#define TSCH_CONF_MAC_MAX_FRAME_RETRIES    14

#define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR   16
/*  MY ADDs */
#define TSCH_CONF_EB_PERIOD (15 * CLOCK_SECOND)  // ADD
//#define TSCH_CONF_MAX_EB_PERIOD (10 * CLOCK_SECOND)
//#define TSCH_CONF_KEEPALIVE_TIMEOUT (4 * CLOCK_SECOND)
//#define QUEUEBUF_CONF_NUM 8
//#define MSF_CONF_LIM_NUM_CELLS_USED_HIGH   75
//#define MSF_CONF_MAX_NUM_CELLS             100

#define MSF_CONF_MAX_NUM_NEGOTIATED_TX_CELLS   100
#define MSF_CONF_MAX_NUM_NEGOTIATED_RX_CELLS   100


#define SERVER_UDP_PORT 1000
#define APP1_CLIENT_UDP_PORT 1020
#define APP2_CLIENT_UDP_PORT 1030

#define APP1_SEND_INTERVAL (5.02 * CLOCK_SECOND)
#define APP2_SEND_INTERVAL (1 * CLOCK_SECOND)

//#define TSCH_QUEUE_CONF_NUM_PER_NEIGHBOR   32

//#define RPL_CONF_DAO_MAX_RETRANSMISSIONS   1
//#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT   (30 * CLOCK_SECOND)
#define RPL_CONF_WITH_DAO_ACK   0

//#define RPL_CONF_PROBING_INTERVAL  (30 * CLOCK_SECOND)
//#define RPL_CONF_TRICKLE_REFRESH_DAO_ROUTES   1
//#define RPL_CONF_DIS_INTERVAL  (130 * CLOCK_SECOND)
//#define RPL_CONF_DEFAULT_LIFETIME_UNIT  120

#define RPL_OF0_CONF_SR     RPL_OF0_FIXED_SR
#define RPL_CONF_OF_OCP     RPL_OCP_OF0 /* tells to use OF0 for DAGs rooted at this node */
#define RPL_CONF_SUPPORTED_OFS {&rpl_of0, &rpl_mrhof} /* tells to compile in support for both OF0 and MRHOF */

//#define TSCH_CONF_DESYNC_THRESHOLD     (30 * TSCH_MAX_KEEPALIVE_TIMEOUT)
//#define RPL_CONF_DELAY_BEFORE_LEAVING  (60 * 60 * CLOCK_SECOND)
//#define RPL_CONF_DIS_INTERVAL          (30 * CLOCK_SECOND)
//#define RPL_CONF_PROBING_INTERVAL (20 * CLOCK_SECOND)


#define COMP_WITH_SDN 1

#define COMP_SDN_LOG_LEVEL                         LOG_LEVEL_INFO

#define LOG_CONF_LEVEL_MSF                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_WARN

#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6top                        LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT                     0

#endif /* PROJECT_CONF_H_ */
