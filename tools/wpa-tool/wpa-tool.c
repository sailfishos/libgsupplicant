/*
 * Copyright (C) 2015-2020 Jolla Ltd.
 * Copyright (C) 2015-2020 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gsupplicant.h"
#include "gsupplicant_bss.h"
#include "gsupplicant_network.h"
#include "gsupplicant_interface.h"

#include <gutil_log.h>
#include <gutil_strv.h>
#include <gutil_macros.h>

#include <stdlib.h>

#define RET_OK          (0)
#define RET_NOTFOUND    (1)
#define RET_ERR         (2)
#define RET_TIMEOUT     (3)

typedef struct app_action AppAction;
typedef GCancellable* (*AppActionRunFunc)(AppAction* action);
typedef void (*AppActionFreeFunc)(AppAction* action);

typedef struct app {
    gint timeout;
    GMainLoop* loop;
    GSupplicant* supplicant;
    GCancellable* call;
    AppAction* actions;
    gboolean dump_properties;
    gboolean list_interfaces;
    gboolean list_caps;
    gboolean list_eap_methods;
    gboolean follow_properties;
    gboolean pick_interface;
    gulong supplicant_valid_signal_id;
    char* iface_path;
    GSupplicantInterface* iface;
    gulong iface_valid_signal_id;
    gulong iface_prop_signal_id;
    char* bss_path;
    GSupplicantBSS* bss;
    gulong bss_valid_signal_id;
    gulong bss_prop_signal_id;
    char* network_path;
    GSupplicantNetwork* network;
    gulong network_valid_signal_id;
    gulong network_prop_signal_id;
    int ret;
} App;

struct app_action {
    AppAction* next;
    App* app;
    AppActionRunFunc fn_run;
    AppActionFreeFunc fn_free;
};

typedef struct app_action_str {
    AppAction action;
    char* str;
} AppActionStr;

typedef struct app_action_int {
    AppAction action;
    int value;
} AppActionInt;

#define dump_supplicant_caps(bits,d1,d2) \
    dump_bits(bits, gsupplicant_caps_name, d1, d2)
#define dump_supplicant_eap_methods(bits,d1,d2) \
    dump_bits(bits, gsupplicant_eap_method_name, d1, d2)
#define dump_cipher_suites(bits,d1,d2) \
    dump_bits(bits, gsupplicant_cipher_suite_name, d1, d2)
#define dump_keymgmt_suites(bits,d1,d2) \
    dump_bits(bits, gsupplicant_keymgmt_suite_name, d1, d2)

static
void
app_follow(
    App* app);

static
void
app_quit(
    App* app)
{
    g_idle_add((GSourceFunc)g_main_loop_quit, app->loop);
}

static
void
app_next_action(
    App* app)
{
    while (app->actions && !app->call) {
        AppAction* action = app->actions;
        app->actions = action->next;
        action->next = NULL;
        app->call = action->fn_run(action);
        if (!app->call) {
            action->fn_free(action);
        }
    }
    if (!app->call) {
        if (app->follow_properties) {
            app_follow(app);
        } else {
            app_quit(app);
        }
    }
}

static
void
app_action_call_done(
    AppAction* action,
    const GError* error)
{
    App* app = action->app;
    app->call = NULL;
    action->fn_free(action);
    if (error) {
        GERR("%s", error->message);
        app->ret = RET_ERR;
        app_quit(app);
    } else {
        app_next_action(app);
    }
}

static
void
app_add_action(
    App* app,
    AppAction* action)
{
    if (app->actions) {
        AppAction* last = app->actions;
        while (last->next) {
            last = last->next;
        }
        last->next = action;
    } else {
        app->actions = action;
    }
}

static
void
app_action_free(
    AppAction* action)
{
    g_free(action);
}

static
void
app_action_str_free(
    AppAction* action)
{
    AppActionStr* str_action = G_CAST(action, AppActionStr, action);
    g_free(str_action->str);
    g_free(str_action);
}

static
AppAction*
app_action_new(
    App* app,
    AppActionRunFunc run)
{
    AppAction* action = g_new0(AppAction, 1);
    action->app = app;
    action->fn_run = run;
    action->fn_free = app_action_free;
    return action;
}

static
AppAction*
app_action_str_new(
    App* app,
    AppActionRunFunc run,
    const char* str)
{
    AppActionStr* str_action = g_new0(AppActionStr, 1);
    AppAction* action = &str_action->action;
    action->app = app;
    action->fn_run = run;
    action->fn_free = app_action_str_free;
    str_action->str = g_strdup(str);
    return action;
}

static
AppAction*
app_action_int_new(
    App* app,
    AppActionRunFunc run,
    int value)
{
    AppActionInt* int_action = g_new0(AppActionInt, 1);
    AppAction* action = &int_action->action;
    action->app = app;
    action->fn_run = run;
    action->fn_free = app_action_free;
    int_action->value = value;
    return action;
}

static
void
dump_strv(
   const GStrV* strv,
   const char* d1,
   const char* d2)
{
    if (strv) {
        const char* delimiter = d1;
        while (*strv) {
            printf("%s%s", delimiter, *strv);
            delimiter = d2;
            strv++;
        }
    }
}

static
void
dump_bits(
   guint bits,
   const char* (*proc)(guint bits, guint* bit),
   const char* d1,
   const char* d2)
{
    guint bit;
    const char* name;
    const char* delimiter = d1;
    while ((name = proc(bits, &bit)) != NULL) {
        printf("%s%s", delimiter, name);
        delimiter = d2;
        bits &= ~bit;
    }
}

static
void
dump_bytes(
   GBytes* bytes,
   const char* d1,
   const char* d2)
{
    if (bytes) {
        gsize i, size = 0;
        const guint8* data = g_bytes_get_data(bytes, &size);
        const char* delimiter = d1;
        for (i=0; i<size; i++) {
            printf("%s%02x", delimiter, data[i]);
            delimiter = d2;
        }
    } else {
        printf("%s(null)", d1);
    }
}

static
void
dump_interface_property(
      GSupplicantInterface* iface,
      GSUPPLICANT_INTERFACE_PROPERTY property)
{
    switch (property) {
    case GSUPPLICANT_INTERFACE_PROPERTY_CAPS:
        printf("Caps: <not implemented>\n");
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_STATE:
        printf("State: %s\n", gsupplicant_interface_state_name(iface->state));
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_SCANNING:
        printf("Scanning: %s\n", iface->scanning ? "yes" : "no");
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_AP_SCAN:
        printf("ApScan: %u\n", iface->ap_scan);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_COUNTRY:
        printf("Country: %s\n", iface->country);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_DRIVER:
        printf("Driver: %s\n", iface->driver);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_IFNAME:
        printf("Ifname: %s\n", iface->ifname);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_BRIDGE_IFNAME:
        printf("BridgeIfname: %s\n", iface->bridge_ifname);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_CURRENT_BSS:
        printf("CurrentBSS: %s\n", iface->current_bss);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_CURRENT_NETWORK:
        printf("CurrentNetwork: %s\n", iface->current_network);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_SCAN_INTERVAL:
        printf("ScanInterval: %d\n", iface->scan_interval);
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_BSSS:
        printf("BSSs:");
        dump_strv(iface->bsss, " ", ",");
        printf("\n");
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_NETWORKS:
        printf("Networks:");
        dump_strv(iface->networks, " ", ",");
        printf("\n");
        break;
    case GSUPPLICANT_INTERFACE_PROPERTY_STATIONS:
        printf("Stations:");
        dump_strv(iface->stations, " ", ",");
        printf("\n");
        break;
    default:
        break;
    }
}

static
void
dump_bss_property(
      GSupplicantBSS* bss,
      GSUPPLICANT_BSS_PROPERTY property)
{
    switch (property) {
    case GSUPPLICANT_BSS_PROPERTY_SSID:
        printf("SSID: %s\n", bss->ssid_str);
        break;
    case GSUPPLICANT_BSS_PROPERTY_BSSID:
        dump_bytes(bss->bssid, "BSSID: ", ":");
        printf("\n");
        break;
    case GSUPPLICANT_BSS_PROPERTY_WPA:
        if (bss->wpa) {
            const GSupplicantBSSWPA* wpa = bss->wpa;
            printf("WPA:");
            if (wpa->keymgmt) {
                printf(" KeyMgmt(");
                dump_keymgmt_suites(wpa->keymgmt, "", ",");
                printf(")");
            }
            if (wpa->pairwise) {
                printf(" Pairwise(");
                dump_cipher_suites(wpa->pairwise, "", ",");
                printf(")");
            }
            if (wpa->group) {
                printf(" Group(");
                dump_cipher_suites(wpa->group, "", ",");
                printf(")");
            }
            printf("\n");
        }
        break;
    case GSUPPLICANT_BSS_PROPERTY_RSN:
        if (bss->rsn) {
            const GSupplicantBSSRSN* rsn = bss->rsn;
            printf("RSN:");
            if (rsn->keymgmt) {
                printf(" KeyMgmt(");
                dump_keymgmt_suites(rsn->keymgmt, "", ",");
                printf(")");
            }
            if (rsn->pairwise) {
                printf(" Pairwise(");
                dump_cipher_suites(rsn->pairwise, "", ",");
                printf(")");
            }
            if (rsn->group) {
                printf(" Group(");
                dump_cipher_suites(rsn->group, "", ",");
                printf(")");
            }
            if (rsn->mgmt_group) {
                printf(" MgmtGroup(");
                dump_cipher_suites(rsn->mgmt_group, "", ",");
                printf(")");
            }
            printf("\n");
        }
        break;
    case GSUPPLICANT_BSS_PROPERTY_IES:
        dump_bytes(bss->ies, "IEs: ", ":");
        printf("\n");
        break;
    case GSUPPLICANT_BSS_PROPERTY_PRIVACY:
        printf("Privacy: %s\n", bss->privacy ? "yes" : "no");
        break;
    case GSUPPLICANT_BSS_PROPERTY_MODE:
        printf("Mode: ");
        switch (bss->mode) {
        case GSUPPLICANT_BSS_MODE_INFRA:
            printf("infrastructure");
            break;
        case GSUPPLICANT_BSS_MODE_AD_HOC:
            printf("ad-hoc");
            break;
        default:
            printf("%d", bss->mode);
            break;
        }
        printf("\n");
        break;
    case GSUPPLICANT_BSS_PROPERTY_FREQUENCY:
        printf("Frequency: %u\n", bss->frequency);
        break;
    case GSUPPLICANT_BSS_PROPERTY_RATES:
        if (bss->rates) {
            guint i;
            printf("Rates: [");
            for (i=0; i<bss->rates->count; i++) {
                if (i > 0) printf(",");
                printf("%u", bss->rates->values[i]);
            }
            printf("]\n");
        }
        break;
    case GSUPPLICANT_BSS_PROPERTY_SIGNAL:
        printf("Signal: %d\n", bss->signal);
        break;
    default:
        break;
    }
}

static
void
dump_network_property(
      GSupplicantNetwork* network,
      GSUPPLICANT_NETWORK_PROPERTY property)
{
    switch (property) {
    case GSUPPLICANT_NETWORK_PROPERTY_ENABLED:
        printf("Enabled: %s\n", network->enabled ? "yes" : "no");
        break;
    case GSUPPLICANT_NETWORK_PROPERTY_PROPERTIES:
        if (network->properties) {
            const GStrV* ptr;
            GHashTable* p = network->properties;
            if (p) {
                char** keys = (char**)g_hash_table_get_keys_as_array(p, NULL);
                printf("Properties: %d key(s)\n", g_hash_table_size(p));
                gutil_strv_sort(keys, TRUE);
                for (ptr = keys; *ptr; ptr++) {
                    const char* key = *ptr;
                    const char* value = g_hash_table_lookup(p, key);
                    printf("  %s: %s\n", key, value);
                }
                g_free(keys);
            } else {
                printf("Properties: (null)\n");
            }
        }
        break;
    default:
        break;
    }
}

static
void
app_action_generic_result(
    GSupplicant* supplicant,
    GCancellable* cancel,
    const GError* error,
    void* data)
{
    app_action_call_done(data, error);
}

static
void
app_action_generic_interface_result(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GError* error,
    void* data)
{
    app_action_call_done(data, error);
}

static
void
app_action_string_result(
    GSupplicant* supplicant,
    GCancellable* cancel,
    const GError* error,
    const char* str,
    void* data)
{
    AppAction* action = data;
    if (!error) {
        printf("%s\n", str);
    }
    app_action_call_done(action, error);
}

static
void
app_action_signal_poll_result(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GError* error,
    const GSupplicantSignalPoll* info,
    void* data)
{
    AppAction* action = data;
    if (info) {
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_LINKSPEED) {
            printf("linkspeed: %d\n", info->linkspeed);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_NOISE) {
            printf("noise: %d\n", info->noise);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_FREQUENCY) {
            printf("frequency: %u\n", info->frequency);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_RSSI) {
            printf("rssi: %d\n", info->rssi);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_AVG_RSSI) {
            printf("avg_rssi: %d\n", info->avg_rssi);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ1) {
            printf("center_frq1: %d\n", info->center_frq1);
        }
        if (info->flags & GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ2) {
            printf("center_frq2: %d\n", info->center_frq2);
        }
    }
    app_action_call_done(action, error);
}

static
GCancellable*
app_action_signal_poll(
    AppAction* action)
{
    App* app = action->app;
    return gsupplicant_interface_signal_poll(app->iface,
        app_action_signal_poll_result, action);
}

static
GCancellable*
app_action_ap_scan(
    AppAction* action)
{
    App* app = action->app;
    AppActionInt* int_action = G_CAST(action,AppActionInt,action);
    GDEBUG("Settings ap_scan to %d", int_action->value);
    gsupplicant_interface_set_ap_scan(app->iface, int_action->value);
    return NULL;
}

static
GCancellable*
app_action_country(
    AppAction* action)
{
    App* app = action->app;
    AppActionStr* str_action = G_CAST(action,AppActionStr,action);
    GDEBUG("Settings country to %s", str_action->str);
    gsupplicant_interface_set_country(app->iface, str_action->str);
    return NULL;
}

static
GCancellable*
app_action_create_interface(
    AppAction* action)
{
    App* app = action->app;
    AppActionStr* sa = G_CAST(action,AppActionStr,action);
    GSupplicantCreateInterfaceParams params;
    memset(&params, 0, sizeof(params));
    params.ifname = sa->str;
    return gsupplicant_create_interface(app->supplicant, &params,
        app_action_string_result, sa);
}

static
GCancellable*
app_action_get_interface(
    AppAction* action)
{
    App* app = action->app;
    AppActionStr* sa = G_CAST(action,AppActionStr,action);
    return gsupplicant_get_interface(app->supplicant, sa->str,
        app_action_string_result, sa);
}

static
GCancellable*
app_action_remove_interface(
    AppAction* action)
{
    App* app = action->app;
    AppActionStr* sa = G_CAST(action,AppActionStr,action);
    return gsupplicant_remove_interface(app->supplicant, sa->str,
        app_action_generic_result, action);
}

static
GCancellable*
app_action_passive_scan(
    AppAction* action)
{
    App* app = action->app;
    GDEBUG("Doing passive scan");
    return gsupplicant_interface_scan(app->iface, NULL,
        app_action_generic_interface_result, action);
}

static
GCancellable*
app_action_active_scan(
    AppAction* action)
{
    App* app = action->app;
    AppActionStr* str_action = G_CAST(action,AppActionStr,action);
    GSupplicantScanParams params;
    GCancellable* call;
    GBytes* ssid = g_bytes_new(str_action->str, strlen(str_action->str));
    GBytes* ssids[2];
    ssids[0] = ssid;
    ssids[1] = NULL;
    GDEBUG("Doing active scan for %s", str_action->str);
    memset(&params, 0, sizeof(params));
    params.type = GSUPPLICANT_SCAN_TYPE_ACTIVE;
    params.ssids = ssids;
    call = gsupplicant_interface_scan(app->iface, &params,
        app_action_generic_interface_result, action);
    g_bytes_unref(ssid);
    return call;
}

static
GCancellable*
app_action_dump_properties(
    AppAction* action)
{
    App* app = action->app;
    if (app->iface) {
        if (app->iface->present) {
            GSUPPLICANT_INTERFACE_PROPERTY p;
            for (p = GSUPPLICANT_INTERFACE_PROPERTY_ANY+1;
                 p < GSUPPLICANT_INTERFACE_PROPERTY_COUNT;
                 p++) {
                dump_interface_property(app->iface, p);
            }
        } else {
            printf("%s is not present\n", app->iface->path);
        }
    }
    if (app->bss) {
        if (app->bss->present) {
            GSUPPLICANT_BSS_PROPERTY p;
            for (p = GSUPPLICANT_BSS_PROPERTY_ANY+1;
                 p < GSUPPLICANT_BSS_PROPERTY_COUNT;
                 p++) {
                dump_bss_property(app->bss, p);
            }
        } else {
            printf("%s is not present\n", app->bss->path);
        }
    }
    if (app->network) {
        if (app->network->present) {
            GSUPPLICANT_NETWORK_PROPERTY p;
            for (p = GSUPPLICANT_NETWORK_PROPERTY_ANY+1;
                 p < GSUPPLICANT_NETWORK_PROPERTY_COUNT;
                 p++) {
                dump_network_property(app->network, p);
            }
        } else {
            printf("%s is not present\n", app->network->path);
        }
    }
    return NULL;
}

static
void
interface_property_changed(
    GSupplicantInterface* iface,
    GSUPPLICANT_INTERFACE_PROPERTY property,
    void* arg)
{
    dump_interface_property(iface, property);
}

static
void
interface_invalid_exit(
    GSupplicantInterface* iface,
    void* arg)
{
    if (!iface->valid) {
        App* app = arg;
        gsupplicant_interface_remove_handler(iface, app->iface_valid_signal_id);
        app->iface_valid_signal_id = 0;
        GDEBUG("Interface %s is invalid, exiting...", iface->path);
        g_main_loop_quit(app->loop);
    }
}

static
void
bss_property_changed(
    GSupplicantBSS* bss,
    GSUPPLICANT_BSS_PROPERTY property,
    void* arg)
{
    dump_bss_property(bss, property);
}

static
void
bss_invalid_exit(
    GSupplicantBSS* bss,
    void* arg)
{
    if (!bss->valid) {
        App* app = arg;
        gsupplicant_bss_remove_handlers(bss, &app->bss_valid_signal_id, 1);
        GDEBUG("BSS %s is invalid, exiting...", bss->path);
        g_main_loop_quit(app->loop);
    }
}

static
void
network_property_changed(
    GSupplicantNetwork* bss,
    GSUPPLICANT_NETWORK_PROPERTY property,
    void* arg)
{
    dump_network_property(bss, property);
}

static
void
network_invalid_exit(
    GSupplicantNetwork* network,
    void* arg)
{
    if (!network->valid) {
        App* app = arg;
        gsupplicant_network_remove_handlers(network,
            &app->network_valid_signal_id, 1);
        GDEBUG("Network %s is invalid, exiting...", network->path);
        g_main_loop_quit(app->loop);
    }
}

static
void
app_follow(
    App* app)
{
    if (app->iface) {
        app->iface_valid_signal_id =
            gsupplicant_interface_add_handler(app->iface,
                GSUPPLICANT_INTERFACE_PROPERTY_VALID,
                interface_invalid_exit, app);
        app->iface_prop_signal_id =
            gsupplicant_interface_add_property_changed_handler(app->iface,
                GSUPPLICANT_INTERFACE_PROPERTY_ANY,
                interface_property_changed, app);
    }
    if (app->bss) {
        app->bss_valid_signal_id =
            gsupplicant_bss_add_handler(app->bss,
                GSUPPLICANT_BSS_PROPERTY_VALID,
                bss_invalid_exit, app);
        app->bss_prop_signal_id =
            gsupplicant_bss_add_property_changed_handler(app->bss,
                GSUPPLICANT_BSS_PROPERTY_ANY,
                bss_property_changed, app);
    }
    if (app->network) {
        app->network_valid_signal_id =
            gsupplicant_network_add_handler(app->network,
                GSUPPLICANT_NETWORK_PROPERTY_VALID,
                network_invalid_exit, app);
        app->network_prop_signal_id =
            gsupplicant_network_add_property_changed_handler(app->network,
                GSUPPLICANT_NETWORK_PROPERTY_ANY,
                network_property_changed, app);
    }
}

static
void
interface_valid_handler(
    GSupplicantInterface* iface,
    void* arg)
{
    if (iface->valid) {
        App* app = arg;
        gsupplicant_interface_remove_handlers(iface,
            &app->iface_valid_signal_id, 1);
        app->iface_valid_signal_id = 0;
        app_next_action(app);
    }
}

static
void
bss_valid_handler(
    GSupplicantBSS* bss,
    void* arg)
{
    if (bss->valid) {
        App* app = arg;
        gsupplicant_bss_remove_handlers(bss, &app->bss_valid_signal_id, 1);
        app_next_action(app);
    }
}

static
void
network_valid_handler(
    GSupplicantNetwork* network,
    void* arg)
{
    if (network->valid) {
        App* app = arg;
        gsupplicant_network_remove_handlers(network,
            &app->network_valid_signal_id, 1);
        app_next_action(app);
    }
}

static
void
supplicant_valid(
    App* app)
{
    GSupplicant* sup = app->supplicant;
    GDEBUG("Supplicant is running");
    if (sup->failed) {
        GERR("Not authorized?");
        app->ret = RET_ERR;
    } else {
        app->ret = RET_OK;
        if (app->pick_interface) {
            if (sup->interfaces && sup->interfaces[0]) {
                g_free(app->iface_path);
                app->iface_path = g_strdup(sup->interfaces[0]);
                GDEBUG("Picked %s", app->iface_path);
            }
        }
        if (app->iface_path) {
            if (gutil_strv_contains(sup->interfaces, app->iface_path)) {
                app->iface = gsupplicant_interface_new(app->iface_path);
                if (app->iface->valid) {
                    app_next_action(app);
                } else {
                    GDEBUG("Waiting for %s", app->iface_path);
                    app->iface_valid_signal_id =
                        gsupplicant_interface_add_handler(app->iface,
                            GSUPPLICANT_INTERFACE_PROPERTY_VALID,
                            interface_valid_handler, app);
                }
            } else {
                GERR("Interface %s not found", app->iface_path);
                app->ret = RET_NOTFOUND;
            }
        }
        if (app->bss_path) {
            app->bss = gsupplicant_bss_new(app->bss_path);
            if (app->bss->valid) {
                app_next_action(app);
            } else {
                GDEBUG("Waiting for %s", app->bss_path);
                app->bss_valid_signal_id =
                    gsupplicant_bss_add_handler(app->bss,
                        GSUPPLICANT_BSS_PROPERTY_VALID,
                        bss_valid_handler, app);
            }
        }
        if (app->network_path) {
            app->network = gsupplicant_network_new(app->network_path);
            if (app->network->valid) {
                app_next_action(app);
            } else {
                GDEBUG("Waiting for %s", app->network_path);
                app->network_valid_signal_id =
                    gsupplicant_network_add_handler(app->network,
                        GSUPPLICANT_NETWORK_PROPERTY_VALID,
                        network_valid_handler, app);
            }
        }
        if (!app->iface_path && !app->bss_path && !app->network_path) {
            if (app->dump_properties) {
                printf("Interfaces:");
                if (sup->interfaces) {
                    const char* delimiter = " ";
                    char* const* ptr = sup->interfaces;
                    while (*ptr) {
                        printf("%s%s", delimiter, *ptr++);
                        delimiter = ",";
                    }
                }
                printf("\nCapabilities:");
                dump_supplicant_caps(sup->caps, " ", ",");
                printf("\nEAP Methods:");
                dump_supplicant_eap_methods(sup->eap_methods, " ", ",");
                printf("\n");
            } else {
                if (app->list_interfaces) {
                    GDEBUG("Interfaces:");
                    if (sup->interfaces) {
                        char* const* ptr = sup->interfaces;
                        while (*ptr) printf("%s\n", *ptr++);
                    }
                }
                if (app->list_caps) {
                    GDEBUG("Capabilities:");
                    dump_supplicant_caps(sup->caps, "", "\n");
                }
                if (app->list_eap_methods) {
                    GDEBUG("EAP Methods:");
                    dump_supplicant_eap_methods(sup->eap_methods, "", "\n");
                }
            }
        }
    }
    if (!app->iface_valid_signal_id &&
        !app->bss_valid_signal_id &&
        !app->network_valid_signal_id) {
        if (app->actions) {
            app_next_action(app);
        } else {
            g_main_loop_quit(app->loop);
        }
    }
}

static
void
supplicant_valid_handler(
    GSupplicant* supplicant,
    void* arg)
{
    if (supplicant->valid) {
        App* app = arg;
        gsupplicant_remove_handler(supplicant, app->supplicant_valid_signal_id);
        app->supplicant_valid_signal_id = 0;
        supplicant_valid(app);
    }
}

static
int
app_run(
    App* app)
{
    app->supplicant = gsupplicant_new();
    app->loop = g_main_loop_new(NULL, FALSE);
    if (app->timeout > 0) GDEBUG("Timeout %d sec", app->timeout);
    if (app->supplicant->valid) {
        supplicant_valid(app);
    } else {
        app->supplicant_valid_signal_id =
            gsupplicant_add_handler(app->supplicant,
                GSUPPLICANT_PROPERTY_VALID, supplicant_valid_handler, app);
    }
    g_main_loop_run(app->loop);
    gsupplicant_remove_handler(app->supplicant,
        app->supplicant_valid_signal_id);
    if (app->iface) {
        gsupplicant_interface_remove_handler(app->iface,
                app->iface_valid_signal_id);
        gsupplicant_interface_remove_handler(app->iface,
                app->iface_prop_signal_id);
        gsupplicant_interface_unref(app->iface);
        }
    if (app->bss) {
        gsupplicant_bss_remove_handler(app->bss,
            app->bss_valid_signal_id);
        gsupplicant_bss_remove_handler(app->bss,
            app->bss_prop_signal_id);
        gsupplicant_bss_unref(app->bss);
    }
    if (app->network) {
        gsupplicant_network_remove_handler(app->network,
            app->network_valid_signal_id);
        gsupplicant_network_remove_handler(app->network,
            app->network_prop_signal_id);
        gsupplicant_network_unref(app->network);
    }
    g_main_loop_unref(app->loop);
    gsupplicant_unref(app->supplicant);
    return app->ret;
}

static
gboolean
app_log_verbose(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_VERBOSE;
    return TRUE;
}

static
gboolean
app_log_quiet(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_ERR;
    return TRUE;
}

static
gboolean
app_option_signal_poll(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_new(app, app_action_signal_poll));
    return TRUE;
}

static
gboolean
app_option_country(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_str_new(app, app_action_country, value));
    return TRUE;
}

static
gboolean
app_option_ap_scan(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    if (value) {
        const int ap_scan = atoi(value);
        if (ap_scan >= 0) {
            app_add_action(app, app_action_int_new(app, app_action_ap_scan,
                ap_scan));
            return TRUE;
        } else {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "Invalid ap_scan value \'%s\'", value);
        }
    } else {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
            "Missing ap_scan value");
    }
    return FALSE;
}

static
gboolean
app_option_passive_scan(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_new(app, app_action_passive_scan));
    return TRUE;
}

static
gboolean
app_option_active_scan(
    const gchar* name,
    const gchar* ssid,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_str_new(app, app_action_active_scan, ssid));
    return TRUE;
}

static
gboolean
app_option_create_interface(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_str_new(app,
        app_action_create_interface, value));
    return TRUE;
}

static
gboolean
app_option_get_interface(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_str_new(app,
        app_action_get_interface, value));
    return TRUE;
}

static
gboolean
app_option_remove_interface(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app_add_action(app, app_action_str_new(app,
        app_action_remove_interface, value));
    return TRUE;
}

static
gboolean
app_option_dump_properties(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    App* app = data;
    app->dump_properties = TRUE;
    app_add_action(app, app_action_new(app, app_action_dump_properties));
    return TRUE;
}

static
gboolean
app_init(
    App* app,
    int argc,
    char* argv[])
{
    gboolean ok = FALSE;
    GOptionEntry entries[] = {
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_log_verbose, "Enable verbose output", NULL },
        { "quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_log_quiet, "Be quiet", NULL },
        { "timeout", 't', 0, G_OPTION_ARG_INT,
          &app->timeout, "Timeout in seconds", "SEC" },
        { "list", 'l', 0, G_OPTION_ARG_NONE, &app->list_interfaces,
          "List interfaces", NULL },
        { "interface", 'i', 0, G_OPTION_ARG_STRING, &app->iface_path,
          "Select interface", "PATH" },
        { "bss", 'b', 0, G_OPTION_ARG_STRING, &app->bss_path,
          "Select BSS", "PATH" },
        { "network", 'n', 0, G_OPTION_ARG_STRING, &app->network_path,
          "Select network", "PATH" },
        { "capabilities", 'c', 0, G_OPTION_ARG_NONE, &app->list_caps,
          "List capabilities", NULL },
        { "eap-methods", 'm', 0, G_OPTION_ARG_NONE, &app->list_eap_methods,
          "List EAP methods", NULL },
        { "follow", 'f', 0, G_OPTION_ARG_NONE, &app->follow_properties,
          "Follow property changes", NULL },
        { NULL }
    };
    GOptionEntry interface_entries[] = {
        { "properties", 'p', G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_IN_MAIN,
          G_OPTION_ARG_CALLBACK, app_option_dump_properties,
          "Dump properties of the selected object", NULL },
        { "pick-interface", 'I', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
          &app->pick_interface, "Pick the first available interface", NULL },
        { "create-interface", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_CALLBACK,
          app_option_create_interface, "Create interface for IFNAME",
          "IFNAME" },
        { "get-interface", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_CALLBACK,
          app_option_get_interface, "Get interface path for the IFNAME",
          "IFNAME" },
        { "remove-interface", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_CALLBACK,
          app_option_remove_interface, "Remove interface", "PATH" },
        { "signal-poll", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_option_signal_poll, "Show signal poll values", NULL },
        { "ap-scan", 0, 0, G_OPTION_ARG_CALLBACK,
          app_option_ap_scan, "Set ap_scan parameter", NULL },
        { "passive-scan", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_option_passive_scan, "Perform passive scan", NULL },
        { "active-scan", 0, 0, G_OPTION_ARG_CALLBACK,
          app_option_active_scan, "Perform active scan for SSID", "SSID" },
        { "country", 0, 0, G_OPTION_ARG_CALLBACK,
          app_option_country, "Set the country", "COUNTRY" },
        { NULL }
    };
    GError* error = NULL;
    GOptionContext* options = g_option_context_new(NULL);
    GOptionGroup* interface_group = g_option_group_new("interface",
        "Interface Options:", "Show interface options", app, NULL);
    g_option_context_add_main_entries(options, entries, NULL);
    g_option_group_add_entries(interface_group, interface_entries);
    g_option_context_add_group(options, interface_group);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (argc == 1 && !(app->bss_path && app->iface_path)) {
            ok = TRUE;
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            fprintf(stderr, "%s", help);
            g_free(help);
        }
    } else {
        GERR("%s", error->message);
        g_error_free(error);
    }
    g_option_context_free(options);
    return ok;
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    App app;
    memset(&app, 0, sizeof(app));
    app.timeout = -1;
    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDERR, "wpa-tool");
    gutil_log_default.level = GLOG_LEVEL_DEFAULT;
    if (app_init(&app, argc, argv)) {
        ret = app_run(&app);
    }
    g_free(app.iface_path);
    g_free(app.bss_path);
    g_free(app.network_path);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
