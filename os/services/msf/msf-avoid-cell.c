/*
 * Copyright (c) 2020, Alexrayne.
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
 *         MSF Avoiding Cell APIs
 * \author
 *         alexrayne <alexraynepe196@gmail.com>
 */

#include "contiki.h"
#include <stdint.h>
#include "assert.h"

#include "lib/random.h"
#include "net/linkaddr.h"
#include "net/mac/tsch/tsch.h"

#include "msf-conf.h"
#include "msf-avoid-cell.h"
#include "msf-negotiated-cell.h"

#include "sys/log.h"
#define LOG_MODULE "MSF void"
#define LOG_LEVEL LOG_LEVEL_MSF

#ifndef MSF_USED_LIST_LIMIT
#ifdef TSCH_JOIN_HOPPING_SEQUENCE_SIZE
#define MSF_USED_LIST_LIMIT (MSF_SLOTFRAME_LENGTH*TSCH_JOIN_HOPPING_SEQUENCE_SIZE())
#else
#define MSF_USED_LIST_LIMIT (MSF_SLOTFRAME_LENGTH*TSCH_HOPPING_SEQUENCE_MAX_LEN)
#endif
#endif

unsigned                avoids_list_num = 0;
msf_cell_t              avoids_list[MSF_USED_LIST_LIMIT];// = {0};
const tsch_neighbor_t*  avoids_nbrs[MSF_USED_LIST_LIMIT];
AvoidOption             avoids_ops [MSF_USED_LIST_LIMIT];// = {0};

// special codes for cells.raw
enum {
    //< cell ffff.ffff - no value, use for deleted cells
    cellFREE = 0xffffffff,

    // this is trigegr value to cleanup free values from nouse list
    cellFREE_COUNT_TRIGGER = 8
};

unsigned    nouse_free_count = 0;
static void msf_unuse_cleanup();
static int msf_avoids_cell_idx(msf_cell_t x);
static int msf_uses_cell_idx(msf_cell_t x, unsigned ops);
static int msf_avoids_nbr_cell_idx(msf_cell_t x, const tsch_neighbor_t *n);

static
tsch_neighbor_t* get_addr_nbr(const linkaddr_t *addr){
    if (addr == NULL)
        return NULL;
    if (linkaddr_cmp(addr,&linkaddr_node_addr))
        return NULL;
    //if (linkaddr_cmp(addr,&tsch_broadcast_address))
    //    return NULL;
    return tsch_queue_get_nbr(addr);
}


int msf_is_avoid_cell(msf_cell_t x){
    return msf_avoids_cell_idx(x);
}

int  msf_is_avoid_local_cell(msf_cell_t x){
    return msf_uses_cell_idx(x, aoUSE_LOCAL);
}

int  msf_is_avoid_cell_at(uint16_t slot_offset, uint16_t channel_offset){
    return msf_is_avoid_cell( msf_cell_at(slot_offset, channel_offset) );
}

AvoidOptionsResult  msf_is_avoid_link_cell(const tsch_link_t* x){
    return msf_is_avoid_nbr_cell(msf_cell_of_link(x), get_addr_nbr(&x->addr));
}

AvoidOptionsResult  msf_is_avoid_nbr_cell(msf_cell_t cell, const tsch_neighbor_t *n){
    int x = msf_avoids_nbr_cell_idx(cell, n);
    if (x < 0)
        return x;
    x = avoids_ops[x];
    return x;
}

static
int msf_avoids_cell_idx(msf_cell_t x){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = avoids_list_num; idx > 0; --idx, cell++){
        if (cell->raw == x.raw){
            int idx = cell-avoids_list;
            if (avoids_ops[idx]& aoUSE)
                return idx;
        }
    }
    return -1;
}

static
int msf_uses_cell_idx(msf_cell_t x, unsigned ops){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = avoids_list_num; idx > 0; --idx, cell++){
        if (cell->raw == x.raw){
            int i = cell-avoids_list;
            if ((avoids_ops[i] & ops) != 0)
                return i;
        }
    }
    return -1;
}

