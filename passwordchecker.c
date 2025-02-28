#include "passwordchecker-ldap.h"

int
main ()
{
    GDateTime *dt = NULL;
    gchar *dt_str = NULL;
    PasswordcheckerLdap *pwc_ldap = passwordchecker_ldap_new ("ldap://srt-dc1.smb.basealt.ru", "dc=smb,dc=basealt,dc=ru");
    int rc;

    rc = passwordchecker_ldap_get_date_time (pwc_ldap, &dt);

    if (!dt || !rc) {
        g_object_unref (pwc_ldap);
        return 1;
    }

    dt_str = g_date_time_format(dt, "%d-%m-%Y %H:%M:%S");
    g_print ("%s\n", dt_str);

    g_date_time_unref (dt);
    g_free (dt_str);
    g_object_unref (pwc_ldap);

    return 0;
}