/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 Red Hat, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define G_UDEV_API_IS_SUBJECT_TO_CHANGE
#include <gudev/gudev.h>

#include "mm-modem-huawei-cdma.h"
#include "mm-modem-cdma-sms.h"
#include "mm-modem-cdma-phone.h"
#include "mm-errors.h"
#include "mm-callback-info.h"
#include "mm-serial-port.h"
#include "mm-serial-parsers.h"
#include "mm-modem-helpers.h"
#include "mm-log.h"

static void modem_cdma_init (MMModemCdma *cdma_class);
static void modem_cdma_sms_init (MMModemCdmaSms *cdma_sms_class);
static void modem_cdma_phone_init (MMModemCdmaPhone *cdma_phone_class);

G_DEFINE_TYPE_EXTENDED (MMModemHuaweiCdma, mm_modem_huawei_cdma, MM_TYPE_GENERIC_CDMA, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_CDMA, modem_cdma_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_CDMA_SMS, modem_cdma_sms_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_MODEM_CDMA_PHONE, modem_cdma_phone_init))
 
static void simple_free_gvalue (gpointer data);
static GValue *simple_uint_value (guint32 i);
static GValue *simple_boolean_value (gboolean b);
static GValue *simple_string_value (const char *str);

static void cmti_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);
static void ring_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);
static void clip_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);
static void orig_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);
static void conn_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);
static void cend_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data);

MMModem *
mm_modem_huawei_cdma_new (const char *device,
                         const char *driver,
                         const char *plugin,
                         gboolean evdo_rev0,
                         gboolean evdo_revA,
                         guint32 vendor,
                         guint32 product)
{
    gboolean try_css = TRUE;

    g_return_val_if_fail (device != NULL, NULL);
    g_return_val_if_fail (driver != NULL, NULL);
    g_return_val_if_fail (plugin != NULL, NULL);

    /* Don't use AT+CSS on EVDO-capable hardware for determining registration
     * status, because often the device will have only an EVDO connection and
     * AT+CSS won't necessarily report EVDO registration status, only 1X.
     */
    if (evdo_rev0 || evdo_revA)
        try_css = FALSE;

    return MM_MODEM (g_object_new (MM_TYPE_MODEM_HUAWEI_CDMA,
                                   MM_MODEM_MASTER_DEVICE, device,
                                   MM_MODEM_DRIVER, driver,
                                   MM_MODEM_PLUGIN, plugin,
                                   MM_GENERIC_CDMA_EVDO_REV0, evdo_rev0,
                                   MM_GENERIC_CDMA_EVDO_REVA, evdo_revA,
                                   MM_GENERIC_CDMA_REGISTRATION_TRY_CSS, try_css,
                                   MM_MODEM_HW_VID, vendor,
                                   MM_MODEM_HW_PID, product,
                                   NULL));
}

/* Unsolicited message handlers */

static gint
parse_quality (const char *str, const char *detail)
{
    long int quality = 0;

    errno = 0;
    quality = strtol (str, NULL, 10);
    if (errno == 0) {
        quality = CLAMP (quality, 0, 100);
        mm_dbg ("%s: %ld", detail, quality);
        return (gint) quality;
    }
    return -1;
}

static void
handle_1x_quality_change (MMAtSerialPort *port,
                          GMatchInfo *match_info,
                          gpointer user_data)
{
    MMModemHuaweiCdma *self = MM_MODEM_HUAWEI_CDMA (user_data);
    char *str;
    gint quality;

    str = g_match_info_fetch (match_info, 1);
    quality = parse_quality (str, "1X signal quality");
    g_free (str);

    if (quality >= 0)
        mm_generic_cdma_update_cdma1x_quality (MM_GENERIC_CDMA (self), (guint32) quality);
}

static void
handle_evdo_quality_change (MMAtSerialPort *port,
                            GMatchInfo *match_info,
                            gpointer user_data)
{
    MMModemHuaweiCdma *self = MM_MODEM_HUAWEI_CDMA (user_data);
    char *str;
    gint quality;

    str = g_match_info_fetch (match_info, 1);
    quality = parse_quality (str, "EVDO signal quality");
    g_free (str);

    if (quality >= 0)
        mm_generic_cdma_update_evdo_quality (MM_GENERIC_CDMA (self), (guint32) quality);
}