msf_chanel_mask_t msf_avoided_slot_chanels(uint16_t slot_offset){
    msf_chanel_mask_t res = 0;
    msf_cell_t* cell = avoids_list;
    enum {
        BITNMSK = ((sizeof(msf_chanel_mask_t)<<3) -1),
    };

    for (unsigned idx = avoids_list_num; idx > 0; --idx, cell++){
        if (cell->field.slot == slot_offset){
#ifdef TSCH_JOIN_HOPPING_SEQUENCE_SIZE
            unsigned ch = cell->field.chanel % TSCH_JOIN_HOPPING_SEQUENCE_SIZE();
#else
            unsigned ch = cell->field.chanel % tsch_hopping_sequence_length.val;
#endif
            res |= 1 << (ch & BITNMSK);
        }
    }
    return res;
}


static
int msf_avoids_nbr_cell_idx(msf_cell_t x, const tsch_neighbor_t *n){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == x.raw)
        if (avoids_nbrs[idx] == n)
        if (avoids_ops[idx]& aoUSE)
            return idx;
    }
    return -1;
}

AvoidOptionsResult  msf_is_avoid_slot_range(uint16_t slot_offset, AvoidOptions range ){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = avoids_list_num; idx > 0; --idx, cell++){
        if (cell->field.slot == slot_offset){
            int idx = cell-avoids_list;
            if (avoids_ops[idx]& range)
                return idx;
        }
    }
    return -1;
}

AvoidOptionsResult  msf_is_avoid_slot(uint16_t slot_offset){
    return msf_is_avoid_slot_range(slot_offset, aoUSE);
}


AvoidOptionsResult msf_is_avoid_local_slot(uint16_t slot_offset){
    return msf_is_avoid_slot_range(slot_offset, aoUSE_LOCAL);
}

// check for RX cells in slots
AvoidOptionsResult  msf_is_avoid_slot_nbr_range(uint16_t slot_offset, const tsch_neighbor_t * n
                                                , AvoidOptions range)
{
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (avoids_nbrs[idx] == n)
        if (cell->field.slot == slot_offset){
            if ((avoids_ops[idx] & range) != 0)
                return avoids_ops[idx];
        }
    }
    return -1;
}

AvoidOptionsResult  msf_is_avoid_local_slot_nbr(uint16_t slot_offset, const tsch_neighbor_t * n){
    return msf_is_avoid_slot_nbr_range(slot_offset, n, aoUSE_LOCAL);
}



AvoidOptionsResult  msf_is_avoid_nbr_slot(uint16_t slot_offset, const tsch_neighbor_t *n){
    return msf_is_avoid_slot_nbr_range(slot_offset, n, aoUSE);
}

// check for RX cells in slot
const tsch_neighbor_t*  msf_is_avoid_local_slot_rx(uint16_t slot_offset){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (cell->field.slot == slot_offset){
            if ((avoids_ops[idx] & aoUSE_LOCAL) != 0)
            if ((avoids_ops[idx] & aoTX) == 0)
                return avoids_nbrs[idx];
        }
    }
    return NULL;
}

/* @brief check that cell is used by 1hop nbr, not local
 * @return < 0 - no cell found
 *         >= 0 - have some cell
*/
int  msf_is_avoid_close_slot_outnbr(uint16_t slot_offset, const tsch_neighbor_t *skip_nbr)
{
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (avoids_nbrs[idx] != skip_nbr)
        if (cell->field.slot == slot_offset){
            if ((avoids_ops[idx] & aoUSE) == aoUSE_REMOTE_1HOP)
                return avoids_ops[idx];
        }
    }
    return -1;
}



/*
 * @return > 0 - appends new cell
 *         = 0 - change current
 *         < 0 - nothing change for exist cell
 */
