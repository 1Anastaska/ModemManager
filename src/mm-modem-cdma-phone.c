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
 * Copyright (C) 2009 Novell, Inc.
 */

#include <string.h>
#include <dbus/dbus-glib.h>

#include "mm-modem-cdma-phone.h"
#include "mm-errors.h"
#include "mm-callback-info.h"
#include "mm-marshal.h"

static void impl_cdma_modem_phone_call (MMModemCdmaPhone *modem,
										const gchar *number,
										DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_answer (MMModemCdmaPhone *modem,
										DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_end (MMModemCdmaPhone *modem,
										DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_send_dtmf (MMModemCdmaPhone *modem,
										GHashTable *properties,
                                        DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_get_status (MMModemCdmaPhone *modem,
                                        DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_mute (MMModemCdmaPhone *modem,
										gboolean mute,
										DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_clvl (MMModemCdmaPhone *modem,
										guint level,
										DBusGMethodInvocation *context);

static void impl_cdma_modem_phone_cmiclvl (MMModemCdmaPhone *modem,
										guint level,
										DBusGMethodInvocation *context);

#include "mm-modem-cdma-phone-glue.h"

enum {
    RING,
	CLIP,
	ORIG,
	CONN,
	ENDED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/*****************************************************************************/

static void
async_call_done (MMModem *modem, GError *error, gpointer user_data)
{
    DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;

    if (error)
        dbus_g_method_return_error (context, error);
    else
        dbus_g_method_return (context);
}

static void
async_call_not_supported (MMModemCdmaPhone *self,
                          MMModemFn callback,
                          gpointer user_data)
{
    MMCallbackInfo *info;

    info = mm_callback_info_new (MM_MODEM (self), callback, user_data);
    info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                                       "Operation not supported");
    mm_callback_info_schedule (info);
}

static void
phone_get_status_done (MMModemCdmaPhone *self,
              GHashTable *properties,
              GError *error,
              gpointer user_data)
{
    DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;

    if (error)
        dbus_g_method_return_error (context, error);
    else
        dbus_g_method_return (context, properties);
}

static void
async_mute_done (MMModem *modem, GError *error, gpointer user_data)
{
    DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;

    if (error)
        dbus_g_method_return_error (context, error);
    else
        dbus_g_method_return (context);
}

static void
async_clvl_done (MMModem *modem, GError *error, gpointer user_data)
{
    DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;

    if (error)
        dbus_g_method_return_error (context, error);
    else
        dbus_g_method_return (context);
}

static void
async_cmiclvl_done (MMModem *modem, GError *error, gpointer user_data)
{
    DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;

    if (error)
        dbus_g_method_return_error (context, error);
    else
        dbus_g_method_return (context);
}

/*****************************************************************************/

void
mm_modem_cdma_phone_send_dtmf (MMModemCdmaPhone *self,
                       guint32 id,
					   const gchar *digit,
                       MMModemFn callback,
                       gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->send_dtmf)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->send_dtmf (self, id, digit, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);

}

static void
phone_get_status_invoke (MMCallbackInfo *info)
{
    MMModemCdmaPhoneGetFn callback = (MMModemCdmaPhoneGetFn) info->callback;

    callback (MM_MODEM_CDMA_PHONE (info->modem), NULL, info->error, info->user_data);
}

void
mm_modem_cdma_phone_get_status (MMModemCdmaPhone *self,
                       MMModemCdmaPhoneGetFn callback,
                       gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->get_status)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->get_status (self, callback, user_data);
    else {
        MMCallbackInfo *info;

        info = mm_callback_info_new_full (MM_MODEM (self),
                                          phone_get_status_invoke,
                                          G_CALLBACK (callback),
                                          user_data);

        info->error = g_error_new_literal (MM_MODEM_ERROR, MM_MODEM_ERROR_OPERATION_NOT_SUPPORTED,
                                           "Operation not supported");
        mm_callback_info_schedule (info);
    }
}

void
mm_modem_cdma_phone_end (MMModemCdmaPhone *self,
                       MMModemFn callback,
                       gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->end)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->end (self, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);

}

void
mm_modem_cdma_phone_answer (MMModemCdmaPhone *self,
                       MMModemFn callback,
                       gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->answer)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->answer (self, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);

}

void
mm_modem_cdma_phone_call (MMModemCdmaPhone *self,
                         const gchar *number,
                         MMModemFn callback,
                         gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->call)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->call (self, number, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);
}

void
mm_modem_cdma_phone_mute (MMModemCdmaPhone *self,
                         gboolean mute,
                         MMModemFn callback,
                         gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->mute)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->mute (self, mute, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);
}

