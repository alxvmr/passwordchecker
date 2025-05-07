#ifndef PASSWORDCHECKERAPP_H
#define PASSWORDCHECKERAPP_H
#ifdef USE_ADWAITA
    #include <adwaita.h>
#else
    #include <gtk/gtk.h>
#endif
#include <locale.h>
#include <libintl.h>

#define _(STRING) gettext(STRING)
G_BEGIN_DECLS

#define PASSWORDCHECKER_TYPE_APP (passwordchecker_app_get_type ())
#ifdef USE_ADWAITA
    G_DECLARE_FINAL_TYPE (PasswordcheckerApp, passwordchecker_app, PASSWORDCHECKER, APP, AdwApplication)
#else
    G_DECLARE_FINAL_TYPE (PasswordcheckerApp, passwordchecker_app, PASSWORDCHECKER, APP, GtkApplication)
#endif

PasswordcheckerApp* passwordchecker_app_new (const char *application_id, GApplicationFlags flags);

G_END_DECLS

#endif