#ifndef PASSWORDCHECKERINDICATOR_H
#define PASSWORDCHECKERINDICATOR_H
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <locale.h>
#include <libintl.h>

#define _(STRING) gettext(STRING)
G_BEGIN_DECLS

#define PASSWORDCHECKER_TYPE_INDICATOR (passwordchecker_indicator_get_type())

G_DECLARE_FINAL_TYPE (PasswordcheckerIndicator, 
                      passwordchecker_indicator, 
                      PASSWORDCHECKER, 
                      INDICATOR, 
                      GObject)

PasswordcheckerIndicator *passwordchecker_indicator_new ();

void passwordchecker_indicator_setup (PasswordcheckerIndicator *self,
                                      const gchar *app_id,
                                      const gchar *icon_name);

void passwordchecker_indicator_set_icon (PasswordcheckerIndicator *self,
                                         const gchar *icon_name);

void passwordchecker_indicator_set_time (PasswordcheckerIndicator *self,
                                         gchar *time);

G_END_DECLS

#endif // PASSWORDCHECKER_INDICATOR_H