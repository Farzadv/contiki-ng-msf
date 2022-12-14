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
 *         MSF Reserved Cell APIs
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inria.fr>
 */

#include "contiki.h"
#include <stdint.h>
#include "assert.h"

#include "lib/random.h"
#include "net/linkaddr.h"
#include "net/mac/tsch/tsch.h"

#include "msf-autonomous-cell.h"
#include "msf-housekeeping.h"
#include "msf-negotiated-cell.h"
#include "msf-reserved-cell.h"
#include "msf-avoid-cell.h"

// provide: ffz
#include "msf-bittwiddings.h"

#include "sys/log.h"
#define LOG_MODULE "MSF"
#define LOG_LEVEL LOG_LEVEL_MSF

/* variables */
extern struct tsch_asn_divisor_t tsch_hopping_sequence_length;

/*---------------------------------------------------------------------------*/
long msf_find_unused_slot_offset(tsch_slotframe_t *slotframe
                                , ReserveMode mode
                                , const linkaddr_t *peer_addr)
{
  long ret;
  uint16_t slot_offset;

  const tsch_link_t *autonomous_rx_cell = msf_autonomous_cell_get_rx();
  (void)autonomous_rx_cell;
  assert(autonomous_rx_cell != NULL);

  tsch_neighbor_t* nbr = NULL;
  if (mode == RESERVE_NBR_BUSY_CELL)
      nbr = tsch_queue_get_nbr(peer_addr);

  slot_offset = random_rand() % slotframe->size.val;
  ret = -1;
  for(int i = 0; i < slotframe->size.val; ++i, ++slot_offset) {
    if (slot_offset >= slotframe->size.val)
        slot_offset = 0;
    tsch_link_t* sheduled_link = tsch_schedule_get_any_link_by_timeslot(slotframe, slot_offset);
    if(sheduled_link != NULL)
        continue;

    if (mode == RESERVE_NBR_BUSY_CELL)
    // search only slots that have busy at other nbrs
    if ( msf_is_avoid_close_slot_outnbr(slot_offset, nbr ) < 0 )
        continue;

    if (msf_is_avoid_local_slot(slot_offset) < 0){
        // this slot is not alocated by own cells, so can share it with other
        //      neighbors by channel resolve
        if (ret < 0)
            ret = slot_offset;
    }

    if (msf_is_avoid_slot(slot_offset) < 0)
    {
      /*
       * avoid using the slot offset of 0, which is used by the
       * minimal schedule, as well as the slot offset of the autono???ous
       * RX cell
       */
      return slot_offset;
      break;
    } else {
      /* try the next one */
    }
  }
  return ret;
}

int  msf_find_unused_slot_chanel(uint16_t slot, msf_chanel_mask_t skip){
    msf_chanel_mask_t busych = skip | msf_avoided_slot_chanels(slot);

    // random ch better avoids conflicts with other nbrs
    uint16_t ch = random_rand();
    ch %= tsch_hopping_sequence_length.val;

    // there is free chanels, select random one
    if ( ((busych >> ch)& 1)==0 )
        return ch;

    int res = ffz(busych);
#ifdef TSCH_JOIN_HOPPING_SEQUENCE_SIZE
    int slot_limit = TSCH_JOIN_HOPPING_SEQUENCE_SIZE();
#else
    int slot_limit = tsch_hopping_sequence_length.val;
#endif
    if ( res >= slot_limit )
    {
        // all chanels are busy, select any random for hope
        return -1;
    }

    //search ch-n free chanel in set of busych, wraped around it's free cells
    for (unsigned cset = busych>>res ; ch > 0
        ; cset = cset >> 1, ++res )
    {
        if (res >= tsch_hopping_sequence_length.val){
            res = ffz(busych);
            cset = busych>>res;
        }
        if ((cset&1)==0){
            --ch;
        }
    }

    return res;
}

