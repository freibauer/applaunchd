// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Collabora Ltd
 * Copyright (C) 2022 Konsulko Group
 */

#include <glib.h>
#include <glib-unix.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <stdio.h>
#include <unistd.h>
#include "app_launcher.h"
#include "applaunch-dbus.h"

#define APPLAUNCH_DBUS_NAME "org.automotivelinux.AppLaunch"
#define APPLAUNCH_DBUS_PATH "/org/automotivelinux/AppLaunch"

GMainLoop *main_loop = NULL;

static gboolean quit_cb(gpointer user_data)
{
    g_info("Quitting...");

    if (main_loop)
        g_idle_add(G_SOURCE_FUNC(g_main_loop_quit), main_loop);
    else
        exit(0);

    return G_SOURCE_REMOVE;
}

static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data)
{
    AppLauncher *launcher = user_data;

    g_debug("Bus acquired, starting service...");
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(launcher),
                                     connection, APPLAUNCH_DBUS_PATH, NULL);
}

static void name_acquired_cb(GDBusConnection *connection, const gchar *name,
                             gpointer user_data)
{
    g_debug("D-Bus name '%s' was acquired", name);
}

static void name_lost_cb(GDBusConnection *connection, const gchar *name,
                         gpointer user_data)
{
    g_critical("Lost the '%s' service name, quitting...", name);
    g_main_loop_quit(main_loop);
}
    FILE *filePointer;

int main(int argc, char *argv[])
{
    g_unix_signal_add(SIGTERM, quit_cb, NULL);
    g_unix_signal_add(SIGINT, quit_cb, NULL);
    main_loop = g_main_loop_new(NULL, FALSE);

    char str[] = "applaunchd";

    // opening the file in write mode
    filePointer = fopen("/tmp/applaunchd.txt.txt", "+w");
    fwrite(str, 1, sizeof(str) - 1, filePointer);
    fsync(fileno(filePointer));


    AppLauncher *launcher = app_launcher_get_default();

    gint owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, APPLAUNCH_DBUS_NAME,
                                   G_BUS_NAME_OWNER_FLAGS_NONE, bus_acquired_cb,
                                   name_acquired_cb, name_lost_cb,
                                   launcher, NULL);

    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    g_object_unref(launcher);

    g_bus_unown_name (owner_id);

    return 0;
}
