// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Collabora Ltd
 * Copyright (C) 2022 Konsulko Group
 */

#ifndef APPLAUNCHER_H
#define APPLAUNCHER_H

#include <glib-object.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include "applaunch-dbus.h"
#include "app_info.h"

G_BEGIN_DECLS

#define APPLAUNCHD_TYPE_APP_LAUNCHER app_launcher_get_type()

G_DECLARE_FINAL_TYPE(AppLauncher, app_launcher, APPLAUNCHD, APP_LAUNCHER,
                     applaunchdAppLaunchSkeleton);

AppLauncher *app_launcher_get_default(void);

gboolean app_launcher_start_app(AppLauncher *self, AppInfo *app_info);

G_END_DECLS

#endif
