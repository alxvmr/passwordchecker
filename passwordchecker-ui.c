#include <gtk/gtk.h>

#define SCHEMA_NAME "org.altlinux.passwordchecker"

typedef struct _PasswordcheckerUI {
    GtkApplication *app;
    const gchar *id;
    GSettings *settings;
    GtkWidget *url;
    GtkWidget *base_dn;
    GtkWidget *start_warning_time_days;
    GtkWidget *warning_frequencies_days;
    GtkWidget *warning_frequencies_hours;
    GtkWidget *warning_frequencies_min;
} PasswordCheckerUI;

enum {
    TO_MINS,
    TO_HOURS,
    TO_DAYS
};

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

static void
send_notification (const gchar       *title,
                   const gchar       *body,
                   PasswordCheckerUI *pwd_ui)
{
    GNotification *notification = g_notification_new (title);
    g_notification_set_body (notification, body);

    g_application_send_notification (G_APPLICATION (pwd_ui->app), pwd_ui->id, notification);

    g_object_unref (notification);
}

static void
cb_button_conn (GtkWidget *button,
                gpointer   user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;

    const gchar *url_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->url));
    const gchar *base_dn_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->base_dn));

    g_settings_set_string (pwd_ui->settings, "url", url_new);
    g_settings_set_string (pwd_ui->settings, "base-dn", base_dn_new);

    send_notification ("PasswordChecker: change settings", "Connection settings have been successfully changed", pwd_ui);
}