/*---------------------------------------------------------------------------*/
int
msf_reserved_cell_get_num_cells(const linkaddr_t *peer_addr)
{
  tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
  tsch_link_t *cell;
  int ret;

  if(slotframe == NULL) {
    ret = 0;
  } else {
    ret = 0;
    for(cell = list_head(slotframe->links_list);
        cell != NULL;
        cell = (tsch_link_t *)list_item_next(cell)) {
      if((cell->link_options & LINK_OPTION_RESERVED_LINK) &&
         linkaddr_cmp(&cell->addr, peer_addr)) {
        /* this is a reserved cell for the peer */
        ret += 1;
      } else {
        /* this is not */
      }
    }
  }

  return ret;
}
/*---------------------------------------------------------------------------*/
tsch_link_t *
msf_reserved_cell_get(const linkaddr_t *peer_addr,
                      long slot_offset, long channel_offset)
{
  tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
  tsch_link_t *cell;

  if(slotframe == NULL) {
    cell = NULL;
  } else {
    for(cell = list_head(slotframe->links_list);
        cell != NULL;
        cell = (tsch_link_t *)list_item_next(cell)) {
      if((cell->link_options & LINK_OPTION_RESERVED_LINK) &&
         linkaddr_cmp(&cell->addr, peer_addr) &&
         (slot_offset < 0 || cell->timeslot == slot_offset) &&
         (channel_offset < 0 || cell->channel_offset == channel_offset)) {
        /* return the first found one */
        break;
      } else {
        /* try next */
      }
    }
  }

  return cell;
}

/*---------------------------------------------------------------------------*/
static
bool is_valid_new_link(const linkaddr_t *peer_addr,
                      msf_negotiated_cell_type_t cell_type,
                      int32_t slot_offset, int32_t channel )
{
    tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
    tsch_link_t *cell;

    for(cell = list_head(slotframe->links_list)
        ; cell != NULL
        ; cell = (tsch_link_t *)list_item_next(cell) )
    {
        if (cell->timeslot != slot_offset) continue;

        // RELOCATION may requre chanel migration in one slot.
        // so allow to insert TX-TX/RX-RX to peer on different chanels.
        if (linkaddr_cmp(&cell->addr, peer_addr)){
            if ( ((cell->link_options & LINK_OPTION_RX) != 0)
                 != (cell_type == MSF_NEGOTIATED_CELL_TYPE_RX)
                )
                return false;

            if (cell->channel_offset == channel)
                return false;

            continue;
        }

        // don't allow RX at one slot, only TX can mix concurently
        if ( (cell->link_options & LINK_OPTION_RX) != 0
            || (cell_type == MSF_NEGOTIATED_CELL_TYPE_RX)
            )
            return false;

        if (cell->channel_offset != channel)
            continue;

        return false;
    }
    return true;
}
/*---------------------------------------------------------------------------*/
tsch_link_t *
msf_reserved_cell_add(const linkaddr_t *peer_addr,
                      msf_negotiated_cell_type_t cell_type,
                      int32_t slot_offset, int32_t channel_offset)
{

    if(slot_offset > 0)
    if ( msf_is_avoid_local_slot(slot_offset) >= 0) {
        /* this slot is used; we cannot reserve a cell */
        LOG_DBG("reserve %s miss at slot_offset:%ld, channel_offset:%ld\n",
                msf_negotiated_cell_type_str(cell_type),
                (long)slot_offset, (long)channel_offset);
        return NULL;
    }

    return msf_reserved_cell_add_anyvoid(peer_addr, cell_type,
                                            slot_offset, channel_offset
                                           );
}

/** This is less strictive msf_reserved_cell_add - it allow new cells in alredy ocupied slot.
 *  RELOCATIE use it for allocate links that migrate chanel in slot
 */
tsch_link_t *msf_reserved_cell_over(const linkaddr_t *peer_addr,
                                   msf_negotiated_cell_type_t cell_type,
                                   msf_cell_t new_cell)
{

    if ( msf_is_avoid_local_cell(new_cell) >= 0) {
        /* this slot is used; we cannot reserve a cell */
        LOG_DBG("reserve %s miss at slot_offset:%d, channel_offset:%d\n",
                msf_negotiated_cell_type_str(cell_type),
                new_cell.field.slot, new_cell.field.chanel);
        return NULL;
    }

    return msf_reserved_cell_add_anyvoid(peer_addr, cell_type,
                    new_cell.field.slot, new_cell.field.chanel
                                           );
}

/** This is less stricted msf_reserved_cell_add - it not check cell avoidance.
 *  RELOCATIE use it for allocate links that migrate chanel in slot
 */
