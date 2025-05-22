#include "passwordchecker-app.h"
#include "passwordchecker-window.h"

struct _PasswordcheckerApp {
#ifdef USE_ADWAITA
    AdwApplication parent_instance;
#else
    GtkApplication parent_instance;
#endif

    PasswordcheckerWindow *window;
};
    
#ifdef USE_ADWAITA
    G_DEFINE_TYPE (PasswordcheckerApp, passwordchecker_app, ADW_TYPE_APPLICATION);
#else
    G_DEFINE_TYPE (PasswordcheckerApp, passwordchecker_app, GTK_TYPE_APPLICATION);
#endif

#ifdef USE_ADWAITA
static void
passwordchecker_app_about_action (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       userdata)
{
    static const char *developers[] = {
        "Maria Alexeeva",
        NULL
    };

    PasswordcheckerApp *self = userdata;
    GtkWindow *window = NULL;

    g_assert (PASSWORDCHECKER_IS_APP (self));

    window = gtk_application_get_active_window (GTK_APPLICATION (self));
    
    adw_show_about_dialog (GTK_WIDGET (window),
                           "application-name", "PasswordChecker",
                           "version", VERSION,
                           "copyright", "Copyright (C) 2025 Maria O. Alexeeva\nalxvmr@altlinux.org",
                           "issue-url", "https://github.com/alxvmr/passwordchecker/issues",
                           "license-type", GTK_LICENSE_GPL_3_0,
                           "developers", developers,
                           NULL);
}
#endif

static void
passwordchecker_app_quit_action (GSimpleAction *action,
                                 GVariant      *parametr,
                                 gpointer       userdata)
{
    PasswordcheckerApp *self = userdata;

    g_assert (PASSWORDCHECKER_IS_APP (self));

    g_application_quit (G_APPLICATION (self));
}

static void
passwordchecker_app_press_enter (GSimpleAction *action,
                                 GVariant      *parametr,
                                 gpointer       userdata)
{
    PasswordcheckerApp *self = userdata;

#ifdef USE_ADWAITA
    GtkWidget *stack_page =  adw_view_stack_get_visible_child (ADW_VIEW_STACK (self->window->stack));
    const gchar* id_page = gtk_widget_get_name (stack_page);
#else
    gint stack_page_number = gtk_notebook_get_current_page (GTK_NOTEBOOK (self->window->stack));
    gchar *id_page = NULL;
    if (stack_page_number == 0)
        id_page = "notebook-page-application";
    else
        id_page = "notebook-page-connection";
#endif

    if (g_strcmp0 (id_page, "notebook-page-connection") == 0) {
        g_signal_emit_by_name (self->window->button_conn, "clicked", self->window);
    } else if (g_strcmp0 (id_page, "notebook-page-application") == 0) {
        g_signal_emit_by_name (self->window->button_app, "clicked", self->window);
    }
}

static void
passwordchecker_app_activate (GApplication *app)
{
    g_assert (PASSWORDCHECKER_IS_APP (app));

    PASSWORDCHECKER_APP (app)->window = PASSWORDCHECKER_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (app)));

    if (PASSWORDCHECKER_APP (app)->window == NULL) {
        PASSWORDCHECKER_APP (app)->window = g_object_new (PASSWORDCHECKER_TYPE_WINDOW,
                                                          "application", app,
                                                          NULL);
    }

    gtk_window_present (GTK_WINDOW (PASSWORDCHECKER_APP(app)->window));
}

static void
passwordchecker_app_class_init (PasswordcheckerAppClass *class)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (class);

    app_class->activate = passwordchecker_app_activate;
}

PasswordcheckerApp *
passwordchecker_app_new (const char        *application_id, 
                         GApplicationFlags flags)
{
    g_return_val_if_fail (application_id != NULL, NULL);

    return g_object_new (PASSWORDCHECKER_TYPE_APP,
                         "application-id", application_id,
                         "flags", flags,
                         NULL);
}

static const GActionEntry app_actions[] = {
    {"quit", passwordchecker_app_quit_action},
    {"press_enter", passwordchecker_app_press_enter},
    #ifdef USE_ADWAITA
    {"about", passwordchecker_app_about_action}
    #endif
};

static void
passwordchecker_app_init (PasswordcheckerApp *self)
{
    g_action_map_add_action_entries (G_ACTION_MAP (self),
                                     app_actions,
                                     G_N_ELEMENTS (app_actions),
                                     self);

    gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                           "app.quit",
                                           (const char *[]) { "Escape", NULL });
    /*
    TODO: is this how a accelerator should be created for a widget?
    */
    gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                           "app.press_enter",
                                           (const char *[]) { "Return", NULL});
}

int
main (int     argc,
      char  **argv)
{
    setlocale (LC_ALL, "");
    bindtextdomain ("passwordchecker", "/usr/share/locale/");
    textdomain ("passwordchecker");

    g_set_application_name ("PasswordChecker");

    PasswordcheckerApp *app = passwordchecker_app_new ("org.altlinux.PasswordChecker", G_APPLICATION_DEFAULT_FLAGS);
    
    int status = 0;
    status = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref (app);

    return status;
}