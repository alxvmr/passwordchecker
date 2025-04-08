#ifndef WINDBINDHELPER_H
#define WINDBINDHELPER_H
#include <stdint.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <wbclient.h>

gboolean get_dc_names (gchar ***dc_names, size_t *num_dcs);
gboolean get_base_dn (gchar *url, gchar **base_dn);

#endif