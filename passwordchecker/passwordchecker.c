#include "passwordchecker-ldap.h"
#include "winbind-helper.h"
#include <math.h>
#include <locale.h>
#include <libintl.h>

#define _(STRING) gettext(STRING)

static gint64 START_WARNING_TIME; //days
static gint64 WARNING_FREQ;       //mins
static guint TIMER_WARNING_ID = 0;
static guint LDAP_SEARCH_TIME = 24; //hours

static gint TIME_CONV_START = 24;
static gint TIME_CONV_FREQ = 60;

typedef struct _PasswordChecker {
    GMainLoop *loop;
    const gchar *app_id;
    guint owner_id;

    PasswordcheckerLdap *pwc_ldap;
    GSettings *settings;
    gulong handler_id;

    gchar *expiry_time;
} PasswordChecker;

typedef struct _Notification {
    guint notification_id;
    guint signal_invoked_action_id;
    guint signal_notification_closed_id;
} Notification;

PasswordChecker *pwc = NULL;
guint CURRENT_NOTIFICATION_ID = 0;


static void
cleanup (PasswordChecker *pwc)
{
    if (pwc->settings) {
        g_signal_handler_disconnect (pwc->settings, pwc->handler_id);
        g_object_unref (pwc->settings);
    }

    if (pwc->expiry_time != NULL)
        g_free (pwc->expiry_time);
}

static void
on_subprocess_finished (GObject *source_object,
                        GAsyncResult *result,
                        gpointer user_data)
{
    GError *error = NULL;
    GSubprocess *subprocess = G_SUBPROCESS (source_object);

    gint exit_code = g_subprocess_wait_finish (subprocess, result, &error);

    if (error) {
        g_printerr("Error waiting for subprocess: %s\n", error->message);
        g_error_free(error);
    } else {
        g_print("Subprocess exited with code: %d\n", exit_code);
    }

    g_object_unref (subprocess);
}

static void
on_run_subprocess (const gchar *command)
{
    // g_print ("<Run change password>\n");
    
    GSubprocess *subprocess = NULL;
    GError *error = NULL;

    subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                                   &error,
                                   command, NULL);

    if (subprocess == NULL) {
        g_printerr ("Error creating subprocess: %s\n", error->message);
        g_error_free (error);
    } else {
        g_subprocess_wait_async (subprocess, NULL, on_subprocess_finished, NULL);
    }
}

static void
on_action_invoked (GDBusConnection *conn,
                   const gchar     *sender_name,
                   const gchar     *object_path,
                   const gchar     *interface_name,
                   const gchar     *signal_name,
                   GVariant        *parameters,
                   gpointer         user_data)
{
    gchar *action_key = NULL;
    guint id;

    g_variant_get (parameters, "(us)", &id, &action_key);

    if (g_strcmp0 (action_key, "change-password") == 0) {
        on_run_subprocess ("userpasswd");
    }
    else if (g_strcmp0 (action_key, "change-settings") == 0) {
        on_run_subprocess ("PasswordCheckerSettings");
    }

    if (action_key != NULL) {
        g_free (action_key);
    }
}

static void
on_notification_close (GDBusConnection *conn,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
    Notification *notification = (Notification *) user_data;
    if (conn != NULL) {
        if (notification->signal_invoked_action_id != 0) {
            g_dbus_connection_signal_unsubscribe (conn, notification->signal_invoked_action_id );
        }
        if (notification->signal_notification_closed_id != 0) {
            g_dbus_connection_signal_unsubscribe (conn, notification->signal_notification_closed_id );
        }
    }

    g_free (notification);
}