static
AvoidResult msf_avoid_mark_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n, unsigned ops){
    int idxnbr = msf_avoids_nbr_cell_idx(x, n);
    if (idxnbr>=0){
        AvoidOption was  = avoids_ops[idxnbr];

        if ((was & aoUSE_LOCAL) > (ops & aoUSE_LOCAL)){
            //do not trust far cells, since they maybe a echo returned in cycle
            if (( ops & aoUSE_REMOTE) > aoUSE_REMOTE_1HOP)
                return arEXIST_KEEP;

            // do not override local cell by remote.
            // try update remote status only
            avoids_ops[idxnbr] = ops | (was & ~aoUSE_REMOTE);
        }
        else {
            avoids_ops[idxnbr] = ops | (was & ~aoUSE);
        }

        LOG_DBG("avoid %u+%u/%x->%x "
                , (unsigned)(x.field.slot), (unsigned)(x.field.chanel)
                , was, avoids_ops[idxnbr]);
        LOG_DBG_LLADDR( tsch_queue_get_nbr_address(n) );
        LOG_DBG_("\n");
        if (avoids_ops[idxnbr] != was){
            avoids_ops[idxnbr] &= ~aoMARK;
            return arEXIST_CHANGE;
        }
        return arEXIST_KEEP;
    }

    //local and close cells are sure valid, and so it ready for concurent
    //  allow enum them as concurent nbr for same cell
    bool force_new = ((ops & aoUSE_REMOTE) <= aoUSE_REMOTE_1HOP);

    if (!force_new) {
    int idx = msf_avoids_cell_idx(x);
    if (idx >= 0){
        AvoidOption was  = avoids_ops[idx];
        // check that not override by far cells
        if ((was & aoUSE) > (ops & aoUSE)) {
            //override current cell, by more close
            avoids_nbrs[avoids_list_num] = n;
            avoids_ops[idx]  = ops | (was & ~aoUSE);

        LOG_DBG("avoid %u+%u/%x->%x "
                , (unsigned)(x.field.slot), (unsigned)(x.field.chanel)
                , was, avoids_ops[idx]);
        LOG_DBG_LLADDR( tsch_queue_get_nbr_address(n) );
        LOG_DBG_("\n");

        if (avoids_ops[idx] != was){
            avoids_ops[idx] &= ~aoMARK;
            return arEXIST_CHANGE;
        }
        }
        return arEXIST_KEEP;
    }
    }// if (!force_new)

    if (avoids_list_num < MSF_USED_LIST_LIMIT){
        LOG_DBG("avoid+ %u+%u/%x ->"
                , (unsigned)(x.field.slot), (unsigned)(x.field.chanel)
                , ops);
        LOG_DBG_LLADDR( tsch_queue_get_nbr_address(n) );
        LOG_DBG_("\n");
        avoids_list[avoids_list_num] = x;
        avoids_nbrs[avoids_list_num] = n;
        avoids_ops[avoids_list_num]  = ops;
        ++avoids_list_num;
    }
    else {
        LOG_WARN("rich limit reserve for cell %lx\n", (unsigned long)x.raw);
    }
    return arNEW;
}

AvoidResult msf_avoid_link_cell(const tsch_link_t* x){
    return msf_avoid_nbr_link_cell( x, get_addr_nbr(&x->addr) );
}

AvoidResult msf_avoid_nbr_link_cell(const tsch_link_t* x, const tsch_neighbor_t *n){
    return msf_avoid_mark_nbr_cell(msf_cell_of_link(x), n
                                , aoUSE_LOCAL
                                    | msf_avoid_link_option_xx(x->link_options)
                                );
}

// avoids cell, that can't move - autonomous calls are.
AvoidResult msf_avoid_fixed_link_cell(const tsch_link_t* x){
    return msf_avoid_mark_nbr_cell(msf_cell_of_link(x), get_addr_nbr(&x->addr)
                                , aoUSE_LOCAL | aoFIXED
                                    | msf_avoid_link_option_xx(x->link_options)
                                );
}

AvoidResult msf_avoid_nbr_use_cell(msf_cell_t x, const tsch_neighbor_t *n, AvoidOptions userange){
    return msf_avoid_mark_nbr_cell(x, n, userange);
}



void msf_unvoid_all_cells(){
    LOG_DBG("unvoid all\n");
    // protect own slot0 - since it is for EB
    avoids_list[0].raw = 0;
    avoids_nbrs[0]     = NULL;
    avoids_ops[0]      = aoDEFAULT | aoUSE_LOCAL;

    avoids_list_num  = 1;
    nouse_free_count = 0;
}

