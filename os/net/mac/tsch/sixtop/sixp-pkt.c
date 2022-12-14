/*
 * Copyright (c) 2016, Yasuyuki Tanaka.
 * Copyright (c) 2016, Centre for Development of Advanced Computing (C-DAC).
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
 *         6top Protocol (6P) Packet Manipulation
 * \author
 *         Shalu R         <shalur@cdac.in>
 *         Lijo Thomas     <lijo@cdac.in>
 *         Yasuyuki Tanaka <yasuyuki.tanaka@inf.ethz.ch>
 */
#include "contiki.h"
#include "contiki-lib.h"
#include "lib/assert.h"
#include "net/packetbuf.h"
#include "net/mac/tsch/tsch.h"

#include "sixp.h"
#include "sixp-pkt.h"
#include "sixp-pkt-ex.h"
#include "sixp-trans.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "6top pkt"
#define LOG_LEVEL LOG_LEVEL_6TOP

static int32_t get_metadata_offset(sixp_pkt_type_t type, sixp_pkt_code_t code);
static int32_t get_cell_options_offset(sixp_pkt_type_t type,
                                       sixp_pkt_code_t code);
static int32_t get_num_cells_offset(sixp_pkt_type_t type, sixp_pkt_code_t code);
static int32_t get_reserved_offset(sixp_pkt_type_t type, sixp_pkt_code_t code);
static int32_t get_offset_offset(sixp_pkt_type_t type, sixp_pkt_code_t code);
static int32_t get_max_num_cells_offset(sixp_pkt_type_t type,
                                        sixp_pkt_code_t code);
static int32_t get_cell_list_offset(sixp_pkt_type_t type, sixp_pkt_code_t code);
static int32_t get_rel_cell_list_offset(sixp_pkt_type_t type,
                                        sixp_pkt_code_t code);
static int32_t get_total_num_cells_offset(sixp_pkt_type_t type,
                                          sixp_pkt_code_t code);
static int32_t get_payload_offset(sixp_pkt_type_t type,
                                  sixp_pkt_code_t code);

