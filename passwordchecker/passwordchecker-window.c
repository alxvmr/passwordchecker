#include "passwordchecker-window.h"

#ifdef USE_ADWAITA
    G_DEFINE_FINAL_TYPE (PasswordcheckerWindow, passwordchecker_window, ADW_TYPE_APPLICATION_WINDOW)
#else
    G_DEFINE_FINAL_TYPE (PasswordcheckerWindow, passwordchecker_window, GTK_TYPE_APPLICATION_WINDOW)
#endif

#define SCHEMA_NAME "org.altlinux.passwordchecker"

#define _(STRING) gettext(STRING)

enum {
    TO_MINS,
    TO_HOURS,
    TO_DAYS
};

static void
cleanup (PasswordcheckerWindow *self)
{
    // g_object_unref (pwd_ui->app);

    if (self->settings) {
        g_object_unref (self->settings);
    }

    g_free (self);
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

static gboolean
create_connection (GDBusConnection   **conn)
{
    GError *error = NULL;

    *conn = g_bus_get_sync (G_BUS_TYPE_SESSION,
                            NULL,
                            &error);
    if (error) {
        g_printerr ("Error connecting to D-Bus: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

static gboolean
check_enable_service (PasswordcheckerWindow *pwd_ui,
                      gboolean              *result)
{
    GVariant *parametrs = NULL;
    GVariant *reply = NULL;
    GDBusConnection *conn = NULL;
    GError *error = NULL;
    gchar *state = NULL;

    if (!create_connection (&conn)) {
        return FALSE;
    }

    parametrs = g_variant_new ("(s)",
                               "passwordchecker-user.service");

    reply = g_dbus_connection_call_sync (conn,
                                         "org.freedesktop.systemd1",
                                         "/org/freedesktop/systemd1",
                                         "org.freedesktop.systemd1.Manager",
                                         "GetUnitFileState",
                                         parametrs,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (reply == NULL) {
        g_printerr ("Error getting service states: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    g_variant_get (reply, "(s)", &state);
    if (g_strcmp0 (state, "disabled") == 0) {
        *result = FALSE;
    } else {
        if (g_strcmp0 (state, "enabled") == 0) {
            *result = TRUE;
        }
    }

    g_free (state);
    g_variant_unref (reply);
    g_object_unref (conn);

    return TRUE;
}

static gboolean
disable_service (PasswordcheckerWindow *pwd_ui)
{
    GVariant *disable_result = NULL;
    GVariant *stop_result = NULL;
    GDBusConnection *conn = NULL;
    GError *error = NULL;
    gchar *state = NULL;

    if (!create_connection (&conn)) {
        return FALSE;
    }

    stop_result = g_dbus_connection_call_sync (conn,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "StopUnit",
                                             g_variant_new ("(ss)", "passwordchecker-user.service", "replace"),
                                             (GVariantType *) "(o)",
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);

    if (!stop_result)
    {
      g_printerr ("Error stopping passwordchecker-user.service: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

    disable_result = g_dbus_connection_call_sync (conn,
                                                  "org.freedesktop.systemd1",
                                                  "/org/freedesktop/systemd1",
                                                  "org.freedesktop.systemd1.Manager",
                                                  "DisableUnitFiles",
                                                  g_variant_new ("(^asb)",
                                                              (gchar *[]) {"passwordchecker-user.service", NULL},
                                                              FALSE),
                                                              (GVariantType *) "(a(sss))",
                                                              G_DBUS_CALL_FLAGS_NONE,
                                                              -1,
                                                              NULL,
                                                              &error);

    if (disable_result == NULL) {
        g_printerr ("Service disable error: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

static gboolean
enable_service (PasswordcheckerWindow *pwd_ui)
{
    GVariant *start_result = NULL;
    GVariant *enable_result = NULL;
    GDBusConnection *conn = NULL;
    GError *error = NULL;
    gchar *state = NULL;

    if (!create_connection (&conn)) {
        return FALSE;
    }

    start_result = g_dbus_connection_call_sync (conn,
                                                "org.freedesktop.systemd1",
                                                "/org/freedesktop/systemd1",
                                                "org.freedesktop.systemd1.Manager",
                                                "StartUnit",
                                                g_variant_new ("(ss)",
                                                               "passwordchecker-user.service",
                                                               "replace"),
                                                (GVariantType *) "(o)",
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                &error);

  if (!start_result)
    {
      g_printerr ("Error starting passwordchecker-user.service %s", error->message);
      g_error_free (error);
      return FALSE;
    }

    enable_result = g_dbus_connection_call_sync (conn,
                                                 "org.freedesktop.systemd1",
                                                 "/org/freedesktop/systemd1",
                                                 "org.freedesktop.systemd1.Manager",
                                                 "EnableUnitFiles",
                                                 g_variant_new ("(^asbb)",
                                                                (gchar *[]) {"passwordchecker-user.service", NULL},
                                                                FALSE, TRUE),
                                                                (GVariantType *) "(ba(sss))",
                                                                G_DBUS_CALL_FLAGS_NONE,
                                                                -1,
                                                                NULL,
                                                                &error);

    if (enable_result == NULL) {
        g_printerr ("Service enable error: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

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
send_notification (const gchar           *body,
                   const gchar           *status,
                   PasswordcheckerWindow *pwd_ui)
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

static gboolean
cb_switch_row_activate (PasswordcheckerWindow *pwd_ui)
{
    gboolean active;
#ifdef USE_ADWAITA
    active = adw_switch_row_get_active (ADW_SWITCH_ROW (pwd_ui->switch_row));
#else
    active = gtk_switch_get_active (GTK_SWITCH (pwd_ui->switch_row));
#endif
    if (active) {
        return enable_service (pwd_ui);
        // dbus StartUnitFile ?
    } else {
        return disable_service (pwd_ui);
        // dbus DisableUnitFiles (false, true)
    }
}

static void
cb_button_conn (GtkWidget *button,
                gpointer   user_data)
{
    PasswordcheckerWindow *pwd_ui = (PasswordcheckerWindow *) user_data;

    const gchar *url_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->url));
    const gchar *base_dn_new = gtk_editable_get_text (GTK_EDITABLE (pwd_ui->base_dn));

    if (g_settings_set_string (pwd_ui->settings, "url", url_new) && 
        g_settings_set_string (pwd_ui->settings, "base-dn", base_dn_new))
    {
        send_notification (_("Connection settings have been successfully changed"), "success", pwd_ui);

        if (!g_settings_set_boolean (pwd_ui->settings, "change-conn-settings-by-user", TRUE)) {
            g_warning ("Could not change the change-conn-settings-by-user key\n");
        }

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
    PasswordcheckerWindow *pwd_ui = (PasswordcheckerWindow *) user_data;

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

    if (gint64_start_warning_time != -1 && gint64_warning_freq != -1 && cb_switch_row_activate (pwd_ui)) {
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
setup_passwordchecker_window (PasswordcheckerWindow *self)
{
    GSettings *settings = NULL;
    int status;

    settings = g_settings_new (SCHEMA_NAME);
    if (!settings)
      g_printerr ("Unable to load gsettings schema\n");

    self->settings = settings;

    g_settings_bind (self->settings, "url", self->url, "text", G_SETTINGS_BIND_GET);
    g_settings_bind (self->settings, "base-dn", self->base_dn, "text", G_SETTINGS_BIND_GET);

    g_settings_bind_with_mapping (self->settings,
                                  "start-warning-time",
                                  self->start_warning_time_days,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_x,
                                  NULL,
                                  self,
                                  NULL);

    g_settings_bind_with_mapping (self->settings,
                                  "warning-frequencies",
                                  self->warning_frequencies_days,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_DAYS),
                                  NULL);

    g_settings_bind_with_mapping (self->settings,
                                  "warning-frequencies",
                                  self->warning_frequencies_hours,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_HOURS),
                                  NULL);

    g_settings_bind_with_mapping (self->settings,
                                  "warning-frequencies",
                                  self->warning_frequencies_min,
                                  "text",
                                  G_SETTINGS_BIND_GET,
                                  convert_mins,
                                  NULL,
                                  GINT_TO_POINTER (TO_MINS),
                                  NULL);                                
}

static void
passwordchecker_window_class_init (PasswordcheckerWindowClass *class)
{
#ifdef USE_ADWAITA
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/altlinux/PasswordCheckerSettings/PasswordCheckerSettings-gnome.ui");
#else
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/altlinux/PasswordCheckerSettings/PasswordCheckerSettings-gtk.ui");
#endif

    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, toast_overlay);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, stack);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, error_start);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, error_freq);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, start_warning_time_days);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, warning_frequencies_days);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, warning_frequencies_hours);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, warning_frequencies_min);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, switch_row);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, url);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, base_dn);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, button_conn);
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, button_app);
#ifdef USE_ADWAITA
    gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PasswordcheckerWindow, menu);
#endif
}

