#include "passwordchecker-ldap.h"
#include <math.h>

static gint64 START_WARNING_TIME; //days
static gint64 WARNING_FREQ;       //mins
static guint TIMER_WARNING_ID = 0;
static guint LDAP_SEARCH_TIME = 24; //hours

static gint TIME_CONV_START = 24;
static gint TIME_CONV_FREQ = 60;

typedef struct _PasswordChecker {
    const gchar *app_id;
    guint owner_id;

    PasswordcheckerLdap *pwc_ldap;
    GSettings *settings;
    gulong handler_id;
} PasswordChecker;

PasswordChecker *pwc = NULL;

static void
cleanup (GSettings *settings,
         gulong    handler_id)
{
    if (settings) {
        g_signal_handler_disconnect (settings, handler_id);
        g_object_unref (settings);
    }
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
        g_object_unref (conn);
        return FALSE;
    }

    return TRUE;
}

gboolean
send_warning (gpointer user_data)
{
    GVariant *parametrs = NULL;
    GDBusConnection *conn = NULL;
    GError *error = NULL;
    gchar *mess = NULL;

    gchar *expiry_time = (gchar *) user_data;
    mess = g_strdup_printf ("Your password expires on %s", expiry_time);

    if (!create_connection (pwc, &conn)) {
        g_free (mess);
        return FALSE;
    }

    parametrs = g_variant_new ("(susssasa{sv}i)",
                               "passwordchecker",
                               0u,
                               "",
                               "Password change required",
                               mess,
                               NULL,
                               NULL,
                               -1,
                               NULL);

    g_dbus_connection_call_sync (conn,
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

    if (error) {
        g_printerr("Error sending notification: %s\n", error->message);
        g_error_free (error);
        g_free (mess);
        g_variant_unref (parametrs);
        return FALSE;
    }

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
    gchar *expiry_time = NULL;
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

    if (TIMER_WARNING_ID != 0) {
        g_source_remove (TIMER_WARNING_ID);
        TIMER_WARNING_ID = 0;

        if (expiry_time != NULL)
            g_free (expiry_time);
    }

    if (START_WARNING_TIME >= diff_hours) {
        expiry_time = g_date_time_format (dt, "%d-%m-%Y %H:%M:%S");

        send_warning (expiry_time);
        TIMER_WARNING_ID = g_timeout_add_seconds (WARNING_FREQ, send_warning, expiry_time);
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
        const gchar* value = NULL;
        value = g_variant_get_string (value_gv, NULL);

        if (g_strcmp0 (key, "url") == 0) {
            passwordchecker_ldap_set_url (g_strdup (value), pwc_ldap);
        }

        if (g_strcmp0 (key, "base-dn") == 0) {
            passwordchecker_ldap_set_base_dn (g_strdup (value), pwc_ldap);
        }
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
                PasswordcheckerLdap *pwc_ldap,
                GSettings           **settings,
                gulong              *handler_id)
{
    gchar *url = NULL;
    gchar *base_dn = NULL;

    *settings = g_settings_new (schema_name);
    if (!*settings)
        return FALSE;

    *handler_id = g_signal_connect (*settings, "changed", G_CALLBACK (settings_changed), pwc_ldap);

    url = g_settings_get_string (*settings, "url");
    base_dn = g_settings_get_string (*settings, "base-dn");
    START_WARNING_TIME = g_settings_get_int64 (*settings, "start-warning-time") * TIME_CONV_START;
    WARNING_FREQ = g_settings_get_int64 (*settings, "warning-frequencies") * TIME_CONV_FREQ;

    passwordchecker_ldap_set_url (url, pwc_ldap);
    passwordchecker_ldap_set_base_dn (base_dn, pwc_ldap);

    g_free (url);
    g_free (base_dn);

    return TRUE;
}

static gint
activate (PasswordChecker *pwc)
{
    pwc->pwc_ldap = passwordchecker_ldap_new (NULL, NULL);

    if (!load_gsettings ("org.altlinux.passwordchecker", pwc->pwc_ldap, &pwc->settings, &pwc->handler_id))
        return EXIT_FAILURE;

    check_password (pwc->pwc_ldap);
    g_timeout_add_seconds (LDAP_SEARCH_TIME * 1440, check_password, pwc->pwc_ldap);
}

int
main ()
{
    // PasswordcheckerLdap *pwc_ldap = passwordchecker_ldap_new ("ldap://srt-dc1.smb.basealt.ru", "dc=smb,dc=basealt,dc=ru");
    gint rc;
    GError *error = NULL;

    pwc = g_new (PasswordChecker, 1);
    pwc->app_id = "org.altlinux.passwordchecker";
    activate (pwc);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);

    g_object_unref (pwc->pwc_ldap);
    cleanup (pwc->settings, pwc->handler_id);
    g_free (pwc);

    return rc;
} 