static
void msf_unmark_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n, unsigned ops){
    int idx = msf_avoids_nbr_cell_idx(x, n);
    if (idx >= 0){
        msf_cell_t* cell = avoids_list + idx;

        avoids_ops[idx]  &= ~ops;
        if ( (avoids_ops[idx] & aoUSE) != 0) {
            LOG_DBG("unuse %u+%u = %x\n"
                        , (unsigned)cell->field.slot
                        , (unsigned)cell->field.chanel
                        , avoids_ops[idx]);
            return;
        }

        LOG_DBG("free %u+%u ->", (unsigned)(cell->field.slot), (unsigned)(cell->field.chanel));
        LOG_DBG_LLADDR( tsch_queue_get_nbr_address(avoids_nbrs[idx]) );
        LOG_DBG_("\n");

        cell->raw = cellFREE;
        avoids_nbrs[idx]     = NULL;
        nouse_free_count++;
        if (nouse_free_count >= cellFREE_COUNT_TRIGGER){
            msf_unuse_cleanup();
        }
    }
}


void msf_unvoid_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n){
    msf_unmark_nbr_cell(x, n, aoUSE);
}

void msf_unvoid_link_cell(const tsch_link_t* x){
    msf_unmark_nbr_cell(msf_cell_of_link(x), get_addr_nbr(&x->addr), aoUSE);
}

void msf_release_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n){
    msf_unmark_nbr_cell(x, n, aoUSE_REMOTE);
}

// unvoids nbr local using cells
void msf_release_nbr_cell_local(msf_cell_t x, const tsch_neighbor_t *n){
    msf_unmark_nbr_cell(x, n, aoUSE_LOCAL);
}


static
void msf_unmark_nbr_cells(const tsch_neighbor_t* n, unsigned ops){
    LOG_DBG("unvoid:%x ->%p\n", ops, n);

    msf_cell_t*             cells= avoids_list;
    const tsch_neighbor_t** nbrs = avoids_nbrs;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cells){
        if (nbrs[idx] != n)
            continue;
        if (cells->raw == cellFREE) continue;

        avoids_ops[idx]  &= ~ops;
        if ( (avoids_ops[idx] & aoUSE) != 0) {
            LOG_DBG("unuse %u+%u = %x\n"
                        , (unsigned)cells->field.slot
                        , (unsigned)cells->field.chanel
                        , avoids_ops[idx]);
            continue;
        }

        ++nouse_free_count;
        LOG_DBG("free %u+%u\n", (unsigned)cells->field.slot, (unsigned)cells->field.chanel);
        cells->raw = cellFREE;
    }
    if (nouse_free_count >= cellFREE_COUNT_TRIGGER){
        msf_unuse_cleanup();
    }
}

//   mark cell as no aoUSE_REMOTE_xxx.
//          if range < stored one, cell remark as aoDROPED, else unvoids
//   @return <0 - no cells unused
//           0  - cells unused
//           >0 - cell is marked as aoDROPED, return last cell  AvoidOptions
int  msf_unvoid_drop_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n, AvoidOption range)
{
    LOG_DBG("unuse:%lx ->/%x\n", (unsigned long)x.raw, (unsigned)range);

    msf_cell_t*             cells= avoids_list;

    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cells){
        if (cells->raw != x.raw) continue;

        //only remote cells drop
        if ( (avoids_ops[idx] & aoUSE_LOCAL) != 0)
            continue;

        int save = avoids_ops[idx];
        // for close cells make a drop.
        if ((save&aoUSE_REMOTE) <= range) {

            // close cells should validates for exact nbrs, since thay can concurent
            if ((save&aoUSE_REMOTE) <= aoUSE_REMOTE_1HOP)
            if ( avoids_nbrs[idx] != n )
                continue;

            avoids_ops[idx]  &= ~(aoUSE_REMOTE | aoMARK);
            //assign nbr of clearer
            avoids_nbrs[idx]           = n;

            LOG_DBG("unuse %u+%u = %x\n"
                        , (unsigned)cells->field.slot
                        , (unsigned)cells->field.chanel
                        , avoids_ops[idx]);
        }
        else {
            // far cells unvoids
            ++nouse_free_count;
            LOG_DBG("free %u+%u\n", (unsigned)cells->field.slot, (unsigned)cells->field.chanel);
            cells->raw = cellFREE;
            save = 0;
        }

        return save;
    }
    return -1;
}

