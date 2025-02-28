#include "passwordchecker-ldap.h"
#include <math.h>

static gint64 START_WARNING_TIME; //hours
static gint64 WARNING_FREQ;       //hours
static gboolean TIMER_ON = FALSE;
static guint TIMER_WARNING_ID = 0;
static guint LDAP_SEARCH_TIME = 24;

static gint TIME_CONV = 3600;

static void
cleanup (GSettings *settings,
         gulong    handler_id)
{
    g_signal_handler_disconnect (settings, handler_id);
    g_object_unref (settings);
}

gboolean
print_warning ()
{
    g_print ("CHANGE PASSWORD!!!\n");
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

    if (TIMER_WARNING_ID != 0) {
        g_source_remove (TIMER_WARNING_ID);
        TIMER_WARNING_ID = 0;
    }

    if (START_WARNING_TIME >= diff_hours) {
        print_warning ();
        TIMER_WARNING_ID = g_timeout_add_seconds (WARNING_FREQ, print_warning, NULL);
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
            START_WARNING_TIME = value;
        }
        if (g_strcmp0 (key, "warning-frequencies") == 0) {
            WARNING_FREQ = value * TIME_CONV;
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
    START_WARNING_TIME = g_settings_get_int64 (*settings, "start-warning-time");
    WARNING_FREQ = g_settings_get_int64 (*settings, "warning-frequencies") * TIME_CONV;

    passwordchecker_ldap_set_url (url, pwc_ldap);
    passwordchecker_ldap_set_base_dn (base_dn, pwc_ldap);

    g_free (url);
    g_free (base_dn);

    return TRUE;
}

int
main ()
{
    GDateTime *dt = NULL;
    gchar *dt_str = NULL;
    PasswordcheckerLdap *pwc_ldap = NULL;
    GSettings *settings = NULL;
    gulong handler_id;
    // PasswordcheckerLdap *pwc_ldap = passwordchecker_ldap_new ("ldap://srt-dc1.smb.basealt.ru", "dc=smb,dc=basealt,dc=ru");
    int rc;

    pwc_ldap = passwordchecker_ldap_new (NULL, NULL);
    if (!load_gsettings ("org.altlinux.passwordchecker", pwc_ldap, &settings, &handler_id))
        return EXIT_FAILURE;

    check_password (pwc_ldap);
    g_timeout_add_seconds (LDAP_SEARCH_TIME * TIME_CONV, check_password, pwc_ldap);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    g_main_loop_unref (loop);
    g_object_unref (pwc_ldap);
    cleanup (settings, handler_id);

    return 0;
}