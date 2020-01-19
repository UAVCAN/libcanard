/*
 * Copyright (c) 2016-2019 UAVCAN Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Contributors: https://github.com/UAVCAN/libcanard/contributors
 */

#include "canard.h"
#include <string.h>
#include <assert.h>

/// This macro is needed only for testing and for library development. Do not redefine this in production.
#ifndef CANARD_INTERNAL
#    define CANARD_INTERNAL static
#endif

// ---------------------------------------- CONSTANTS ----------------------------------------

#define TRANSFER_CRC_INITIAL 0xFFFFU

#define TRANSFER_ID_BIT_LEN 5U

#define CAN_EXT_ID_MASK ((1UL << 29U) - 1U)

// ---------------------------------------- INTERNAL TYPES ----------------------------------------

struct CanardInternalTxQueueItem
{
    CanardInternalTxQueueItem* next;
    uint64_t                   deadline_usec;
    CanardCANFrame             frame;
};

/// The fields are ordered to minimize padding on all platforms.
struct CanardInternalInputSession
{
    struct CanardInternalInputSession* next;

    uint8_t* payload;
    size_t   payload_length;    ///< How many bytes received so far.
    size_t   payload_capacity;  ///< Payload past this limit may be discarded by the library.

    uint64_t timestamp_usec;            ///< Time of last update of this session. Used for removal on timeout.
    uint32_t transfer_id_timeout_usec;  ///< When (current time - update timestamp) exceeds this, it's dead.

    const uint32_t session_specifier;  ///< Differentiates this session from other sessions.
    uint16_t       calculated_crc;     ///< Updated with the received payload in real time.
    uint8_t        transfer_id;
    bool           next_toggle;
};

// ---------------------------------------- PRIVATE FUNCTIONS ----------------------------------------

CANARD_INTERNAL inline uint32_t makeMessageSessionSpecifier(const uint16_t subject_id, const uint8_t src_node_id)
{
    CANARD_ASSERT(subject_id <= CANARD_SUBJECT_ID_MAX);
    CANARD_ASSERT(src_node_id <= CANARD_NODE_ID_MAX);
    return ((uint32_t) src_node_id) | ((uint32_t) subject_id << 8U);
}

CANARD_INTERNAL inline uint32_t makeServiceSessionSpecifier(const uint16_t service_id,
                                                            const bool     request_not_response,
                                                            const uint8_t  src_node_id,
                                                            const uint8_t  dst_node_id)
{
    CANARD_ASSERT(service_id <= CANARD_SERVICE_ID_MAX);
    CANARD_ASSERT(src_node_id <= CANARD_NODE_ID_MAX);
    CANARD_ASSERT(dst_node_id <= CANARD_NODE_ID_MAX);
    return ((uint32_t) src_node_id) | ((uint32_t) dst_node_id << 7U) | ((uint32_t) service_id << 14U) |
           (request_not_response ? (1U << 24U) : 0U);
}

CANARD_STATIC_ASSERT(sizeof(float) == 4, "Native float format shall match IEEE 754 binary32");