/*****************************************************************************/

static const char *
strip_response (const char *resp, const char *cmd)
{
    const char *p = resp;

    if (p) {
        if (!strncmp (p, cmd, strlen (cmd)))
            p += strlen (cmd);
        while (*p == ' ')
            p++;
    }
    return p;
}

static gboolean
uint_from_match_item (GMatchInfo *match_info, guint32 num, guint32 *val)
{
    long int tmp;
    char *str;
    gboolean success = FALSE;

    str = g_match_info_fetch (match_info, num);
    g_return_val_if_fail (str != NULL, FALSE);

    errno = 0;
    tmp = strtol (str, NULL, 10);
    if (errno == 0 && tmp >= 0 && tmp <= G_MAXUINT) {
        *val = (guint32) tmp;
        success = TRUE;
    }
    g_free (str);
    return success;
}

static void
sysinfo_done (MMAtSerialPort *port,
              GString *response,
              GError *error,
              gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    GRegex *r;
    GMatchInfo *match_info;
    const char *reply;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        /* Leave superclass' reg state alone if AT^SYSINFO isn't supported */
        goto done;
    }

    reply = strip_response (response->str, "^SYSINFO:");

    /* Format is "<srv_status>,<srv_domain>,<roam_status>,<sys_mode>,<sim_state>" */
    r = g_regex_new ("\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)",
                     G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    if (!r) {
        mm_warn ("Huawei: ^SYSINFO parse regex creation failed.");
        goto done;
    }

    g_regex_match (r, reply, 0, &match_info);
    if (g_match_info_get_match_count (match_info) >= 5) {
        MMModemCdmaRegistrationState reg_state;
        guint32 val = 0;

        /* At this point the generic code already knows we've been registered */
        reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;

        if (uint_from_match_item (match_info, 1, &val)) {
            if (val == 2) {
                /* Service available, check roaming state */
                val = 0;
                if (uint_from_match_item (match_info, 3, &val)) {
                    if (val == 0)
                        reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
                    else if (val == 1)
                        reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
                }
            }
        }

        /* Check service type */
        val = 0;
        if (uint_from_match_item (match_info, 4, &val)) {
            if (val == 2)
                mm_generic_cdma_query_reg_state_set_callback_1x_state (info, reg_state);
            else if (val == 4)
                mm_generic_cdma_query_reg_state_set_callback_evdo_state (info, reg_state);
            else if (val == 8) {
                mm_generic_cdma_query_reg_state_set_callback_1x_state (info, reg_state);
                mm_generic_cdma_query_reg_state_set_callback_evdo_state (info, reg_state);
            }
        } else {
            /* Say we're registered to something even though sysmode parsing failed */
            mm_generic_cdma_query_reg_state_set_callback_1x_state (info, reg_state);
        }
    } else
        mm_warn ("Huawei: failed to parse ^SYSINFO response.");

    g_match_info_free (match_info);
    g_regex_unref (r);

done:
    mm_callback_info_schedule (info);
}

static void
query_registration_state (MMGenericCdma *cdma,
                          MMModemCdmaRegistrationState cur_cdma_state,
                          MMModemCdmaRegistrationState cur_evdo_state,
                          MMModemCdmaRegistrationStateFn callback,
                          gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_generic_cdma_query_reg_state_callback_info_new (cdma, cur_cdma_state, cur_evdo_state, callback, user_data);

    port = mm_generic_cdma_get_best_at_port (cdma, &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "^SYSINFO", 3, sysinfo_done, info);
}

static void
enable_all_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
post_enable (MMGenericCdma *cdma,
            MMModemFn callback,
            gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (cdma), callback, user_data);
    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (cdma), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }
    mm_at_serial_port_queue_command (port, "+IFC=2,2", 3, enable_all_done, info);

    info = mm_callback_info_new (MM_MODEM (cdma), callback, user_data);
    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (cdma), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }
    mm_at_serial_port_queue_command (port, "+CTA=0", 3, enable_all_done, info);
}

