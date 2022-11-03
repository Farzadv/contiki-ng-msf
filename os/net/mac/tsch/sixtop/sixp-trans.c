/*
 * Copyright (c) 2016, Yasuyuki Tanaka
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
 * \addtogroup sixtop
 * @{
 */
/**
 * \file
 *         Transaction Management for 6top Protocol (6P)
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inf.ethz.ch>
 */

#include "contiki-lib.h"
#include "lib/assert.h"
#include <string.h>

#include "sixtop.h"
#include "sixtop-conf.h"
#include "sixp-nbr.h"
#include "sixp-trans.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "6top-trans"
#define LOG_LEVEL LOG_LEVEL_6TOP

/**
 * \brief 6P Transaction Data Structure (for internal use)
 */
typedef struct sixp_trans {
  struct sixp_trans *next;
  const sixtop_sf_t *sf;
  linkaddr_t peer_addr;
  uint8_t seqno;
  sixp_pkt_cmd_t cmd;
  sixp_trans_state_t state;
  sixp_trans_mode_t mode;
  struct {
    sixp_sent_callback_t func;
    void *arg;
    uint16_t arg_len;
  } callback;
  struct ctimer timer;
} sixp_trans_t;

void sixp_handle_trans_timeout(void *ptr);
void sixp_process_trans(void *ptr);
static void schedule_trans_process(sixp_trans_t *trans);
static sixp_trans_mode_t determine_trans_mode(const sixp_pkt_t *req);

MEMB(trans_memb, sixp_trans_t, SIXTOP_MAX_TRANSACTIONS);
LIST(trans_list);

