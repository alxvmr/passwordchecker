#include "passwordchecker-ldap.h"
#include <math.h>

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

PasswordChecker *pwc = NULL;

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
    GVariant *reply = NULL;
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

static gchar*
get_netlogon_conn ()
{
    GError *error = NULL;
    GSubprocess *subprocess = NULL;
    gchar *url = NULL;
    gsize url_length;

    subprocess = g_subprocess_new (
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error,
        "wbinfo", "-P", NULL
    );

    if (error) {
        g_printerr ("Error creating subprocess (wbinfo): %s\n", error->message);
        g_error_free (error);
        return NULL;
    }

    GSource *timeout = NULL;
    GInputStream *instream = NULL;
    gchar buf[1025];
    
    instream = g_subprocess_get_stdout_pipe (subprocess);

    /*
    TODO: change to asynchronous call and add execution timeout
    */
    if (!g_input_stream_read_all (instream, buf, sizeof (buf), &url_length, NULL, &error)) {
        g_printerr ("Failed to read the output of the child process: %s\n", error->message);
        g_error_free (error);
        g_object_unref (subprocess);
        
        return NULL;
    }

    url = g_strndup (buf, url_length);

    g_object_unref (subprocess);

    return url;
}

static gchar*
get_url_ldap () {
    gchar *netlogon_conn_str = NULL;
    gchar *url = NULL;
    GRegex *regex = NULL;
    GMatchInfo *match_info = NULL;
    GError *error = NULL;

    netlogon_conn_str = get_netlogon_conn ();

    if (!netlogon_conn_str) {
        return NULL;
    }

    regex = g_regex_new ("\"(.*?)\"", 0, 0, &error);
    if (error) {
        g_printerr ("Error creating regex: %s\n", error->message);
        g_error_free (error);
        g_free (netlogon_conn_str);

        return NULL;
    }

    if (g_regex_match (regex, netlogon_conn_str, 0, &match_info)) {
        g_free (netlogon_conn_str);
        g_regex_unref (regex);

        gchar *prefix = "ldap://";
        gchar *url_with_prefix = NULL;

        url = g_match_info_fetch (match_info, 1);
        g_match_info_free (match_info);
        url_with_prefix = g_strconcat (prefix, url, NULL);

        g_free (url);

        return url_with_prefix;
    }

    g_free (netlogon_conn_str);
    g_regex_unref (regex);
    g_match_info_free (match_info);

    return NULL;
}

/*
    It's a bad temporary option
    I don't know how to get base_dn yet
    TODO: fixme
*/
static gchar*
get_base_dn (gchar *url)
{
    gchar *pos = NULL;
    gchar **parts = NULL;
    gchar *base_dn = NULL;
    gchar *url_copy = g_strdup (url);
    gchar *start_ptr = url_copy;

    pos = g_strstr_len (url_copy, -1, "://");
    if (pos != NULL) {
        url_copy = pos + 3;
    }

    parts = g_strsplit (url_copy, ".", -1);
    g_free (start_ptr);

    if (parts != NULL && parts[0] != NULL) {
        parts = parts + 1;

        for (int i = 0; parts[i] != NULL; i++){
            gchar *e = parts[i];
            parts[i] = g_strconcat ("dc=", parts[i], NULL);
            g_free (e);
        }

        base_dn = g_strjoinv (",", parts);

        g_strfreev (parts-1);
    }

    return base_dn;
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

    url = g_settings_get_string (*settings, "url");
    base_dn = g_settings_get_string (*settings, "base-dn");
    START_WARNING_TIME = g_settings_get_int64 (*settings, "start-warning-time") * TIME_CONV_START;
    WARNING_FREQ = g_settings_get_int64 (*settings, "warning-frequencies") * TIME_CONV_FREQ;

    if (g_strcmp0 (url, "") == 0) {
        g_free (url);
        url = get_url_ldap ();
        if (url) {
            if (! g_settings_set_string (*settings, "url", url)) {
                g_warning ("Failed to write the url retrieved from webinfo to the GSettings\n");
            }
        }
    }

    if (g_strcmp0 (base_dn, "") == 0 && url != NULL) {
        g_free (base_dn);
        base_dn = get_base_dn (url);
        if (base_dn) {
            if (! g_settings_set_string (*settings, "base-dn", base_dn)) {
                g_warning ("Failed to write the base-dn retrieved from webinfo to the GSettings\n");
            }
        }
    }

    passwordchecker_ldap_set_url (url, pwc_ldap);
    passwordchecker_ldap_set_base_dn (base_dn, pwc_ldap);

    *handler_id = g_signal_connect (*settings, "changed", G_CALLBACK (settings_changed), pwc_ldap);

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

    pwc->loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (pwc->loop);
    g_main_loop_unref (pwc->loop);

    g_object_unref (pwc->pwc_ldap);
    cleanup (pwc);
    cleanup (pwc);
    g_free (pwc);

    return rc;
} 