/*****************************************************************************/

static void
port_grabbed (MMGenericCdma *cdma,
              MMPort *port,
              MMAtPortFlags pflags,
              gpointer user_data)
{
    if (MM_IS_AT_SERIAL_PORT (port)) {
        /* Set RTS/CTS flow control by default */
        g_object_set (G_OBJECT (port), MM_SERIAL_PORT_RTS_CTS, TRUE, NULL);
    }
}

static void
ports_organized (MMGenericCdma *cdma, MMAtSerialPort *port)
{
    GRegex *regex;
    gboolean evdo0 = FALSE, evdoA = FALSE;

    g_object_set (G_OBJECT (port), MM_PORT_CARRIER_DETECT, FALSE, NULL);

    /* 1x signal level */
    regex = g_regex_new ("\\r\\n\\^RSSILVL:(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, handle_1x_quality_change, cdma, NULL);
    g_regex_unref (regex);

    g_object_get (G_OBJECT (cdma),
                  MM_GENERIC_CDMA_EVDO_REV0, &evdo0,
                  MM_GENERIC_CDMA_EVDO_REVA, &evdoA,
                  NULL);

    if (evdo0 || evdoA) {
        /* EVDO signal level */
        regex = g_regex_new ("\\r\\n\\^HRSSILVL:(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
        mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, handle_evdo_quality_change, cdma, NULL);
        g_regex_unref (regex);
    }

    /* New SMS */
    regex = g_regex_new ("\\r\\n\\+CMTI:\"(\\S+)\",(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, cmti_received, cdma, NULL);
    g_regex_unref (regex);
    regex = g_regex_new ("\\r\\n\\+CDSI: \"(\\S+)\",(\\d+)\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, cmti_received, cdma, NULL);
    g_regex_unref (regex);

    /* Ring */
    regex = g_regex_new ("\\r\\nRING\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, ring_received, cdma, NULL);
    g_regex_unref (regex);

    /* CLIP */
    regex = g_regex_new ("\\r\\n\\+CLIP:(\\S+),(\\d+),,,,(\\d+)\\s*\\r\\n",
                G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, clip_received, cdma, NULL);
    g_regex_unref (regex);

    /* Orig */
    regex = g_regex_new ("\\r\\n\\^ORIG:(\\d+),(\\d+)\\s*\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, orig_received, cdma, NULL);
    g_regex_unref (regex);

    /* Conn */
    regex = g_regex_new ("\\r\\n\\^CONN:(\\d+),(\\d+)\\s*\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, conn_received, cdma, NULL);
    g_regex_unref (regex);

    /* Cend */
    regex = g_regex_new ("\\r\\n\\^CEND:(\\d+),\\S+\\s*\\r\\n", G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    mm_at_serial_port_add_unsolicited_msg_handler (MM_AT_SERIAL_PORT (port), regex, cend_received, cdma, NULL);
    g_regex_unref (regex);
}

static void
get_1x_signal_quality_done (MMAtSerialPort *port,
                         GString *response,
                         GError *error,
                         gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
    } else {
        const char *reply = response->str;
        int quality, ber;

        /* Got valid reply */
        if (!strncmp (reply, "^CSQLVL:", 8))
            reply += 8;

        if (sscanf (reply, "%d,%d", &quality, &ber)) {
            /* 0 means unknown/no service */
            if (quality == 0) {
                info->error = g_error_new_literal (MM_MOBILE_ERROR,
                                                   MM_MOBILE_ERROR_NO_NETWORK,
                                                   "No service");
            } else {
                mm_callback_info_set_result (info, GUINT_TO_POINTER (quality), NULL);
                mm_generic_cdma_update_cdma1x_quality (MM_GENERIC_CDMA (info->modem), (guint32) quality);
            }
        } else
            info->error = g_error_new (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                       "%s", "Could not parse signal quality results");
    }

    mm_callback_info_schedule (info);
}

static void
get_evdo_signal_quality_done (MMAtSerialPort *port,
                         GString *response,
                         GError *error,
                         gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
    } else {
        const char *reply = response->str;
        int quality;

        /* Got valid reply */
        if (!strncmp (reply, "^HDRCSQLVL:", 11))
            reply += 11;

        if (sscanf (reply, "%d", &quality)) {
            /* 0 means unknown/no service */
            if (quality == 0) {
                info->error = g_error_new_literal (MM_MOBILE_ERROR,
                                                   MM_MOBILE_ERROR_NO_NETWORK,
                                                   "No service");
            } else {
                mm_callback_info_set_result (info, GUINT_TO_POINTER (quality), NULL);
                mm_generic_cdma_update_evdo_quality (MM_GENERIC_CDMA (info->modem), (guint32) quality);
            }
        } else
            info->error = g_error_new (MM_MODEM_ERROR, MM_MODEM_ERROR_GENERAL,
                                       "%s", "Could not parse signal quality results");
    }

    mm_callback_info_schedule (info);
}

static void
get_signal_quality (MMModemCdma *modem,
                    MMModemUIntFn callback,
                    gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *at_port;

    info = mm_callback_info_uint_new (MM_MODEM (modem), callback, user_data);
    at_port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!at_port) {
        guint32 quality = 0;

        mm_dbg ("Returning saved signal quality %d", quality);
        mm_callback_info_set_result (info, GUINT_TO_POINTER (quality), NULL);
        mm_callback_info_schedule (info);
        return;
    }
    g_clear_error (&info->error);

    if (at_port) {
        MMModemCdmaRegistrationState evdo_reg_state;

        evdo_reg_state = mm_generic_cdma_evdo_get_registration_state_sync (MM_GENERIC_CDMA (modem));
        if (evdo_reg_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)
          mm_at_serial_port_queue_command (at_port, "^HDRCSQLVL", 3, get_evdo_signal_quality_done, info);
        else
          mm_at_serial_port_queue_command (at_port, "^CSQLVL", 3, get_1x_signal_quality_done, info);
    }
}

/*****************************************************************************/
/* MMModemCdmaSms interface */

static void
sms_send_done (MMAtSerialPort *port,
               GString *response,
               GError *error,
               gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
sms_send (MMModemCdmaSms *modem,
          const char *number,
          const char *text,
          const char *smsc,
          guint validity,
          guint class,
          MMModemFn callback,
          gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;
    gchar command[256];
    size_t len0;
    glong len1;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    /* Use 'text' mode */
    mm_at_serial_port_queue_command (port, "+CMGF=1", 3, NULL, NULL);

    if(!g_utf8_validate (text, -1, NULL)) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Validates utf8 text string failed!");
        mm_callback_info_schedule (info);
        return;
    }

    len0 = strlen (text);
    len1 = g_utf8_strlen (text, -1);

    if (len0 == len1) { /* ascii */
        gsize l0 = 0, l1 = 0;

        mm_at_serial_port_queue_command (port, "^HSMSSS=0,0,1,0", 3, NULL, NULL);

        l1 = snprintf (command, 256, "^HCMGS=\"%s\"\r", number);
        /* Max length 160 */
        l0 = (160 > len0) ? len0 : 160;
        memmove (command + l1, text, l0);
        memmove (command + l1 + l0, "\x1a\x00", 2);

        mm_at_serial_port_queue_command (port, command, 10, sms_send_done, info);
    } else { /* unicode */
        gsize l0 = 0, l1 = 0;
        gchar *p = NULL;

        mm_at_serial_port_queue_command (port, "^HSMSSS=0,0,6,0", 3, NULL, NULL);

        l1 = snprintf (command, 256, "^HCMGS=\"%s\"\r", number);
        p = g_convert (text, len0, "UCS-2BE", "UTF8", NULL, &l0, &info->error);
        if (!p) {
            mm_callback_info_schedule (info);
            return;
        }
        /* Max length 140 */
        l0 = (140 > l0) ? l0 : 140;
        memmove (command + l1, p, l0);
        memmove (command + l1 + l0, "\x00\x1a", 2);
        g_free (p);

        mm_at_serial_port_queue_command_len (port, command, l1 + l0 + 2,
                    10, sms_send_done, info);
    }
}

static void
sms_get_done (MMAtSerialPort *port,
              GString *response,
              GError *error,
              gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    GHashTable *properties;
    gchar *str, number[20];
    int rv, year, month, day, hour, minute, second, lang;
    int format, length, prt, prv, type, stat, offset;
    gchar *text = NULL;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto out;
    }

    str = g_strstr_len (response->str, response->len, "^HCMGR:");
    if (!str) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Response string invalid!");
        goto out;
    }

    rv = sscanf (str, "^HCMGR:%[0-9*#],%d,%d,%d,"
                "%d,%d,%d,%d,""%d,%d,%d,%d,""%d,%d %n",
                number, &year, &month, &day,
                &hour, &minute, &second, &lang,
                &format, &length, &prt, &prv, 
                &type, &stat, &offset);
    if (rv != 14) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Failed to parse ^HCMGR response (parsed %d items)",
                                   rv);
        goto out;
    }

    /* message text */
    if (1 == format) { /* ascii */
        text = g_malloc0 (length + 2);
        memmove (text, str + offset, length);
    } else if (6 == format) { /* unicode */
        gsize wl = 0;

        text = g_convert (str + offset, length, "UTF8", "UCS-2BE",
                    NULL, &wl, &info->error);
        if (!text)
          goto out;
        length = wl;
    } else { /* other */
        text = NULL;
        length = 0;
    }

    properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                simple_free_gvalue);
    if (!properties) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Failed to create hash table!");
        goto out;
    }

    /* Set properties */
    g_hash_table_insert (properties, "number",
                simple_string_value (number));
    g_hash_table_insert (properties, "year",
                simple_uint_value (year));
    g_hash_table_insert (properties, "month",
                simple_uint_value (month));
    g_hash_table_insert (properties, "day",
                simple_uint_value (day));
    g_hash_table_insert (properties, "hour",
                simple_uint_value (hour));
    g_hash_table_insert (properties, "minute",
                simple_uint_value (minute));
    g_hash_table_insert (properties, "second",
                simple_uint_value (second));
    g_hash_table_insert (properties, "lang",
                simple_uint_value (lang));
    g_hash_table_insert (properties, "format",
                simple_uint_value (format));
    g_hash_table_insert (properties, "length",
                simple_uint_value (length));
    g_hash_table_insert (properties, "prt",
                simple_uint_value (prt));
    g_hash_table_insert (properties, "prv",
                simple_uint_value (prv));
    g_hash_table_insert (properties, "type",
                simple_uint_value (type));
    g_hash_table_insert (properties, "stat",
                simple_uint_value (stat));
    g_hash_table_insert (properties, "completed",
                simple_boolean_value (TRUE));
    g_hash_table_insert (properties, "text",
                simple_string_value (text));
    g_free(text);

    mm_callback_info_set_data (info, "get-sms", properties,
                               (GDestroyNotify) g_hash_table_unref);