/*---------------------------------------------------------------------------*/
void sixp_handle_trans_timeout(void *ptr)
{
  sixp_trans_t *trans = (sixp_trans_t *)ptr;

  assert(trans != NULL);
  if(trans == NULL) {
    return;
  }

  LOG_DBG("trans(%p) timeout wake\n", trans);

  if(trans->sf->timeout != NULL) {
    trans->sf->timeout(trans->cmd,
                       (const linkaddr_t *)&trans->peer_addr);
  }

  (void)sixp_trans_transit_state(trans, SIXP_TRANS_STATE_TERMINATING);
}
/*---------------------------------------------------------------------------*/
static void
start_trans_timer(sixp_trans_t *trans)
{
  LOG_DBG("trans(%p) timeout(%p) %lu\n", trans, &trans->timer
                          , (unsigned long)trans->sf->timeout_interval);
  ctimer_set(&trans->timer, trans->sf->timeout_interval,
          sixp_handle_trans_timeout, trans);
}
/*---------------------------------------------------------------------------*/
void sixp_process_trans(void *ptr)
{
  sixp_trans_t *trans = (sixp_trans_t *)ptr;

  assert(trans != NULL);
  if(trans == NULL) {
    return;
  }

  /* make sure that the timer is stopped */
  ctimer_stop(&trans->timer);

  LOG_DBG("trans [peer_addr:");
  LOG_DBG_LLADDR((const linkaddr_t *)&trans->peer_addr);
  LOG_DBG_(", seqno:%u]... \n", trans->seqno);

  /* state-specific operation */
  if(trans->state == SIXP_TRANS_STATE_TERMINATING) {
    /* handle the terminating state first */
    LOG_INFO("6P-trans: trans [peer_addr:");
    LOG_INFO_LLADDR((const linkaddr_t *)&trans->peer_addr);
    LOG_INFO_(", seqno:%u] is going to be freed\n", trans->seqno);
    sixp_trans_free(trans);
    return;
  }

  switch(trans->state) {
    /* do for others */
    case SIXP_TRANS_STATE_RESPONSE_SENT:
    case SIXP_TRANS_STATE_RESPONSE_RECEIVED:
      if(trans->mode == SIXP_TRANS_MODE_2_STEP) {
        (void)sixp_trans_transit_state(trans, SIXP_TRANS_STATE_TERMINATING);
      }
      break;
    case SIXP_TRANS_STATE_CONFIRMATION_SENT:
    case SIXP_TRANS_STATE_CONFIRMATION_RECEIVED:
      (void)sixp_trans_transit_state(trans, SIXP_TRANS_STATE_TERMINATING);
      break;
    case SIXP_TRANS_STATE_TERMINATING:
    default:
      break;
  }

  if(trans->state != SIXP_TRANS_STATE_TERMINATING) {
    /* set the timer with a timeout values defined by the SF */
    start_trans_timer(trans);
  }
}
/*---------------------------------------------------------------------------*/
static void
schedule_trans_process(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return;
  }

  ctimer_stop(&trans->timer);
  ctimer_set(&trans->timer, 0, sixp_process_trans, trans); /* expires immediately */
}
/*---------------------------------------------------------------------------*/
void
sixp_trans_free(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return;
  }

  LOG_INFO("trans %p (state %x) free\n", trans, trans->state);

  if (trans->state == SIXP_TRANS_STATE_WAIT_FREE) {
    trans->state = SIXP_TRANS_STATE_UNAVAILABLE;
  } else {
    /* stop the timer that may still be running */
    ctimer_stop(&trans->timer);
    /*
     * remove this trans from the list so that a new trans can be
     * started with the same peer
     */
    list_remove(trans_list, trans);
  }

  if(trans->state == SIXP_TRANS_STATE_REQUEST_SENDING ||
     trans->state == SIXP_TRANS_STATE_RESPONSE_SENDING ||
     trans->state == SIXP_TRANS_STATE_CONFIRMATION_SENDING) {
    /* memory is freed later, when mac_callback in sixp.c is called */
    trans->state = SIXP_TRANS_STATE_WAIT_FREE;
  } else {
    memset(trans, 0, sizeof(sixp_trans_t));
    memb_free(&trans_memb, trans);
    SIXP_AFTER_TRANS_FREE();
  }
}
/*---------------------------------------------------------------------------*/
static sixp_trans_mode_t
determine_trans_mode(const sixp_pkt_t *req)
{
  uint16_t cell_list_len;

  assert(req != NULL);
  if(req == NULL) {
    return SIXP_TRANS_MODE_UNAVAILABLE;
  }

  /*
   * We consider a transaction as 3-step if and only if its request command is
   * either Add or Delete AND its cell_list is empty. Otherwise, 2-step.
   */
  if(req->type == SIXP_PKT_TYPE_REQUEST &&
     (req->code.cmd == SIXP_PKT_CMD_ADD ||
      req->code.cmd == SIXP_PKT_CMD_DELETE) &&
     sixp_pkt_get_cell_list(req->type, (sixp_pkt_code_t)req->code.value,
                            NULL, &cell_list_len,
                            req->body, req->body_len) == 0 &&
     cell_list_len == 0) {
    return SIXP_TRANS_MODE_3_STEP;
  }

  return SIXP_TRANS_MODE_2_STEP;
}
/*---------------------------------------------------------------------------*/
int
sixp_trans_transit_state(sixp_trans_t *trans, sixp_trans_state_t new_state)
{
  sixp_nbr_t *nbr;
  int ret_val;

  assert(trans != NULL);
  if(trans == NULL) {
      /* trans == NULL */
      LOG_ERR("6top: invalid argument, trans is NULL\n");
      return -1;
  }

  assert(new_state != SIXP_TRANS_STATE_UNAVAILABLE);
  LOG_INFO("trans %p state %x->%x\n", trans, trans->state, new_state);

  /* enforce state transition rules  */
      bool ok = (new_state == SIXP_TRANS_STATE_TERMINATING);
      if (!ok){
          // check that trans direction keep allowed, or comes to final (indirected) state
          ok = ((new_state & SIXP_TRANS_STATE_IO_Msk) == 0)
             || ((new_state & SIXP_TRANS_STATE_IO_Msk & trans->state) != 0);

          //check that next sten is successed from current
          if (ok)
          ok = ((new_state & SIXP_TRANS_STATE_STEP_Msk)
                  == ( (trans->state& SIXP_TRANS_STATE_STEP_Msk) + SIXP_TRANS_STATE_STEP1));
          if (ok)
          if ((new_state & SIXP_TRANS_STATE_STEP_Msk) >= SIXP_TRANS_STATE_STEP4)
              ok = (trans->mode == SIXP_TRANS_MODE_3_STEP);
      }
  if (ok){
      if(new_state == SIXP_TRANS_STATE_REQUEST_SENT) {
        /* next_seqno should have been updated in sixp_output() */
      } else if(new_state == SIXP_TRANS_STATE_RESPONSE_SENT) {
        if((nbr = sixp_nbr_find(&trans->peer_addr)) == NULL) {
          LOG_ERR("6top: cannot update next_seqno\n");
        } else if(trans->cmd == SIXP_PKT_CMD_CLEAR) {
          /* next_seqno must have been reset to 0 already; keep it */
          assert(sixp_nbr_get_next_seqno(nbr) == 0);
        } else {
          /* override next_seqno with the one in the request */
          sixp_nbr_set_next_seqno(nbr, trans->seqno);
          sixp_nbr_increment_next_seqno(nbr);
        }
      }
      trans->state = new_state;
      schedule_trans_process(trans);
      ret_val = 0;
  } else {
    /* invalid transition */
    LOG_ERR("6P-trans: invalid transition, from %x to %x, on trans %p\n",
            trans->state, new_state, trans);
    /* inform the corresponding SF */
    assert(trans->sf != NULL);
    if(trans->sf->error != NULL) {
      trans->sf->error(SIXP_ERROR_INVALID_TRANS_STATE_TRANSITION,
                sixp_trans_get_cmd(trans),
                sixp_trans_get_seqno(trans),
                sixp_trans_get_peer_addr(trans));
    }
    ret_val = -1;
  }// if (ok)
  return ret_val;
}
/*---------------------------------------------------------------------------*/
const sixtop_sf_t *
sixp_trans_get_sf(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return NULL;
  } else {
    return trans->sf;
  }
}
/*---------------------------------------------------------------------------*/
sixp_pkt_cmd_t
sixp_trans_get_cmd(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return SIXP_PKT_CMD_UNAVAILABLE;
  }
  return trans->cmd;
}
/*---------------------------------------------------------------------------*/
sixp_trans_state_t
sixp_trans_get_state(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return SIXP_TRANS_STATE_UNAVAILABLE;
  }
  return trans->state;
}
/*---------------------------------------------------------------------------*/
int16_t
sixp_trans_get_seqno(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    LOG_ERR("6P-trans: sixp_trans_get_seqno() fails because trans is NULL\n");
    return -1;
  }
  return trans->seqno;
}
/*---------------------------------------------------------------------------*/
sixp_trans_mode_t
sixp_trans_get_mode(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    LOG_ERR("6P-trans: sixp_trans_get_mode() fails because trans is NULL\n");
    return SIXP_TRANS_STATE_UNAVAILABLE;
  }
  return trans->mode;
}
/*---------------------------------------------------------------------------*/
const linkaddr_t *
sixp_trans_get_peer_addr(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return NULL;
  } else {
    return (const linkaddr_t *)&trans->peer_addr;
  }
}
/*---------------------------------------------------------------------------*/
sixp_trans_t* sixp_current_trans = NULL;