void msf_unvoid_nbr_cells(const tsch_neighbor_t* n){
    msf_unmark_nbr_cells(n, aoUSE);
}

void msf_release_nbr_cells(const tsch_neighbor_t* n){
    msf_unmark_nbr_cells(n, aoUSE_REMOTE);
}


void msf_release_nbr_cells_local(const tsch_neighbor_t* n){
    msf_unmark_nbr_cells(n, aoUSE_LOCAL);
}

static
void msf_unuse_cleanup(){
    LOG_DBG("cleanup for %u\n", nouse_free_count);

    msf_cell_t*             cell = avoids_list;
    const tsch_neighbor_t** nbrs = avoids_nbrs;

    //drop tail free cells
    for (int idx = avoids_list_num-1; idx >= 0; --idx){
        if (cell[idx].raw != cellFREE)
            break;
        avoids_list_num = idx;
    }

    for (int idx = avoids_list_num-1; idx >= 0; --idx){
        if (cell[idx].raw != cellFREE)
            continue;
        int k = idx;
        for (; k > 0; --k){
            if ( cell[idx-1].raw != cellFREE)
                break;
        }
        memmove(cell+k, cell+idx, sizeof(cell[0])*(avoids_list_num-idx) );
        memmove(nbrs+k, nbrs+idx, sizeof(nbrs[0])*(avoids_list_num-idx) );
        memmove(avoids_ops+k, avoids_ops+idx, sizeof(avoids_ops[0])*(avoids_list_num-idx) );
        avoids_list_num -= (idx-k);
        idx = k-2;
    }
    nouse_free_count = 0;
}



// mark avoid cell defult - denotes, that all nbrs are know it.
//      default cells are ignored in enumerations
void msf_avoid_link_cell_default(const tsch_link_t* x){
    int idx = msf_avoids_nbr_cell_idx( msf_cell_of_link(x), get_addr_nbr(&x->addr) );
    if (idx >= 0)
        avoids_ops[idx] |= aoDEFAULT;
}

//@return - amount of avoid cells
int msf_avoid_num_local_cells(){
    return msf_avoid_num_cells_at_range(aoUSE_LOCAL|aoMARK);
}

int msf_avoid_num_cells_at_range(unsigned range)
{
    int res = 0;
    msf_cell_t* cell = avoids_list;
    unsigned skip_mark    = aoDEFAULT | (aoMARK & ~range);
    unsigned range_for    = range & aoUSE;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == cellFREE) continue;
        if ( (avoids_ops[idx] & skip_mark) != 0 ) continue;
        if ( (avoids_ops[idx] & aoUSE) != range_for )
            continue;
        ++res;
    }
    return res;
}

int msf_avoid_num_cells_in_range(unsigned range){
    int res = 0;
    msf_cell_t* cell = avoids_list;
    unsigned skip_mark    = aoDEFAULT | (aoMARK & ~range);
    unsigned range_for    = range & aoUSE;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == cellFREE) continue;
        if ( (avoids_ops[idx] & skip_mark) != 0 ) continue;
        if ( (avoids_ops[idx] & aoUSE) > range_for )
            continue;
        ++res;
    }
    return res;
}

