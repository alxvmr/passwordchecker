#include "passwordchecker-ldap.h"

static void
cleanup (GSettings *settings,
         gulong    handler_id)
{
    g_signal_handler_disconnect (settings, handler_id);
    g_object_unref (settings);
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
 
    GVariant *value_gv = NULL;
    const gchar* value = NULL;
 
    value_gv = g_settings_get_value (settings, key);
    value = g_variant_get_string (value_gv, NULL);

    g_variant_unref (value_gv);
 
    if (g_strcmp0 (key, "url") == 0) {
        g_print ("Current: key: %s, value: %s\n", key, pwc_ldap->url);
        passwordchecker_ldap_set_url (g_strdup (value), pwc_ldap);
        return;
    }

    if (g_strcmp0 (key, "base-dn") == 0) {
        passwordchecker_ldap_set_base_dn (g_strdup (value), pwc_ldap);
        return;
    }

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

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    g_main_loop_unref (loop);
    g_object_unref (pwc_ldap);
    cleanup (settings, handler_id);

    // rc = passwordchecker_ldap_get_date_time (pwc_ldap, &dt);

    // if (!dt || !rc) {
    //     g_object_unref (pwc_ldap);
    //     return 1;
    // }

    // dt_str = g_date_time_format(dt, "%d-%m-%Y %H:%M:%S");
    // g_print ("%s\n", dt_str);

    // g_date_time_unref (dt);
    // g_free (dt_str);
    // g_object_unref (pwc_ldap);

    return 0;
}