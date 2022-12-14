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
 *         MSF 6P-related common helpers (for internal use)
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inria.fr>
 */

#include "contiki.h"
#include "assert.h"

#include "lib/random.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/sixtop/sixp-pkt.h"

#include "msf.h"
#include "msf-negotiated-cell.h"
#include "msf-reserved-cell.h"
#include "msf-housekeeping.h"
#include "msf-sixp.h"

#include "sys/log.h"
#define LOG_MODULE "MSF"
#define LOG_LEVEL LOG_LEVEL_MSF

/* variables */
static struct timer request_wait_timer[MSF_TIMER_TOTAL];
static struct timer retry_wait_timer;

/*---------------------------------------------------------------------------*/
const char *
msf_sixp_get_rc_str(sixp_pkt_rc_t rc)
{
  static const struct _rc_str_entry {
    sixp_pkt_rc_t rc;
    const char *str;
  } rc_str_list[] = {
    {SIXP_PKT_RC_SUCCESS, "RC_SUCCESS"},
    {SIXP_PKT_RC_EOL, "RC_EOL"},
    {SIXP_PKT_RC_ERR, "RC_ERR"},
    {SIXP_PKT_RC_RESET, "RC_RESET"},
    {SIXP_PKT_RC_ERR_VERSION, "RC_ERR_VERSION"},
    {SIXP_PKT_RC_ERR_SFID, "RC_ERR_SFID"},
    {SIXP_PKT_RC_ERR_SEQNUM, "RC_ERR_SEQNUM"},
    {SIXP_PKT_RC_ERR_CELLLIST, "RC_ERR_CELLLIST"},
    {SIXP_PKT_RC_ERR_BUSY, "RC_ERR_BUSY"},
    {SIXP_PKT_RC_ERR_LOCKED, "RC_ERR_LOCKED"},
  };
  static const char na[] = "N/A";
  const char *ret = na;
  for(int i = 0; i < sizeof(rc_str_list); i++) {
    if(rc_str_list[i].rc == rc) {
      ret = rc_str_list[i].str;
      break;
    }
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
void
msf_sixp_set_cell_params(uint8_t *buf, const tsch_link_t *cell)
{
  if(buf == NULL || cell == NULL) {
    /* do nothing */
  } else {
    buf[0] = cell->timeslot & 0xff;
    buf[1] = cell->timeslot >> 8;
    buf[2] = cell->channel_offset & 0xff;
    buf[3] = cell->channel_offset >> 8;
  }
}
/*---------------------------------------------------------------------------*/
void
msf_sixp_get_cell_params(const uint8_t *buf,
                         uint16_t *slot_offset, uint16_t *channel_offset)
{
  assert((buf != NULL) && (slot_offset != NULL) && (channel_offset != NULL));
    *slot_offset = buf[0] + (buf[1] << 8);
    *channel_offset = buf[2] + (buf[3] << 8);
  }
//==============================================================================
static
MSFTimerID peer_timerid(const linkaddr_t *peer_addr){
    const linkaddr_t* parent =  msf_housekeeping_get_parent_addr();
    if (parent == NULL)
        return MSF_TIMER_NBR;
    if (linkaddr_cmp(peer_addr, parent))
        return MSF_TIMER_PARENT;
    else
        return MSF_TIMER_NBR;
}
/*---------------------------------------------------------------------------*/
bool
msf_sixp_is_request_wait_timer_expired(MSFTimerID tid)
{
  return timer_expired(&request_wait_timer[tid]);
}

bool msf_sixp_is_request_peer_timer_expired(const linkaddr_t *peer_addr){
    MSFTimerID tid = peer_timerid(peer_addr);
    return timer_expired(&request_wait_timer[tid]);
}

int  msf_sixp_request_timer_remain(const linkaddr_t *peer_addr){
    MSFTimerID tid = peer_timerid(peer_addr);
    return timer_remaining(&request_wait_timer[tid]);
}

/*---------------------------------------------------------------------------*/
void
msf_sixp_stop_request_wait_timer(const linkaddr_t *peer_addr)
{
  MSFTimerID tid = peer_timerid(peer_addr);
  timer_set(&request_wait_timer[tid], 0);
  LOG_DBG("delay%d request off\n", tid);
}
/*---------------------------------------------------------------------------*/
void
msf_sixp_start_request_wait_timer(const linkaddr_t *peer_addr)
{
  MSFTimerID tid = peer_timerid(peer_addr);

  clock_time_t wait_duration_seconds;
  unsigned short random_value = random_rand();

  assert(MSF_WAIT_DURATION_MIN_SECONDS < MSF_WAIT_DURATION_MAX_SECONDS);
  wait_duration_seconds = (MSF_WAIT_DURATION_MIN_SECONDS +
                           ((MSF_WAIT_DURATION_MAX_SECONDS -
                             MSF_WAIT_DURATION_MIN_SECONDS) *
                            random_value /
                            RANDOM_RAND_MAX));

  assert(timer_expired(&request_wait_timer[tid]) != 0);
  timer_set(&request_wait_timer[tid], wait_duration_seconds * CLOCK_SECOND);
  LOG_DBG("delay%d the next request for %u seconds\n", tid, (unsigned)wait_duration_seconds );
}
/*---------------------------------------------------------------------------*/
bool msf_sixp_is_retry_wait_timer_expired()
{
  return timer_expired(&retry_wait_timer);
}
/*---------------------------------------------------------------------------*/
void msf_sixp_stop_retry_wait_timer()
{
  timer_set(&retry_wait_timer, 0);
}
/*---------------------------------------------------------------------------*/
void msf_sixp_start_retry_wait_timer(const linkaddr_t *peer_addr)
{
  if (peer_timerid(peer_addr) != MSF_TIMER_PARENT)
      return;

  clock_time_t wait_duration_seconds;
  unsigned short random_value = random_rand();

  //retry about 1-2 frame slots
  wait_duration_seconds = MSF_SLOTFRAME_LENGTH
                        +(( MSF_SLOTFRAME_LENGTH * random_value) / RANDOM_RAND_MAX )
                        ;
  wait_duration_seconds = (wait_duration_seconds* CLOCK_SECOND) / TSCH_SLOTS_PER_SECOND;

  assert(timer_expired(&retry_wait_timer) != 0);
  timer_set(&retry_wait_timer, wait_duration_seconds );
  LOG_DBG("delay the next retry for %lu [tick]\n", wait_duration_seconds);
}
//==============================================================================
/*---------------------------------------------------------------------------*/
static
size_t msf_sixp_reserves_cell_list(const linkaddr_t *peer_addr,
                        msf_negotiated_cell_type_t cell_type, ReserveMode mode,
                        uint8_t *cell_list, size_t cell_list_len)
{
  tsch_link_t *reserved_cell;
  size_t filled_cell_list_len = 0;
  while(filled_cell_list_len < cell_list_len) {
    reserved_cell = msf_reserved_cell_add(peer_addr, cell_type, mode, -1);
    if(reserved_cell == NULL) {
      break;
    } else {
      msf_sixp_set_cell_params(cell_list + filled_cell_list_len, reserved_cell);
      filled_cell_list_len += sizeof(sixp_pkt_cell_t);
    }
  }
  return filled_cell_list_len;
}

size_t
msf_sixp_fill_cell_list(const linkaddr_t *peer_addr,
                        msf_negotiated_cell_type_t cell_type,
                        uint8_t *cell_list, size_t cell_list_len)
{
    return msf_sixp_reserves_cell_list(peer_addr, cell_type, RESERVE_NEW_CELL
                                        , cell_list, cell_list_len);
}

static
size_t msf_sixp_reserve_cell_pkt(const linkaddr_t *peer_addr, ReserveMode mode,
                                SIXPCellsPkt* pkt, unsigned cells_limit
                                )
{
    size_t len;
    msf_negotiated_cell_type_t cell_type = (msf_negotiated_cell_type_t)pkt->head.cell_options;
    len = msf_sixp_reserves_cell_list(peer_addr, cell_type, mode
            , (uint8_t*)&(pkt->cells[pkt->head.num_cells])
            , (cells_limit - pkt->head.num_cells)* sizeof(sixp_cell_t)
            );
    len = len/sizeof(sixp_cell_t);
    pkt->head.num_cells+= len;
    return len;
}
/*---------------------------------------------------------------------------*/
size_t msf_sixp_reserve_cells_pkt(const linkaddr_t *peer_addr,
                                SIXPCellsPkt* pkt, unsigned cells_limit
                                )
{
    /* prefer to reserve slots, that are used by close nbrs. This helps keep free
     * slots avail to same nbrs
     * */
    int cnt = msf_sixp_reserve_cell_pkt(peer_addr, RESERVE_NBR_BUSY_CELL ,pkt, cells_limit);
    if( cnt > 0 )
        LOG_DBG("reserved %d 1hop cells\n", cnt);

    if (pkt->head.num_cells >= cells_limit)
        return cnt;

    cnt += msf_sixp_reserve_cell_pkt(peer_addr, RESERVE_NEW_CELL, pkt, cells_limit);

    if (cnt < cells_limit)
    if (LOG_LEVEL > LOG_LEVEL_DBG){
          msf_negotiated_cell_type_t cell_type
                      = (msf_negotiated_cell_type_t)pkt->head.cell_options;
          LOG_DBG("cells busy for %s:\n", msf_negotiated_cell_type_str(cell_type));
          msf_avoid_dump_local_cells();
    }

    return cnt;
}
/*---------------------------------------------------------------------------*/
static
tsch_link_t * msf_sixp_reserve_one_cell_of_list(const linkaddr_t *peer_addr,
                            // this is slot wich is reserves by override strategy
                            uint16_t slot_override,
                          msf_negotiated_cell_type_t cell_type,
                          const void *cell_src, size_t cell_list_len
                          )
{
  size_t offset;
  uint16_t slot_offset, channel_offset;
  tsch_link_t *reserved_cell;
  const uint8_t* cell_list = (const uint8_t *)cell_src;

  if(peer_addr == NULL || cell_list == NULL) {
    reserved_cell = NULL;
  } else {
    for(offset = 0, reserved_cell = NULL;
        offset < cell_list_len;
        offset += sizeof(sixp_pkt_cell_t)) {
      msf_sixp_get_cell_params(cell_list + offset,
                               &slot_offset, &channel_offset);
      if (slot_offset != slot_override)
      reserved_cell = msf_reserved_cell_add(peer_addr, cell_type,
                                            slot_offset, channel_offset);
      else
          reserved_cell = msf_reserved_cell_over(peer_addr, cell_type,
                                  msf_cell_at(slot_offset, channel_offset) );
      if((reserved_cell) != NULL)
      {
        return reserved_cell;
      } else {
        /*
         * cannot reserve a cell (most probably, this slot_offset is
         * occupied; try the next one
         */
        if (LOG_LEVEL > LOG_LEVEL_DBG){
            LOG_DBG_("slot[%u] busy:", slot_offset);
            msf_avoid_dump_slot( slot_offset );
            //msf_avoid_dump_cell( msf_cell_at(slot_offset, channel_offset) );
        }
      }
    }
  }
  /*
   * cannot reserve a cell (most probably, this slot_offset is
   * occupied; try the next one
   */
  if (LOG_LEVEL > LOG_LEVEL_DBG){
      LOG_DBG("cells busy for %s:\n", msf_negotiated_cell_type_str(cell_type));
      msf_avoid_dump_local_cells();
  }
  return reserved_cell;
}

/*---------------------------------------------------------------------------*/
tsch_link_t * msf_sixp_reserve_one_cell(const linkaddr_t *peer_addr,
                          msf_negotiated_cell_type_t cell_type,
                          const uint8_t *cell_list, size_t cell_list_len)
{
    return msf_sixp_reserve_one_cell_of_list(peer_addr, (uint16_t)~0u, cell_type
                        ,cell_list, cell_list_len);
}

tsch_link_t *msf_sixp_reserve_cell_over(tsch_link_t * cell_to_override,
                                       const void *cell_list, size_t cell_list_len)
{
    return msf_sixp_reserve_one_cell_of_list( &cell_to_override->addr
                        , cell_to_override->timeslot
                        , msf_negotiated_link_cell_type(cell_to_override)
                        , cell_list, cell_list_len);
}

/**
 * \brief Reserve one of TX cells found in a given CellList, mixing with existing TX
 *          slots. This is last-chance allocation, when no any slots availiable. Used
 *        for establish least-chance negotiated connection no-conflict with peer.
 *        Allow provide multiple TX links in same slot to different peers.
 * \param peer_addr MAC address of the peer
 * \param cell_type Type of a cell to reserve
 * \param cell_list A pointer to a CellList buffer
 * \param cell_list_len The length of the CellList buffer
 * \return A pointer to a reserved cell on success, otherwise NULL
 */
tsch_link_t* msf_sixp_reserve_tx_over(const tsch_neighbor_t* n,
                                       const void* cell_src, size_t cell_list_len)
{
    size_t offset;
    uint16_t slot_offset, channel_offset;
    tsch_link_t *reserved_cell;
    const uint8_t* cell_list = (const uint8_t *)cell_src;
    const linkaddr_t *peer_addr = tsch_queue_get_nbr_address(n);

    // select apropriate slot for override cell:
    //  have no RX connects
    //  have no any peer_addr connects
    for(offset = 0, reserved_cell = NULL;
        offset < cell_list_len;
        offset += sizeof(sixp_pkt_cell_t))
    {
      msf_sixp_get_cell_params(cell_list + offset,
                               &slot_offset, &channel_offset);
      const tsch_neighbor_t* rxnbr = msf_is_avoid_local_slot_rx(slot_offset);
      if ( rxnbr != 0 )
          // do not allow mix with rx
          continue;
      AvoidOptions ops = msf_is_avoid_local_slot_nbr(slot_offset, n);
      if ( ops >= 0 )
          // do not allow mix to same peer
          continue;

      // found good slot?
      reserved_cell = msf_reserved_cell_over(peer_addr, MSF_NEGOTIATED_CELL_TYPE_TX,
                              msf_cell_at(slot_offset, channel_offset) );

      if (reserved_cell != NULL)
          return reserved_cell;

    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
size_t msf_sixp_reserve_migrate_chanels_pkt(const tsch_link_t *cell_to_relocate,
                                SIXPCellsPkt* pkt, unsigned pkt_limit
                                )
{
    msf_negotiated_cell_type_t cell_type = (msf_negotiated_cell_type_t)pkt->head.cell_options;

    LOG_DBG("reserving last-chance chanels for slot %d\n", cell_to_relocate->timeslot);

    int len = pkt->head.num_cells;

    msf_cell_t new_cell;
    new_cell.field.slot = cell_to_relocate->timeslot;
    msf_chanel_mask_t ocupied_ch = 0;

    if (len < pkt_limit)
    if (LOG_LEVEL >= LOG_LEVEL_DBG){
        LOG_DBG("cells %s busy for slot:\n", msf_negotiated_cell_type_str(cell_type));
        msf_avoid_dump_slot(new_cell.field.slot);
    }

    for (int i = len; i < pkt_limit; ++i){
        int new_ch = msf_find_unused_slot_chanel(new_cell.field.slot, ocupied_ch);
        if (new_ch < 0)
            break;

        tsch_link_t *reserved;
        reserved = msf_reserved_cell_add_anyvoid( &cell_to_relocate->addr
                                , cell_type, cell_to_relocate->timeslot, new_ch);
        if (reserved == NULL)
            break;

        ocupied_ch |= 1<< new_ch;
        new_cell.field.chanel = new_ch;
        pkt->cells[i].raw = new_cell.raw;
        pkt->head.num_cells++;
    }
    return pkt->head.num_cells - len;
}
/*---------------------------------------------------------------------------*/
tsch_link_t *
msf_sixp_find_scheduled_cell(const linkaddr_t *peer_addr,
                             uint8_t link_options,
                             const uint8_t *cell_list, size_t cell_list_len)
{
  tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
  tsch_link_t *cell = NULL;

  if(peer_addr == NULL ||
     cell_list == NULL ||
     cell_list_len < sizeof(sixp_pkt_cell_t) ||
     slotframe == NULL) {
    /* invalid contion */
  } else {
    uint16_t slot_offset, channel_offset;

    for(const uint8_t *p = cell_list;
        p < cell_list + cell_list_len;
        p += sizeof(sixp_pkt_cell_t)) {

      msf_sixp_get_cell_params(cell_list, &slot_offset, &channel_offset);
      // TODO: rely that only one link for peer assignaed at one slot
      cell = tsch_schedule_get_link_by_timeslot(slotframe, slot_offset, channel_offset);

      if(cell == NULL)
          continue;
      if (cell->link_options == link_options &&
         linkaddr_cmp(&cell->addr, peer_addr))
      {
        /* found */
        break;
      } else {
        /* try next */
      }
    }
  }

  return cell;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Moves reserved cell for peer to negotiated one.
 * \param peer_addr MAC address of the peer
 * \param cell_list A pointer to a CellList buffer
 * \param cell_list_len The length of the CellList buffer
 * \return A pointer to a reserved cell on success, otherwise NULL
 */
int msf_sixp_reserved_cell_negotiate(const linkaddr_t *peer_addr, sixp_cell_t cell)
{
    tsch_link_t* cell_to_relocate = msf_reserved_cell_get(peer_addr
                                        , cell.field.slot, cell.field.chanel);
    if( cell_to_relocate != NULL) {

        msf_negotiated_cell_type_t cell_type;
        if(cell_to_relocate->link_options & LINK_OPTION_TX) {
          cell_type = MSF_NEGOTIATED_CELL_TYPE_TX;
        } else {
          cell_type = MSF_NEGOTIATED_CELL_TYPE_RX;
        }

      /* this is a cell which we proposed in the request */
      msf_reserved_cell_delete_all(peer_addr);

      int ok = msf_negotiated_cell_add(peer_addr, cell_type,
                                          cell.field.slot, cell.field.chanel);
      if( ok >= 0)
      {
          msf_housekeeping_delete_cell_to_relocate();
          /* all good */
      } else {
          msf_housekeeping_resolve_inconsistency(peer_addr);
      }
      return ok;
    }
    else {
        LOG_ERR("received a cell which we didn't propose\n");
        LOG_ERR("SCHEDULE INCONSISTENCY is likely to happen; ");
        msf_reserved_cell_delete_all(peer_addr);
        msf_housekeeping_resolve_inconsistency(peer_addr);
        return irNOCELL;
    }
}

/*---------------------------------------------------------------------------*/
bool msf_sixp_is_valid_rxtx(sixp_pkt_cell_options_t cell_options){
    if(cell_options == SIXP_PKT_CELL_OPTION_TX
       || cell_options == SIXP_PKT_CELL_OPTION_RX
       )
        return true;

    LOG_INFO("bad CellOptions - %02X (should be %02X or %02X)\n",
           cell_options, SIXP_PKT_CELL_OPTION_TX, SIXP_PKT_CELL_OPTION_RX);
    return false;
}