void
sixp_trans_invoke_callback(sixp_trans_t *trans, sixp_output_status_t status)
{
  assert(trans != NULL);

  if(trans == NULL || trans->callback.func == NULL) {
    return;
  }
  sixp_current_trans = trans;
  trans->callback.func(trans->callback.arg, trans->callback.arg_len,
                       &trans->peer_addr, status);
}
/*---------------------------------------------------------------------------*/
void
sixp_trans_set_callback(sixp_trans_t *trans,
                        sixp_sent_callback_t func, void *arg, uint16_t arg_len)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return;
  }
  trans->callback.func = func;
  trans->callback.arg = arg;
  trans->callback.arg_len = arg_len;
}
/*---------------------------------------------------------------------------*/
sixp_trans_t *
sixp_trans_alloc(const sixp_pkt_t *pkt, const linkaddr_t *peer_addr)
{
  const sixtop_sf_t *sf;
  sixp_trans_t *trans;

  assert(pkt != NULL && peer_addr != NULL);
  if(pkt == NULL || peer_addr == NULL) {
    LOG_ERR("6P-trans: sixp_trans_alloc() fails because of invalid argument\n");
    return NULL;
  }

  if((sf = sixtop_find_sf(pkt->sfid)) == NULL) {
    LOG_ERR("6P-trans: sixp_trans_alloc() fails; no suitable SF [sfid:%u]\n",
            pkt->sfid);
    return NULL;
  }

  if(0) //this check has alredy evaluated by caller
  if(sixp_trans_find_for_pkt(peer_addr, pkt) != NULL) {
    LOG_ERR("6P-trans: sixp_trans_alloc() fails because another trans with ");
    LOG_ERR_LLADDR((const linkaddr_t *)peer_addr);
    LOG_ERR_("is in process\n");
    return NULL;
  }

  if((trans = memb_alloc(&trans_memb)) == NULL) {
    LOG_ERR("6P-trans: sixp_trans_alloc() fails because of lack of memory\n");
    return NULL;
  }

  memset(trans, 0, sizeof(sixp_trans_t));
  trans->sf = sf;
  trans->peer_addr = *peer_addr;
  trans->seqno = pkt->seqno;
  trans->cmd = pkt->code.value;
  trans->state = SIXP_TRANS_STATE_INIT;
  trans->mode = determine_trans_mode(pkt);
  list_add(trans_list, trans);
  start_trans_timer(trans);

  LOG_DBG("new trans(%p) sf%u [peer_addr:", trans, trans->sf->sfid);
  LOG_DBG_LLADDR((const linkaddr_t *)&trans->peer_addr);
  LOG_DBG_(", seqno:%u] code:%x\n", trans->seqno, trans->cmd);

  return trans;
}
/*---------------------------------------------------------------------------*/
sixp_trans_t *sixp_trans_head(){
    return list_head(trans_list);
}

