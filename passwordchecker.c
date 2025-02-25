#include <gio/gio.h>
#include <ldap.h>
#include <gssapi/gssapi.h>

int
main ()
{
    LDAP *ld;
    gint rc;

    rc = ldap_initialize (&ld, "ldap://srt-dc1.smb.basealt.ru");
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_initialize failed: %s\n", ldap_err2string (rc));
        ldap_unbind_ext_s (ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    int version = LDAP_VERSION3;
    rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_set_option failed: %s\n", ldap_err2string (rc));
        ldap_unbind_ext_s (ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    // ldap_set_option (ld, LDAP_OPT_X_SASL_NOCANON, LDAP_OPT_ON);
    rc = ldap_sasl_bind_s(ld, NULL, "GSSAPI", NULL, NULL, NULL, NULL);
    if (rc != 14 && rc != LDAP_SUCCESS) {
        g_printerr ("ldap_sasl_bind_s failed: %s\n", ldap_err2string (rc));
        ldap_unbind_ext_s (ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    LDAPMessage *result, *entry;
    // work with ldap_search_ext_s (ld, NULL,...)
    rc = ldap_search_ext_s (ld, "dc=smb,dc=basealt,dc=ru", LDAP_SCOPE_BASE, "(Objectclass=*)", NULL, 0, NULL, NULL, NULL, 0, &result);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_search_ext_s failed: %s\n", ldap_err2string (rc));
        ldap_unbind_ext_s (ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    entry = ldap_first_entry (ld, result);
    if (entry == NULL) {
        g_printerr ("ldap_first_entry: No entries found\n");
        ldap_msgfree (result);
        return FALSE;
    }

    struct berval **vals;
    gchar *attr;
    BerElement *ber;
    
    for (entry = ldap_first_entry (ld, result); entry != NULL; entry = ldap_next_entry (ld, result)) {
        if (entry == NULL) {
            g_printerr ("ldap_first_entry: No entries found\n");
            ldap_msgfree (result);
            break;
        }
        for (attr = ldap_first_attribute (ld, entry, &ber); attr != NULL; attr = ldap_next_attribute (ld, entry, ber)) {
            vals = ldap_get_values_len (ld, entry, attr);
            for (int i = 0; vals[i] != NULL; i++) {
                g_print ("%s: %s\n", attr, vals[i]->bv_val);
            }
        }
    }

    rc = ldap_unbind_ext_s (ld, NULL, NULL);
    if (rc != 0) {
        g_printerr ("ldap_unbind failed: %s\n", ldap_err2string (rc));
        ldap_unbind_ext_s (ld, NULL, NULL);
        return EXIT_FAILURE;
    }

    /*
    TODO: add free
    */
}