// @result - cells->head.num_cells= amount of filled cells
// @return - >0 - amount of cells append
// @return - =0 - no cells to enumerate
// @return - <0 - no cells append, not room to <cells>
int msf_avoid_enum_cells(SIXPCellsPkt* pkt, unsigned limit
                        , unsigned range_ops
                        , tsch_neighbor_t* nbr_skip)
{
    if (pkt)
    if ( pkt->head.num_cells >= limit)
        return -1;

    int res = 0;
    msf_cell_t* cell = avoids_list;

    unsigned skip_mark    = aoDEFAULT | (aoMARK & ~range_ops);
    unsigned range = range_ops & aoUSE;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == cellFREE) continue;
        if ( avoids_nbrs[idx] == nbr_skip) continue;
        unsigned ops = avoids_ops[idx];
        if ( (ops & skip_mark) != 0 ) continue;

        if ( (ops & aoUSE)  != range )
            continue;

        if ( (range_ops & aoTX) && !(ops & aoTX)  ) continue;
        if ( (range_ops & aoFIXED) && !(ops & aoFIXED) ) continue;

        ++res;
        if (pkt)
        //if ( pkt->head.num_cells < limit)
        if (!sixp_pkt_cells_have(pkt, *cell))
        {
            //msf_cell_t* x = pkt->cells + pkt->head.num_cells;
            //x->field.slot   = cell->field.slot;
            //x->field.chanel = cell->field.chanel;
            pkt->cells[pkt->head.num_cells].raw = cell->raw;
            ++(pkt->head.num_cells);
            if (pkt->head.num_cells >= limit)
                return res;
        }
    }
    return res;
}


/* Drop from pkt cells that belongs nbr */
int msf_avoid_clean_cells_for_nbr(SIXPCellsPkt* pkt, const tsch_neighbor_t *n){
    int res = 0;
    sixp_cell_t* cell = pkt->cells;

    for (unsigned idx = 0; idx < pkt->head.num_cells; ++idx, cell++){
        msf_cell_t x;
        x.field.slot    = cell->field.slot;
        x.field.chanel  = cell->field.chanel;
        if (msf_is_avoid_nbr_cell(x, n) < 0 ){
            pkt->cells[res] = *cell;
            ++res;
        }
    }
    pkt->head.num_cells = res;
    return res;
}

// NRSF use it to notify cells exposed to nbrs
void msf_avoid_mark_all_fresh(){
    for (unsigned idx = 0; idx < avoids_list_num; ++idx){
        avoids_ops[idx]  |= aoMARK;
    }
}

// Mark local cell as aoRELOCATE
AvoidOptionsResult msf_avoid_relocate_link_cell(const tsch_link_t* x){
    int idxnbr = msf_avoids_nbr_cell_idx( msf_cell_of_link(x), get_addr_nbr(&x->addr) );
    if (idxnbr >= 0){
        avoids_ops[idxnbr]  |= aoRELOCATE;
        return avoids_ops[idxnbr];
    }
    else
        return arNO;
}

// @brief Mark cells to aoEXPOSED. use it to notify cells exposed to nbrs.
//  for aoRELOCATE cells, this use to show that relocation started/done
AvoidOptionsResult msf_avoid_expose_link_cell(const tsch_link_t* x){
    return msf_avoid_expose_nbr_cell(msf_cell_of_link(x), get_addr_nbr(&x->addr));
}

AvoidOptionsResult msf_avoid_expose_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n){
    LOG_DBG("mark cell %u+%u ->\n", x.field.slot, x.field.chanel);
    int idxnbr = msf_avoids_nbr_cell_idx( x, n );
    if (idxnbr >= 0){
        avoids_ops[idxnbr]  |= aoMARK;
        return avoids_ops[idxnbr];
    }
    else
        return arNO;
}

// this unvoids all cells that are no any aoUSE
//      such cells registred NRSF for deleted cells
void msf_release_unused(){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (cell->raw == cellFREE) continue;
        if (( avoids_ops[idx] & aoUSE) != 0 ) continue;

        ++nouse_free_count;
        avoids_list[idx].raw = cellFREE;
    }
    if (nouse_free_count >= cellFREE_COUNT_TRIGGER){
        msf_unuse_cleanup();
    }
}


/* @brief takes first avail cell marked as aoWAIT_RELOC
 * @return - nbr for enumerated cells
 *           cells->head.meta - AvoidOption of enumed cells
 * */
