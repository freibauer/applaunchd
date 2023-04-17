// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Konsulko Group
 */

#include <stdbool.h>
#include "systemd_manager.h"
#include "utils.h"

// Pull in for sd_bus_path_encode, as there's no obvious alternative
// other than coding up a duplicate.  No other sd_bus functions should
// be used!
#include <systemd/sd-bus.h>

// gdbus generated headers
#include "systemd1_manager_interface.h"
#include "systemd1_unit_interface.h"

extern GMainLoop *main_loop;

// Object data
struct _SystemdManager {
    GObject parent_instance;

    GDBusConnection *conn;
    Systemd1Manager *proxy;

    GList *apps_list;
};

G_DEFINE_TYPE(SystemdManager, systemd_manager, G_TYPE_OBJECT);

enum {
  STARTED,
  TERMINATED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];

/*
 * Application info structure, used for storing relevant data
 * in the `running_apps` list
 */
struct systemd_runtime_data {
    gchar *esc_service;
    SystemdManager *mgr;
    Systemd1Unit *proxy;
};

/*
 * Internal functions
 */

/*
 * Get app unit list
 */
static gboolean systemd_manager_enumerate_app_units(SystemdManager *self,
						    GList **units)
{
    g_return_val_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self), FALSE);
    g_return_val_if_fail(units != NULL, FALSE);

    GVariant *matched_units = NULL;
    GError *error = NULL;
    const gchar *const states[1] = { NULL }; 
    const gchar *const patterns[2] = { "agl-app*@*.service", NULL }; 
    if (!systemd1_manager_call_list_unit_files_by_patterns_sync(self->proxy,
								states,
								patterns,
								&matched_units,
								NULL,
								&error)) {
        g_critical("Failed to issue method call: %s", error ? error->message : "unspecified");
	g_error_free(error);
        goto finish;
    }

    // We expect the response GVariant to be format "a(ss)"
    GVariantIter *array;
    g_variant_get(matched_units, "a(ss)", &array);
    const char *unit;
    const char *status;
    while (g_variant_iter_loop(array, "(ss)", &unit, &status)) {
        if (!g_str_has_suffix(unit, "@.service"))
            *units = g_list_prepend(*units, g_strdup(unit));
    }
    g_variant_iter_free(array);
    g_variant_unref(matched_units);

    return TRUE;

finish:
    g_list_free_full(*units, g_free);
    *units = NULL;
    return FALSE;
}

/*
 * Get app unit description property
 */
static gboolean systemd_manager_get_app_description(SystemdManager *self,
                                                    gchar *service,
                                                    gchar **description)
{
    g_return_val_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self), FALSE);
    g_return_val_if_fail(service != NULL, FALSE);
    g_return_val_if_fail(description != NULL, FALSE);

    // Get the escaped unit name in the systemd hierarchy
    gchar *esc_service = NULL;
    sd_bus_path_encode("/org/freedesktop/systemd1/unit", service, &esc_service);

    GError *error = NULL;
    Systemd1Unit *proxy = systemd1_unit_proxy_new_sync(self->conn,
						       G_DBUS_PROXY_FLAGS_NONE,
						       "org.freedesktop.systemd1",
						       esc_service,
						       NULL,
						       &error);
    if (!proxy) {
        g_critical("Failed to create org.freedesktop.systemd1.Unit proxy: %s",
		   error ? error->message : "unspecified");
	g_error_free(error);
	goto finish;
    }

    gchar *value = systemd1_unit_dup_description(proxy);
    if (value) {
	*description = value;
    }

    g_object_unref(proxy);
    return TRUE;

finish:
    g_free(*description);
    *description = NULL;
    return FALSE;
}

/*
 * This function is executed during the object initialization. It goes through
 * all available applications on the system and creates a static list
 * containing all the relevant info (ID, name, unit, icon...) for further
 * processing.
 */
static void systemd_manager_update_applications_list(SystemdManager *self)
{
    g_auto(GStrv) dirlist = NULL;

    char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs)
        dirlist = g_strsplit(getenv("XDG_DATA_DIRS"), ":", -1);

    GList *units = NULL;
    if (!systemd_manager_enumerate_app_units(self, &units)) {
        return;
    }

    GList *iterator;
    for (iterator = units; iterator != NULL; iterator = iterator->next) {
        g_autofree const gchar *app_id = NULL;
        g_autofree const gchar *icon_path = NULL;
        gchar *service = NULL;
        AppInfo *app_info = NULL;

        if (!iterator->data)
            continue;

        // Parse service and app id out of unit filename
        gchar *p = g_strrstr(iterator->data, "/");
        if (!p)
            service = iterator->data;
        else
            service = p + 1;

        g_autofree char *tmp = g_strdup(service);
        char *end = tmp + strlen(tmp);
        while (end > tmp && *end != '.') {
            --end;
        }
        if (end > tmp) {
            *end = '\0';
        } else {
            g_free(tmp);
            continue;
        }
        while (end > tmp && *end != '@') {
            --end;
        }
        if (end > tmp) {
            app_id = g_strdup(end + 1);
        }
        // Potentially handle non-template agl-app-foo.service units here

        // Try getting display name from unit Description property
        g_autofree gchar *name = NULL;
        if (!systemd_manager_get_app_description(self,
                                                 service,
                                                 &name) ||
            name == NULL) {

            // Fall back to the application ID
            g_warning("Could not retrieve Description of '%s'", service);
            name = g_strdup(app_id);
        }

        /*
         * GAppInfo retrieves the icon data but doesn't provide a way to retrieve
         * the corresponding file name, so we have to look it up by ourselves.
         */
        if (app_id && dirlist)
            icon_path = applaunchd_utils_get_icon(dirlist, app_id);

        app_info = app_info_new(app_id,
				name,
				icon_path ? icon_path : "",
				service);

        g_debug("Adding application '%s' with display name '%s'", app_id, name);
        self->apps_list = g_list_append(self->apps_list, app_info);
    }
    g_list_free_full(units, g_free);
}


