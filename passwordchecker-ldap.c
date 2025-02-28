#include "passwordchecker-ldap.h"

G_DEFINE_TYPE (PasswordcheckerLdap, passwordchecker_ldap, G_TYPE_OBJECT);

typedef struct sasl_defaults_gssapi {
    gchar *mech;
    gchar *realm;
    gchar *authcid;
    gchar *passwd;
    gchar *authzid;
} sasl_defaults_gssapi;

static int
ldap_sasl_interact (LDAP *ld, 
                    unsigned flags,
                    void *defs,
                    void *in)
{
    sasl_defaults_gssapi *defaults = (sasl_defaults_gssapi *) defs;
    sasl_interact_t *interact = (sasl_interact_t *) in;

    if (ld == NULL) {
        return LDAP_PARAM_ERROR;
    }

    while (interact->id != SASL_CB_LIST_END) {
        const char *dflt = interact->defresult;

        switch (interact->id) {
            case SASL_CB_GETREALM:
                if (defaults)
                    dflt = defaults->realm;
                break;
            case SASL_CB_AUTHNAME:
                if (defaults)
                    dflt = defaults->authcid;
                break;
            case SASL_CB_PASS:
                if (defaults)
                    dflt = defaults->passwd;
                break;
            case SASL_CB_USER:
                if (defaults)
                    dflt = defaults->authzid;
                break;
            case SASL_CB_NOECHOPROMPT:
                break;
            case SASL_CB_ECHOPROMPT:
                break;
        }

        if (dflt && !*dflt) {
                dflt = NULL;
            }

        /* input must be empty */
        interact->result = (dflt && *dflt) ? dflt : "";
        interact->len = strlen((const gchar *) interact->result);
        interact++;
    }

    return LDAP_SUCCESS;
}

static gboolean
get_date_from_ad_timestamp (gchar     *s,
                            GDateTime **output)
{
    if (!s)
        return FALSE;
    
    guint64 ad_timestamp = g_ascii_strtoull (s, NULL, 10);
    guint64 nanoseconds = (ad_timestamp - 116444736000000000ULL) * 100;
    time_t seconds = nanoseconds / 1000000000;

    *output = g_date_time_new_from_unix_local (seconds); // local???
    return TRUE;
}

static gboolean
set_options_for_ld (LDAP *ld)
{
    if (ld == NULL)
        return FALSE;

    gint rc;

    gint version = LDAP_VERSION3;
    rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_set_option failed: %s\n", ldap_err2string (rc));
        return FALSE;
    }

    rc = ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_set_option failed: %s\n", ldap_err2string (rc));
        return FALSE;
    }

    rc = ldap_set_option(ld, LDAP_OPT_X_SASL_NOCANON, LDAP_OPT_ON);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_set_option failed: %s\n", ldap_err2string (rc));
        return FALSE;
    }

    return TRUE;
}

/*
  The function is intended for obtaining the value
  of one specific attribute from one record
  (the uniqueness of the record is set by the filter)
*/
static gboolean
get_value_of_attr (LDAP *ld,
                   gchar *base_dn,
                   gchar *filter,
                   gchar *attr,
                   gchar **output_value)
{
    LDAPMessage *result = NULL;
    LDAPMessage *entry = NULL;
    struct berval **vals = NULL;
    gchar *cur_entry_attr = NULL;
    BerElement *ber = NULL;
    gint rc;
    *output_value = NULL;

    gchar **attrs = NULL;
    if (attr) {
        attrs = g_new (gchar *, 2);
        attrs[0] = g_strdup (attr);
        attrs[1] = NULL;
    }

    rc = ldap_search_ext_s (ld,
                            base_dn,
                            LDAP_SCOPE_SUBTREE, // subtree or base???
                            filter,
                            attrs,
                            0,
                            NULL,
                            NULL,
                            NULL,
                            LDAP_NO_LIMIT,
                            &result);
    if (attrs) {
        g_free (attrs[0]);
        g_free (attrs);
    }

    if (rc != LDAP_SUCCESS) {
        g_print ("ldap_search_ext_s failed: %s\n", ldap_err2string (rc));
        ldap_msgfree (result);
        return FALSE;
    }

    entry = ldap_first_entry (ld, result);
    if (entry == NULL) {
        ldap_msgfree (result);
        return TRUE;
    }

    cur_entry_attr = ldap_first_attribute (ld, entry, &ber);
    if (cur_entry_attr != NULL) {
        vals = ldap_get_values_len (ld, entry, cur_entry_attr);
        *output_value = g_strdup (vals[0]->bv_val);

        ldap_value_free_len (vals);
        ldap_memfree (cur_entry_attr);
    }

    ber_free (ber, 0);
    ldap_msgfree (result);

    return TRUE;
}