void
mm_modem_cdma_phone_clvl (MMModemCdmaPhone *self,
                         guint level,
                         MMModemFn callback,
                         gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->clvl)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->clvl (self, level, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);
}

void
mm_modem_cdma_phone_cmiclvl (MMModemCdmaPhone *self,
                         guint level,
                         MMModemFn callback,
                         gpointer user_data)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));
    g_return_if_fail (callback != NULL);

    if (MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->cmiclvl)
        MM_MODEM_CDMA_PHONE_GET_INTERFACE (self)->cmiclvl (self, level, callback, user_data);
    else
        async_call_not_supported (self, callback, user_data);
}

void
mm_modem_cdma_phone_ring (MMModemCdmaPhone *self)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));

    g_signal_emit (self, signals[RING], 0);
}

void
mm_modem_cdma_phone_clip (MMModemCdmaPhone *self,
					const gchar *number)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));

    g_signal_emit (self, signals[CLIP], 0, number);
}

void
mm_modem_cdma_phone_orig (MMModemCdmaPhone *self,
					guint32 id)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));

    g_signal_emit (self, signals[ORIG], 0, id);
}

void
mm_modem_cdma_phone_conn (MMModemCdmaPhone *self,
					guint32 id)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));

    g_signal_emit (self, signals[CONN], 0, id);
}

void
mm_modem_cdma_phone_cend (MMModemCdmaPhone *self,
					guint32 id)
{
    g_return_if_fail (MM_IS_MODEM_CDMA_PHONE (self));

    g_signal_emit (self, signals[ENDED], 0, id);
}

/*****************************************************************************/

typedef struct {
    guint num1;
    guint num2;
    guint num3;
    guint num4;
    guint num5;
    char *str;
    GHashTable *hash;
} PhoneAuthInfo;

static void
phone_auth_info_destroy (gpointer data)
{
    PhoneAuthInfo *info = data;

    if (info->hash)
        g_hash_table_destroy (info->hash);
    g_free (info->str);
    memset (info, 0, sizeof (PhoneAuthInfo));
    g_free (info);
}

static void
destroy_gvalue (gpointer data)
{
    GValue *value = (GValue *) data;

    g_value_unset (value);
    g_slice_free (GValue, value);
}

static PhoneAuthInfo *
phone_auth_info_new (guint num1,
                   guint num2,
                   guint num3,
                   guint num4,
                   guint num5,
                   const char *str,
                   GHashTable *hash)
{
    PhoneAuthInfo *info;

    info = g_malloc0 (sizeof (PhoneAuthInfo));
    info->num1 = num1;
    info->num2 = num2;
    info->num3 = num3;
    info->num4 = num4;
    info->num5 = num5;
    info->str = g_strdup (str);

    /* Copy the hash table if we're given one */
    if (hash) {
        GHashTableIter iter;
        gpointer key, value;

        info->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, destroy_gvalue);

        g_hash_table_iter_init (&iter, hash);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
            const char *str_key = (const char *) key;
            GValue *src = (GValue *) value;
            GValue *dst;

            dst = g_slice_new0 (GValue);
            g_value_init (dst, G_VALUE_TYPE (src));
            g_value_copy (src, dst);
            g_hash_table_insert (info->hash, g_strdup (str_key), dst);
        }
    }

    return info;
}

/*****************************************************************************/

