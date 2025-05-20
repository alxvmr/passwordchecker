#include "passwordchecker-indicator.h"

struct _PasswordcheckerIndicator {
    GObject parent_instance;
    
    AppIndicator *indicator;
    GtkWidget *menu;
    GtkWidget *expiration_date_item;
};

G_DEFINE_TYPE (PasswordcheckerIndicator, passwordchecker_indicator, G_TYPE_OBJECT)

extern void on_run_subprocess (const gchar *command);
extern gboolean is_file_exist (gchar *path);

static void
passwordchecker_indicator_change_password (GSimpleAction *action,
                                           GVariant      *parametr,
                                           gpointer       userdata)
{
    on_run_subprocess ("userpasswd");
}

static void
passwordchecker_indicator_change_settings (GSimpleAction *action,
                                           GVariant      *parametr,
                                           gpointer       userdata)
{
    on_run_subprocess ("PasswordCheckerSettings");
}

static void passwordchecker_indicator_dispose(GObject *gobject)
{
    PasswordcheckerIndicator *self = PASSWORDCHECKER_INDICATOR(gobject);
    
    if (self->indicator) {
        g_object_unref (self->indicator);
        self->indicator = NULL;
    }
    
    if (self->menu) {
        gtk_widget_destroy (self->menu);
        self->menu = NULL;
    }

    if (self->expiration_date_item) {
        gtk_widget_destroy (self->expiration_date_item);
        self->expiration_date_item = NULL;
    }
    
    G_OBJECT_CLASS (passwordchecker_indicator_parent_class)->dispose(gobject);
}

static void passwordchecker_indicator_class_init (PasswordcheckerIndicatorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = passwordchecker_indicator_dispose;
}

static void passwordchecker_indicator_init(PasswordcheckerIndicator *self) {
    self->indicator = NULL;
    self->menu = NULL;
}

PasswordcheckerIndicator*
passwordchecker_indicator_new (gchar *expiry_time)
{
    PasswordcheckerIndicator *self = PASSWORDCHECKER_INDICATOR (g_object_new (PASSWORDCHECKER_TYPE_INDICATOR, NULL));

    return self;
}

void
passwordchecker_indicator_set_time (PasswordcheckerIndicator *self,
                                    gchar *time)
{
    gchar *time_row = g_strdup_printf (_("The password is valid until:\n%s"), time ? time : "N/A");
    gtk_menu_item_set_label (GTK_MENU_ITEM(self->expiration_date_item), time_row);
    g_free (time_row);
}

void passwordchecker_indicator_add_menu_item (PasswordcheckerIndicator *self,
                                              const gchar              *label,
                                              GCallback                 callback,
                                              gpointer                  user_data)
{
    g_return_if_fail (label != NULL);
    g_return_if_fail (self->menu != NULL);
    
    GtkWidget *item = gtk_menu_item_new_with_label (label);

    if (callback) {
        g_signal_connect (item, "activate", callback, user_data);
    }

    gtk_menu_shell_append (GTK_MENU_SHELL(self->menu), item);
    gtk_widget_show (item);
}

void passwordchecker_indicator_setup (PasswordcheckerIndicator *self,
                                      const gchar *app_id,
                                      const gchar *icon_name)
{
    g_return_if_fail (PASSWORDCHECKER_IS_INDICATOR(self));
    g_return_if_fail (app_id != NULL);
    g_return_if_fail (icon_name != NULL);
    
    if (self->indicator) {
        g_warning("Indicator already setup");
        return;
    }
    
    self->indicator = app_indicator_new (app_id, icon_name, 
                                         APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
    self->menu = gtk_menu_new();
    app_indicator_set_menu(self->indicator, GTK_MENU(self->menu));

    passwordchecker_indicator_add_menu_item (self,
                                             _("Change the notification settings"),
                                             G_CALLBACK (passwordchecker_indicator_change_settings),
                                             g_application_get_default ());

    if (is_file_exist ("/usr/bin/userpasswd")) {
        passwordchecker_indicator_add_menu_item (self,
                                                 _("Change password"),
                                                 G_CALLBACK (passwordchecker_indicator_change_password),
                                                 g_application_get_default ());
    }

    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append (GTK_MENU_SHELL(self->menu), separator);
    gtk_widget_show (separator);

    self->expiration_date_item = gtk_menu_item_new_with_label (_("The password is valid until: UNKNOWN"));
    gtk_menu_shell_append (GTK_MENU_SHELL(self->menu), self->expiration_date_item);
    gtk_widget_set_sensitive ( self->expiration_date_item, FALSE);
    gtk_widget_show (self->expiration_date_item);

    app_indicator_set_status(self->indicator, APP_INDICATOR_STATUS_ACTIVE);
}

void passwordchecker_indicator_set_icon (PasswordcheckerIndicator *self,
                                         const gchar *icon_name)
{
    g_return_if_fail (PASSWORDCHECKER_IS_INDICATOR(self));
    g_return_if_fail (icon_name != NULL);
    g_return_if_fail (self->indicator != NULL);
    
    app_indicator_set_icon (self->indicator, icon_name);
}