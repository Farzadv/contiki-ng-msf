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

/**
 * \file
 *         MSF 6P ADD handlers
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inria.fr>
 */

#include "contiki.h"
#include "assert.h"
#include <stdbool.h>

#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/sixtop/sixtop.h"
#include "net/mac/tsch/sixtop/sixp.h"
#include "net/mac/tsch/sixtop/sixp-trans.h"

#include "msf.h"
#include "msf-autonomous-cell.h"
#include "msf-housekeeping.h"
#include "msf-negotiated-cell.h"
#include "msf-reserved-cell.h"
#include "msf-sixp.h"

#include "sys/log.h"
#define LOG_MODULE "MSF"
#define LOG_LEVEL LOG_LEVEL_MSF

/* static functions */
static bool is_valid_request(sixp_pkt_cell_options_t cell_options,
                             sixp_pkt_num_cells_t num_cells,
                             const uint8_t *cell_list, uint16_t cell_list_len);
static void sent_callback_initiator(void *arg, uint16_t arg_len,
                                        const linkaddr_t *dest_addr,
                                        sixp_output_status_t status);
static void sent_callback_responder(void *arg, uint16_t arg_len,
                                        const linkaddr_t *dest_addr,
                                        sixp_output_status_t status);
static void send_response(const linkaddr_t *peer_addr,
                          sixp_pkt_cell_options_t cell_options,
                          sixp_pkt_num_cells_t num_cells,
                          const uint8_t *cell_list, uint16_t cell_list_len);

