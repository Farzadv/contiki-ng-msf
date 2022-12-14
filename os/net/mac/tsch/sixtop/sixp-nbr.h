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
 *         Neighbor Management APIs for 6top Protocol (6P)
 * \author
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inf.ethz.ch>
 */

#include <stdint.h>
#include "sixp-pkt.h"

typedef struct sixp_nbr sixp_nbr_t;

/**
 * \brief Find a neighbor
 * \param addr The MAC address of a neighbor
 * \return The pointer to a neighbor, NULL on failure
 */
sixp_nbr_t *sixp_nbr_find(const linkaddr_t *addr);

/**
 * \brief Allocate a neighbor
 * \param addr The MAC address of a neighbor
 * \return The pointer to a allocated neighbor, NULL on failure
 */
sixp_nbr_t *sixp_nbr_alloc(const linkaddr_t *addr);

/**
 * \brief Free a neighbor
 * \param nbr The pointer to a neighbor to free
 */
void sixp_nbr_free(sixp_nbr_t *nbr);

/**
 * \brief Get the next sequence number of a neighbor
 * \param nbr The pointer to a neighbor
 * \return A sequence number to use for a next request, -1 on failure
 */
int sixp_nbr_get_next_seqno(sixp_nbr_t *nbr);

/**
 * \brief Set the specified value to the next sequence number of a neighbor
 * \param nbr The pointer to a neighbor
 * \param seqno Value to be set
 * \return 0 on success, -1 on failure
 */
int sixp_nbr_set_next_seqno(sixp_nbr_t *nbr, uint16_t seqno);

/**
 * \brief Reset the next sequence number of a neighbor to zero
 * \param nbr The pointer to a neighbor
 * \return 0 on success, -1 on failure
 */
int sixp_nbr_reset_next_seqno(sixp_nbr_t *nbr);

/**
 * \brief Increment the next sequence number of a neighbor
 * \param nbr The pointer to a neighbor
 * \return 0 on success, -1 on failure
 */
int sixp_nbr_increment_next_seqno(sixp_nbr_t *nbr);

/**
 * \brief Initialize 6p Neighbor Table
 */
int sixp_nbr_init(void);



//==============================================================================
typedef struct sixp_nbr {
  struct sixp_nbr *next;
  linkaddr_t addr;
  uint8_t next_seqno;
} sixp_nbr_t;

/**
 * Get the first neighbor cache in nbr_table
 * \return the pointer to the first neighbor cache entry
 */
sixp_nbr_t* sixp_nbr_head(void);

/**
 * Get the next neighbor cache of a specified one
 * \param nbr the pointer to a neighbor cache
 * \return the pointer to the next one on success, NULL on failure
 */
sixp_nbr_t* sixp_nbr_next(sixp_nbr_t *nbr);

/*---------------------------------------------------------------------------*/
/** @} */
