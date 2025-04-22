#include <gtk/gtk.h>
#ifdef USE_ADWAITA
    #include <adwaita.h>
#endif
#include <locale.h>
#include <libintl.h>

#define SCHEMA_NAME "org.altlinux.passwordchecker"
#define UI_PATH "/usr/share/PasswordCheckerSettings"

#define _(STRING) gettext(STRING)

#ifndef USE_ADWAITA
typedef struct _Notification_ui {
        GtkWidget *widget;
        guint timer_id;
    } Notification_ui;
#endif

typedef struct _PasswordcheckerUI {
#ifdef USE_ADWAITA
    AdwApplication *app;
#else
    GtkApplication *app;
#endif
    const gchar *id;
    GSettings *settings;
    GtkWidget *url;
    GtkWidget *base_dn;
    GtkWidget *start_warning_time_days;
    GtkWidget *warning_frequencies_days;
    GtkWidget *warning_frequencies_hours;
    GtkWidget *warning_frequencies_min;
    GtkWidget *error_start;
    GtkWidget *error_freq;

    GtkWidget *window;
    GtkWidget *on_enter_button;
    GtkWidget *button_conn;
    GtkWidget *button_app;

    GtkWidget *toast_overlay;
#ifndef USE_ADWAITA
    Notification_ui *notification;
#endif

    GtkWidget *stack;
} PasswordCheckerUI;

enum {
    TO_MINS,
    TO_HOURS,
    TO_DAYS
};

static void
cleanup (PasswordCheckerUI *pwd_ui)
{
    g_object_unref (pwd_ui->app);

    if (pwd_ui->settings) {
        g_object_unref (pwd_ui->settings);
    }

    g_free (pwd_ui);
}

static gboolean
convert_x (GValue   *value,
           GVariant *variant,
           gpointer  user_data)
{
	gint64 to_set = g_variant_get_int64 (variant);
	gchar *str;

	str = g_strdup_printf ("%ld", to_set);
	g_value_set_string (value, str);
	g_free (str);

	return TRUE;
}

static gboolean
convert_mins (GValue   *value,
              GVariant *variant,
              gpointer  user_data)
{
    gint convert_type = GPOINTER_TO_INT (user_data);

    gint64 mins = g_variant_get_int64 (variant);
    gint16 days;
    gint16 hours;
    gint16 mins_out;
    gchar *str = NULL;

    days = mins / 1440;
    mins %= 1440;
    hours = mins / 60;
    mins_out = mins % 60;

    switch (convert_type)
    {
        case TO_MINS:
            str = g_strdup_printf ("%d", mins_out);
            break;
        case TO_HOURS:
            str = g_strdup_printf ("%d", hours);
            break;
        case TO_DAYS:
            str = g_strdup_printf ("%d", days);
            break;
        default:
            return FALSE;
    }

    g_value_set_string (value, str);
    g_free (str);

    return TRUE;
}

#ifndef USE_ADWAITA
static gboolean
hide_notification (Notification_ui **notification_ptr)
{
    if (notification_ptr && *notification_ptr) {
        Notification_ui *notification = *notification_ptr;

        if (notification->widget) {
            gtk_widget_unparent (notification->widget);
            notification->widget = NULL;
        }

        g_source_remove (notification->timer_id);
        g_free (notification);
        *notification_ptr = NULL;
    }

    return TRUE;
}
#endif

static void
send_notification (const gchar       *body,
                   const gchar       *status,
                   PasswordCheckerUI *pwd_ui)
{
#ifdef USE_ADWAITA
    adw_toast_overlay_dismiss_all (ADW_TOAST_OVERLAY (pwd_ui->toast_overlay));
    AdwToast *toast = adw_toast_new (body);
    adw_toast_set_timeout (toast, 3);
    adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (pwd_ui->toast_overlay), toast);
#else
    if (pwd_ui->notification) {
        hide_notification (&pwd_ui->notification);
    }

    Notification_ui *notification = g_new (Notification_ui, 1);
    pwd_ui->notification = notification;

    notification->widget = GTK_WIDGET (gtk_label_new (body));
    gtk_widget_set_margin_bottom (notification->widget, 50);
    gtk_widget_set_valign(notification->widget, GTK_ALIGN_END);
    gtk_widget_set_vexpand(notification->widget, TRUE);

    gtk_overlay_add_overlay (GTK_OVERLAY (pwd_ui->toast_overlay), notification->widget);
    notification->timer_id = g_timeout_add_seconds (3, (GSourceFunc)hide_notification, &pwd_ui->notification);
