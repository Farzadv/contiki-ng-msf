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
 *         Transaction Management APIs for 6top Protocol (6P)
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inf.ethz.ch>
 */

#include "sixp.h"
#include "sixp-pkt.h"
#include <stdbool.h>
#include "sixtop.h"

/**
 * \brief 6P Transaction States (for internal use)
 */
typedef enum {
  SIXP_TRANS_STATE_UNAVAILABLE = 0,
  SIXP_TRANS_STATE_REQ_INIT,
  SIXP_TRANS_STATE_REQ_SENDING,
  SIXP_TRANS_STATE_REQ_SENT,
  SIXP_TRANS_STATE_REQ_RECEIVED,
  SIXP_TRANS_STATE_RESP_SENDING,
  SIXP_TRANS_STATE_RESP_SENT,
  SIXP_TRANS_STATE_RESP_RECEIVED,
  SIXP_TRANS_STATE_CONFIRM_SENDING,
  SIXP_TRANS_STATE_CONFIRM_SENT,
  SIXP_TRANS_STATE_CONFIRM_RECEIVED,
  SIXP_TRANS_STATE_TERMINATING,
  SIXP_TRANS_STATE_WAIT_FREE,

  // transactions can concurent for incomin/outgoing requests. This field helps
  // denote - wich direction is state of transaction
  SIXP_TRANS_STATE_IN_REQ  = 0x40,
  SIXP_TRANS_STATE_OUT_REQ = 0x80,
  SIXP_TRANS_STATE_IO_Msk  = SIXP_TRANS_STATE_IN_REQ | SIXP_TRANS_STATE_OUT_REQ,

  // transaction sub-step, is sequences steps for in/out req
  SIXP_TRANS_STATE_STEP0   = 0,
  SIXP_TRANS_STATE_STEP1   = 0x100,
  SIXP_TRANS_STATE_STEP2   = 0x200,
  SIXP_TRANS_STATE_STEP3   = 0x300,
  SIXP_TRANS_STATE_STEP4   = 0x400,
  SIXP_TRANS_STATE_STEP5   = 0x500,
  SIXP_TRANS_STATE_STEP_Msk= 0xf00,

  SIXP_TRANS_STATE_INIT                 = SIXP_TRANS_STATE_REQ_INIT
                                        | SIXP_TRANS_STATE_IN_REQ
                                        | SIXP_TRANS_STATE_OUT_REQ,
  SIXP_TRANS_STATE_REQUEST_SENDING  = SIXP_TRANS_STATE_REQ_SENDING
                                        | SIXP_TRANS_STATE_STEP1
                                        | SIXP_TRANS_STATE_OUT_REQ,
  SIXP_TRANS_STATE_REQUEST_SENT     = SIXP_TRANS_STATE_REQ_SENT
                                        | SIXP_TRANS_STATE_STEP2
                                        | SIXP_TRANS_STATE_OUT_REQ,
  SIXP_TRANS_STATE_RESPONSE_RECEIVED= SIXP_TRANS_STATE_RESP_RECEIVED
                                        | SIXP_TRANS_STATE_STEP3
                                        | SIXP_TRANS_STATE_OUT_REQ,
  SIXP_TRANS_STATE_CONFIRMATION_SENDING = SIXP_TRANS_STATE_CONFIRM_SENDING
                                        | SIXP_TRANS_STATE_STEP4
                                        | SIXP_TRANS_STATE_OUT_REQ,
  SIXP_TRANS_STATE_CONFIRMATION_SENT    = SIXP_TRANS_STATE_CONFIRM_SENT
                                        | SIXP_TRANS_STATE_STEP5
                                        | SIXP_TRANS_STATE_OUT_REQ,

  SIXP_TRANS_STATE_REQUEST_RECEIVED = SIXP_TRANS_STATE_REQ_RECEIVED
                                        | SIXP_TRANS_STATE_STEP1
                                        | SIXP_TRANS_STATE_IN_REQ,
  SIXP_TRANS_STATE_RESPONSE_SENDING = SIXP_TRANS_STATE_RESP_SENDING
                                        | SIXP_TRANS_STATE_STEP2
                                        | SIXP_TRANS_STATE_IN_REQ,
  SIXP_TRANS_STATE_RESPONSE_SENT    = SIXP_TRANS_STATE_RESP_SENT
                                        | SIXP_TRANS_STATE_STEP3
                                        | SIXP_TRANS_STATE_IN_REQ,
  SIXP_TRANS_STATE_CONFIRMATION_RECEIVED= SIXP_TRANS_STATE_CONFIRM_RECEIVED
                                        | SIXP_TRANS_STATE_STEP4
                                        | SIXP_TRANS_STATE_IN_REQ,

} sixp_trans_state_t;

/**
 * \brief 6P Transaction Modes (for internal use)
 */
typedef enum {
  SIXP_TRANS_MODE_UNAVAILABLE = 0,
  SIXP_TRANS_MODE_2_STEP,
  SIXP_TRANS_MODE_3_STEP,
} sixp_trans_mode_t;