out:
    mm_callback_info_schedule (info);
}

static void
sms_get_invoke (MMCallbackInfo *info)
{
    MMModemCdmaSmsGetFn callback = (MMModemCdmaSmsGetFn) info->callback;

    callback (MM_MODEM_CDMA_SMS (info->modem),
              (GHashTable *) mm_callback_info_get_data (info, "get-sms"),
              info->error, info->user_data);
}

static void
sms_get (MMModemCdmaSms *modem,
         guint idx,
         MMModemCdmaSmsGetFn callback,
         gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new_full (MM_MODEM (modem),
                                      sms_get_invoke,
                                      G_CALLBACK (callback),
                                      user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("^HCMGR=%d\r\n", idx);
    mm_at_serial_port_queue_command (port, command, 10, sms_get_done, info);
    g_free (command);
}

static void
sms_delete_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
sms_delete (MMModemCdmaSms *modem,
            guint idx,
            MMModemFn callback,
            gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("+CMGD=%d\r\n", idx);
    mm_at_serial_port_queue_command (port, command, 10, sms_delete_done, info);
    g_free (command);
}

static void
sms_list_done (MMAtSerialPort *port,
               GString *response,
               GError *error,
               gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;
    GPtrArray *results = NULL;
    gint rv, status, offset;
    gchar *rstr;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto out;
    }

    results = g_ptr_array_new ();
    rstr = g_strstr_len (response->str, response->len, "^HCMGL:");
    if (0<response->len && !rstr) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Response string invalid!");
        goto out;
    }

    while (rstr && *rstr) {
        GHashTable *properties;
        int idx;

        rv = sscanf (rstr, "^HCMGL:%d,%d %n",
                    &idx, &status, &offset);
        if (2 != rv) {
            info->error = g_error_new (MM_MODEM_ERROR,
                                       MM_MODEM_ERROR_GENERAL,
                                       "Failed to parse ^HCMGL response (parsed %d items)",
                                       rv);
            goto out;
        }
        rstr += offset;

        properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                            simple_free_gvalue);
        if (properties) {
            g_hash_table_insert (properties, "index",
                                 simple_uint_value (idx));
            g_hash_table_insert (properties, "status",
                                 simple_uint_value (status));
            g_hash_table_insert (properties, "completed",
                        simple_boolean_value (TRUE));

            g_ptr_array_add (results, properties);
        }
    }
    /*
     * todo(njw): mm_cdma_destroy_scan_data does what we want
     * (destroys a GPtrArray of g_hash_tables), but it should be
     * renamed to describe that or there should be a function
     * named for what we're doing here.
     */
    if (results)
        mm_callback_info_set_data (info, "list-sms", results,
                                   mm_cdma_destroy_scan_data);