static void
passwordchecker_window_init (PasswordcheckerWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    gtk_window_set_default_size (GTK_WINDOW (self), 600, 400);

    g_signal_connect (G_OBJECT (self->button_app), "clicked", G_CALLBACK (cb_button_app), self);
    g_signal_connect (G_OBJECT (self->button_conn), "clicked", G_CALLBACK (cb_button_conn), self);

    gtk_image_set_from_resource (GTK_IMAGE (self->error_start), "/org/altlinux/PasswordCheckerSettings/error.svg");
    gtk_image_set_from_resource (GTK_IMAGE (self->error_freq), "/org/altlinux/PasswordCheckerSettings/error.svg");

    gboolean is_enable_unit;
    check_enable_service (self, &is_enable_unit);
#ifdef USE_ADWAITA
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->switch_row), is_enable_unit);
    GMenu *menu = g_menu_new ();
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->menu), G_MENU_MODEL (menu));

    g_menu_append (menu, _("About"), "app.about");
    g_menu_append (menu, _("Quit"), "app.quit");
#else
    gtk_switch_set_active (GTK_SWITCH (self->switch_row), is_enable_unit);
#endif

#ifndef USE_ADWAITA
    self->notification = NULL;
#endif
    
    setup_passwordchecker_window (self);
}