static gint64
convert_str_2_gint64 (gchar  *str,
                      gint64 *out)
{
    gchar *end_ptr = NULL;

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

    const gchar *start_warning_time_days = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->start_warning_time_days));

    const gchar *warning_freq_mins = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_min));
    const gchar *warning_freq_hours = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_hours));
    const gchar *warning_freq_days = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->warning_frequencies_days));

    gint64 freq_min;
    gint64 freq_hours;
    gint64 freq_days;
    gint64 gint64_start_warning_time = -1;
    gint64 gint64_warning_freq = -1;

    if (! convert_str_2_gint64 (g_strdup (start_warning_time_days), &gint64_start_warning_time)) {
        gint64_start_warning_time = -1;
    }

    if (convert_str_2_gint64 (g_strdup (warning_freq_mins), &freq_min)) {
        if (convert_str_2_gint64 (g_strdup (warning_freq_hours), &freq_hours)) {
            freq_hours *= 60;
            if (convert_str_2_gint64 (g_strdup (warning_freq_days), &freq_days)) {
                freq_days *= 1440;
                gint64_warning_freq = freq_min + freq_hours + freq_days;
            }
        }
    }

    if (gint64_start_warning_time != -1 && gint64_warning_freq != -1) {
        g_settings_set_int64 (pwd_ui->settings, "start-warning-time", gint64_start_warning_time);
        g_settings_set_int64 (pwd_ui->settings, "warning-frequencies", gint64_warning_freq);

        send_notification ("PasswordChecker: change settings", "Application settings have been successfully changed", pwd_ui);
    }
    else {
        send_notification ("PasswordChecker: change settings", "The application settings have not been applied.\nAn error occurred while saving the settings", pwd_ui);
    }
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    PasswordCheckerUI *pwd_ui = (PasswordCheckerUI *) user_data;
    GtkWidget *window = NULL;
    GError *error = NULL;

    GSettingsSchema *schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), SCHEMA_NAME, TRUE);
    GSettingsSchemaKey *key_url = g_settings_schema_get_key (schema, "url");
    GSettingsSchemaKey *key_base_dn = g_settings_schema_get_key (schema, "base-dn");
    GSettingsSchemaKey *key_start = g_settings_schema_get_key (schema, "start-warning-time");
    GSettingsSchemaKey *key_freq = g_settings_schema_get_key (schema, "warning-frequencies");

    GtkBuilder *builder = gtk_builder_new ();
    gtk_builder_add_from_file (builder, "/home/SMB.BASEALT.RU/alekseevamo/Develop/passwordchecker/data/ui/page_connection.glade", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->url = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry1"));
    pwd_ui->base_dn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-entry2"));
    GtkWidget *button_conn = GTK_WIDGET (gtk_builder_get_object (builder, "page1-button1"));

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "PasswordCheckerSettings");
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

    GtkWidget *main_container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *notebook = gtk_notebook_new ();

    /* page 1 */
    GObject *box_page_connection = gtk_builder_get_object (builder, "notebook-page-connection");

    GtkWidget *label1 = GTK_WIDGET(gtk_builder_get_object (builder, "page1-label1"));
    gtk_label_set_text (GTK_LABEL (label1), g_settings_schema_key_get_summary (key_url));
    gtk_widget_set_tooltip_text (label1, g_settings_schema_key_get_description (key_url));

    GtkWidget *label2 = GTK_WIDGET(gtk_builder_get_object (builder, "page1-label2"));
    gtk_label_set_text (GTK_LABEL (label2), g_settings_schema_key_get_summary (key_base_dn));
    gtk_widget_set_tooltip_text (label2, g_settings_schema_key_get_description (key_base_dn));

    GtkWidget *label_page_connection = gtk_label_new ("Connection");
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), GTK_WIDGET (box_page_connection), label_page_connection);

    gtk_button_set_label (GTK_BUTTON (button_conn), "Apply connection settings");
    g_signal_connect (G_OBJECT (button_conn), "clicked", G_CALLBACK (cb_button_conn), pwd_ui);

    g_settings_bind (pwd_ui->settings, "url", pwd_ui->url, "text", G_SETTINGS_BIND_GET);
    g_settings_bind (pwd_ui->settings, "base-dn", pwd_ui->base_dn, "text", G_SETTINGS_BIND_GET);

    /* page 2 */
    gtk_builder_add_from_file (builder, "/home/SMB.BASEALT.RU/alekseevamo/Develop/passwordchecker/data/ui/page_application.glade", &error);
    if (error){
        g_printerr("Error loading Glade file: %s\n", error->message);
        g_clear_error(&error);
        return;
    }

    pwd_ui->start_warning_time_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry1-days"));
    pwd_ui->warning_frequencies_days = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-days"));
    pwd_ui->warning_frequencies_hours = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-hours"));
    pwd_ui->warning_frequencies_min = GTK_WIDGET (gtk_builder_get_object (builder, "page2-entry2-min"));

    GtkWidget *button_app = GTK_WIDGET (gtk_builder_get_object (builder, "page2-button1"));

    GtkWidget *label_start = GTK_WIDGET(gtk_builder_get_object (builder, "page2-label1"));
    gtk_label_set_text (GTK_LABEL (label_start), g_settings_schema_key_get_summary (key_start));
    gtk_widget_set_tooltip_text (label_start, g_settings_schema_key_get_description (key_start));

    GtkWidget *label_freq = GTK_WIDGET(gtk_builder_get_object (builder, "page2-label2"));
    gtk_label_set_text (GTK_LABEL (label_freq), g_settings_schema_key_get_summary (key_freq));
    gtk_widget_set_tooltip_text (label_freq, g_settings_schema_key_get_description (key_freq));

    GObject *box_page_application = gtk_builder_get_object (builder, "notebook-page-application");
    GtkWidget *label_page_application = gtk_label_new ("Application");
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), GTK_WIDGET (box_page_application), label_page_application);

    gtk_button_set_label (GTK_BUTTON (button_app), "Apply application settings");
    g_signal_connect (G_OBJECT (button_app), "clicked", G_CALLBACK (cb_button_app), pwd_ui);

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
  
    gtk_box_append (GTK_BOX (main_container), notebook);
    gtk_window_set_child (GTK_WINDOW (window), main_container);
    gtk_window_present (GTK_WINDOW (window));

    g_object_unref (builder);
    g_settings_schema_key_unref (key_url);
    g_settings_schema_key_unref (key_base_dn);
    g_settings_schema_unref (schema);
}

int
main (int    argc,
      char **argv)
{
    GSettings *settings = NULL;
    int status;

    settings = g_settings_new (SCHEMA_NAME);
    if (!settings)
      g_printerr ("Unable to load gsettings schema\n");

    PasswordCheckerUI *pwdui = g_new (PasswordCheckerUI, 1);
    pwdui->settings = settings;

    pwdui->id = "org.altlinux.passwordchecker";

    pwdui->app = gtk_application_new (pwdui->id, G_APPLICATION_FLAGS_NONE);
    g_signal_connect (pwdui->app, "activate", G_CALLBACK (activate), pwdui);
    status = g_application_run (G_APPLICATION (pwdui->app), argc, argv);
    g_object_unref (pwdui->app);

    return status;
}