static void
phone_call_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    PhoneAuthInfo *info = user_data;
    GError *error = NULL;
    const gchar *number = NULL;

    /* Return any authorization error, otherwise make the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
		number = info->str;
        mm_modem_cdma_phone_call (self, number, async_call_done, context);
    }
}

static void
impl_cdma_modem_phone_call (MMModemCdmaPhone *modem,
                           const gchar *number,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;
    PhoneAuthInfo *info;

    info = phone_auth_info_new (0, 0, 0, 0, 0, number, NULL);

    /* Make sure the caller is authorized to make an call */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_call_auth_cb,
                                info,
                                phone_auth_info_destroy,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

/*****************************************************************************/

static void
phone_answer_auth_cb (MMAuthRequest *req,
                  GObject *owner,
                  DBusGMethodInvocation *context,
                  gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    GError *error = NULL;

    /* Return any authorization error, otherwise answer the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error))
        goto done;

done:
    if (error) {
        async_call_done (MM_MODEM (self), error, context);
        g_error_free (error);
    } else
        mm_modem_cdma_phone_answer (self, async_call_done, context);
}

static void
impl_cdma_modem_phone_answer (MMModemCdmaPhone *modem,
                         DBusGMethodInvocation *context)
{
    GError *error = NULL;

    /* Make sure the caller is authorized to answer the call */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_answer_auth_cb,
                                NULL,
                                NULL,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

/*****************************************************************************/

static void
phone_end_auth_cb (MMAuthRequest *req,
                  GObject *owner,
                  DBusGMethodInvocation *context,
                  gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    GError *error = NULL;

    /* Return any authorization error, otherwise end the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error))
        goto done;

done:
    if (error) {
        async_call_done (MM_MODEM (self), error, context);
        g_error_free (error);
    } else
        mm_modem_cdma_phone_end (self, async_call_done, context);
}

static void
impl_cdma_modem_phone_end (MMModemCdmaPhone *modem,
							DBusGMethodInvocation *context)
{
    GError *error = NULL;

    /* Make sure the caller is authorized to end the call */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_end_auth_cb,
                                NULL,
                                NULL,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

/*****************************************************************************/

static void
phone_send_dtmf_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    PhoneAuthInfo *info = user_data;
    GError *error = NULL;
    GValue *value;
	guint32 id = 0;
    const gchar *digit = NULL;


    value = (GValue *) g_hash_table_lookup (info->hash, "id");
    if (value)
        id = g_value_get_uint (value);

    value = (GValue *) g_hash_table_lookup (info->hash, "digit");
    if (value)
        digit = g_value_get_string (value);

    /* Return any authorization error, otherwise send the dtmf */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
        mm_modem_cdma_phone_send_dtmf (self, id, digit, async_call_done, context);
    }
}

static void
impl_cdma_modem_phone_send_dtmf (MMModemCdmaPhone *modem,
                           GHashTable *properties,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;
    PhoneAuthInfo *info;

    info = phone_auth_info_new (0, 0, 0, 0, 0, NULL, properties);

    /* Make sure the caller is authorized to send the dtmf */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_send_dtmf_auth_cb,
                                info,
                                phone_auth_info_destroy,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

static void
phone_get_status_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    GError *error = NULL;

    /* Return any authorization error, otherwise send the dtmf */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
        mm_modem_cdma_phone_get_status (self, phone_get_status_done, context);
    }
}