sixp_trans_t *sixp_trans_next(sixp_trans_t* x){
    return x->next;
}

bool sixp_trans_any(){
    return list_head(trans_list) != NULL;
}

sixp_trans_t *
sixp_trans_find(const linkaddr_t *peer_addr)
{
  sixp_trans_t *trans;

  assert(peer_addr != NULL);
  if(peer_addr == NULL) {
    return NULL;
  }

  /*
   * XXX: we don't support concurrent 6P transactions which is mentioned in
   * Section 4.3.3, draft-ietf-6tisch-6top-protocol-03.
   *
   * The assumption here is that there is one transactions for a single peer at
   * most.
   */
  for(trans = list_head(trans_list);
      trans != NULL; trans = trans->next) {
    if( linkaddr_cmp(peer_addr, &trans->peer_addr) ) {
      return trans;
    }
  }

  return NULL;
}

/*---------------------------------------------------------------------------*/
sixp_trans_t *sixp_trans_find_for_sfid(const linkaddr_t *peer_addr, uint8_t sfid){
    sixp_trans_t *trans;

    assert(peer_addr != NULL);
    if(peer_addr == NULL) {
      return NULL;
    }

    for(trans = list_head(trans_list);
        trans != NULL; trans = trans->next) {
      if( linkaddr_cmp(peer_addr, &trans->peer_addr) )
      if( trans->sf->sfid == sfid)
      {
        return trans;
      }
    }

    return NULL;
}