gboolean
passwordchecker_ldap_get_date_time (PasswordcheckerLdap *self,
                                    GDateTime           **datetime)
{
    LDAP *ld;
    gint rc, rc_set_opt;
    gchar *value;

    if (self->url == NULL || self->base_dn == NULL)
        return FALSE;

    rc = ldap_initialize (&ld, self->url);
    if (rc != LDAP_SUCCESS) {
        g_printerr ("ldap_initialize failed: %s\n", ldap_err2string (rc));
        goto close;
    }

    if (!set_options_for_ld (ld)) 
        goto close;

    // Setup sasl_defaults_gssapi
    struct sasl_defaults_gssapi defaults;
    defaults.mech = (gchar *) "GSSAPI";
    ldap_get_option(ld, LDAP_OPT_X_SASL_REALM, &defaults.realm);
    ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHCID, &defaults.authcid);
    ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHZID, &defaults.authzid);
    defaults.passwd = NULL;

    rc = ldap_sasl_interactive_bind_s (ld, NULL, defaults.mech, NULL, NULL, LDAP_SASL_QUIET, ldap_sasl_interact, &defaults);
    ldap_memfree(defaults.realm);
    ldap_memfree(defaults.authcid);
    ldap_memfree(defaults.authzid);

    gboolean res = get_value_of_attr (ld, self->base_dn, self->filter, self->attr, &value);

    if (!res)
        goto close;

    if (!value)
        g_print ("No value\n");
    else {
        GDateTime *res = NULL;
        if (get_date_from_ad_timestamp (value, &res)) {
            *datetime = res;

            g_free (value);

            rc = ldap_unbind_ext_s (ld, NULL, NULL);
            ld = NULL;
            if (rc != 0) {
                g_printerr ("ldap_unbind failed: %s\n", ldap_err2string (rc));
            }
            
            return TRUE;
        }

        g_free (value);
    }

    goto close;

close:
    rc = ldap_unbind_ext_s (ld, NULL, NULL);
    ld = NULL;
    if (rc != 0) {
        g_printerr ("ldap_unbind failed: %s\n", ldap_err2string (rc));
    }

    return FALSE;
}

static gchar*
get_username ()
{
    uid_t uid = getuid();

    struct passwd pwd;
    struct passwd *pwd_res = NULL;
    gchar buf[4096];

    int ret = getpwuid_r (uid, &pwd, buf, sizeof(buf), &pwd_res);
    if (ret) {
        return NULL;
    }
    return g_strdup (pwd.pw_name);
}

static gboolean
passwordchecker_ldap_set_url (gchar               *url,
                              PasswordcheckerLdap *self)
{
    self->url = url;
}

static gboolean
passwordchecker_ldap_set_base_dn (gchar               *base_dn,
                                  PasswordcheckerLdap *self)
{
    self->base_dn = base_dn;
}

static void
passwordchecker_ldap_finalize (GObject *object)
{
    PasswordcheckerLdap *self = PASSWORDCHECKER_LDAP (object);

    g_free (self->url);
    g_free (self->base_dn);
    g_free (self->filter);

    G_OBJECT_CLASS (passwordchecker_ldap_parent_class)->finalize (object);
}

static void
passwordchecker_ldap_class_init (PasswordcheckerLdapClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = passwordchecker_ldap_finalize;
}

static void
passwordchecker_ldap_init (PasswordcheckerLdap *self) {}

PasswordcheckerLdap *
passwordchecker_ldap_new (gchar *url,
                          gchar *base_dn)
{
    PasswordcheckerLdap *self = PASSWORDCHECKER_LDAP (g_object_new (PASSWORDCHECKER_TYPE_LDAP, NULL));
    self->url = g_strdup (url);
    self->base_dn = g_strdup (base_dn);
    self->attr = "msDS-UserPasswordExpiryTimeComputed";

    gchar *username = get_username ();
    if (!username) {
        g_printerr ("[ERROR] Username is NULL\n");
    }

    self->filter = g_strdup_printf("(&(!(userAccountControl:1.2.840.113556.1.4.803:=65536))(sAMAccountName=%s))", username);
    g_free (username);

    return self;
}