out:
    mm_callback_info_schedule (info);
}

static void
sms_list_invoke (MMCallbackInfo *info)
{
    MMModemCdmaSmsListFn callback = (MMModemCdmaSmsListFn) info->callback;

    callback (MM_MODEM_CDMA_SMS (info->modem),
              (GPtrArray *) mm_callback_info_get_data (info, "list-sms"),
              info->error, info->user_data);
}

static void
sms_list (MMModemCdmaSms *modem,
          MMModemCdmaSmsListFn callback,
          gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_new_full (MM_MODEM (modem),
                                      sms_list_invoke,
                                      G_CALLBACK (callback),
                                      user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "^HCMGL=4", 10, sms_list_done, info);
}

/*****************************************************************************/

static void
phone_call_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_call (MMModemCdmaPhone *modem,
                  const char *number,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("+CDV%s", number);
    mm_at_serial_port_queue_command (port, command, 10, phone_call_done, info);
    g_free (command);
}

static void
phone_answer_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_answer (MMModemCdmaPhone *modem,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "A", 10, phone_answer_done, info);
}

static void
phone_end_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_end (MMModemCdmaPhone *modem,
                    MMModemFn callback,
                    gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "+CHV", 10, phone_end_done, info);
}

static void
phone_send_dtmf_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_send_dtmf (MMModemCdmaPhone *modem,
                guint32 id,
                const char *digit,
                MMModemFn callback,
                gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("^DTMF=%d,%s", id, digit);
    mm_at_serial_port_queue_command (port, command, 10, phone_send_dtmf_done, info);
    g_free (command);
}