/*---------------------------------------------------------------------------*/
static bool
is_valid_request(sixp_pkt_cell_options_t cell_options,
                 sixp_pkt_num_cells_t num_cells,
                 const uint8_t *cell_list, uint16_t cell_list_len)
{
  bool ret = false;

  if(!msf_sixp_is_valid_rxtx(cell_options)) {}
  else if(num_cells != 1) {
    LOG_INFO("bad NumCells - %u (should be 1)\n", num_cells);
  } else if(cell_list == NULL) {
    LOG_INFO("no CellList\n");
  } else if(cell_list_len < sizeof(sixp_pkt_cell_t)) {
    LOG_INFO("too short CellList - %u octets\n", cell_list_len);
  } else {
    ret = true;
  }

  return ret;
}
/*---------------------------------------------------------------------------*/
static void
sent_callback_initiator(void *arg, uint16_t arg_len,
                            const linkaddr_t *dest_addr,
                            sixp_output_status_t status)
{
  assert(dest_addr != NULL);
  if(status == SIXP_OUTPUT_STATUS_SUCCESS) {
    /*
     * our request is acknowledged by MAC layer of the peer; nothing
     * to do.
     */
  } else if(status == SIXP_OUTPUT_STATUS_FAILURE) {
    LOG_ERR("ADD transaction failed\n");
    msf_reserved_cell_delete_all(dest_addr);
    /* retry later */
    msf_sixp_start_request_wait_timer(dest_addr);
  } else {
    /* do nothing; SIXP_OUTPUT_STATUS_ABORTED included */
  }
}
/*---------------------------------------------------------------------------*/
static void
sent_callback_responder(void *arg, uint16_t arg_len,
                            const linkaddr_t *dest_addr,
                            sixp_output_status_t status)
{
  tsch_link_t *reserved_cell = (tsch_link_t *)arg;
  assert(dest_addr != NULL);
  assert(arg == NULL || arg_len == sizeof(tsch_link_t));

  bool is_new_nbr = !msf_negotiated_is_scheduled_peer(dest_addr);

  if(status == SIXP_OUTPUT_STATUS_SUCCESS) {
    LOG_INFO("ADD transaction completes successfully\n");
    if(reserved_cell == NULL) {
      /* we returned an empty CellList; do nothing */
    } else {
      msf_negotiated_cell_type_t cell_type;
      uint16_t slot_offset, channel_offset;
      cell_type = ((reserved_cell->link_options & LINK_OPTION_TX)
                   ? MSF_NEGOTIATED_CELL_TYPE_TX
                   : MSF_NEGOTIATED_CELL_TYPE_RX);
      slot_offset = reserved_cell->timeslot;
      channel_offset = reserved_cell->channel_offset;

      msf_reserved_cell_delete_all(dest_addr);

      if(msf_negotiated_cell_add(dest_addr, cell_type,
                                 slot_offset, channel_offset) < 0) {
        LOG_ERR("failed to add a negotiated cell\n");
        /* don't try to resolve the inconsitency from the responder */
      } else {
        /* all good */
      }
    }
  } else {
    LOG_ERR("ADD transaction failed\n");
    msf_reserved_cell_delete_all(dest_addr);
  }

  //nbr have no negotiations, so inform about new nbr
  if ( is_new_nbr ){
      tsch_neighbor_t *nbr;
      nbr = tsch_queue_get_nbr(dest_addr);
      assert(nbr != NULL);
      if ( nbr != NULL ){
          MSF_ON_NEW_NBR(nbr);
      }
  }
}
/*---------------------------------------------------------------------------*/
static void
send_response(const linkaddr_t *peer_addr,
              sixp_pkt_cell_options_t cell_options,
              sixp_pkt_num_cells_t num_cells,
              const uint8_t *cell_list, uint16_t cell_list_len)
{
  tsch_link_t *reserved_cell;
  sixp_pkt_cell_t cell_to_return;
  sixp_pkt_rc_t rc;

  assert(peer_addr != NULL);

  msf_negotiated_cell_type_t cell_type;
  if(cell_options == SIXP_PKT_CELL_OPTION_TX) {
    cell_type = MSF_NEGOTIATED_CELL_TYPE_RX;
  } else {
    cell_type = MSF_NEGOTIATED_CELL_TYPE_TX;
  }

  if(is_valid_request(cell_options, num_cells, cell_list, cell_list_len)) {
    // TODO: need to improve reservation links so that allow concurent querys
    //          operaqtes on same peer. this is reduce schedule time-to-converge,
    //          and save transmitions, especialy when bootstrap on conflicted cell
    if ( msf_is_reserved_for_peer(peer_addr) ){
          LOG_ERR("busy by concurent to send an ADD %s ->", msf_negotiated_cell_type_str(cell_type) );
          LOG_ERR_LLADDR(peer_addr);
          LOG_ERR_("\n");
          msf_sixp_start_retry_wait_timer(peer_addr);
          rc = SIXP_PKT_RC_ERR_BUSY;
          reserved_cell = NULL;
    }
    else {

    rc = SIXP_PKT_RC_SUCCESS;
    reserved_cell = msf_sixp_reserve_one_cell(peer_addr, cell_type,
                                              cell_list, cell_list_len);
    if(reserved_cell != NULL) {
        msf_sixp_set_cell_params((uint8_t *)&cell_to_return, reserved_cell);
    } else {
        if (cell_type == MSF_NEGOTIATED_CELL_TYPE_TX) {
            const tsch_neighbor_t * n = tsch_queue_get_nbr(peer_addr);
        // if have no any TX cells to peer, try to allocate at least one. This
        //      helps establish good no-conflict connection to it.
            if ( !msf_negotiated_nbr_is_scheduled_tx(n) ) {
                LOG_WARN("Try to allocate TX in busy slot\n");
                reserved_cell = msf_sixp_reserve_tx_over(n, cell_list, cell_list_len);
            }
        }
        if(reserved_cell == NULL)
            LOG_ERR("cannot reserve a cell; going to send an empty CellList\n");
    }

    }// else if ( msf_is_reserved_for_peer(parent_addr)
  } else {
    rc = SIXP_PKT_RC_ERR;
    reserved_cell = NULL;
  }

  if(sixp_output(SIXP_PKT_TYPE_RESPONSE, (sixp_pkt_code_t)(uint8_t)rc,
                 MSF_SFID,
                 reserved_cell == NULL ? NULL : (uint8_t *)&cell_to_return,
                 reserved_cell == NULL ? 0 : sizeof(sixp_pkt_cell_t),
                 peer_addr, sent_callback_responder, reserved_cell,
                 reserved_cell == NULL ? 0 : sizeof(tsch_link_t)) < 0)
  {
    LOG_ERR("failed to send a response to ");
    LOG_ERR_LLADDR(peer_addr);
    LOG_ERR_("\n");
    msf_reserved_cell_delete_all(peer_addr);
  } else {
    LOG_INFO("sent a response with %s to ", msf_sixp_get_rc_str(rc));
    LOG_INFO_LLADDR(peer_addr);
    LOG_INFO_("\n");
  }
}