#endif
    return;
}

static void
cb_button_conn (GtkWidget *button,
                gpointer   user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;

    const gchar *url_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->url));
    const gchar *base_dn_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->base_dn));

    if (g_settings_set_string (pwd_ui->settings, "url", url_new) && 
        g_settings_set_string (pwd_ui->settings, "base-dn", base_dn_new))
    {
        send_notification (_("Connection settings have been successfully changed"), "success", pwd_ui);
        return;
    }

    send_notification (_("Unable to change connection settings"), "error", pwd_ui);
    return;
}

static gboolean
is_numeric (gchar *str)
{
    GRegex *regex = g_regex_new ("^[0-9]+$", 0, 0, NULL);
    gboolean result = g_regex_match (regex, str, 0, NULL);

    g_regex_unref (regex);
    return result;
}

static gint64
convert_str_2_gint64 (gchar   *str,
                      gint64  *out,
                      gchar  **error)
{
    gchar *end_ptr = NULL;

    if (str == NULL || *str == '\0') {
        *error = g_strdup (_("Time units can't be empty"));
        return FALSE;
    }

    if (*str == '-') {
        *error = g_strdup (_("Time units can't be negative"));
        return FALSE;
    }

    if (!is_numeric (str)) {
        *error = g_strdup (_("The record contains invalid characters"));
        return FALSE;
    }

    *out = g_ascii_strtoll (str, &end_ptr, 10);

    if (end_ptr == str) {
        g_printerr ("Error converting string to number\n");
        g_free (str);
        g_free (end_ptr);
        return FALSE;
    }

    g_free (str);
    return TRUE;
}

static void
cb_button_app (GtkWidget *button,
               gpointer   user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;

    gtk_widget_set_visible (pwd_ui->error_start, FALSE);
    gtk_widget_set_visible (pwd_ui->error_freq, FALSE);

    const gchar *start_warning_time_days = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->start_warning_time_days));

    const gchar *warning_freq_mins = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_min));
    const gchar *warning_freq_hours = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_hours));
    const gchar *warning_freq_days = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_days));

    gint64 freq_min;
    gint64 freq_hours;
    gint64 freq_days;
    gint64 gint64_start_warning_time = -1;
    gint64 gint64_warning_freq = -1;

    gchar *error_mess = NULL;

    if (! convert_str_2_gint64 (g_strdup (start_warning_time_days), &gint64_start_warning_time, &error_mess)) {
        gint64_start_warning_time = -1;

        gtk_widget_set_visible (pwd_ui->error_start, TRUE);
        gtk_widget_set_tooltip_text (pwd_ui->error_start, error_mess);
    }

    error_mess = NULL;

    if (convert_str_2_gint64 (g_strdup (warning_freq_mins), &freq_min, &error_mess)) {
        if (convert_str_2_gint64 (g_strdup (warning_freq_hours), &freq_hours, &error_mess)) {
            freq_hours *= 60;
            if (convert_str_2_gint64 (g_strdup (warning_freq_days), &freq_days, &error_mess)) {
                freq_days *= 1440;
                gint64_warning_freq = freq_min + freq_hours + freq_days;
            }
        }
    } 
    
    if (error_mess) {
        gtk_widget_set_visible (pwd_ui->error_freq, TRUE);
        gtk_widget_set_tooltip_text (pwd_ui->error_freq, error_mess);

        g_free (error_mess);
    }

    if (gint64_start_warning_time != -1 && gint64_warning_freq != -1) {
        if (g_settings_set_int64 (pwd_ui->settings, "start-warning-time", gint64_start_warning_time) && 
            g_settings_set_int64 (pwd_ui->settings, "warning-frequencies", gint64_warning_freq))
        {
            send_notification (_("Application settings have been successfully changed"), "sucess", pwd_ui);
            return;
        }
    }

    send_notification (_("Unable to change application settings"), "error", pwd_ui);
    return;
}