/*
 * Initialization & cleanup functions
 */

static void systemd_manager_dispose(GObject *object)
{
    SystemdManager *self = APPLAUNCHD_SYSTEMD_MANAGER(object);

    g_return_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self));

    if (self->apps_list)
        g_list_free_full(g_steal_pointer(&self->apps_list), g_object_unref);

    g_object_unref(self->proxy);
    g_object_unref(self->conn);

    G_OBJECT_CLASS(systemd_manager_parent_class)->dispose(object);
}

static void systemd_manager_finalize(GObject *object)
{
    G_OBJECT_CLASS(systemd_manager_parent_class)->finalize(object);
}

static void systemd_manager_class_init(SystemdManagerClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;

    object_class->dispose = systemd_manager_dispose;
    object_class->finalize = systemd_manager_finalize;

    signals[STARTED] = g_signal_new("started", G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST, 0 ,
                                    NULL, NULL, NULL, G_TYPE_NONE,
                                    1, G_TYPE_STRING);

    signals[TERMINATED] = g_signal_new("terminated", G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_LAST, 0 ,
                                       NULL, NULL, NULL, G_TYPE_NONE,
                                       1, G_TYPE_STRING);
}

static void systemd_manager_init(SystemdManager *self)
{
    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!conn) {
        g_critical("Failed to connect to D-Bus: %s", error ? error->message : "unspecified");
	g_error_free(error);
	return;
    }
    self->conn = conn;

    Systemd1Manager *proxy = systemd1_manager_proxy_new_sync(conn,
							     G_DBUS_PROXY_FLAGS_NONE,
							     "org.freedesktop.systemd1",
							     "/org/freedesktop/systemd1",
							     NULL,
							     &error);
    if (!proxy) {
        g_critical("Failed to create org.freedesktop.systemd1.Manager proxy: %s",
		   error ? error->message : "unspecified");
	g_error_free(error);
	return;
    }
    self->proxy = proxy;

    systemd_manager_update_applications_list(self);
}


/*
 * Internal callbacks
 */

/*
 * This function is called when "PropertiesChanged" signal happens for
 * the matched Unit - check its "ActiveState" to update the app status
 */
static void unit_properties_changed_cb(GDBusProxy *proxy,
				       GVariant *changed_properties,
				       const gchar* const *invalidated_properties,
				       gpointer user_data)
{
    AppInfo *app_info = user_data;
    struct systemd_runtime_data *data;

    data = app_info_get_runtime_data(app_info);
    if(!data) {
        g_critical("Couldn't find runtime data for %s!", app_info_get_app_id(app_info));
        return;
    }

    // NOTE: changed_properties and invalidated_properties are guaranteed to never be NULL
    gchar *new_state = NULL;
    bool found = false;
    if (g_variant_n_children(changed_properties) > 0) {
	GVariantIter *iter;
	const gchar *key;
	GVariant *value;

	g_variant_get(changed_properties, "a{sv}", &iter);
	while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
	    if (!g_strcmp0(key, "ActiveState")) {
		    new_state = g_variant_dup_string(value, NULL);
		    found = true;
	    }
	}
	g_variant_iter_free(iter);
    }

    // Ignore invalidated_properties for now

    // Return if the changed property isn't "ActiveState"
    if (!found)
	return;

    if(!g_strcmp0(new_state, "inactive"))
    {
        if(app_info_get_status(app_info) != APP_STATUS_STARTING)
        {
	    g_debug("Application %s has terminated", app_info_get_app_id(app_info));
            app_info_set_status(app_info, APP_STATUS_INACTIVE);
            app_info_set_runtime_data(app_info, NULL);

            g_signal_emit(data->mgr, signals[TERMINATED], 0, app_info_get_app_id(app_info));
            systemd_manager_free_runtime_data(data);
	}
    }
    else if(!g_strcmp0(new_state, "active"))
    {
        // PropertiesChanged signal gets triggered multiple times, only handle it once
        if(app_info_get_status(app_info) != APP_STATUS_RUNNING)
        {
            g_debug("Application %s has started", app_info_get_app_id(app_info));
            app_info_set_status(app_info, APP_STATUS_RUNNING);
            g_signal_emit(data->mgr, signals[STARTED], 0, app_info_get_app_id(app_info));
        }
    }
    g_free(new_state);
}


