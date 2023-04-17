// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Collabora Ltd
 */

#ifndef APPINFO_H
#define APPINFO_H

#include <glib-object.h>

typedef enum {
    APP_STATUS_INACTIVE,
    APP_STATUS_STARTING,
    APP_STATUS_RUNNING
} AppStatus;

G_BEGIN_DECLS

#define APPLAUNCHD_TYPE_APP_INFO app_info_get_type()

G_DECLARE_FINAL_TYPE(AppInfo, app_info, APPLAUNCHD,
                     APP_INFO, GObject);

AppInfo *app_info_new(const gchar *app_id, const gchar *name,
                      const gchar *icon_path, const gchar *service);

/* Accessors for read-only members */
const gchar *app_info_get_app_id(AppInfo *self);
const gchar *app_info_get_name(AppInfo *self);
const gchar *app_info_get_icon_path(AppInfo *self);
const gchar *app_info_get_service(AppInfo *self);

/* Accessors for read-write members */
AppStatus app_info_get_status(AppInfo *self);
void app_info_set_status(AppInfo *self, AppStatus status);

gpointer app_info_get_runtime_data(AppInfo *self);
void app_info_set_runtime_data(AppInfo *self, gpointer runtime_data);

G_END_DECLS

#endif
