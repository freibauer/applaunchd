// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Collabora Ltd
 * Copyright (C) 2022 Konsulko Group
 */

#include "app_info.h"
#include "app_launcher.h"
#include "systemd_manager.h"

#include <syslog.h>


static void logme(char *x)
{

    openlog ("applaunchd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog (LOG_INFO,"%s",x);
    closelog ();
}


typedef struct _AppLauncher {
    applaunchdAppLaunchSkeleton parent;

    SystemdManager *systemd_manager;

} AppLauncher;




static void app_launcher_iface_init(applaunchdAppLaunchIface *iface);

G_DEFINE_TYPE_WITH_CODE(AppLauncher, app_launcher,
                        APPLAUNCHD_TYPE_APP_LAUNCH_SKELETON,
                        G_IMPLEMENT_INTERFACE(APPLAUNCHD_TYPE_APP_LAUNCH,
                                              app_launcher_iface_init));

static void app_launcher_started_cb(AppLauncher *self,
                                    const gchar *app_id,
                                    gpointer caller);

/*
 * Internal functions
 */

/*
 * Construct the application list to be sent over D-Bus. It has format "av", meaning
 * the list itself is an array, each item being a variant consisting of 3 strings:
 *   - app-id
 *   - app name
 *   - icon path
 */
static GVariant *app_launcher_get_list_variant(AppLauncher *self)
{
    GVariantBuilder builder;
    GList *apps_list = systemd_manager_get_app_list(self->systemd_manager);
    if (!apps_list)
        return NULL;
    guint len = g_list_length(apps_list);

    /* Init array variant for storing the applications list */
    g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

    for (guint i = 0; i < len; i++) {
        GVariantBuilder app_builder;
        AppInfo *app_info = g_list_nth_data(apps_list, i);

        g_variant_builder_init (&app_builder, G_VARIANT_TYPE("(sss)"));

        /* Create application entry */
        g_variant_builder_add(&app_builder, "s", app_info_get_app_id(app_info));
        g_variant_builder_add(&app_builder, "s", app_info_get_name(app_info));
        g_variant_builder_add(&app_builder, "s", app_info_get_icon_path(app_info));

        /* Add entry to apps list */
        g_variant_builder_add(&builder, "v", g_variant_builder_end(&app_builder));
    }

    return g_variant_builder_end(&builder);
}

/*
 * Starts the requested application using either the D-Bus activation manager
 * or the process manager.
 */
gboolean app_launcher_start_app(AppLauncher *self, AppInfo *app_info)
{
    g_return_val_if_fail(APPLAUNCHD_IS_APP_LAUNCHER(self), FALSE);
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(app_info), FALSE);

    return systemd_manager_start_app(self->systemd_manager, app_info);
}

/*
 * Internal callbacks
 */

/*
 * Handler for the "start" D-Bus method.
 */
static gboolean app_launcher_handle_start(applaunchdAppLaunch *object,
                                          GDBusMethodInvocation *invocation,
                                          const gchar *app_id)
{
    AppInfo *app;
    AppLauncher *self = APPLAUNCHD_APP_LAUNCHER(object);
    g_return_val_if_fail(APPLAUNCHD_IS_APP_LAUNCHER(self), FALSE);

    /* Search the apps list for the given app-id */
    app = systemd_manager_get_app_info(self->systemd_manager, app_id);
    if (!app) {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Unknown application '%s'",
                                              app_id);
        return FALSE;
    }

    char buf[128];
    sprintf(buf,"start app=%s",app_id);
    logme(buf);
    

    app_launcher_start_app(self, app);
    applaunchd_app_launch_complete_start(object, invocation);

    return TRUE;
}

/*
 * Handler for the "listApplications" D-Bus method.
 */
static gboolean app_launcher_handle_list_applications(applaunchdAppLaunch *object,
                                                      GDBusMethodInvocation *invocation,
                                                      gboolean graphical)
{
    GVariant *result = NULL;
    AppLauncher *self = APPLAUNCHD_APP_LAUNCHER(object);
    g_return_val_if_fail(APPLAUNCHD_IS_APP_LAUNCHER(self), FALSE);

    /* Retrieve the applications list in the right format for sending over D-Bus */
    result = app_launcher_get_list_variant(self);
    if (result)
        applaunchd_app_launch_complete_list_applications(object, invocation, result);

    return TRUE;
}

/*
 * Callback for the "started" signal emitted by both the
 * process manager and D-Bus activation manager. Forwards
 * the signal to other applications through D-Bus.
 */
static void app_launcher_started_cb(AppLauncher *self,
                                    const gchar *app_id,
                                    gpointer caller)
{
    applaunchdAppLaunch *iface = APPLAUNCHD_APP_LAUNCH(self);
    g_return_if_fail(APPLAUNCHD_IS_APP_LAUNCH(iface));

    g_debug("Application '%s' started", app_id);
    /*
     * Emit the "started" D-Bus signal so subscribers get notified
     * the application with ID "app_id" started and should be
     * activated
     */
    applaunchd_app_launch_emit_started(iface, app_id);
}

/*
 * Callback for the "terminated" signal emitted by both the
 * process manager and D-Bus activation manager. Forwards
 * the signal to other applications through D-Bus.
 */
static void app_launcher_terminated_cb(AppLauncher *self,
                                       const gchar *app_id,
                                       gpointer caller)
{
    applaunchdAppLaunch *iface = APPLAUNCHD_APP_LAUNCH(self);
    g_return_if_fail(APPLAUNCHD_IS_APP_LAUNCH(iface));


    char buf[128];
    sprintf(buf,"terminated app=%s",app_id);
    logme(buf);

    g_debug("Application '%s' terminated", app_id);
    /*
     * Emit the "terminated" D-Bus signal so subscribers get
     * notified the application with ID "app_id" terminated
     */
    applaunchd_app_launch_emit_terminated(iface, app_id);
}

/*
 * Initialization & cleanup functions
 */

static void app_launcher_dispose(GObject *object)
{
    AppLauncher *self = APPLAUNCHD_APP_LAUNCHER(object);

    g_clear_object(&self->systemd_manager);

    G_OBJECT_CLASS(app_launcher_parent_class)->dispose(object);
}

static void app_launcher_finalize(GObject *object)
{
    G_OBJECT_CLASS(app_launcher_parent_class)->finalize(object);
}

static void app_launcher_class_init(AppLauncherClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->dispose = app_launcher_dispose;
}

static void app_launcher_iface_init(applaunchdAppLaunchIface *iface)
{
    iface->handle_start = app_launcher_handle_start;
    iface->handle_list_applications = app_launcher_handle_list_applications;
}

static void app_launcher_init (AppLauncher *self)
{
    /*
     * Create the systemd manager and connect to its signals
     * so we get notified on app startup/termination
     */
    self->systemd_manager = systemd_manager_get_default();
    systemd_manager_connect_callbacks(self->systemd_manager,
				      G_CALLBACK(app_launcher_started_cb),
                                      G_CALLBACK(app_launcher_terminated_cb),
				      self);
}

/*
 * Public functions
 */

AppLauncher *app_launcher_get_default(void)
{
    static AppLauncher *launcher;

    /*
    * AppLauncher is a singleton, only create the object if it doesn't
    * exist already.
    */
    if (launcher == NULL) {
        g_debug("Initializing app launcher service...");
        launcher = g_object_new(APPLAUNCHD_TYPE_APP_LAUNCHER, NULL);
        g_object_add_weak_pointer(G_OBJECT(launcher), (gpointer *)&launcher);
    }

    return launcher;
}
