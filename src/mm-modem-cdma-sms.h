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

#ifndef MM_MODEM_CDMA_SMS_H
#define MM_MODEM_CDMA_SMS_H

#include <mm-modem.h>

#define MM_TYPE_MODEM_CDMA_SMS      (mm_modem_cdma_sms_get_type ())
#define MM_MODEM_CDMA_SMS(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_MODEM_CDMA_SMS, MMModemCdmaSms))
#define MM_IS_MODEM_CDMA_SMS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_MODEM_CDMA_SMS))
#define MM_MODEM_CDMA_SMS_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), MM_TYPE_MODEM_CDMA_SMS, MMModemCdmaSms))

typedef struct _MMModemCdmaSms MMModemCdmaSms;

typedef void (*MMModemCdmaSmsGetFn) (MMModemCdmaSms *modem,
                                    GHashTable *properties,
                                    GError *error,
                                    gpointer user_data);

typedef void (*MMModemCdmaSmsListFn) (MMModemCdmaSms *modem,
                                     GPtrArray *resultlist,
                                     GError *error,
                                     gpointer user_data);

struct _MMModemCdmaSms {
    GTypeInterface g_iface;

    /* Methods */
    void (*send) (MMModemCdmaSms *modem,
                  const char *number,
                  const char *text,
                  const char *smsc,
                  guint validity,
                  guint class,
                  MMModemFn callback,
                  gpointer user_data);

    void (*get) (MMModemCdmaSms *modem,
                 guint32 index,
                 MMModemCdmaSmsGetFn callback,
                 gpointer user_data);

    void (*delete) (MMModemCdmaSms *modem,
                    guint32 index,
                    MMModemFn callback,
                    gpointer user_data);

    void (*list) (MMModemCdmaSms *modem,
                  MMModemCdmaSmsListFn callback,
                  gpointer user_data);

    /* Signals */
    void (*sms_received) (MMModemCdmaSms *self,
                          guint32 index,
                          gboolean completed);

    void (*completed)    (MMModemCdmaSms *self,
                          guint32 index,
                          gboolean completed);
};

GType mm_modem_cdma_sms_get_type (void);

void mm_modem_cdma_sms_send (MMModemCdmaSms *self,
                            const char *number,
                            const char *text,
                            const char *smsc,
                            guint validity,
                            guint class,
                            MMModemFn callback,
                            gpointer user_data);

void mm_modem_cdma_sms_get (MMModemCdmaSms *self,
                           guint idx,
                           MMModemCdmaSmsGetFn callback,
                           gpointer user_data);

void mm_modem_cdma_sms_delete (MMModemCdmaSms *self,
                              guint idx,
                              MMModemFn callback,
                              gpointer user_data);

void mm_modem_cdma_sms_list (MMModemCdmaSms *self,
                            MMModemCdmaSmsListFn callback,
                            gpointer user_data);

void mm_modem_cdma_sms_received (MMModemCdmaSms *self,
                                guint idx,
                                gboolean complete);

void mm_modem_cdma_sms_completed (MMModemCdmaSms *self,
                                guint idx,
                                gboolean complete);


#endif /* MM_MODEM_CDMA_SMS_H */