tsch_neighbor_t* msf_avoid_append_cell_to_relocate(SIXPCellsPkt* pkt){
    assert(pkt != NULL);

    msf_cell_t* cell = avoids_list;

    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == cellFREE) continue;
        if ( (avoids_ops[idx] & aoRELOCATE)  == 0 ) continue;
        if ( (avoids_ops[idx] & aoMARK) != 0 ) continue;
        // only local cells can relocate
        if ( (avoids_ops[idx] & aoUSE_LOCAL)  == 0 ) continue;

        //if ( pkt->head.num_cells < limit)
        {
            msf_cell_t* x = pkt->cells + pkt->head.num_cells;
            x->field.slot   = cell->field.slot;
            x->field.chanel = cell->field.chanel;
            pkt->head.meta  = avoids_ops[idx];
            ++(pkt->head.num_cells);
            return (tsch_neighbor_t*) avoids_nbrs[idx];
        }
    }
    return NULL;
}

/* @brief takes first avail cell marked as aoWAIT_RELOC
// @result - cells->head.num_cells= amount of filled cells
// @return - >0 - amount of cells append
// @return - =0 - no cells to enumerate
 * */
int msf_avoid_append_nbr_cell_to_relocate(SIXPCellsPkt* pkt, tsch_neighbor_t *n){
    assert(pkt != NULL);

    msf_cell_t* cell = avoids_list;

    for (unsigned idx = 0; idx < avoids_list_num; ++idx, cell++){
        if (cell->raw == cellFREE) continue;
        if (avoids_nbrs[idx] != n) continue;
        if ( (avoids_ops[idx] & aoRELOCATE)  == 0 ) continue;
        // only local cells can relocate
        if ( (avoids_ops[idx] & aoUSE_LOCAL)  == 0 ) continue;
        if ( (avoids_ops[idx] & aoMARK) != 0 ) continue;

        //if ( pkt->head.num_cells < limit)
        {
            msf_cell_t* x = pkt->cells + pkt->head.num_cells;
            x->field.slot   = cell->field.slot;
            x->field.chanel = cell->field.chanel;
            pkt->head.meta  = avoids_ops[idx];
            ++(pkt->head.num_cells);
            return 1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
static
const char* cell_arrow(int x){
    return msf_negotiated_cell_type_arrow(
            (x & aoTX)? MSF_NEGOTIATED_CELL_TYPE_TX
                                    : MSF_NEGOTIATED_CELL_TYPE_RX
                                         );
}


// @brief Dumps nbr cell
#define MSF_PRINTF(...) LOG_OUTPUT(__VA_ARGS__)
void msf_avoid_dump_idx_cell(int idx){
    msf_cell_t* cell = &(avoids_list[idx]);
    MSF_PRINTF("[%02u+%02u] : %02x %s", cell->field.slot, cell->field.chanel
                    , avoids_ops[idx], cell_arrow(avoids_ops[idx])
                    );
    LOG_LLADDR(LOG_LEVEL_ERR, tsch_queue_get_nbr_address(avoids_nbrs[idx]) );
    MSF_PRINTF("\n");
}

void msf_avoid_dump_nbr_cell(msf_cell_t x, const tsch_neighbor_t *n){
    int idx = msf_avoids_nbr_cell_idx(x, n);
    if (idx >= 0)
        msf_avoid_dump_idx_cell(idx);
}

void msf_avoid_dump_peer_cell(msf_cell_t x, const linkaddr_t * peer_addr){
    msf_avoid_dump_nbr_cell(x, get_addr_nbr(peer_addr) );
}

void msf_avoid_dump_cell(msf_cell_t x){
    msf_cell_t* cell = avoids_list;
    bool ok = false;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (cell->raw == x.raw){
            msf_avoid_dump_idx_cell(idx);
            ok = true;
        }
    }
    if (!ok){
        MSF_PRINTF("[%02u+%02u] : #\n", x.field.slot, x.field.chanel);
    }
}

void msf_avoid_dump_local_cells(void){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (cell->raw != cellFREE)
        if ( (avoids_ops[idx] & aoUSE_LOCAL) != 0 )
            msf_avoid_dump_idx_cell(idx);
    }
}

void msf_avoid_dump_slot(unsigned slot){
    msf_cell_t* cell = avoids_list;
    for (unsigned idx = 0; idx < avoids_list_num; ++idx, ++cell){
        if (cell->field.slot == slot)
        if ( (avoids_ops[idx] & aoUSE_LOCAL) != 0 )
            msf_avoid_dump_idx_cell(idx);
    }
}