/*
 * Public functions
 */

SystemdManager *systemd_manager_get_default(void)
{
    static SystemdManager *manager;

    /*
    * SystemdManager is a singleton, only create the object if it doesn't
    * exist already.
    */
    if (manager == NULL) {
        g_debug("Initializing app launcher service...");
        manager = g_object_new(APPLAUNCHD_TYPE_SYSTEMD_MANAGER, NULL);
        g_object_add_weak_pointer(G_OBJECT(manager), (gpointer*) &manager);
    }

    return manager;
}

void systemd_manager_connect_callbacks(SystemdManager *self,
                                       GCallback started_cb,
                                       GCallback terminated_cb,
                                       void *data)
{
    if (started_cb)
        g_signal_connect_swapped(self, "started", started_cb, data);

    if (terminated_cb)
        g_signal_connect_swapped(self, "terminated", terminated_cb, data);
}

/*
 * Search the applications list for an app which matches the provided app-id
 * and return the corresponding AppInfo object.
 */
AppInfo *systemd_manager_get_app_info(SystemdManager *self, const gchar *app_id)
{
    g_return_val_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self), NULL);

    guint len = g_list_length(self->apps_list);

    for (guint i = 0; i < len; i++) {
        AppInfo *app_info = g_list_nth_data(self->apps_list, i);

        if (g_strcmp0(app_info_get_app_id(app_info), app_id) == 0)
            return app_info;
    }

    g_warning("Unable to find application with ID '%s'", app_id);

    return NULL;
}

GList *systemd_manager_get_app_list(SystemdManager *self)
{
    g_return_val_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self), NULL);

    return self->apps_list;
}

/*
 * Start an application by executing its service.
 */
gboolean systemd_manager_start_app(SystemdManager *self,
                                   AppInfo *app_info)
{
    g_return_val_if_fail(APPLAUNCHD_IS_SYSTEMD_MANAGER(self), FALSE);
    g_return_val_if_fail(APPLAUNCHD_IS_APP_INFO(app_info), FALSE);

    AppStatus app_status = app_info_get_status(app_info);
    const gchar *app_id = app_info_get_app_id(app_info);

    switch (app_status) {
    case APP_STATUS_STARTING:
        g_debug("Application '%s' is already starting", app_id);
        return TRUE;
    case APP_STATUS_RUNNING:
        g_debug("Application '%s' is already running", app_id);

        /*
        * The application may be running in the background, notify
        * subscribers it should be activated/brought to the foreground.
        */
        g_signal_emit(self, signals[STARTED], 0, app_id);
        return TRUE;
    case APP_STATUS_INACTIVE:
        // Fall through and start the application
        break;
    default:
        g_critical("Unknown status %d for application '%s'", app_status, app_id);
        return FALSE;
    }

    gchar *esc_service = NULL;
    const gchar *service = app_info_get_service(app_info);
    struct systemd_runtime_data *runtime_data;

    runtime_data = g_new0(struct systemd_runtime_data, 1);
    if (!runtime_data) {
        g_critical("Unable to allocate runtime data structure for '%s'", app_id);
        return FALSE;
    }

    // Get the escaped unit name in the systemd hierarchy
    sd_bus_path_encode("/org/freedesktop/systemd1/unit", service, &esc_service);
    g_debug("Trying to start service '%s', unit path '%s'", service, esc_service);

    runtime_data->mgr = self;
    runtime_data->esc_service = esc_service;

    GError *error = NULL;
    Systemd1Unit *proxy = systemd1_unit_proxy_new_sync(self->conn,
						       G_DBUS_PROXY_FLAGS_NONE,
						       "org.freedesktop.systemd1",
						       esc_service,
						       NULL,
						       &error);
    if (!proxy) {
        g_critical("Failed to create org.freedesktop.systemd1.Unit proxy: %s",
		   error ? error->message : "unspecified");
	g_error_free(error);
	goto finish;
    }
    runtime_data->proxy = proxy;

    app_info_set_runtime_data(app_info, runtime_data);
    g_signal_connect(proxy,
		     "g-properties-changed",
		     G_CALLBACK(unit_properties_changed_cb),
		     app_info);

    // The application is now starting, wait for notification to mark it running
    g_debug("Application %s is now being started", app_info_get_app_id(app_info));
    app_info_set_status(app_info, APP_STATUS_STARTING);

    if (!systemd1_manager_call_start_unit_sync(self->proxy,
					       service,
					       "replace",
					       NULL,
					       NULL,
					       &error)) {

        g_critical("Failed to issue method call: %s", error ? error->message : "unspecified");
	g_error_free(error);
        goto finish;
    }

    return TRUE;

finish:
    if(runtime_data->esc_service)
        g_free(runtime_data->esc_service);
    g_free(runtime_data);
    return FALSE;
}

void systemd_manager_free_runtime_data(gpointer data)
{
    struct systemd_runtime_data *runtime_data = data;

    g_return_if_fail(runtime_data != NULL);

    g_free(runtime_data->esc_service);
    g_free(runtime_data);
}