tsch_link_t * msf_reserved_cell_add_anyvoid(const linkaddr_t *peer_addr,
                      msf_negotiated_cell_type_t cell_type,
                      int32_t slot_offset, int32_t channel_offset)
{
  tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
  tsch_link_t *cell;
  uint8_t link_options;
  int32_t _slot_offset;
  int32_t _channel_offset;

  assert(peer_addr != NULL);

  if(slotframe == NULL) {
    return NULL;
  }

  if(cell_type == MSF_NEGOTIATED_CELL_TYPE_TX) {
    link_options = LINK_OPTION_TX | LINK_OPTION_RESERVED_LINK;
  } else if(cell_type == MSF_NEGOTIATED_CELL_TYPE_RX) {
    link_options = LINK_OPTION_RX | LINK_OPTION_RESERVED_LINK;
  } else {
    /* unsupported */
    LOG_ERR("invalid negotiated cell type value: %u\n", cell_type);
    return NULL;
  }

  if(slot_offset < 0) {
    _slot_offset = msf_find_unused_slot_offset(slotframe, (ReserveMode)slot_offset, peer_addr);
  } else if ( !is_valid_new_link(peer_addr, cell_type, slot_offset, channel_offset) ){
      /* this slot is used; we cannot reserve a cell */
      _slot_offset = -1;
  } else {
    _slot_offset = slot_offset;
  }

    cell = NULL;
  if(_slot_offset >= 0) {
    if(channel_offset < 0) {
      /* pick a channel offset */
      _channel_offset = msf_find_unused_slot_chanel(_slot_offset, 0);
    } else if (channel_offset > (tsch_hopping_sequence_length.val - 1)) {
      /* invalid channel offset */
      _channel_offset = -1;
    } else {
      msf_chanel_mask_t busych =  msf_avoided_slot_chanels(slot_offset);
      if ( ( (busych >> channel_offset) & 1) == 0)
          _channel_offset = channel_offset;
      else
          //chanel is busy by local or nbr cells
          _channel_offset = -1;
    }

    /* the reserved cell doesn't have any link option on */
    if(_channel_offset >= 0)
        cell = tsch_schedule_add_link(slotframe, link_options,
                                      LINK_TYPE_NORMAL, peer_addr,
                                      (uint16_t)_slot_offset,
                                      (uint16_t)_channel_offset ,
                                      0 // do not create multiple reserved links?
                                      );
    if (cell != NULL){
        cell->data = NULL;
        LOG_DBG("reserved a %s cell at slot_offset:%ld, channel_offset:%ld\n",
                msf_negotiated_cell_type_str(cell_type),
                (long)_slot_offset, (long)_channel_offset);
    } else  {
      LOG_ERR("failed to reserve a %s cell at "
              "slot_offset:%ld, channel_offset:%ld\n",
              msf_negotiated_cell_type_str(cell_type),
              (long)_slot_offset, (long)_channel_offset);
    }
  }
  else{
      LOG_DBG("reserve %s miss at slot_offset:%ld, channel_offset:%ld\n",
              msf_negotiated_cell_type_str(cell_type),
              (long)slot_offset, (long)channel_offset);
  }

  return cell;
}
/*---------------------------------------------------------------------------*/
void
msf_reserved_cell_delete_all(const linkaddr_t *peer_addr)
{
  tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
  tsch_link_t *cell;
  tsch_link_t *next_cell;

  assert(slotframe != NULL);

  for(cell = list_head(slotframe->links_list); cell != NULL; cell = next_cell) {
    next_cell = (tsch_link_t *)list_item_next(cell);
    if(cell->link_options & LINK_OPTION_RESERVED_LINK) {
      if(peer_addr == NULL ||
         linkaddr_cmp(&cell->addr, peer_addr))
      {
          msf_reserved_release_link(cell);
      }
    } else {
      /* this is not a reserved cell; skip it */
    }
  }
}

void msf_reserved_release_link(tsch_link_t *cell){
    uint16_t slot_offset = cell->timeslot;
    uint16_t channel_offset = cell->channel_offset;
    msf_housekeeping_delete_cell_later(cell);
    LOG_DBG("released a reserved cell at "
            "slot_offset:%u, channel_offset:%u\n",
            slot_offset, channel_offset);
}


bool msf_is_reserved_for_peer(const linkaddr_t *peer_addr){
    tsch_slotframe_t *slotframe = msf_negotiated_cell_get_slotframe();
    tsch_link_t *cell;
    tsch_link_t *next_cell;

    assert(slotframe != NULL);

    for(cell = list_head(slotframe->links_list); cell != NULL; cell = next_cell) {
      next_cell = (tsch_link_t *)list_item_next(cell);

      if ((cell->link_options & LINK_OPTION_RESERVED_LINK) == 0) continue;

      if(peer_addr == NULL)
              return true;

      if(linkaddr_cmp(&cell->addr, peer_addr))
          return true;
    }
    return false;
}
