// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Konsulko Group
 */

#ifndef SYSTEMDMANAGER_H
#define SYSTEMDMANAGER_H

#include <glib-object.h>

#include "app_info.h"

G_BEGIN_DECLS

#define APPLAUNCHD_TYPE_SYSTEMD_MANAGER systemd_manager_get_type()

G_DECLARE_FINAL_TYPE(SystemdManager, systemd_manager,
                     APPLAUNCHD, SYSTEMD_MANAGER, GObject);

SystemdManager *systemd_manager_get_default(void);

void systemd_manager_connect_callbacks(SystemdManager *self,
                                       GCallback started_cb,
                                       GCallback terminated_cb,
                                       void *data);

AppInfo *systemd_manager_get_app_info(SystemdManager *self,
                                      const gchar *app_id);

GList *systemd_manager_get_app_list(SystemdManager *self);

gboolean systemd_manager_start_app(SystemdManager *self,
                                   AppInfo *app_info);

G_END_DECLS

void systemd_manager_free_runtime_data(gpointer data);

#endif