static gboolean
create_connection (PasswordChecker  *pwc,
                   GDBusConnection **conn)
{
    GError *error = NULL;

    *conn = g_bus_get_sync (G_BUS_TYPE_SESSION,
                            NULL,
                            &error);
    if (error) {
        g_printerr ("Error connecting to D-Bus: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    pwc->owner_id = g_bus_own_name_on_connection (*conn,
                                                   pwc->app_id,
                                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   NULL);

    if (pwc->owner_id == 0) {
        g_printerr ("Failed to register a name on DBus: %s\n", pwc->app_id);
        g_object_unref (*conn);
        *conn = FALSE;
        *conn = NULL;
        return FALSE;
    }

    return TRUE;
}

gboolean
send_warning (gpointer user_data)
{
    GVariant *parametrs = NULL;
    GVariant *reply = NULL;
    GDBusConnection *conn = NULL;
    GError *error = NULL;
    gchar *mess = NULL;
    Notification *notification = NULL;
    GVariantBuilder actions_builder;

    gchar *expiry_time = (gchar *) user_data;
    mess = g_strdup_printf (_("Your password expires on %s"), expiry_time);

    if (!create_connection (pwc, &conn)) {
        g_free (mess);
        return FALSE;
    }

    notification = g_new (Notification, 1);

    if (CURRENT_NOTIFICATION_ID != 0) {
        GVariant *params = NULL;
        GVariant *reply = NULL;

        params = g_variant_new ("(u)", CURRENT_NOTIFICATION_ID);

        reply = g_dbus_connection_call_sync (conn,
                                             "org.freedesktop.Notifications",
                                             "/org/freedesktop/Notifications",
                                             "org.freedesktop.Notifications",
                                             "CloseNotification",
                                             params,
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);

        CURRENT_NOTIFICATION_ID = 0;

        if (error) {
            g_printerr ("Error closing notification: %s\n", error->message);
            g_error_free (error);
            error = NULL;
        }
        if (reply != NULL) {
            g_variant_unref (reply);
        }
    }

    g_variant_builder_init (&actions_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&actions_builder, "s", "change-password");
    g_variant_builder_add (&actions_builder, "s", _("Change password"));
    g_variant_builder_add (&actions_builder, "s", "change-settings");
    g_variant_builder_add (&actions_builder, "s", _("Change the notification settings"));

    notification->signal_invoked_action_id = g_dbus_connection_signal_subscribe (conn,
                                                                                 "org.freedesktop.Notifications",
                                                                                 "org.freedesktop.Notifications",
                                                                                 "ActionInvoked",
                                                                                 "/org/freedesktop/Notifications",
                                                                                 NULL,
                                                                                 G_DBUS_SIGNAL_FLAGS_NONE,
                                                                                 on_action_invoked,
                                                                                 NULL,
                                                                                 NULL);

    notification->signal_notification_closed_id = g_dbus_connection_signal_subscribe (conn,
                                                                                      "org.freedesktop.Notifications",
                                                                                      "org.freedesktop.Notifications",
                                                                                      "NotificationClosed",
                                                                                      "/org/freedesktop/Notifications",
                                                                                      NULL,
                                                                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                                                                      on_notification_close,
                                                                                      notification,
                                                                                      NULL);


    parametrs = g_variant_new ("(susssasa{sv}i)",
                               "passwordchecker",
                               0u,
                               "",
                               _("Password change required"),
                               mess,
                               &actions_builder,
                               NULL,
                               -1,
                               NULL);

    reply = g_dbus_connection_call_sync (conn,
                                         "org.freedesktop.Notifications",
                                         "/org/freedesktop/Notifications",
                                         "org.freedesktop.Notifications",
                                         "Notify",
                                         parametrs,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (reply == NULL) {
        g_printerr("Error sending notification: %s\n", error->message);
        g_error_free (error);
        g_free (mess);
        return FALSE;
    }

    g_variant_get (reply, "(u)", &(notification->notification_id));
    CURRENT_NOTIFICATION_ID = notification->notification_id;

    g_variant_unref (reply);
    g_free (mess);
    g_bus_unown_name (pwc->owner_id);
    g_object_unref (conn);

    return TRUE;
}

gboolean
check_password (void *data)
{
    GDateTime *dt = NULL;
    GDateTime *current_time = NULL;
    gint64 diff_sec;
    gint64 diff_hours;
    int rc;

    PasswordcheckerLdap *pwc_ldap = (PasswordcheckerLdap *) data;

    rc = passwordchecker_ldap_get_date_time (pwc_ldap, &dt);
    if (!dt || !rc) {
        /*
        TODO: add notification
        */
        g_printerr ("Failed to get date from LDAP server\n");
        return TRUE;
    }

    current_time = g_date_time_new_now_local();
    diff_sec = ABS (g_date_time_difference(dt, current_time)) / 1000000;
    diff_hours = diff_sec / 3600;
    diff_hours = floor (diff_hours);

    // g_print ("current_time: %s\n", g_date_time_format(current_time, "%d-%m-%Y %H:%M:%S"));
    // g_print ("password_time: %s\n\n", g_date_time_format(dt, "%d-%m-%Y %H:%M:%S"));
    // g_print ("diff_hours = %ld\n", diff_hours);
    // g_print ("start_warning = %ld\n", START_WARNING_TIME);

    if (pwc->expiry_time != NULL){
        g_free (pwc->expiry_time);
        pwc->expiry_time = NULL;
    }

    if (TIMER_WARNING_ID != 0) {
        g_source_remove (TIMER_WARNING_ID);
        TIMER_WARNING_ID = 0;
    }

    if (START_WARNING_TIME >= diff_hours) {
        pwc->expiry_time = g_date_time_format (dt, "%d-%m-%Y %H:%M:%S");

        send_warning (pwc->expiry_time);
        TIMER_WARNING_ID = g_timeout_add_seconds (WARNING_FREQ, send_warning, pwc->expiry_time);
    }

    g_date_time_unref (dt);
    g_date_time_unref (current_time);

    return TRUE;
}

static void
settings_changed (GSettings *settings,
                  gchar     *key,
                  gpointer  *userdata)
{
    /*
    TODO: Notification that settings have been changed
    */
    PasswordcheckerLdap *pwc_ldap = PASSWORDCHECKER_LDAP (userdata);

    const GVariantType *type = NULL;
    GVariant *value_gv = NULL;

    value_gv = g_settings_get_value (settings, key);
    type = g_variant_get_type (value_gv);

    if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING)) {
        gchar* value = NULL;
        g_variant_get (value_gv, "s", &value);

        if (g_strcmp0 (key, "url") == 0) {
            passwordchecker_ldap_set_url (value, pwc_ldap);
        }

        if (g_strcmp0 (key, "base-dn") == 0) {
            passwordchecker_ldap_set_base_dn (value, pwc_ldap);
        }

        if (value)
            g_free (value);
    }

    if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64)) {
        const gint64 value = g_variant_get_int64 (value_gv);
        if (g_strcmp0 (key, "start-warning-time") == 0) {
            START_WARNING_TIME = value * TIME_CONV_START;
        }
        if (g_strcmp0 (key, "warning-frequencies") == 0) {
            WARNING_FREQ = value * TIME_CONV_FREQ;
        }
    }

    check_password (pwc_ldap);

    g_variant_unref (value_gv);
    return;
}