static void
phone_get_status_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *)user_data;
    GHashTable *properties;
    gchar *str = NULL, number[20];
    guint id, dir, state, mode, mpty, type, alpha, priority;
    gint rv, offset;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error) {
        info->error = g_error_copy (error);
        goto out;
    }

    str = g_strstr_len (response->str, response->len, "+CLCC:");
    if (!str) {
        properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                    simple_free_gvalue);
        if (!properties) {
            info->error = g_error_new (MM_MODEM_ERROR,
                                       MM_MODEM_ERROR_GENERAL,
                                       "Failed to create hash table!");
            goto out;
        }

        /* Set properties */
        g_hash_table_insert (properties, "id",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "dir",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "state",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "mode",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "mpty",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "number",
                    simple_string_value (""));
        g_hash_table_insert (properties, "type",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "alpha",
                    simple_uint_value (0));
        g_hash_table_insert (properties, "priority",
                    simple_uint_value (0));

        mm_callback_info_set_data (info, "get-status", properties,
                                   (GDestroyNotify) g_hash_table_unref);
        goto out;
    }

    rv = sscanf (str, "+CLCC:%d,%d,%d,%d,%d,"
                "%[0-9*#],%d,%d,%d %n", &id, &dir,
                &state, &mode, &mpty, number,
                &type, &alpha, &priority, &offset);
    if (rv != 9) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Failed to parse +CLCC response (parsed %d items)",
                                   rv);
        goto out;
    }

    properties = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                simple_free_gvalue);
    if (!properties) {
        info->error = g_error_new (MM_MODEM_ERROR,
                                   MM_MODEM_ERROR_GENERAL,
                                   "Failed to create hash table!");
        goto out;
    }

    /* Set properties */
    g_hash_table_insert (properties, "id",
                simple_uint_value (id));
    g_hash_table_insert (properties, "dir",
                simple_uint_value (dir));
    g_hash_table_insert (properties, "state",
                simple_uint_value (state));
    g_hash_table_insert (properties, "mode",
                simple_uint_value (mode));
    g_hash_table_insert (properties, "mpty",
                simple_uint_value (mpty));
    g_hash_table_insert (properties, "number",
                simple_string_value (number));
    g_hash_table_insert (properties, "type",
                simple_uint_value (type));
    g_hash_table_insert (properties, "alpha",
                simple_uint_value (alpha));
    g_hash_table_insert (properties, "priority",
                simple_uint_value (priority));

    mm_callback_info_set_data (info, "get-status", properties,
                               (GDestroyNotify) g_hash_table_unref);

