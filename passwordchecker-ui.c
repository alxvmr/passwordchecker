#include <gtk/gtk.h>

#define SCHEMA_NAME "org.altlinux.passwordchecker"

typedef struct _PasswordcheckerUI {
    GtkApplication *app;
    const gchar *id;
    GSettings *settings;
    GtkWidget *url;
    GtkWidget *base_dn;
} PasswordCheckerUI;

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

    /* page 1*/
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