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

#ifndef MM_MODEM_CDMA_PHONE_H
#define MM_MODEM_CDMA_PHONE_H

#include <mm-modem.h>

#define MM_TYPE_MODEM_CDMA_PHONE      (mm_modem_cdma_phone_get_type ())
#define MM_MODEM_CDMA_PHONE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_MODEM_CDMA_PHONE, MMModemCdmaPhone))
#define MM_IS_MODEM_CDMA_PHONE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_MODEM_CDMA_PHONE))
#define MM_MODEM_CDMA_PHONE_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), MM_TYPE_MODEM_CDMA_PHONE, MMModemCdmaPhone))

typedef struct _MMModemCdmaPhone MMModemCdmaPhone;

typedef void (*MMModemCdmaPhoneGetFn) (MMModemCdmaPhone *modem,
                                    GHashTable *properties,
                                    GError *error,
                                    gpointer user_data);

struct _MMModemCdmaPhone {
    GTypeInterface g_iface;

    /* Methods */
    void (*call) (MMModemCdmaPhone *modem,
                  const char *number,
                  MMModemFn callback,
                  gpointer user_data);

    void (*answer) (MMModemCdmaPhone *modem,
                  MMModemFn callback,
				  gpointer user_data);

    void (*end) (MMModemCdmaPhone *modem,
                    MMModemFn callback,
                    gpointer user_data);

    void (*send_dtmf) (MMModemCdmaPhone *modem,
				guint32 id,
				const char *digit,
				MMModemFn callback,
				gpointer user_data);

    void (*get_status) (MMModemCdmaPhone *modem,
                    MMModemCdmaPhoneGetFn callback,
                    gpointer user_data);

    void (*mute) (MMModemCdmaPhone *modem,
                  gboolean mute,
                  MMModemFn callback,
                  gpointer user_data);

    void (*clvl) (MMModemCdmaPhone *modem,
                  guint level,
                  MMModemFn callback,
                  gpointer user_data);

    void (*cmiclvl) (MMModemCdmaPhone *modem,
                  guint level,
                  MMModemFn callback,
                  gpointer user_data);

    /* Signals */
    void (*ring) (MMModemCdmaPhone *self);

    void (*clip) (MMModemCdmaPhone *self,
				const char *number);

    void (*orig) (MMModemCdmaPhone *self,
				guint32 id);

    void (*conn) (MMModemCdmaPhone *self,
				guint32 id);

    void (*cend) (MMModemCdmaPhone *self,
				guint32 id);
};

GType mm_modem_cdma_phone_get_type (void);

void mm_modem_cdma_phone_call (MMModemCdmaPhone *self,
                            const char *number,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_answer (MMModemCdmaPhone *self,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_end (MMModemCdmaPhone *self,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_send_dtmf (MMModemCdmaPhone *self,
							guint32 id,
							const char *digit,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_get_status (MMModemCdmaPhone *self,
                            MMModemCdmaPhoneGetFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_mute (MMModemCdmaPhone *self,
                            gboolean mute,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_clvl (MMModemCdmaPhone *self,
                            guint level,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_cmiclvl (MMModemCdmaPhone *self,
                            guint level,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_phone_ring (MMModemCdmaPhone *self);

void mm_modem_cdma_phone_clip (MMModemCdmaPhone *self,
							const char *number);

void mm_modem_cdma_phone_orig (MMModemCdmaPhone *self,
							guint32 id);

void mm_modem_cdma_phone_conn (MMModemCdmaPhone *self,
							guint32 id);

void mm_modem_cdma_phone_cend (MMModemCdmaPhone *self,
							guint32 id);


#endif /* MM_MODEM_CDMA_PHONE_H */