out:
    mm_callback_info_schedule (info);
}

static void
phone_get_status_invoke (MMCallbackInfo *info)
{
    MMModemCdmaPhoneGetFn callback = (MMModemCdmaPhoneGetFn) info->callback;

    callback (MM_MODEM_CDMA_PHONE (info->modem),
              (GHashTable *) mm_callback_info_get_data (info, "get-status"),
              info->error, info->user_data);
}

static void
phone_get_status (MMModemCdmaPhone *modem,
            MMModemCdmaPhoneGetFn callback,
            gpointer user_data)
{
    MMCallbackInfo *info;
    MMAtSerialPort *port;

    info = mm_callback_info_new_full (MM_MODEM (modem),
                                      phone_get_status_invoke,
                                      G_CALLBACK (callback),
                                      user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    mm_at_serial_port_queue_command (port, "+CLCC", 10, phone_get_status_done, info);
}

static void
phone_mute_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_mute (MMModemCdmaPhone *modem,
                  gboolean mute,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("+CMUT=%u", mute?1:0);
    mm_at_serial_port_queue_command (port, command, 10, phone_mute_done, info);
    g_free (command);
}

static void
phone_clvl_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_clvl (MMModemCdmaPhone *modem,
                  guint level,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("+CLVL=%u", level);
    mm_at_serial_port_queue_command (port, command, 10, phone_clvl_done, info);
    g_free (command);
}

static void
phone_cmiclvl_done (MMAtSerialPort *port,
                 GString *response,
                 GError *error,
                 gpointer user_data)
{
    MMCallbackInfo *info = (MMCallbackInfo *) user_data;

    /* If the modem has already been removed, return without
     * scheduling callback */
    if (mm_callback_info_check_modem_removed (info))
        return;

    if (error)
        info->error = g_error_copy (error);

    mm_callback_info_schedule (info);
}

static void
phone_cmiclvl (MMModemCdmaPhone *modem,
                  guint level,
                  MMModemFn callback,
                  gpointer user_data)
{
    MMCallbackInfo *info;
    char *command;
    MMAtSerialPort *port;

    info = mm_callback_info_new (MM_MODEM (modem), callback, user_data);

    port = mm_generic_cdma_get_best_at_port (MM_GENERIC_CDMA (modem), &info->error);
    if (!port) {
        mm_callback_info_schedule (info);
        return;
    }

    command = g_strdup_printf ("^CMICLVL=%u", level);
    mm_at_serial_port_queue_command (port, command, 10, phone_cmiclvl_done, info);
    g_free (command);
}

/*****************************************************************************/

static void
simple_free_gvalue (gpointer data)
{
    g_value_unset ((GValue *) data);
    g_slice_free (GValue, data);
}

static GValue *
simple_uint_value (guint32 i)
{
    GValue *val;

    val = g_slice_new0 (GValue);
    g_value_init (val, G_TYPE_UINT);
    g_value_set_uint (val, i);

    return val;
}

static GValue *
simple_boolean_value (gboolean b)
{
    GValue *val;

    val = g_slice_new0 (GValue);
    g_value_init (val, G_TYPE_BOOLEAN);
    g_value_set_boolean (val, b);

    return val;
}

static GValue *
simple_string_value (const char *str)
{
    GValue *val;

    val = g_slice_new0 (GValue);
    g_value_init (val, G_TYPE_STRING);
    g_value_set_string (val, str);

    return val;
}

/*****************************************************************************/

static void
cmti_received (MMAtSerialPort *port,
               GMatchInfo *info,
               gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);
    guint idx = 0;
    char *str;

    str = g_match_info_fetch (info, 2);
    if (str)
        idx = atoi (str);
    g_free (str);

    /* todo: parse pdu to know if the sms is complete */
    mm_modem_cdma_sms_received (MM_MODEM_CDMA_SMS (self),
                               idx,
                               TRUE);

    /* todo: send mm_modem_cdma_sms_completed if complete */
}