/*---------------------------------------------------------------------------*/
sixp_trans_t *sixp_trans_find_for_pkt(const linkaddr_t *peer_addr
                                        , const sixp_pkt_t* pkt
                                        )
{
  sixp_trans_t *trans;

  assert(peer_addr != NULL);
  if(peer_addr == NULL) {
    return NULL;
  }

  /*
   * Here support concurrent 6P transactions which is mentioned in
   * Section 4.3.3, draft-ietf-6tisch-6top-protocol-03.
   *
   * The assumption here is that each SF can have transactions to a single peer and
   *     one from it
   */
  sixp_trans_state_t pkt_back = (~pkt->dir) & SIXP_TRANS_STATE_IO_Msk;

  for(trans = list_head(trans_list); trans != NULL; trans = trans->next)
  {
    if ( !linkaddr_cmp(peer_addr, &trans->peer_addr) )   continue;

    assert(trans->sf != NULL);
    if (trans->sf->sfid != pkt->sfid) continue;

    unsigned dir = trans->state & SIXP_TRANS_STATE_IO_Msk;

    //reserved packet - don`t know how to mutch it
    assert(pkt->type != SIXP_PKT_TYPE_RESERVED);

    switch (pkt->type){
        case SIXP_PKT_TYPE_REQUEST:
        case SIXP_PKT_TYPE_CONFIRMATION:
            // every request - is a new incomig transaction, or outgoing
            //  so look for trans that are request-related
            if (dir == pkt->dir){
                LOG_DBG("trans(%p)[%x/%u] mutch for %x pkt:%d\n"
                                , trans, trans->state, trans->seqno
                                                            , pkt->dir, pkt->type);
                return trans;
            }
            break;

        case SIXP_PKT_TYPE_RESPONSE:
            // responces comes in for outgoing inrequest, or comes out for incoming
            if (dir == pkt_back){
                LOG_DBG("trans(%p)[%x/%u] mutch for %x pkt:%d\n"
                                 , trans, trans->state, trans->seqno
                                                            , pkt->dir, pkt->type);
                return trans;
            }
            break;

        case SIXP_PKT_TYPE_RESERVED:
            //TODO: reserved incoming packets don`t match any transaction?
            return NULL;
    }
  }

  return NULL;
}

/*---------------------------------------------------------------------------*/
void
sixp_trans_terminate(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return;
  } else {
    sixp_trans_transit_state(trans, SIXP_TRANS_STATE_TERMINATING);
  }
}
/*---------------------------------------------------------------------------*/
void
sixp_trans_abort(sixp_trans_t *trans)
{
  assert(trans != NULL);
  if(trans == NULL) {
    return;
  } else {
    LOG_INFO("6P-trans: trans [peer_addr:");
    LOG_INFO_LLADDR((const linkaddr_t *)&trans->peer_addr);
    LOG_INFO_(", seqno:%u] is going to be aborted\n", trans->seqno);
    sixp_trans_terminate(trans);
    sixp_trans_invoke_callback(trans, SIXP_OUTPUT_STATUS_ABORTED);
    /* process_trans() should be scheduled, which we will be stop */
    assert( !ctimer_expired(&trans->timer) );
    ctimer_stop(&trans->timer);
    /* call process_trans() directly*/
    sixp_process_trans((void *)trans);
  }
}
/*---------------------------------------------------------------------------*/
int
sixp_trans_init(void)
{
  sixp_trans_t *trans, *next_trans;

  /* make sure there's no timer task left before the initialization */
  for(trans = list_head(trans_list);
      trans != NULL; trans = next_trans) {
    next_trans = trans->next;
    ctimer_stop(&trans->timer);
    sixp_trans_free(trans);
  }

  list_init(trans_list);
  memb_init(&trans_memb);
  return 0;
}
/*---------------------------------------------------------------------------*/
/** @} */
