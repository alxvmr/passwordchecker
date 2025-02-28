#ifndef PASSWORDCHECKERLDAP_H
#define PASSWORDCHECKERLDAP_H
#include <sasl/sasl.h>
#include <ldap.h>
#include <gio/gio.h>
#include <pwd.h>

G_BEGIN_DECLS

#define PASSWORDCHECKER_TYPE_LDAP (passwordchecker_ldap_get_type ())
G_DECLARE_FINAL_TYPE (PasswordcheckerLdap, passwordchecker_ldap, PASSWORDCHECKER, LDAP, GObject)

typedef struct _PasswordcheckerLdap {
    GObject parent_instance;

    gchar *url;
    gchar *base_dn;
    gchar *filter;
    gchar *attr;
    gchar *username;
} PasswordcheckerLdap;

PasswordcheckerLdap*
passwordchecker_ldap_new (gchar *url,
                          gchar *base_dn);

static gboolean
passwordchecker_ldap_set_url (gchar               *url,
                              PasswordcheckerLdap *self);

static gboolean
passwordchecker_ldap_set_base_dn (gchar               *base_dn,
                                  PasswordcheckerLdap *self);

gboolean
passwordchecker_ldap_get_date_time (PasswordcheckerLdap *self,
                                    GDateTime           **datetime);

G_END_DECLS

#endif