static void
ring_received (MMAtSerialPort *port,
               GMatchInfo *info,
               gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);

    mm_modem_cdma_phone_ring (MM_MODEM_CDMA_PHONE (self));
}

static void clip_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);
    char *str;

    str = g_match_info_fetch (info, 1);
    mm_modem_cdma_phone_clip (MM_MODEM_CDMA_PHONE (self), str);
}

static void orig_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);
    guint idx = 0;
    char *str;

    str = g_match_info_fetch (info, 1);
    if (str)
        idx = atoi (str);
    g_free (str);

    mm_modem_cdma_phone_orig (MM_MODEM_CDMA_PHONE (self), idx);
}

static void conn_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);
    guint idx = 0;
    char *str;

    str = g_match_info_fetch (info, 1);
    if (str)
        idx = atoi (str);
    g_free (str);

    mm_modem_cdma_phone_conn (MM_MODEM_CDMA_PHONE (self), idx);
}

static void cend_received (MMAtSerialPort *port,
                           GMatchInfo *info,
                           gpointer user_data)
{
    MMGenericCdma *self = MM_GENERIC_CDMA (user_data);
    guint idx = 0;
    char *str;

    str = g_match_info_fetch (info, 1);
    if (str)
        idx = atoi (str);
    g_free (str);

    mm_modem_cdma_phone_cend (MM_MODEM_CDMA_PHONE (self), idx);
}

/*****************************************************************************/

static void
modem_cdma_init (MMModemCdma *cdma_class)
{
    cdma_class->get_signal_quality = get_signal_quality;
}

static void
modem_cdma_sms_init (MMModemCdmaSms *class)
{
    class->send = sms_send;
    class->get = sms_get;
    class->delete = sms_delete;
    class->list = sms_list;
}

static void
modem_cdma_phone_init (MMModemCdmaPhone *class)
{
	class->call = phone_call;
	class->answer = phone_answer;
	class->end = phone_end;
	class->send_dtmf = phone_send_dtmf;
	class->get_status = phone_get_status;
	class->mute = phone_mute;
	class->clvl = phone_clvl;
	class->cmiclvl = phone_cmiclvl;
}

static void
mm_modem_huawei_cdma_init (MMModemHuaweiCdma *self)
{
}

static void
mm_modem_huawei_cdma_class_init (MMModemHuaweiCdmaClass *klass)
{
    MMGenericCdmaClass *cdma_class = MM_GENERIC_CDMA_CLASS (klass);

    mm_modem_huawei_cdma_parent_class = g_type_class_peek_parent (klass);

    cdma_class->port_grabbed= port_grabbed;
    cdma_class->ports_organized = ports_organized;
    cdma_class->query_registration_state = query_registration_state;
    cdma_class->post_enable = post_enable;
}