typedef struct sixp_trans sixp_trans_t;

/**
 * \brief Change the state of a specified transaction
 * \param trans The pointer to a transaction
 * \param new_state New state to move the transaction to
 * \return 0 on success, -1 on failure
 */
int sixp_trans_transit_state(sixp_trans_t *trans,
                             sixp_trans_state_t new_state);

/**
 * \brief Return the scheduling function associated with a specified transaction
 * \param trans The pointer to  a transaction
 * \return Pointer to a scheduling function (sixp_sf_t)
 */
const sixtop_sf_t *sixp_trans_get_sf(sixp_trans_t *trans);

/**
 * \brief Return the command associated with a specified transaction
 * \param trans The pointer to  a transaction
 * \return Command identifier; SIXP_PKT_CMD_UNAVAILABLE on failure
 */
sixp_pkt_cmd_t sixp_trans_get_cmd(sixp_trans_t *trans);

/**
 * \brief Return the state of a specified transaction
 * \param trans The pointer to a transaction
 * \return a state of the transaction; SIXP_TRANS_STATE_UNAVAILABLE if the
 * transaction is not found in the system.
 */
sixp_trans_state_t sixp_trans_get_state(sixp_trans_t *trans);

/**
 * \brief Return the sequence number associated with a specified transaction
 * \param trans The pointer of a transaction
 * \return 0 or larger than 0 on success, -1 on failure
 */
int16_t sixp_trans_get_seqno(sixp_trans_t *trans);

/**
 * \brief Return the mode, 2-step or 3-step, of a specified transaction
 * \param trans The pointer to a transaction
 * \return The mode of the transaction, SIXP_TRANS_MODE_UNAVAILABLE on failure
 */
sixp_trans_mode_t sixp_trans_get_mode(sixp_trans_t *trans);

/**
 * \brief Return the peer addr of a specified transaction
 * \param trans The pointer to a transaction
 * \return The pointer to the peer addr
 */
const linkaddr_t *sixp_trans_get_peer_addr(sixp_trans_t *trans);

/**
 * \brief Invoke the output callback of a specified transaction
 * \param trans The pointer to a transaction
 * \param status An output result value
 */
void sixp_trans_invoke_callback(sixp_trans_t *trans,
                                sixp_output_status_t status);

static inline
sixp_trans_t* sixp_trans_now(){
    extern sixp_trans_t* sixp_current_trans;
    return sixp_current_trans;
}

/**
 * \brief Set an output callback to a specified transaction
 * \param trans The pointer to a transaction
 * \param func The pointer to a callback function
 * \param arg The pointer to an argument which will be passed to func
 * \param arg_len The length of the argument
 */
void sixp_trans_set_callback(sixp_trans_t *trans,
                             sixp_sent_callback_t func,
                             void *arg,
                             uint16_t arg_len);

/**
 * \brief Allocate a transaction
 * \param pkt The pointer to a packet which triggers the allocation
 * \param peer_addr The peer address which will be associated
 * \return A pointer to an newly allocated transaction, NULL on failure
 */
sixp_trans_t *sixp_trans_alloc(const sixp_pkt_t *pkt,
                               const linkaddr_t *peer_addr);

/**
 * \brief Helper function to terminate a transaction
 * \param trans The pointer to a transaction to terminate
 */
void sixp_trans_terminate(sixp_trans_t *trans);

/**
 * \brief Helper function to abort a transaction immediately
 * \param trans The pointer to a transaction to terminate
 */
void sixp_trans_abort(sixp_trans_t *trans);

/**
 * \brief Free a transaction
 * \param trans The pointer to a transaction to be freed
 * \note This function is for internal use only, which is NOT expected
 * to be called by a scheduling function, for instance.
 */
void sixp_trans_free(sixp_trans_t *trans);

/**
 * \brief Find a transaction
 * \param peer_addr The peer address
 * \return The pointer to a transaction; NULL on failure
 */
sixp_trans_t *sixp_trans_find(const linkaddr_t *peer_addr);

/* \brief Find a transaction for pkt - mutch by fsid, type, sequm
 * */
sixp_trans_t *sixp_trans_find_for_pkt(const linkaddr_t *peer_addr, const sixp_pkt_t* pkt);

sixp_trans_t *sixp_trans_find_for_sfid(const linkaddr_t *peer_addr, uint8_t sfid);

// @brief checks that have any active transaction
// @return true - some transaction are active
bool    sixp_trans_any();

/**
 * \brief Initialize Memory and List for 6P transactions
 * This function removes and frees existing transactions.
 */
int sixp_trans_init(void);



/*---------------------------------------------------------------------------*/

/* callbacks */
#ifdef SIXP_AFTER_TRANS_FREE
void SIXP_AFTER_TRANS_FREE(void);
#else
#define SIXP_AFTER_TRANS_FREE(...)
#endif

/*---------------------------------------------------------------------------*/

/** @} */
