/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBQCDM_LOG_ITEMS_H
#define LIBQCDM_LOG_ITEMS_H

enum {
    /* CDMA and EVDO items */
    DM_LOG_ITEM_CDMA_ACCESS_CHANNEL_MSG         = 0x1004,
    DM_LOG_ITEM_CDMA_REV_CHANNEL_TRAFFIC_MSG    = 0x1005,
    DM_LOG_ITEM_CDMA_SYNC_CHANNEL_MSG           = 0x1006,
    DM_LOG_ITEM_CDMA_PAGING_CHANNEL_MSG         = 0x1007,
    DM_LOG_ITEM_CDMA_FWD_CHANNEL_TRAFFIC_MSG    = 0x1008,
    DM_LOG_ITEM_CDMA_FWD_LINK_VOCODER_PACKET    = 0x1009,
    DM_LOG_ITEM_CDMA_REV_LINK_VOCODER_PACKET    = 0x100A,
    DM_LOG_ITEM_CDMA_MARKOV_STATS               = 0x100E,
    DM_LOG_ITEM_EVDO_HANDOFF_STATE              = 0x105E,
    DM_LOG_ITEM_EVDO_ACTIVE_PILOT_SET           = 0x105F,
    DM_LOG_ITEM_EVDO_REV_LINK_PACKET_SUMMARY    = 0x1060,
    DM_LOG_ITEM_EVDO_REV_TRAFFIC_RATE_COUNT     = 0x1062,
    DM_LOG_ITEM_EVDO_REV_POWER_CONTROL          = 0x1063,
    DM_LOG_ITEM_EVDO_ARQ_EFFECTIVE_RECEIVE_RATE = 0x1066,
    DM_LOG_ITEM_EVDO_AIR_LINK_SUMMARY           = 0x1068,
    DM_LOG_ITEM_EVDO_POWER                      = 0x1069
    DM_LOG_ITEM_EVDO_FWD_LINK_PACKET_SNAPSHOT   = 0x106A,
    DM_LOG_ITEM_EVDO_ACCESS_ATTEMPT             = 0x106C,
    DM_LOG_ITEM_EVDO_REV_ACTIVITY_BITS_BUFFER   = 0x106D,
    DM_LOG_ITEM_CDMA_REVERSE_POWER_CONTROL      = 0x102c,
    DM_LOG_ITEM_CDMA_SERVICE_CONFIG             = 0x102e,

    /* WCDMA items */
    DM_LOG_ITEM_WCDMA_AGC_INFO             = 0x4105,
    DM_LOG_ITEM_WCDMA_RRC_STATE            = 0x4125,

    /* GSM items */
    DM_LOG_ITEM_GSM_BURST_METRICS          = 0x506c,
    DM_LOG_ITEM_GSM_BCCH_MESSAGE           = 0x5134,
};


/* DM_LOG_ITEM_CDMA_PAGING_CHANNEL_MSG */
struct DMLogItemPagingChannelMsg {
    u_int8_t msg_len;  /* size of entire struct including this field */
    u_int8_t msg_type; /* MSG_TYPE as in 3GPP2 C.S0004 Table 3.1.2.3.1.1.2 */
    u_int8_t data[0];  /* Packed message as in 3GPP2 C.S0005 3.7.2.3.2.x */
} __attribute ((packed));
typedef struct DMLogItemPagingChannelMsg DMLogItemPagingChannelMsg;


/* DM_LOG_ITEM_CDMA_REVERSE_POWER_CONTROL */
struct DMLogItemRPCItem {
    u_int8_t channel_set_mask;
    u_int16_t frame_count;
    u_int8_t len_per_frame;
    u_int16_t dec_history;
    u_int8_t rx_agc_vals;
    u_int8_t tx_power_vals;
    u_int8_t tx_gain_adjust;
} __attribute__ ((packed));
typedef struct DMLogItemRPCItem DMLogItemRPCItem;

struct DMLogItemCdmaReversePowerControl {
    u_int8_t frame_offset;
    u_int8_t band_class;
    u_int16_t rev_chan_rc;
    u_int8_t pilot_gating_rate;
    u_int8_t step_size;
    u_int8_t num_records;
    DMLogItemRPCItem records[];
} __attribute__ ((packed));
typedef struct DMLogItemCdmaReversePowerControl DMLogItemCdmaReversePowerControl;


/* DM_LOG_ITEM_WCDMA_AGC_INFO */
struct DMLogItemWcdmRrcState {
    u_int8_t num_samples;
    u_int16_t rx_agc;
    u_int16_t tx_agc;
    u_int16_t rx_agc_adj_pdm;
    u_int16_t tx_agc_adj_pdm;
    u_int16_t max_tx;
    /* Bit 4 means tx_agc is valid */
    u_int8_t agc_info;
} __attribute__ ((packed));
typedef struct DMLogItemWcdmRrcState DMLogItemWcdmRrcState;


/* DM_LOG_ITEM_WCDMA_RRC_STATE */
enum {
    DM_LOG_ITEM_WCDMA_RRC_STATE_DISCONNECTED = 0,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CONNECTING   = 1,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_FACH    = 2,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_DCH     = 3,
    DM_LOG_ITEM_WCDMA_RRC_STATE_CELL_PCH     = 4,
    DM_LOG_ITEM_WCDMA_RRC_STATE_URA_PCH      = 5,
};

struct DMLogItemWcdmRrcState {
    u_int8_t rrc_state;
} __attribute__ ((packed));
typedef struct DMLogItemWcdmRrcState DMLogItemWcdmRrcState;


/* DM_LOG_ITEM_GSM_BURST_METRICS */
struct DMLogItemGsmBurstMetric {
    u_int32_t fn;
    u_int16_t arfcn;
    u_int32_t rssi;
    u_int16_t power;
    u_int16_t dc_offset_i;
    u_int16_t dc_offset_q;
    u_int16_t freq_offset;
    u_int16_t timing_offset;
    u_int16_t snr;
    u_int8_t gain_state;
} __attribute__ ((packed));
typedef struct DMLogItemGsmBurstMetric DMLogItemGsmBurstMetric;

struct DMLogItemGsmBurstMetrics {
    u_int8_t channel;
    DMLogItemBurstMetric metrics[4];
} __attribute__ ((packed));
typedef struct DMLogItemGsmBurstMetrics DMLogItemGsmBurstMetrics;


/* DM_LOG_ITEM_GSM_BCCH_MESSAGE */
enum {
    DM_LOG_ITEM_GSM_BCCH_BAND_UNKNOWN  = 0,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_900  = 8,
    DM_LOG_ITEM_GSM_BCCH_BAND_DCS_1800 = 9,
    DM_LOG_ITEM_GSM_BCCH_BAND_PCS_1900 = 10,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_850  = 11,
    DM_LOG_ITEM_GSM_BCCH_BAND_GSM_450  = 12,
};

struct DMLogItemGsmBcchMessage {
    /* Band is top 4 bits; lower 12 is ARFCN */
    u_int16_t bcch_arfcn;
    u_int16_t bsic;
    u_int16_t cell_id;
    u_int8_t lai[5];
    u_int8_t cell_selection_prio;
    u_int8_t ncc_permitted;
} __attribute__ ((packed));
typedef struct DMLogItemGsmBcchMessage DMLogItemGsmBcchMessage;

#endif  /* LIBQCDM_LOG_ITEMS_H */