static void
#ifdef USE_USEADWAITA
activate (AdwApplication *app,
          gpointer        user_data)
#else
activate (GtkApplication* app,
          gpointer        user_data)
#endif
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;

    pwd_ui->window = GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (pwd_ui->app)));
    if (pwd_ui->window != NULL) {
        gtk_window_present (GTK_WINDOW (pwd_ui->window));
        return;
    }
#ifndef USE_ADWAITA
    pwd_ui->notification = NULL;
#endif

    GError *error = NULL;
    GtkBuilder *builder = gtk_builder_new ();

#ifdef USE_ADWAITA
    gtk_builder_add_from_file (builder, UI_PATH "/ui/passwordchecker-gnome-window.ui", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    AdwToolbarView *toolbar = ADW_TOOLBAR_VIEW (gtk_builder_get_object (builder, "toolbar"));
    AdwClamp *container = ADW_CLAMP (gtk_builder_get_object (builder, "container"));

    gtk_builder_add_from_file (builder, UI_PATH "/ui/passwordchecker-gnome-switcher.ui", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->toast_overlay = GTK_WIDGET (gtk_builder_get_object (builder, "toast-overlay"));
    AdwToolbarView *switcher = ADW_TOOLBAR_VIEW (gtk_builder_get_object (builder, "toolbar-page"));
    // AdwViewStack *stack = ADW_VIEW_STACK (gtk_builder_get_object (builder, "stack"));
    pwd_ui->stack = GTK_WIDGET (gtk_builder_get_object (builder, "stack"));

    gtk_builder_add_from_file (builder, UI_PATH "/ui/passwordchecker-gnome-page-app.ui", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->error_start = GTK_WIDGET (gtk_builder_get_object (builder, "page2-error1"));
    pwd_ui->error_freq = GTK_WIDGET (gtk_builder_get_object (builder, "page2-error2"));
    pwd_ui->start_warning_time_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry1-days"));
    pwd_ui->warning_frequencies_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-days"));
    pwd_ui->warning_frequencies_hours = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-hours"));
    pwd_ui->warning_frequencies_min = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-min"));
    pwd_ui->button_app = GTK_WIDGET (gtk_builder_get_object (builder, "page2-button1"));

    GtkBox *app_content = GTK_BOX (gtk_builder_get_object (builder, "notebook-page-application"));
    gtk_widget_set_name (GTK_WIDGET (app_content), "notebook-page-application");
    AdwViewStackPage *app_page = adw_view_stack_add (ADW_VIEW_STACK (pwd_ui->stack), GTK_WIDGET (app_content));
    adw_view_stack_page_set_title (app_page, _("Application"));

    gtk_builder_add_from_file (builder, UI_PATH "/ui/passwordchecker-gnome-page-con.ui", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->url = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry1"));
    pwd_ui->base_dn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry2"));
    pwd_ui->button_conn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-button1"));

    GtkBox *con_content = GTK_BOX (gtk_builder_get_object (builder, "notebook-page-connection"));
    gtk_widget_set_name (GTK_WIDGET (con_content), "notebook-page-connection");
    AdwViewStackPage *con_page = adw_view_stack_add (ADW_VIEW_STACK (pwd_ui->stack), GTK_WIDGET (con_content));
    adw_view_stack_page_set_title (con_page, _("Connection"));

    adw_toolbar_view_set_content (switcher, pwd_ui->stack);
    adw_toast_overlay_set_child (ADW_TOAST_OVERLAY (pwd_ui->toast_overlay), GTK_WIDGET (switcher));

    adw_clamp_set_child (container, pwd_ui->toast_overlay);

    adw_toolbar_view_set_content (toolbar, GTK_WIDGET (container));

    pwd_ui->window = adw_application_window_new (GTK_APPLICATION (app));

    adw_application_window_set_content (ADW_APPLICATION_WINDOW (pwd_ui->window), GTK_WIDGET (toolbar));

#else
    pwd_ui->window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (pwd_ui->window), "PasswordCheckerSettings");

    pwd_ui->toast_overlay = gtk_overlay_new ();
    GtkWidget *main_container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    pwd_ui->stack = gtk_notebook_new ();

    /* page 1 */
    gtk_builder_add_from_file (builder, UI_PATH "/ui/page_application.glade", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->error_start = GTK_WIDGET (gtk_builder_get_object (builder, "page2-error1"));
    pwd_ui->error_freq = GTK_WIDGET (gtk_builder_get_object (builder, "page2-error2"));

    gtk_image_set_from_file (GTK_IMAGE (pwd_ui->error_start), UI_PATH "/icons/error.svg");
    gtk_image_set_from_file (GTK_IMAGE (pwd_ui->error_freq), UI_PATH "/icons/error.svg");

    pwd_ui->start_warning_time_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry1-days"));
    pwd_ui->warning_frequencies_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-days"));
    pwd_ui->warning_frequencies_hours = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-hours"));
    pwd_ui->warning_frequencies_min = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-min"));

    pwd_ui->button_app = GTK_WIDGET (gtk_builder_get_object (builder, "page2-button1"));

    GtkWidget *label_start = GTK_WIDGET(gtk_builder_get_object (builder, "page2-label1"));
    gtk_label_set_text (GTK_LABEL (label_start), _("Notification start time"));
    gtk_widget_set_tooltip_text (label_start, _("How much time to warn the user about password expiration"));

    GtkWidget *label_freq = GTK_WIDGET(gtk_builder_get_object (builder, "page2-label2"));
    gtk_label_set_text (GTK_LABEL (label_freq), _("Frequency of warnings"));
    gtk_widget_set_tooltip_text (label_freq, _("Sets the frequency of password change warning output"));

    GObject *box_page_application = gtk_builder_get_object (builder, "notebook-page-application");
    gtk_widget_set_name (GTK_WIDGET (box_page_application), "notebook-page-application");
    GtkWidget *label_page_application = gtk_label_new (_("Application"));
    gtk_notebook_append_page (GTK_NOTEBOOK (pwd_ui->stack), GTK_WIDGET (box_page_application), label_page_application);
    g_object_unref (label_page_application);

    gtk_button_set_label (GTK_BUTTON (pwd_ui->button_app), _("Apply application settings"));

    gtk_builder_add_from_file (builder, UI_PATH "/ui/page_connection.glade", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->url = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry1"));
    pwd_ui->base_dn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry2"));
    pwd_ui->button_conn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-button1"));

    /* page 2 */
    GObject *box_page_connection = gtk_builder_get_object (builder, "notebook-page-connection");
    gtk_widget_set_name (GTK_WIDGET (box_page_connection), "notebook-page-connection");

    GtkWidget *label1 = GTK_WIDGET(gtk_builder_get_object (builder, "page1-label1"));
    gtk_label_set_text (GTK_LABEL (label1), _("LDAP server address"));
    gtk_widget_set_tooltip_text (label1, _("Specifies the LDAP server address (e.g. ldap://dc1.domain.test.ru)"));

    GtkWidget *label2 = GTK_WIDGET(gtk_builder_get_object (builder, "page1-label2"));
    gtk_label_set_text (GTK_LABEL (label2), _("Search root"));
    gtk_widget_set_tooltip_text (label2, _("Specifies the search root for the desired record (e.g. 'dc=domain,dc=test,dc=ru')"));

    GtkWidget *label_page_connection = gtk_label_new (_("Connection"));
    gtk_notebook_append_page (GTK_NOTEBOOK (pwd_ui->stack), GTK_WIDGET (box_page_connection), label_page_connection);
    g_object_unref (label_page_connection);

    gtk_button_set_label (GTK_BUTTON (pwd_ui->button_conn), _("Apply connection settings"));

    gtk_box_append (GTK_BOX (main_container), pwd_ui->stack);
    gtk_overlay_set_child (GTK_OVERLAY (pwd_ui->toast_overlay), main_container);
    gtk_window_set_child (GTK_WINDOW (pwd_ui->window), pwd_ui->toast_overlay);
#endif

    g_signal_connect (G_OBJECT (pwd_ui->button_app), "clicked", G_CALLBACK (cb_button_app), pwd_ui);
    g_signal_connect (G_OBJECT (pwd_ui->button_conn), "clicked", G_CALLBACK (cb_button_conn), pwd_ui);

    g_settings_bind (pwd_ui->settings, "url", pwd_ui->url, "text", G_SETTINGS_BIND_GET);
    g_settings_bind (pwd_ui->settings, "base-dn", pwd_ui->base_dn, "text", G_SETTINGS_BIND_GET);

    g_settings_bind_with_mapping (pwd_ui->settings,
                                  "start-warning-time",
                                  pwd_ui->start_warning_time_days,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_x,
                                  NULL,
                                  pwd_ui,
                                  NULL);

    g_settings_bind_with_mapping (pwd_ui->settings,
                                  "warning-frequencies",
                                  pwd_ui->warning_frequencies_days,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_DAYS),
                                  NULL);

    g_settings_bind_with_mapping (pwd_ui->settings,
                                  "warning-frequencies",
                                  pwd_ui->warning_frequencies_hours,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_HOURS),
                                  NULL);

    g_settings_bind_with_mapping (pwd_ui->settings,
                                  "warning-frequencies",
                                  pwd_ui->warning_frequencies_min,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_MINS),
                                  NULL);                                

    gtk_window_set_default_size (GTK_WINDOW (pwd_ui->window), 600, 400);
    gtk_window_present (GTK_WINDOW (pwd_ui->window));

    g_object_unref (builder);
}

static void
on_quit_activate (GSimpleAction *action,
                  GVariant      *parametr,
                  gpointer       user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;
    g_application_quit (G_APPLICATION (pwd_ui->app));
}

static void
on_press_enter (GSimpleAction *action,
                GVariant      *parametr,
                gpointer       user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;

#ifdef USE_ADWAITA
    GtkWidget *stack_page =  adw_view_stack_get_visible_child (ADW_VIEW_STACK (pwd_ui->stack));
    const gchar* id_page = gtk_widget_get_name (stack_page);
#else
    gint stack_page_number = gtk_notebook_get_current_page (GTK_NOTEBOOK (pwd_ui->stack));
    gchar *id_page = NULL;
    if (stack_page_number == 0)
        id_page = "notebook-page-application";
    else
        id_page = "notebook-page-connection";
#endif

    if (g_strcmp0 (id_page, "notebook-page-connection") == 0) {
        g_signal_emit_by_name (pwd_ui->button_conn, "clicked", pwd_ui);
    } else if (g_strcmp0 (id_page, "notebook-page-application") == 0) {
        g_signal_emit_by_name (pwd_ui->button_app, "clicked", pwd_ui);
    }
}

static void
startup (GApplication *app,
         gpointer      user_data)
{
    static const GActionEntry actions[] = {
        { "quit", on_quit_activate, NULL, NULL, NULL },
        { "press_enter", on_press_enter, NULL, NULL, NULL}
    };

    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     actions,
                                     G_N_ELEMENTS (actions),
                                     user_data);

    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "app.quit",
                                           (const char *[]) { "Escape", NULL });

    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "app.press_enter",
                                           (const char *[]) { "Return", NULL});
}

int
main (int    argc,
      char **argv)
{
    setlocale (LC_ALL, "");
    bindtextdomain ("passwordchecker", "/usr/share/locale/");
    textdomain ("passwordchecker");

    GSettings *settings = NULL;
    int status;

    settings = g_settings_new (SCHEMA_NAME);
    if (!settings)
      g_printerr ("Unable to load gsettings schema\n");

    PasswordCheckerUI *pwdui = g_new (PasswordCheckerUI, 1);
    pwdui->settings = settings;

    pwdui->id = "org.altlinux.passwordchecker-settings";

#ifdef USE_ADWAITA
    pwdui->app = adw_application_new (pwdui->id, G_APPLICATION_FLAGS_NONE);
#else
    pwdui->app = gtk_application_new (pwdui->id, G_APPLICATION_FLAGS_NONE);
#endif

    g_signal_connect (pwdui->app, "startup", G_CALLBACK (startup), pwdui);
    g_signal_connect (pwdui->app, "activate", G_CALLBACK (activate), pwdui);
    
    status = g_application_run (G_APPLICATION (pwdui->app), argc, argv);
    
    cleanup (pwdui);

    return status;
}