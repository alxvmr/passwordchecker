#include "winbind-helper.h"

static struct wbcInterfaceDetails*
init_interface_details ()
{
    wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
    static struct wbcInterfaceDetails *details;

    if (details)
        return details;

    wbc_status = wbcInterfaceDetails(&details);
    if (!WBC_ERROR_IS_OK (wbc_status)) {
        g_printerr ("could not obtain winbind interface details: %s\n",
                    wbcErrorString (wbc_status));
    }

    return details;
}

static const char*
get_winbind_domain ()
{
    static struct wbcInterfaceDetails *details = NULL;

    details = init_interface_details ();

    if (!details) {
        g_printerr("could not obtain winbind domain name!\n");
        return 0;
    }

    return details->netbios_domain;
}

gboolean
get_dc_names (gchar      ***dc_names,
              size_t       *num_dcs)
{
    wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
    const gchar **dc_names_out;
    const gchar **dc_ips_out;
    size_t num_dcs_out;

    const gchar *domain_name = get_winbind_domain();

    wbc_status = wbcDcInfo (domain_name, &num_dcs_out, &dc_names_out, &dc_ips_out);

    if (!WBC_ERROR_IS_OK (wbc_status)) {
        g_printerr("Could not find dc info %s\n",
		           domain_name ? domain_name : "our domain");

        return FALSE;
    }

    *dc_names = g_new (gchar *, num_dcs_out+1);
    if (*dc_names == NULL) {
        g_printerr ("Failed to allocate memory for dc_names");
        wbcFreeMemory (dc_names_out);
        wbcFreeMemory (dc_ips_out);

        return FALSE;
    }

    for (gint i = 0; i < num_dcs_out; i++) {
        (*dc_names)[i] = g_strdup (dc_names_out[i]);
    }

    (*dc_names)[num_dcs_out] = NULL;
    *num_dcs = num_dcs_out;

    wbcFreeMemory (dc_names_out);
    wbcFreeMemory (dc_ips_out);

    return TRUE;
}

/*
    It's a bad temporary option
    I don't know how to get base_dn yet
    TODO: fixme
*/
gboolean
get_base_dn (gchar  *url,
             gchar **base_dn)
{
    gchar *pos = NULL;
    gchar **parts = NULL;
    gchar *url_copy = g_strdup (url);
    gchar *start_ptr = url_copy;

    pos = g_strstr_len (url_copy, -1, "://");
    if (pos != NULL) {
        url_copy = pos + 3;
    }

    parts = g_strsplit (url_copy, ".", -1);
    g_free (start_ptr);

    if (parts != NULL && parts[0] != NULL) {
        parts = parts + 1;

        for (int i = 0; parts[i] != NULL; i++){
            gchar *e = parts[i];
            parts[i] = g_strconcat ("dc=", parts[i], NULL);
            g_free (e);
        }

        *base_dn = g_strjoinv (",", parts);

        g_strfreev (parts-1);
    } else {
        return FALSE;
    }

    return TRUE;
}