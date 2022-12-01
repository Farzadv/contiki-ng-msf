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
 * \addtogroup msf
 * @{
 */
/**
 * \file
 *         MSF 6P RELOCATE handlers
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inria.fr>
 */

#ifndef _MSF_SIXP_RELOCATE_H_
#define _MSF_SIXP_RELOCATE_H_

#include <stdint.h>

#include "net/linkaddr.h"
#include "net/mac/tsch/sixtop/sixp-pkt.h"

/**
 * \brief Send a RELOCATE request
 * \param cell Cell to relocate
 */
void msf_sixp_relocate_send_request(const tsch_link_t *cell);

/**
 * \brief Handler for reception of a RELOCATE request
 * \param peer_addr The source MAC address of the request
 * \param body A pointer to the body of the request
 * \param body_len The length of body in bytes
 */
void msf_sixp_relocate_recv_request(const linkaddr_t *peer_addr,
                                    const uint8_t *body, uint16_t body_len);

/**
 * \brief Handler for reception of a response for RELOCATE
 * \param peer_addr The source MAC address of the response
 * \param rc Return code in the response
 * \param body A pointer to the body of the request
 * \param body_len The length of body in bytes
 */
void msf_sixp_relocate_recv_response(const linkaddr_t *peer_addr,
                                     sixp_pkt_rc_t rc,
                                     const uint8_t *body, uint16_t body_len);

#endif /* !_MSF_SIXP_RELOCATE_H_ */
/** @} */