static gboolean
load_gsettings (gchar               *schema_name,
                GSettings           **settings,
                gulong              *handler_id)
{
    gchar *url = NULL;
    gchar *base_dn = NULL;

    *settings = g_settings_new (schema_name);
    if (!*settings)
        return FALSE;

    url = g_settings_get_string (*settings, "url");
    base_dn = g_settings_get_string (*settings, "base-dn");
    START_WARNING_TIME = g_settings_get_int64 (*settings, "start-warning-time") * TIME_CONV_START;
    WARNING_FREQ = g_settings_get_int64 (*settings, "warning-frequencies") * TIME_CONV_FREQ;

    if (g_strcmp0 (url, "") == 0) {
        g_free (url);
        url = NULL;

        gchar **dc_names = NULL;
        size_t num_dcs;
        /*
          TODO: in loop check connections for returned dc names
        */
        if (get_dc_names (&dc_names, &num_dcs) && num_dcs > 0) {
            url = g_strconcat ("ldap://", dc_names[0], NULL);
            g_strfreev (dc_names);
        };

        if (url) {
            if (! g_settings_set_string (*settings, "url", url)) {
                g_warning ("Failed to write the url retrieved from webinfo to the GSettings\n");
            }
        }
    }

    pwc->pwc_ldap = passwordchecker_ldap_new (url, base_dn);

    if (g_strcmp0 (base_dn, "") == 0) {
        if (! g_settings_set_string (*settings, "base-dn", pwc->pwc_ldap->base_dn)) {
            g_warning ("Failed to write the base-dn retrieved from webinfo to the GSettings\n");
        }
    }

    *handler_id = g_signal_connect (*settings, "changed", G_CALLBACK (settings_changed), pwc->pwc_ldap);

    g_free (url);
    g_free (base_dn);

    return TRUE;
}

static gint
activate (PasswordChecker *pwc)
{
    if (!load_gsettings ("org.altlinux.passwordchecker", &pwc->settings, &pwc->handler_id))
        return EXIT_FAILURE;

    check_password (pwc->pwc_ldap);
    g_timeout_add_seconds (LDAP_SEARCH_TIME * 1440, check_password, pwc->pwc_ldap);
}

int
main ()
{
    // PasswordcheckerLdap *pwc_ldap = passwordchecker_ldap_new ("ldap://srt-dc1.smb.basealt.ru", "dc=smb,dc=basealt,dc=ru");
    setlocale (LC_ALL, "");
    bindtextdomain ("passwordchecker", "/usr/share/locale/");
    textdomain ("passwordchecker");

    gint rc;
    GError *error = NULL;

    pwc = g_new (PasswordChecker, 1);
    pwc->app_id = "org.altlinux.passwordchecker";
    activate (pwc);

    pwc->loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (pwc->loop);
    g_main_loop_unref (pwc->loop);

    g_object_unref (pwc->pwc_ldap);
    cleanup (pwc);
    cleanup (pwc);
    g_free (pwc);

    return rc;
} 