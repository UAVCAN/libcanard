/*
 * Copyright (c) 2016 UAVCAN Team
 *
 * Distributed under the MIT License, available in the file LICENSE.
 *
 */

#define HAS_CAN_CONFIG_H

#include <canard_AVR.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <can.h>

int canardAVRInit(uint32_t bitrate)
{
    can_bitrate_t br;
    switch (bitrate)
    {
    case 10000:
    {
        br = BITRATE_10_KBPS;
        break;
    }
    case 20000:
    {
        br = BITRATE_20_KBPS;
        break;
    }
    case 50000:
    {
        br = BITRATE_50_KBPS;
        break;
    }
    case 100000:
    {
        br = BITRATE_100_KBPS;
        break;
    }
    case 125000:
    {
        br = BITRATE_125_KBPS;
        break;
    }
    case 250000:
    {
        br = BITRATE_250_KBPS;
        break;
    }
    case 500000:
    {
        br = BITRATE_500_KBPS;
        break;
    }
    case 1000000:
    {
        br = BITRATE_1_MBPS;
        break;
    }
    default:
    {
        br = BITRATE_10_KBPS;
    }
    }

    can_init(br);
    can_set_mode(NORMAL_MODE);

    // create a new filter for receiving all messages
    can_filter_t filter = {
        .id = 0,
        .mask = 0,
        .flags = {
            .rtr = 0,
            .extended = 0
        }
    };

    can_set_filter(0, &filter);

    // enable interrupts
    sei();

    return 0;
}

int canardAVRClose(void)
{
    return 0;
}

int canardAVRTransmit(const CanardCANFrame* frame)
{
    const int poll_result = can_check_free_buffer();
    if (poll_result <= 0)
    {
        return 0;
    }

    can_t transmit_msg;
    memset(&transmit_msg, 0, sizeof(transmit_msg));
    transmit_msg.id = frame->id & CANARD_CAN_EXT_ID_MASK;
    transmit_msg.length = frame->data_len;
    transmit_msg.flags.extended = (frame->id & CANARD_CAN_FRAME_EFF) != 0;
    memcpy(transmit_msg.data, frame->data, frame->data_len);

    const uint8_t res = can_send_message(&transmit_msg);
    if (res <= 0)
    {
        return -1;
    }

    return 1;
}

int canardAVRReceive(CanardCANFrame* out_frame)
{
    const int poll_result = can_check_message();
    if (poll_result <= 0)
    {
        return 0;
    }

    can_t receive_msg;
    const uint8_t res = can_get_message(&receive_msg);
    if (res <= 0)
    {
        return -1;
    }

    out_frame->id = receive_msg.id;
    out_frame->data_len = receive_msg.length;
    memcpy(out_frame->data, receive_msg.data, receive_msg.length);

    if (receive_msg.flags.extended != 0)
    {
        out_frame->id |= CANARD_CAN_FRAME_EFF;
    }

    return 1;
}

int canardAVRConfigureAcceptanceFilters(uint8_t node_id)
{
    uint8_t i = 0;
    uint8_t res = 1;

    // create a new filter for receiving messages
    can_filter_t filter_in = {
        .id = (uint32_t)(node_id << 8),
        .mask = ~(uint32_t)(node_id << 8),
        .flags = {
            .rtr = 0,
            .extended = 0
        }
    };

    for (i = 0; i<7; i++)
    {
        if (!can_set_filter(i, &filter_in))
        {
            res = -1;
        }
    }

    // create a new filter for sending messages
    can_filter_t filter_out = {
        .id = (uint32_t)(node_id),
        .mask = ~(uint32_t)(node_id),
        .flags = {
            .rtr = 0,
            .extended = 0
        }
    };

    for (i = 7; i<14; i++)
    {
        if (!can_set_filter(i, &filter_out))
        {
            res = -1;
        }
    }

    return res;
}