/*---------------------------------------------------------------------------*/
static int32_t
get_metadata_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST) {
    return 0; /* offset */
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_cell_options_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST &&
     (code.cmd == SIXP_PKT_CMD_ADD ||
      code.cmd == SIXP_PKT_CMD_DELETE ||
      code.cmd == SIXP_PKT_CMD_RELOCATE ||
      code.cmd == SIXP_PKT_CMD_COUNT ||
      code.cmd == SIXP_PKT_CMD_LIST)) {
    return sizeof(sixp_pkt_metadata_t);
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_num_cells_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST &&
     (code.value == SIXP_PKT_CMD_ADD ||
      code.value == SIXP_PKT_CMD_DELETE ||
      code.value == SIXP_PKT_CMD_RELOCATE)) {
    return sizeof(sixp_pkt_metadata_t) + sizeof(sixp_pkt_cell_options_t);
  }

  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_reserved_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST &&
     code.value == SIXP_PKT_CMD_LIST) {
    return sizeof(sixp_pkt_metadata_t) + sizeof(sixp_pkt_cell_options_t);
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_offset_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST &&
     code.value == SIXP_PKT_CMD_LIST) {
    return (sizeof(sixp_pkt_metadata_t) +
            sizeof(sixp_pkt_cell_options_t) +
            sizeof(sixp_pkt_reserved_t));
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_max_num_cells_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST &&
     code.value == SIXP_PKT_CMD_LIST) {
    return (sizeof(sixp_pkt_metadata_t) +
            sizeof(sixp_pkt_cell_options_t) +
            sizeof(sixp_pkt_reserved_t) +
            sizeof(sixp_pkt_offset_t));
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_cell_list_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST && (code.value == SIXP_PKT_CMD_ADD ||
                                       code.value == SIXP_PKT_CMD_DELETE)) {
    return (sizeof(sixp_pkt_metadata_t) +
            sizeof(sixp_pkt_cell_options_t) +
            sizeof(sixp_pkt_num_cells_t));
  } else if((type == SIXP_PKT_TYPE_RESPONSE ||
             type == SIXP_PKT_TYPE_CONFIRMATION) &&
            (code.value == SIXP_PKT_RC_SUCCESS ||
             code.value == SIXP_PKT_RC_EOL)) {
    return 0;
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_rel_cell_list_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST && code.value == SIXP_PKT_CMD_RELOCATE) {
    return (sizeof(sixp_pkt_metadata_t) +
            sizeof(sixp_pkt_cell_options_t) +
            sizeof(sixp_pkt_num_cells_t));
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_total_num_cells_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_RESPONSE && code.value == SIXP_PKT_RC_SUCCESS) {
    return 0;
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static int32_t
get_payload_offset(sixp_pkt_type_t type, sixp_pkt_code_t code)
{
  if(type == SIXP_PKT_TYPE_REQUEST && code.value == SIXP_PKT_CMD_SIGNAL) {
    return sizeof(sixp_pkt_metadata_t);
  } else if((type == SIXP_PKT_TYPE_RESPONSE ||
             type == SIXP_PKT_TYPE_CONFIRMATION) &&
            code.value == SIXP_PKT_RC_SUCCESS) {
    return 0;
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_metadata(sixp_pkt_type_t type, sixp_pkt_code_t code,
                      sixp_pkt_metadata_t metadata,
                      uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set metadata; body is null\n");
    return -1;
  }

  if((offset = get_metadata_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set metadata [type=%u, code=%u], invalid type\n",
            type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(metadata))) {
    LOG_ERR("6P-pkt: cannot set metadata, body is too short [body_len=%u]\n",
            body_len);
    return -1;
  }

  /*
   * Copy the content into the Metadata field as it is since 6P has no idea
   * about the internal structure of the field.
   */
  memcpy(body + offset, &metadata, sizeof(metadata));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_metadata(sixp_pkt_type_t type, sixp_pkt_code_t code,
                      sixp_pkt_metadata_t *metadata,
                      const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(metadata == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get metadata; invalid argument\n");
    return -1;
  }

  if((offset = get_metadata_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get metadata [type=%u, code=%u], invalid type\n",
            type, code.value);
    return -1;
  }

  if(body_len < offset + sizeof(*metadata)) {
    LOG_ERR("6P-pkt: cannot get metadata [type=%u, code=%u], ",
            type, code.value);
    LOG_ERR_("body is too short\n");
    return -1;
  }

  /*
   * Copy the content in the Metadata field as it is since 6P has no idea about
   * the internal structure of the field.
   */
  memcpy(metadata, body + offset, sizeof(*metadata));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_cell_options(sixp_pkt_type_t type, sixp_pkt_code_t code,
                          sixp_pkt_cell_options_t cell_options,
                          uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set cell_options; body is null\n");
    return -1;
  }

  if((offset = get_cell_options_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set cell_options [type=%u, code=%u], ",
            type, code.value);
    LOG_ERR_("invalid type\n");
    return -1;
  }

  if(body_len < (offset + sizeof(cell_options))) {
    LOG_ERR("6P-pkt: cannot set cell_options, ");
    LOG_ERR_("body is too short [body_len=%u]\n", body_len);
    return -1;
  }

  /* The CellOption field is an 8-bit bitfield */
  memcpy(body + offset, &cell_options, sizeof(uint8_t));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_cell_options(sixp_pkt_type_t type, sixp_pkt_code_t code,
                          sixp_pkt_cell_options_t *cell_options,
                          const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(cell_options == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get cell_options; invalid argument\n");
    return -1;
  }

  if((offset = get_cell_options_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get cell_options [type=%u, code=%u]",
            type, code.value);
    LOG_ERR_("invalid type\n");
    return -1;
  }

  if(body_len < (offset + sizeof(*cell_options))) {
    LOG_ERR("6P-pkt: cannot get cell_options, ");
    LOG_ERR_("body is too short [body_len=%u]\n", body_len);
    return -1;
  }

  /* The CellOption field is an 8-bit bitfield */
  memcpy(cell_options, body + offset, sizeof(uint8_t));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                       sixp_pkt_num_cells_t num_cells,
                       uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set num_cells; body is null\n");
    return -1;
  }

  if((offset = get_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have NumCells\n",
             type, code.value);
    return -1;
  }

  memcpy(body + offset, &num_cells, sizeof(uint8_t));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                       sixp_pkt_num_cells_t *num_cells,
                       const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(num_cells == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get num_cells; invalid argument\n");
    return -1;
  }

  if((offset = get_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have NumCells\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(*num_cells))) {
    LOG_ERR("6P-pkt: cannot get num_cells; body is too short\n");
    return -1;
  }

  /* NumCells is an 8-bit unsigned integer */
  memcpy(num_cells, body + offset, sizeof(*num_cells));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_reserved(sixp_pkt_type_t type, sixp_pkt_code_t code,
                      sixp_pkt_reserved_t reserved,
                      uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set reserved; body is null\n");
    return -1;
  }

  if((offset = get_reserved_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set reserved; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have Reserved\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(reserved))) {
    LOG_ERR("6P-pkt: cannot set reserved; body is too short\n");
    return -1;
  }

  /* The Reserved field is an 8-bit field */
  memcpy(body + offset, &reserved, sizeof(uint8_t));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_reserved(sixp_pkt_type_t type, sixp_pkt_code_t code,
                      sixp_pkt_reserved_t *reserved,
                      const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(reserved == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get reserved; invalid argument\n");
    return -1;
  }

  if((offset = get_reserved_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get reserved; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have Reserved\n",
             type, code.value);
    return -1;
  }

  /* The Reserved field is an 8-bit field */
  memcpy(reserved, body + offset, sizeof(uint8_t));

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_offset(sixp_pkt_type_t type, sixp_pkt_code_t code,
                    sixp_pkt_offset_t cell_offset,
                    uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set offset; invalid argument\n");
    return -1;
  }

  if((offset = get_offset_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set offset; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have Offset\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(cell_offset))) {
    LOG_ERR("6P-pkt: cannot set offset; body is too short\n");
    return -1;
  }

  /*
   * The (Cell)Offset field is 16-bit long; treat it as a little-endian value of
   * unsigned integer following IEEE 802.15.4-2015.
   */
  (body + offset)[0] = *((uint16_t *)&cell_offset) & 0xff;
  (body + offset)[1] = (*((uint16_t *)&cell_offset) >> 8) & 0xff;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_offset(sixp_pkt_type_t type, sixp_pkt_code_t code,
                    sixp_pkt_offset_t *cell_offset,
                    const uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  const uint8_t *p;

  if(cell_offset == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get offset; invalid argument\n");
    return -1;
  }

  if((offset = get_offset_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get offset; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have Offset\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(*cell_offset))) {
    LOG_ERR("6P-pkt: cannot get offset; body is too short\n");
    return -1;
  }

  /*
   * The (Cell)Offset field is 16-bit long; treat it as a little-endian value of
   * unsigned integer following IEEE 802.15.4-2015.
   */
  p = body + offset;
  *((uint16_t *)cell_offset) = p[0] + (p[1] << 8);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_max_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                           sixp_pkt_max_num_cells_t max_num_cells,
                           uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set max_num_cells; invalid argument\n");
    return -1;
  }

  if((offset = get_max_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set max_num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have MaxNumCells\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(max_num_cells))) {
    LOG_ERR("6P-pkt: cannot set max_num_cells; body is too short\n");
    return -1;
  }

  /*
   * The MaxNumCells field is 16-bit long; treat it as a little-endian value of
   * unsigned integer following IEEE 802.15.4-2015.
   */
  (body + offset)[0] = *((uint16_t *)&max_num_cells) & 0xff;
  (body + offset)[1] = (*((uint16_t *)&max_num_cells) >> 8) & 0xff;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_max_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                           sixp_pkt_max_num_cells_t *max_num_cells,
                           const uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  const uint8_t *p;

  if(max_num_cells == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get max_num_cells; invalid argument\n");
    return -1;
  }

  if((offset = get_max_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get max_num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have MaxNumCells\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(*max_num_cells))) {
    LOG_ERR("6P-pkt: cannot get max_num_cells; body is too short\n");
    return -1;
  }

  /*
   * The MaxNumCells field is 16-bit long; treat it as a little-endian value of
   * unsigned integer following IEEE 802.15.4-2015.
   */
  p = body + offset;
  *((uint16_t *)max_num_cells) = p[0] + (p[1] << 8);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                       const uint8_t *cell_list,
                       uint16_t cell_list_len,
                       uint16_t cell_offset,
                       uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(cell_list == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot set cell_list; invalid argument\n");
    return -1;
  }

  if((offset = get_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have CellList\n",
             type, code.value);
    return -1;
  }

  offset += cell_offset;

  if(body_len < (offset + cell_list_len)) {
    LOG_ERR("6P-pkt: cannot set cell_list; body is too short\n");
    return -1;
  } else if((cell_list_len % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set cell_list; invalid {body, cell_list}_len=%u\n"
            , cell_list_len);
    return -1;
  }

  memcpy(body + offset, cell_list, cell_list_len);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                       const uint8_t **cell_list,
                       sixp_pkt_offset_t *cell_list_len,
                       const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(cell_list_len == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get cell_list\n");
    return -1;
  }

  if((offset = get_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have CellList\n",
             type, code.value);
    return -1;
  }

  if(body_len < offset) {
    LOG_ERR("6P-pkt: cannot set cell_list; body is too short\n");
    return -1;
  } else if(((body_len - offset) % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set cell_list; invalid {body, cell_list}_len\n");
    return -1;
  }

  if(cell_list != NULL) {
    *cell_list = body + offset;
  }

  *cell_list_len = body_len - offset;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_rel_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                           const uint8_t *rel_cell_list,
                           uint16_t rel_cell_list_len,
                           uint16_t cell_offset,
                           uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  sixp_pkt_num_cells_t num_cells;

  if(rel_cell_list == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; invalid argument\n");
    return -1;
  }

  if(sixp_pkt_get_num_cells(type, code, &num_cells, body, body_len) < 0) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; no NumCells field\n");
    return -1;
  }

  if((offset = get_rel_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have RelCellList\n",
             type, code.value);
    return -1;
  }

  offset += cell_offset;

  if(body_len < (offset + rel_cell_list_len)) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; body is too short\n");
    return -1;
  } else if((offset + rel_cell_list_len) >
            (offset + num_cells * sizeof(sixp_pkt_cell_t))) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; RelCellList is too long\n");
    return -1;
  } else if((rel_cell_list_len % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; invalid body_len\n");
    return -1;
  }

  memcpy(body + offset, rel_cell_list, rel_cell_list_len);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_rel_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                           const uint8_t **rel_cell_list,
                           sixp_pkt_offset_t *rel_cell_list_len,
                           const uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  sixp_pkt_num_cells_t num_cells;

  if(rel_cell_list_len == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get rel_cell_list; invalid argument\n");
    return -1;
  }

  if(sixp_pkt_get_num_cells(type, code, &num_cells, body, body_len) < 0) {
    LOG_ERR("6P-pkt: cannot get rel_cell_list; no NumCells field\n");
    return -1;
  }

  if((offset = get_rel_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get rel_cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have RelCellList\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + (num_cells * sizeof(sixp_pkt_cell_t)))) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; body is too short\n");
    return -1;
  } else if(((body_len - offset) % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set rel_cell_list; invalid body_len\n");
    return -1;
  }

  if(rel_cell_list != NULL) {
    *rel_cell_list = body + offset;
  }

  *rel_cell_list_len = num_cells * sizeof(sixp_pkt_cell_t);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_cand_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                            const uint8_t *cand_cell_list,
                            uint16_t cand_cell_list_len,
                            uint16_t cell_offset,
                            uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  sixp_pkt_num_cells_t num_cells;

  if(cand_cell_list == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; invalid argument\n");
    return -1;
  }

  if(sixp_pkt_get_num_cells(type, code, &num_cells, body, body_len) < 0) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; no NumCells field\n");
    return -1;
  }

  if((offset = get_rel_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have RelCellList\n",
             type, code.value);
    return -1;
  }

  offset += cell_offset + num_cells * sizeof(sixp_pkt_cell_t);

  if(body_len < (offset + cand_cell_list_len)) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; body is too short\n");
    return -1;
  } else if((cand_cell_list_len % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; invalid body_len\n");
    return -1;
  }

  memcpy(body + offset, cand_cell_list, cand_cell_list_len);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_cand_cell_list(sixp_pkt_type_t type, sixp_pkt_code_t code,
                            const uint8_t **cand_cell_list,
                            sixp_pkt_offset_t *cand_cell_list_len,
                            const uint8_t *body, uint16_t body_len)
{
  int32_t offset;
  sixp_pkt_num_cells_t num_cells;

  if(cand_cell_list_len == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get cand_cell_list; invalid argument\n");
    return -1;
  }

  if(sixp_pkt_get_num_cells(type, code, &num_cells, body, body_len) < 0) {
    LOG_ERR("6P-pkt: cannot get cand_cell_list; no NumCells field\n");
    return -1;
  }

  if((offset = get_rel_cell_list_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get cand_cell_list; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have RelCellList\n",
             type, code.value);
    return -1;
  }

  offset += num_cells * sizeof(sixp_pkt_cell_t);

  if(body_len < offset) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; body is too short\n");
    return -1;
  } else if(((body_len - offset) % sizeof(sixp_pkt_cell_t)) != 0) {
    LOG_ERR("6P-pkt: cannot set cand_cell_list; invalid body_len\n");
    return -1;
  }

  if(cand_cell_list != NULL) {
    *cand_cell_list = body + offset;
  }

  *cand_cell_list_len = body_len - offset;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_total_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                             sixp_pkt_total_num_cells_t total_num_cells,
                             uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set num_cells; body is null\n");
    return -1;
  }

  if((offset = get_total_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set total_num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have TotalNumCells\n",
             type, code.value);
    return -1;
  }

  /*
   * TotalNumCells for 6P Response is a 16-bit unsigned integer, little-endian.
   */
  body[offset] = (uint8_t)(total_num_cells & 0xff);
  body[offset + 1] = (uint8_t)(total_num_cells >> 8);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_total_num_cells(sixp_pkt_type_t type, sixp_pkt_code_t code,
                             sixp_pkt_total_num_cells_t *total_num_cells,
                             const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(total_num_cells == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get num_cells; invalid argument\n");
    return -1;
  }

  if((offset = get_total_num_cells_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get num_cells; ");
    LOG_ERR_("packet [type=%u, code=%u] won't have TotalNumCells\n",
             type, code.value);
    return -1;
  }

  if(body_len < (offset + sizeof(sixp_pkt_total_num_cells_t))) {
    LOG_ERR("6P-pkt: cannot get num_cells; body is too short\n");
    return -1;
  }

  /* TotalNumCells is a 16-bit unsigned integer, little-endian. */
  *total_num_cells = body[0];
  *total_num_cells += ((uint16_t)body[1]) << 8;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_set_payload(sixp_pkt_type_t type, sixp_pkt_code_t code,
                     const uint8_t *payload, uint16_t payload_len,
                     uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(body == NULL) {
    LOG_ERR("6P-pkt: cannot set metadata; body is null\n");
    return -1;
  }

  if((offset = get_payload_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot set payload [type=%u, code=%u], invalid type\n",
            type, code.value);
    return -1;
  }

  if(body_len < (offset + payload_len)) {
    LOG_ERR("6P-pkt: cannot set payload, body is too short [body_len=%u]\n",
            body_len);
    return -1;
  }

  /*
   * Copy the content into the Payload field as it is since 6P has no idea
   * about the internal structure of the field.
   */
  memcpy(body + offset, payload, payload_len);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_get_payload(sixp_pkt_type_t type, sixp_pkt_code_t code,
                     uint8_t *buf, uint16_t buf_len,
                     const uint8_t *body, uint16_t body_len)
{
  int32_t offset;

  if(buf == NULL || body == NULL) {
    LOG_ERR("6P-pkt: cannot get payload; invalid argument\n");
    return -1;
  }

  if((offset = get_payload_offset(type, code)) < 0) {
    LOG_ERR("6P-pkt: cannot get payload [type=%u, code=%u], invalid type\n",
            type, code.value);
    return -1;
  }

  if((body_len - offset) > buf_len) {
    LOG_ERR("6P-pkt: cannot get payload [type=%u, code=%u], ",
            type, code.value);
    LOG_ERR_("buf_len is too short\n");
    return -1;
  }

  /*
   * Copy the content in the Payload field as it is since 6P has no idea about
   * the internal structure of the field.
   */
  memcpy(buf, body + offset, buf_len);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_parse(const uint8_t *buf, uint16_t len,
               sixp_pkt_t *pkt)
{
  assert(buf != NULL && pkt != NULL);
  if(buf == NULL || pkt == NULL) {
    LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid argument\n");
    return -1;
  }

  memset(pkt, 0, sizeof(sixp_pkt_t));

  /* read the first 4 octets */
  if(len < 4) {
    LOG_ERR("6P-pkt: sixp_pkt_parse() fails because it's a too short packet\n");
    return -1;
  }

  /* parse the header as it's version 0 6P packet */
  pkt->version = buf[0] & 0x0f;
  pkt->type = (buf[0] & 0x30) >> 4;
  pkt->code.value = buf[1];
  pkt->sfid = buf[2];
  pkt->seqno = buf[3];
  pkt->dir   = SIXP_TRANS_STATE_IN_REQ;

  if(pkt->version != SIXP_PKT_VERSION) {
    /* invalid version; stop parsing */
    return -1;
  }

  buf += 4;
  len -= 4;

  LOG_INFO("6P-pkt: sixp_pkt_parse() is processing [type:%u, code:%u, len:%u]\n",
           pkt->type, pkt->code.value, len);

  /* the rest is message body called "Other Fields" */
  if(pkt->type == SIXP_PKT_TYPE_REQUEST) {
    switch(pkt->code.cmd) {
      case SIXP_PKT_CMD_ADD:
      case SIXP_PKT_CMD_DELETE:
        /* Add and Delete has the same request format */
        if(len < (sizeof(sixp_pkt_metadata_t) +
                  sizeof(sixp_pkt_cell_options_t) +
                  sizeof(sixp_pkt_num_cells_t)) ||
           (len % sizeof(uint32_t)) != 0) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_CMD_RELOCATE:
        if(len < (sizeof(sixp_pkt_metadata_t) +
                  sizeof(sixp_pkt_cell_options_t) +
                  sizeof(sixp_pkt_num_cells_t)) ||
           (len % sizeof(uint32_t)) != 0) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_CMD_COUNT:
        if(len != (sizeof(sixp_pkt_metadata_t) +
                   sizeof(sixp_pkt_cell_options_t))) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_CMD_LIST:
        if(len != (sizeof(sixp_pkt_metadata_t) +
                   sizeof(sixp_pkt_cell_options_t) +
                   sizeof(sixp_pkt_reserved_t) +
                   sizeof(sixp_pkt_offset_t) +
                   sizeof(sixp_pkt_max_num_cells_t))) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_CMD_SIGNAL:
        if(len < sizeof(sixp_pkt_metadata_t)) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_CMD_CLEAR:
        if(len != sizeof(sixp_pkt_metadata_t)) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      default:
        LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of unsupported cmd\n");
        return -1;
    }
  } else if(pkt->type == SIXP_PKT_TYPE_RESPONSE ||
            pkt->type == SIXP_PKT_TYPE_CONFIRMATION) {
    switch(pkt->code.rc) {
      case SIXP_PKT_RC_SUCCESS:
        /*
         * The "Other Field" contains
         * - Res to CLEAR:             Empty (length 0)
         * - Res to STATUS:            "Num. Cells" (total_num_cells)
         * - Res to ADD, DELETE, LIST: 0, 1, or multiple 6P cells
         * - Res to SIGNAL:            Payload (arbitrary length)
         */
        /* we accept any length because of SIGNAL */
        break;
      case SIXP_PKT_RC_EOL:
        if((len % sizeof(uint32_t)) != 0) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      case SIXP_PKT_RC_ERR:
      case SIXP_PKT_RC_RESET:
      case SIXP_PKT_RC_ERR_VERSION:
      case SIXP_PKT_RC_ERR_SFID:
      case SIXP_PKT_RC_ERR_SEQNUM:
      case SIXP_PKT_RC_ERR_CELLLIST:
      case SIXP_PKT_RC_ERR_BUSY:
      case SIXP_PKT_RC_ERR_LOCKED:
        if(len != 0) {
          LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of invalid length\n");
          return -1;
        }
        break;
      default:
        LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of unsupported code\n");
        return -1;
    }
  } else {
    LOG_ERR("6P-pkt: sixp_pkt_parse() fails because of unsupported type\n");
    return -1;
  }

  pkt->body = buf;
  pkt->body_len = len;

  return 0;
}
/*---------------------------------------------------------------------------*/
int
sixp_pkt_create(sixp_pkt_type_t type, sixp_pkt_code_t code,
                uint8_t sfid, uint8_t seqno,
                const uint8_t *body, uint16_t body_len, sixp_pkt_t *pkt)
{
    sixp_pkt_t tmp;
    if (pkt == NULL){
        pkt = &tmp;
        sixp_pkt_init(pkt, type, code, sfid);
    }
    return sixp_pkt_build(pkt, seqno, body, body_len);
}

int sixp_pkt_build(sixp_pkt_t *pkt, uint8_t seqno,
                    const uint8_t *body, uint16_t body_len )
{
    assert(pkt != NULL);
    pkt->seqno = seqno;
    pkt->body = body;
    pkt->body_len = body_len;

  uint8_t *hdr;

  assert((body == NULL && body_len == 0) || (body != NULL && body_len > 0));
  if((body == NULL && body_len > 0) || (body != NULL && body_len == 0)) {
    LOG_ERR("6P-pkt: sixp_pkt_create() fails because of invalid argument\n");
    return -1;
  }

  packetbuf_clear();

  /*
   * We're going to create a packet having 6top IE header (4 octets) and body
   * (body_len octets).
   */
  if(PACKETBUF_SIZE < (packetbuf_totlen() + body_len)) {
    LOG_ERR("6P-pkt: sixp_pkt_create() fails because body is too long\n");
    return -1;
  }

  if(packetbuf_hdralloc(4) != 1) {
    LOG_ERR("6P-pkt: sixp_pkt_create fails to allocate header space\n");
    return -1;
  }
  hdr = packetbuf_hdrptr();
  /* header: write the 6top IE header, 4 octets */
  hdr[0] = (pkt->type << 4) | SIXP_PKT_VERSION;
  hdr[1] = pkt->code.value;
  hdr[2] = pkt->sfid;
  hdr[3] = seqno;

  /* data: write body */
  if(body_len > 0 && body != NULL) {
    memcpy(packetbuf_dataptr(), body, body_len);
    packetbuf_set_datalen(body_len);
  }

  /* packetbuf is ready to be sent */
  return 0;
}

void sixp_pkt_init(sixp_pkt_t *pkt, sixp_pkt_type_t type, sixp_pkt_code_t code, uint8_t sfid){
    pkt->type = type;
    pkt->code = code;
    pkt->sfid = sfid;
    pkt->dir   = SIXP_TRANS_STATE_OUT_REQ;
    pkt->seqno  = 0;
    pkt->body   = NULL;
    pkt->body_len = 0;
}

//  same as sixp_pkt_init, but init for incoming
void sixp_pkt_init_in(sixp_pkt_t *pkt, sixp_pkt_type_t type, sixp_pkt_code_t code, uint8_t sfid){
    sixp_pkt_init(pkt, type, code, sfid);
    pkt->dir   = SIXP_TRANS_STATE_IN_REQ;
}



//==============================================================================
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
// like memcopy(dst, h->body+offset, len) with packet strick bounds
static
SIXPError sixp_pkt_load(SIXPHandle* h, void* dst, unsigned offset, unsigned len){
    assert(h != NULL && dst != NULL);
    assert(h->body != NULL);

    if(h->body_len < (offset + len)) {
      LOG_ERR("copy: too short packet(%d:%u) body[%u+%u]\n"
              , h->type, h->code.value, offset, len
              );
      return -1;
    }

    memcpy(dst, h->body + offset, len);

    return sixpOK;
}

static
SIXPError sixp_pkt_parse_cell_options(SIXPHandle* h,
                                    sixp_pkt_cell_options_t *cell_options)
{
    int offset;
    if((offset = get_cell_options_offset(h->type, h->code)) < 0) {
      LOG_ERR("6P-pkt: cannot get cell_options [type=%u, code=%u]",
              h->type, h->code.value);
      LOG_ERR_("invalid type\n");
      return sixpFAIL;
    }
    return sixp_pkt_load(h, cell_options, offset, sizeof(*cell_options));
}

static
SIXPError sixp_pkt_parse_num_cells(SIXPHandle* h,
                       sixp_pkt_num_cells_t *num_cells )
{
    int offset;
    if((offset = get_num_cells_offset(h->type, h->code)) < 0) {
      LOG_ERR("cannot set num_cells; ");
      LOG_ERR_("packet [type=%u, code=%u] won't have NumCells\n",
              h->type, h->code.value);
      return sixpFAIL;
    }
    return sixp_pkt_load(h, num_cells, offset, sizeof(*num_cells));
}

SIXPError sixp_pkt_parse_cell_list(SIXPHandle* h, SIXPCellsHandle* dst){
    int offset;

    if((offset = get_cell_list_offset(h->type, h->code)) < 0) {
      LOG_ERR("cannot get cell_list; ");
      LOG_ERR_("packet [type=%u, code=%u] won't have CellList\n",
              h->type, h->code.value);
      return sixpFAIL;
    }

    if (sixp_pkt_parse_num_cells(h, &dst->num_cells) < 0)
        return sixpFAIL;

    unsigned len = dst->num_cells * sizeof(sixp_pkt_cell_t);
    unsigned lim = (h->body_len - offset);

    if(h->body_len < offset) {
      LOG_ERR("cannot parse cell_list; body is too short\n");
      return sixpFAIL;
    } else if ( len > lim) {
      LOG_ERR("cannot parse cell_list; invalid {body, cell_list}_len=%u\n", len);
      return sixpFAIL;
    }

    dst->cell_list      = (sixp_pkt_cell_t*)(h->body + offset);
    dst->cell_list_len  = len;
    return sixpOK;
}

SIXPError sixp_pkt_parse_cells(SIXPHandle* h, SIXPCellsHandle* dst){
    if (sixp_pkt_parse_cell_list(h, dst) < 0)
        return sixpFAIL;
    if(sixp_pkt_parse_cell_options(h, &(dst->cell_options)) < 0)
        return sixpFAIL;
    memcpy(&dst->meta, h->body, sizeof(dst->meta) );
    return sixpOK;
}

// steps h body to position after dst cells
SIXPError sixp_pkt_after_cells(SIXPHandle* h, SIXPCellsHandle* dst){
    assert(dst->cell_list != NULL);
    const uint8_t* next = (const uint8_t*)dst->cell_list;
    next += dst->num_cells*sizeof(dst->cell_list[0]);
    if ((next - h->body) >= h->body_len){
        return sixpFAIL;
    }

    h->body_len -=(next - h->body);
    h->body     = next;
    return sixpOK;
}

SIXPError sixp_pkt_parse_next_cells(SIXPHandle* h, SIXPCellsHandle* dst){
    SIXPError ok;
    ok = sixp_pkt_after_cells(h, dst);
    if (ok == sixpOK){
        return sixp_pkt_parse_cells(h, dst);
    }
    return ok;
}

bool sixp_pkt_cells_have(SIXPCellsPkt* pkt, sixp_cell_t x){
    for (int i = 0; i < pkt->head.num_cells; ++i)
        if (pkt->cells[i].raw == x.raw)
            return true;
    return false;
}

//==============================================================================
const char* sixp_pkt_cell_option_name(sixp_pkt_cell_option_t x){
    switch (x){
        case SIXP_PKT_CELL_OPTION_TX: return "TX";
        case SIXP_PKT_CELL_OPTION_RX: return "RX";
        case SIXP_PKT_CELL_OPTION_SHARED:return "SH";
        default: return "xx";
    }
}

const char* sixp_pkt_cell_options_str(sixp_pkt_cell_options_t x){
    if (x == SIXP_PKT_CELL_OPTION_TX)
        return "TX";
    else if (x == SIXP_PKT_CELL_OPTION_RX)
        return "RX";
    else
        return "xx";
}

void sixp_pkt_cells_dump(SIXPCellsPkt* pkt){
    printf("{%d:", pkt->head.num_cells);
    for (int i = 0; i < pkt->head.num_cells; ++i){
        sixp_cell_t* x = pkt->cells+i;
        printf("[%u+%u] ", x->field.slot, x->field.chanel);
    }
    printf("}");
}


/** @} */