static void
impl_cdma_modem_phone_get_status (MMModemCdmaPhone *modem,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;

    /* Make sure the caller is authorized to send the dtmf */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_get_status_auth_cb,
                                NULL,
                                NULL,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

/*****************************************************************************/

static void
phone_mute_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    PhoneAuthInfo *info = user_data;
    GError *error = NULL;
	gboolean mute = FALSE;

    /* Return any authorization error, otherwise make the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
		mute = (0==info->num1) ? FALSE : TRUE;
        mm_modem_cdma_phone_mute (self, mute, async_mute_done, context);
    }
}

static void
impl_cdma_modem_phone_mute (MMModemCdmaPhone *modem,
                           gboolean mute,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;
    PhoneAuthInfo *info;

    info = phone_auth_info_new (mute?1:0, 0, 0, 0, 0, NULL, NULL);

    /* Make sure the caller is authorized to mute */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_mute_auth_cb,
                                info,
                                phone_auth_info_destroy,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

static void
phone_clvl_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    PhoneAuthInfo *info = user_data;
    GError *error = NULL;
	guint level = 0;

    /* Return any authorization error, otherwise make the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
		level = info->num1;
        mm_modem_cdma_phone_clvl (self, level, async_clvl_done, context);
    }
}

static void
impl_cdma_modem_phone_clvl (MMModemCdmaPhone *modem,
                           guint level,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;
    PhoneAuthInfo *info;

    info = phone_auth_info_new (level, 0, 0, 0, 0, NULL, NULL);

    /* Make sure the caller is authorized to clvl */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_clvl_auth_cb,
                                info,
                                phone_auth_info_destroy,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

static void
phone_cmiclvl_auth_cb (MMAuthRequest *req,
                    GObject *owner,
                    DBusGMethodInvocation *context,
                    gpointer user_data)
{
    MMModemCdmaPhone *self = MM_MODEM_CDMA_PHONE (owner);
    PhoneAuthInfo *info = user_data;
    GError *error = NULL;
	guint level = 0;

    /* Return any authorization error, otherwise make the call */
    if (!mm_modem_auth_finish (MM_MODEM (self), req, &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    } else {
		level = info->num1;
        mm_modem_cdma_phone_cmiclvl (self, level, async_cmiclvl_done, context);
    }
}

static void
impl_cdma_modem_phone_cmiclvl (MMModemCdmaPhone *modem,
                           guint level,
                           DBusGMethodInvocation *context)
{
    GError *error = NULL;
    PhoneAuthInfo *info;

    info = phone_auth_info_new (level, 0, 0, 0, 0, NULL, NULL);

    /* Make sure the caller is authorized to cmiclvl */
    if (!mm_modem_auth_request (MM_MODEM (modem),
                                MM_AUTHORIZATION_PHONE,
                                context,
                                phone_cmiclvl_auth_cb,
                                info,
                                phone_auth_info_destroy,
                                &error)) {
        dbus_g_method_return_error (context, error);
        g_error_free (error);
    }
}

/*****************************************************************************/

static void
mm_modem_cdma_phone_init (gpointer g_iface)
{
    GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);
    static gboolean initialized = FALSE;

    if (initialized)
        return;

    /* Signals */
    signals[RING] =
        g_signal_new ("ring",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMModemCdmaPhone, ring),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[CLIP] =
        g_signal_new ("clip",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMModemCdmaPhone, clip),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[ORIG] =
        g_signal_new ("orig",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMModemCdmaPhone, orig),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT);

    signals[CONN] =
        g_signal_new ("conn",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMModemCdmaPhone, conn),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT);

    signals[ENDED] =
        g_signal_new ("cend",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MMModemCdmaPhone, cend),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE, 1, G_TYPE_UINT);

    initialized = TRUE;
}

GType
mm_modem_cdma_phone_get_type (void)
{
    static GType phone_type = 0;

    if (!G_UNLIKELY (phone_type)) {
        const GTypeInfo phone_info = {
            sizeof (MMModemCdmaPhone), /* class_size */
            mm_modem_cdma_phone_init,   /* base_init */
            NULL,       /* base_finalize */
            NULL,
            NULL,       /* class_finalize */
            NULL,       /* class_data */
            0,
            0,              /* n_preallocs */
            NULL
        };

        phone_type = g_type_register_static (G_TYPE_INTERFACE,
                                           "MMModemCdmaPhone",
                                           &phone_info, 0);

        g_type_interface_add_prerequisite (phone_type, G_TYPE_OBJECT);
        dbus_g_object_type_install_info (phone_type, &dbus_glib_mm_modem_cdma_phone_object_info);
    }

    return phone_type;
}
