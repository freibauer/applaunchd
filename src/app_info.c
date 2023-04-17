// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Collabora Ltd
 */

#include <gio/gio.h>

#include "app_info.h"

struct _AppInfo {
    GObject parent_instance;

    gchar *app_id;
    gchar *name;
    gchar *icon_path;
    gchar *service;

    AppStatus status;

    /*
     * `runtime_data` is an opaque pointer depending on the app startup method.
     * It is set in by ProcessManager or SystemdManager.
     */
    gpointer runtime_data;
};

G_DEFINE_TYPE(AppInfo, app_info, G_TYPE_OBJECT);

/*
 * Initialization & cleanup functions
 */

static void app_info_dispose(GObject *object)
{
    AppInfo *self = APPLAUNCHD_APP_INFO(object);

    g_clear_pointer(&self->app_id, g_free);
    g_clear_pointer(&self->name, g_free);
    g_clear_pointer(&self->icon_path, g_free);
    g_clear_pointer(&self->service, g_free);
    g_clear_pointer(&self->runtime_data, g_free);

    G_OBJECT_CLASS(app_info_parent_class)->dispose(object);
}

static void app_info_finalize(GObject *object)
{
    G_OBJECT_CLASS(app_info_parent_class)->finalize(object);
}

static void app_info_class_init(AppInfoClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->dispose = app_info_dispose;
    object_class->finalize = app_info_finalize;
}

static void app_info_init(AppInfo *self)
{
}

/*
 * Public functions
 */

AppInfo *app_info_new(const gchar *app_id, const gchar *name,
                      const gchar *icon_path, const gchar *service)
{
    AppInfo *self = g_object_new(APPLAUNCHD_TYPE_APP_INFO, NULL);

    self->app_id = g_strdup(app_id);
    self->name = g_strdup(name);
    self->icon_path = g_strdup(icon_path);
    self->service = g_strdup(service);

    return self;
}

const gchar *app_info_get_app_id(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), NULL);

    return self->app_id;
}

const gchar *app_info_get_name(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), NULL);

    return self->name;
}

const gchar *app_info_get_icon_path(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), NULL);

    return self->icon_path;
}

const gchar *app_info_get_service(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), NULL);

    return self->service;
}

AppStatus app_info_get_status(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), APP_STATUS_INACTIVE);

    return self->status;
}

gpointer app_info_get_runtime_data(AppInfo *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(self), NULL);

    return self->runtime_data;
}

void app_info_set_runtime_data(AppInfo *self, gpointer runtime_data)
{
    g_return_if_fail(APPLAUNCHD_IS_APP_INFO(self));

    self->runtime_data = runtime_data;
}

void app_info_set_status(AppInfo *self, AppStatus status)
{
    g_return_if_fail(APPLAUNCHD_IS_APP_INFO(self));

    self->status = status;
}
