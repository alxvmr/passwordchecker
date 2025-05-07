#ifndef PASSWORDCHECKERWINDOW_H
#define PASSWORDCHECKERWINDOW_H
#ifdef USE_ADWAITA
    #include <adwaita.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <libintl.h>
#include <locale.h>

#define _(STRING) gettext(STRING)

G_BEGIN_DECLS

#define PASSWORDCHECKER_TYPE_WINDOW (passwordchecker_window_get_type ())

#ifdef USE_ADWAITA
    G_DECLARE_FINAL_TYPE (PasswordcheckerWindow, passwordchecker_window, PASSWORDCHECKER, WINDOW, AdwApplicationWindow)
#else
    G_DECLARE_FINAL_TYPE (PasswordcheckerWindow, passwordchecker_window, PASSWORDCHECKER, WINDOW, GtkApplicationWindow)
#endif

#ifndef USE_ADWAITA
    typedef struct _Notification_ui {
        GtkWidget *widget;
        guint timer_id;
    } Notification_ui;
#endif

typedef struct _PasswordcheckerWindow {
#ifdef USE_ADWAITA
    AdwApplicationWindow parent_instance;
#else
    GtkApplicationWindow parent_instance;
#endif

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
    GtkWidget *switch_row;

    GMenu *menu;

#ifndef USE_ADWAITA
    Notification_ui *notification;
#endif

    GtkWidget *stack;

    guint owner_id;

} PasswordcheckerWindow;

G_END_DECLS

#endif