/*---------------------------------------------------------------------------*/
void
msf_sixp_add_send_request(msf_negotiated_cell_type_t cell_type)
{
  const linkaddr_t *parent_addr = msf_housekeeping_get_parent_addr();
  msf_sixp_add_send_request_to(cell_type, parent_addr);
}

void msf_sixp_add_send_request_to(msf_negotiated_cell_type_t cell_type
                              , const linkaddr_t *parent_addr)
{
  const sixp_pkt_type_t type = SIXP_PKT_TYPE_REQUEST;
  const sixp_pkt_code_t code = (sixp_pkt_code_t)(uint8_t)SIXP_PKT_CMD_ADD;
  size_t body_len = 0;
  sixp_pkt_cell_options_t cell_options;
  // amount of cells to add
  const sixp_pkt_num_cells_t num_cells = 1;

  LOG_DBG("ADD requestTO %d\n" , msf_sixp_request_timer_remain(parent_addr) );

  assert(parent_addr != NULL);
  if ( msf_is_reserved_for_peer(parent_addr) ){
      LOG_ERR("busy by concurent to send an ADD %s ->", msf_negotiated_cell_type_str(cell_type));
      LOG_ERR_LLADDR(parent_addr);
      LOG_ERR_("\n");
      msf_sixp_start_retry_wait_timer(parent_addr);
      return;
  }

  MSFCellsPkt  msg;
  memset(&msg, 0, sizeof(msg));

  if(cell_type == MSF_NEGOTIATED_CELL_TYPE_TX) {
    cell_options = SIXP_PKT_CELL_OPTION_TX;
  } else {
    cell_options = SIXP_PKT_CELL_OPTION_RX;
  }
  sixp_pkt_cells_reset(&msg.as_pkt);
  msg.as_pkt.head.cell_options = cell_options;

  int cell_list_len =
  msf_sixp_reserve_cells_pkt(parent_addr, &msg.as_pkt, MSF_6P_CELL_LIST_MAX_LEN);

  body_len = sixp_pkt_cells_total(&msg.as_pkt);
  msg.as_pkt.head.num_cells = num_cells;

  if(cell_list_len <= 0) {
    LOG_ERR("add_send_request: no cell is available\n");
    msf_sixp_start_request_wait_timer(parent_addr);
    return;
  } else if(sixp_output(type, code, MSF_SFID, msg.body, body_len,
                        parent_addr, sent_callback_initiator, NULL, 0) < 0) {
    LOG_ERR("failed to send an ADD %s ->", msf_negotiated_cell_type_str(cell_type));
    LOG_ERR_LLADDR(parent_addr);
    LOG_ERR_("\n");
    msf_reserved_cell_delete_all(parent_addr);
    msf_sixp_start_retry_wait_timer(parent_addr);
  } else {
    LOG_INFO("sent an ADD %s request to the parent: ", msf_negotiated_cell_type_str(cell_type) );
    LOG_INFO_LLADDR(parent_addr);
    LOG_INFO_("\n");
  }
}
/*---------------------------------------------------------------------------*/
void
msf_sixp_add_recv_request(const linkaddr_t *peer_addr,
                          const uint8_t *body, uint16_t body_len)
{
  const sixp_pkt_type_t type = SIXP_PKT_TYPE_REQUEST;
  const sixp_pkt_code_t code = (sixp_pkt_code_t)(uint8_t)SIXP_PKT_CMD_ADD;
  sixp_pkt_cell_options_t cell_options;
  sixp_pkt_num_cells_t num_cells;
  const uint8_t *cell_list;
  uint16_t cell_list_len;

  assert(peer_addr != NULL);

  LOG_INFO("received an ADD request from ");
  LOG_INFO_LLADDR(peer_addr);
  LOG_INFO_("\n");

  if(sixp_pkt_get_cell_options(type, code, &cell_options, body, body_len) < 0 ||
     sixp_pkt_get_num_cells(type, code, &num_cells, body, body_len) < 0 ||
     sixp_pkt_get_cell_list(type, code, &cell_list, &cell_list_len,
                            body, body_len) < 0) {
    LOG_ERR("parse error\n");
  } else {
    send_response(peer_addr, cell_options, num_cells, cell_list, cell_list_len);
  }
}
/*---------------------------------------------------------------------------*/
void
msf_sixp_add_recv_response(const linkaddr_t *peer_addr, sixp_pkt_rc_t rc,
                           const uint8_t *body, uint16_t body_len)
{
  const uint8_t *cell_list;
  uint16_t cell_list_len;

  assert(peer_addr != NULL);

  LOG_INFO("received an ADD response with %s from ", msf_sixp_get_rc_str(rc));
  LOG_INFO_LLADDR(peer_addr);
  LOG_INFO_("\n");

  bool is_new_nbr = !msf_negotiated_is_scheduled_peer(peer_addr);

  if(rc == SIXP_PKT_RC_SUCCESS) {
    LOG_INFO("ADD transaction completes successfully\n");
    if(sixp_pkt_get_cell_list(SIXP_PKT_TYPE_RESPONSE,
                              (sixp_pkt_code_t)(uint8_t)SIXP_PKT_RC_SUCCESS,
                              &cell_list, &cell_list_len,
                              body, body_len) < 0) {
      LOG_ERR("parse error\n");
      msf_reserved_cell_delete_all(peer_addr);
    } else if(cell_list_len == 0) {
      LOG_INFO("received an empty CellList; try another ADD request later\n");
      msf_reserved_cell_delete_all(peer_addr);
      //next attempt try after some time
      msf_sixp_start_request_wait_timer(peer_addr);
    } else if(cell_list_len != sizeof(sixp_pkt_cell_t)) {
      /* invalid length since MSF always requests one cell per ADD request */
      LOG_ERR("received an invalid CellList (%u octets)\n", cell_list_len);
      msf_reserved_cell_delete_all(peer_addr);
    } else {
      msf_sixp_reserved_cell_negotiate(peer_addr, sixp_pkt_get_cell(cell_list, 0) );
    }
  } else {
    LOG_ERR("ADD transaction failed\n");
    msf_reserved_cell_delete_all(peer_addr);
    if(rc == SIXP_PKT_RC_ERR_SEQNUM) {
      msf_housekeeping_resolve_inconsistency(peer_addr);
    } else if(rc == SIXP_PKT_RC_ERR_BUSY) {
      msf_sixp_start_request_wait_timer(peer_addr);
    } else {
      /* do nothing */
    }
  }

  //nbr have no negotiations, so inform about new nbr
  if ( is_new_nbr ){
      tsch_neighbor_t *nbr;
      nbr = tsch_queue_get_nbr(peer_addr);
      assert(nbr != NULL);
      if ( nbr != NULL ){
          MSF_ON_NEW_NBR(nbr);
      }
  }
}
/*---------------------------------------------------------------------------*/
