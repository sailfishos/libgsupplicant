/*
 * Copyright (C) 2015-2018 Jolla Ltd.
 * Copyright (C) 2015-2018 Slava Monich <slava.monich@jolla.com>
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
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
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

#include "gsupplicant_interface.h"
#include "gsupplicant_network.h"
#include "gsupplicant_bss.h"
#include "gsupplicant.h"
#include "gsupplicant_util_p.h"
#include "gsupplicant_dbus.h"
#include "gsupplicant_log.h"
#include "gsupplicant_error.h"

#include <gutil_strv.h>
#include <gutil_misc.h>

/* Generated headers */
#include "fi.w1.wpa_supplicant1.Interface.h"
#include "fi.w1.wpa_supplicant1.Interface.WPS.h"

/* Constants */
#define WPS_DEFAULT_CONNECT_TIMEOUT_SEC (30)

/* Internal data structures */
typedef union gsupplicant_interface_call_func_union {
    GCallback cb;
    GSupplicantInterfaceResultFunc fn_void;
    GSupplicantInterfaceStringResultFunc fn_string;
    GSupplicantInterfaceSignalPollResultFunc fn_signal_poll;
} GSupplicantInterfaceCallFuncUnion;

typedef struct gsupplicant_interface_call GSupplicantInterfaceCall;

typedef
void
(*GSupplicantInterfaceCallFinishFunc)(
    GSupplicantInterfaceCall* call,
    GAsyncResult* result);

struct gsupplicant_interface_call {
    GSupplicantInterface* iface;
    GCancellable* cancel;
    GSupplicantInterfaceCallFinishFunc finish;
    GSupplicantInterfaceCallFuncUnion fn;
    GDestroyNotify destroy;
    void* data;
};

enum add_network_handler_id {
    ADD_NETWORK_VALID_CHANGED,
    ADD_NETWORK_ENABLED_CHANGED,
    ADD_NETWORK_HANDLER_COUNT
};

typedef struct gsupplicant_interface_add_network_call {
    GSupplicantInterface* iface;
    GSupplicantNetwork* network;
    gulong network_event_id[ADD_NETWORK_HANDLER_COUNT];
    GCancellable* cancel;
    gulong cancel_id;
    GVariant* args;
    GHashTable* blobs;
    GHashTableIter iter;
    gboolean pending;
    GSupplicantInterfaceStringResultFunc fn;
    GDestroyNotify destroy;
    void* data;
    guint flags;
    char* path;
} GSupplicantInterfaceAddNetworkCall;

enum gsupplicant_wps_proxy_handler_id {
    WPS_PROXY_EVENT,
    WPS_PROXY_CREDENTIALS,
    WPS_PROXY_HANDLER_COUNT
};

typedef enum gsupplicant_wps_connect_status {
    WPS_CONNECT_PENDING,
    WPS_CONNECT_SUCCESS,
    WPS_CONNECT_FAIL,
    WPS_CONNECT_M2D,
    WPS_CONNECT_PBC_OVERLAP
} WPS_CONNECT_STATE;

typedef struct gsupplicant_interface_wps_connect {
    GSupplicantInterface* iface;
    GSupplicantWPSParams wps;
    FiW1Wpa_supplicant1InterfaceWPS* wps_proxy;
    gulong wps_proxy_handler_id[WPS_PROXY_HANDLER_COUNT];
    char* pin;
    char* new_pin;
    GCancellable* cancel;
    gulong cancel_id;
    guint timeout_id;
    WPS_CONNECT_STATE state;
    GSupplicantInterfaceStringResultFunc fn;
    GDestroyNotify destroy;
    void* data;
} GSupplicantInterfaceWPSConnect;

/* Object definition */
enum supplicant_interface_proxy_handler_id {
    PROXY_BSS_ADDED,
    PROXY_BSS_REMOVED,
    PROXY_NETWORK_ADDED,
    PROXY_NETWORK_REMOVED,
    PROXY_NETWORK_SELECTED,
    PROXY_STA_AUTHORIZED,
    PROXY_STA_DEAUTHORIZED,
    PROXY_NOTIFY_STATE,
    PROXY_NOTIFY_SCANNING,
    PROXY_NOTIFY_AP_SCAN,
    PROXY_NOTIFY_SCAN_INTERVAL,
    PROXY_NOTIFY_CAPABILITIES,
    PROXY_NOTIFY_COUNTRY,
    PROXY_NOTIFY_DRIVER,
    PROXY_NOTIFY_IFNAME,
    PROXY_NOTIFY_BRIDGE_IFNAME,
    PROXY_NOTIFY_CURRENT_BSS,
    PROXY_NOTIFY_CURRENT_NETWORK,
    PROXY_NOTIFY_BSSS,
    PROXY_NOTIFY_NETWORKS,
    PROXY_HANDLER_COUNT
};

enum supplicant_handler_id {
    SUPPLICANT_VALID_CHANGED,
    SUPPLICANT_INTERFACES_CHANGED,
    SUPPLICANT_HANDLER_COUNT
};

struct gsupplicant_interface_priv {
    GDBusConnection* bus;
    FiW1Wpa_supplicant1Interface* proxy;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    gulong supplicant_handler_id[SUPPLICANT_HANDLER_COUNT];
    GSupplicantWPSCredentials wps_credentials;
    guint32 pending_signals;
    GStrV* bsss;
    GStrV* networks;
    GStrV* stations;
    char* path;
    char* country;
    char* driver;
    char* ifname;
    char* bridge_ifname;
    char* current_bss;
    char* current_network;
};

typedef GObjectClass GSupplicantInterfaceClass;
G_DEFINE_TYPE(GSupplicantInterface, gsupplicant_interface, G_TYPE_OBJECT)
#define GSUPPLICANT_INTERFACE_TYPE (gsupplicant_interface_get_type())
#define GSUPPLICANT_INTERFACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GSUPPLICANT_INTERFACE_TYPE, GSupplicantInterface))
#define SUPER_CLASS gsupplicant_interface_parent_class

/* Supplicant interface properties */
#define GSUPPLICANT_INTERFACE_PROPERTIES_(p) \
    p(VALID,valid) \
    p(PRESENT,present) \
    p(CAPS,caps) \
    p(STATE,state) \
    p(WPS_CREDENTIALS,wps-credentials) \
    p(SCANNING,scanning) \
    p(AP_SCAN,ap-scan) \
    p(COUNTRY,country) \
    p(DRIVER,driver) \
    p(IFNAME,ifname) \
    p(BRIDGE_IFNAME,bridge-ifname) \
    p(CURRENT_BSS,current-bss) \
    p(CURRENT_NETWORK,current-network) \
    p(BSSS,bsss) \
    p(NETWORKS,networks) \
    p(SCAN_INTERVAL,scan-interval) \
    p(STATIONS,stations)

typedef enum gsupplicant_interface_signal {
#define SIGNAL_ENUM_(P,p) SIGNAL_##P##_CHANGED,
    GSUPPLICANT_INTERFACE_PROPERTIES_(SIGNAL_ENUM_)
#undef SIGNAL_ENUM_
    SIGNAL_PROPERTY_CHANGED,
    SIGNAL_COUNT
} GSUPPLICANT_INTERFACE_SIGNAL;

#define SIGNAL_BIT(name) (1 << SIGNAL_##name##_CHANGED)

/*
 * The code assumes that VALID is the first one and that their number
 * doesn't exceed the number of bits in pending_signals (currently, 32)
 */
G_STATIC_ASSERT(SIGNAL_VALID_CHANGED == 0);
G_STATIC_ASSERT(SIGNAL_PROPERTY_CHANGED <= 32);

/* Assert that we have covered all publicly defined properties */
G_STATIC_ASSERT((int)SIGNAL_PROPERTY_CHANGED ==
               ((int)GSUPPLICANT_INTERFACE_PROPERTY_COUNT-1));

#define SIGNAL_PROPERTY_CHANGED_NAME            "property-changed"
#define SIGNAL_PROPERTY_CHANGED_DETAIL          "%x"
#define SIGNAL_PROPERTY_CHANGED_DETAIL_MAX_LEN  (8)

static GQuark gsupplicant_interface_property_quarks[SIGNAL_PROPERTY_CHANGED];
static guint gsupplicant_interface_signals[SIGNAL_COUNT];
static const char* gsupplicant_interface_signame[] = {
#define SIGNAL_NAME_(P,p) #p "-changed",
    GSUPPLICANT_INTERFACE_PROPERTIES_(SIGNAL_NAME_)
#undef SIGNAL_NAME_
    SIGNAL_PROPERTY_CHANGED_NAME
};

G_STATIC_ASSERT(G_N_ELEMENTS(gsupplicant_interface_signame) == SIGNAL_COUNT);

/* Proxy properties */
#define PROXY_PROPERTY_NAME_CAPABILITIES        "Capabilities"
#define PROXY_PROPERTY_NAME_STATE               "State"
#define PROXY_PROPERTY_NAME_SCANNING            "Scanning"
#define PROXY_PROPERTY_NAME_AP_SCAN             "ApScan"
#define PROXY_PROPERTY_NAME_BSS_EXPIRE_AGE      "BSSExpireAge"
#define PROXY_PROPERTY_NAME_BSS_EXPIRE_COUNT    "BSSExpireCount"
#define PROXY_PROPERTY_NAME_COUNTRY             "Country"
#define PROXY_PROPERTY_NAME_DRIVER              "Driver"
#define PROXY_PROPERTY_NAME_IFNAME              "Ifname"
#define PROXY_PROPERTY_NAME_BRIDGE_IFNAME       "BridgeIfname"
#define PROXY_PROPERTY_NAME_CURRENT_BSS         "CurrentBSS"
#define PROXY_PROPERTY_NAME_CURRENT_NETWORK     "CurrentNetwork"
#define PROXY_PROPERTY_NAME_CURRENT_AUTH_MODE   "CurrentAuthMode"
#define PROXY_PROPERTY_NAME_BLOBS               "Blobs"
#define PROXY_PROPERTY_NAME_BSSS                "BSSs"
#define PROXY_PROPERTY_NAME_NETWORKS            "Networks"
#define PROXY_PROPERTY_NAME_FAST_REAUTH         "FastReauth"
#define PROXY_PROPERTY_NAME_SCAN_INTERVAL       "ScanInterval"
#define PROXY_PROPERTY_NAME_PKCS11_ENGINE_PATH  "PKCS11EnginePath"
#define PROXY_PROPERTY_NAME_PKCS11_MODULE_PATH  "PKCS11ModulePath"
#define PROXY_PROPERTY_NAME_DISCONNECT_REASON   "DisconnectReason"

/* Weak references to the instances of GSupplicantInterface */
static GHashTable* gsupplicant_interface_table = NULL;

/* States */
static const GSupNameIntPair gsupplicant_interface_states [] = {
    { "disconnected",       GSUPPLICANT_INTERFACE_STATE_DISCONNECTED },
    { "inactive",           GSUPPLICANT_INTERFACE_STATE_INACTIVE },
    { "scanning",           GSUPPLICANT_INTERFACE_STATE_SCANNING },
    { "authenticating",     GSUPPLICANT_INTERFACE_STATE_AUTHENTICATING },
    { "associating",        GSUPPLICANT_INTERFACE_STATE_ASSOCIATING },
    { "associated",         GSUPPLICANT_INTERFACE_STATE_ASSOCIATED },
    { "4way_handshake",     GSUPPLICANT_INTERFACE_STATE_4WAY_HANDSHAKE },
    { "group_handshake",    GSUPPLICANT_INTERFACE_STATE_GROUP_HANDSHAKE },
    { "completed",          GSUPPLICANT_INTERFACE_STATE_COMPLETED },
    { "unknown",            GSUPPLICANT_INTERFACE_STATE_UNKNOWN }
};

/*==========================================================================*
 * Property change signals
 *==========================================================================*/

static inline
GSUPPLICANT_INTERFACE_PROPERTY
gsupplicant_interface_property_from_signal(
    GSUPPLICANT_INTERFACE_SIGNAL sig)
{
    switch (sig) {
#define SIGNAL_PROPERTY_MAP_(P,p) \
    case SIGNAL_##P##_CHANGED: return GSUPPLICANT_INTERFACE_PROPERTY_##P;
    GSUPPLICANT_INTERFACE_PROPERTIES_(SIGNAL_PROPERTY_MAP_)
#undef SIGNAL_PROPERTY_MAP_
    default: /* unreachable */ return GSUPPLICANT_INTERFACE_PROPERTY_ANY;
    }
}

/* "/" means that there's no association at all. */
static inline
const char*
gsupplicant_interface_association_path_filter(
    const char* path)
{
    return (path && path[0] == '/' && path[1] == '\0') ? NULL : path;
}

static
void
gsupplicant_interface_signal_property_change(
    GSupplicantInterface* self,
    GSUPPLICANT_INTERFACE_SIGNAL sig,
    GSUPPLICANT_INTERFACE_PROPERTY prop)
{
    GSupplicantInterfacePriv* priv = self->priv;
    GASSERT(prop > GSUPPLICANT_INTERFACE_PROPERTY_ANY);
    GASSERT(prop < GSUPPLICANT_INTERFACE_PROPERTY_COUNT);
    GASSERT(sig < G_N_ELEMENTS(gsupplicant_interface_property_quarks));
    if (!gsupplicant_interface_property_quarks[sig]) {
        char buf[SIGNAL_PROPERTY_CHANGED_DETAIL_MAX_LEN + 1];
        snprintf(buf, sizeof(buf), SIGNAL_PROPERTY_CHANGED_DETAIL, prop);
        buf[sizeof(buf)-1] = 0;
        gsupplicant_interface_property_quarks[sig] = g_quark_from_string(buf);
    }
    priv->pending_signals &= ~(1 << sig);
    g_signal_emit(self, gsupplicant_interface_signals[sig], 0);
    g_signal_emit(self, gsupplicant_interface_signals[SIGNAL_PROPERTY_CHANGED],
        gsupplicant_interface_property_quarks[sig], prop);
}

static
void
gsupplicant_interface_emit_pending_signals(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    GSUPPLICANT_INTERFACE_SIGNAL sig;
    gboolean valid_changed;

    /* Handlers could drops their references to us */
    gsupplicant_interface_ref(self);

    /* VALID is the last one to be emitted if we BECOME valid */
    if ((priv->pending_signals & SIGNAL_BIT(VALID)) && self->valid) {
        priv->pending_signals &= ~SIGNAL_BIT(VALID);
        valid_changed = TRUE;
    } else {
        valid_changed = FALSE;
    }

    /* Emit the signals. Not that in case if valid has become FALSE, then
     * VALID is emitted first, otherwise it's emitted last */
    for (sig = SIGNAL_VALID_CHANGED;
         sig < SIGNAL_PROPERTY_CHANGED && priv->pending_signals;
         sig++) {
        if (priv->pending_signals & (1 << sig)) {
            gsupplicant_interface_signal_property_change(self, sig,
                gsupplicant_interface_property_from_signal(sig));
        }
    }

    /* Then emit VALID if valid has become TRUE */
    if (valid_changed) {
        gsupplicant_interface_signal_property_change(self,
            SIGNAL_VALID_CHANGED, GSUPPLICANT_INTERFACE_PROPERTY_VALID);
    }

    /* And release the temporary reference */
    gsupplicant_interface_unref(self);
}

/*==========================================================================*
 * Network parameters
 *==========================================================================*/

static
void
gsupplicant_interface_add_network_args_security_wep(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np)
{
    if (np->passphrase && np->passphrase[0]) {
        const char* key = "wep_key0";
        const gsize len = strlen(np->passphrase);
        const guint8* bin = NULL;
        guint8 buf[13];

        if (len == 10 || len == 26) {
            /* Check for hex representation of the WEP key */
            bin = gutil_hex2bin(np->passphrase, len, buf);
        }

        if (bin) {
            gsupplicant_dict_add_value(builder, key, g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE, bin, len/2, 1));
        } else {
            gsupplicant_dict_add_string(builder, key, np->passphrase);
        }
        gsupplicant_dict_add_uint32(builder, "wep_tx_keyidx", 0);
    }
}

static
void
gsupplicant_interface_add_network_args_security_psk(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np)
{
    if (np->passphrase && np->passphrase[0]) {
        const char* key = "psk";
        const gsize len = strlen(np->passphrase);
        const guint8* bin = NULL;
        guint8 buf[32];

        if (len == 64) {
            /* Check for hex representation of the 256-bit pre-shared key */
            bin = gutil_hex2bin(np->passphrase, len, buf);
        }

        if (bin) {
            gsupplicant_dict_add_value(builder, key, g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE, bin, len/2, 1));
        } else {
            gsupplicant_dict_add_string(builder, key, np->passphrase);
        }
    }
}

static
void
gsupplicant_interface_add_network_args_security_peap(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np,
    GHashTable* blobs)
{
    if (np->eap == GSUPPLICANT_EAP_METHOD_PEAP) {
        switch (np->auth_flags &
                (GSUPPLICANT_AUTH_PHASE1_PEAPV0 |
                 GSUPPLICANT_AUTH_PHASE1_PEAPV1)) {
        case GSUPPLICANT_AUTH_PHASE1_PEAPV0:
            gsupplicant_dict_add_string(builder, "phase1", "peapver=0");
            break;
        case GSUPPLICANT_AUTH_PHASE1_PEAPV1:
            gsupplicant_dict_add_string(builder, "phase1", "peapver=1");
            break;
        default:
            GWARN("Trying to force PEAPv0 and v1, ignoring");
        case 0:
            break;
        }
    }
    /*
     * Multiple protocols in phase2 should be allowed,
     * e.g "autheap=MSCHAPV2 autheap=MD5" for EAP-TTLS
     * Does the order matter?
     */
    if (np->phase2 != GSUPPLICANT_EAP_METHOD_NONE) {
        const char* ca_cert2 =
            gsupplicant_check_blob_or_abs_path(np->ca_cert_file2,
                blobs);
        const char* client_cert2 =
            gsupplicant_check_blob_or_abs_path(np->client_cert_file2,
                blobs);
        const char* auth = (np->auth_flags & GSUPPLICANT_AUTH_PHASE2_AUTHEAP) ?
            "autheap" : "auth";
        GString* buf = g_string_new(NULL);
        guint found, phase2 = np->phase2;
        const char* method = gsupplicant_eap_method_name(phase2, &found);
        while (method) {
            if (buf->len) g_string_append_c(buf, ' ');
            g_string_append(buf, auth);
            g_string_append_c(buf, '=');
            g_string_append(buf, method);
            phase2 &= ~found;
            method = gsupplicant_eap_method_name(phase2, &found);
        }
        if (buf->len > 0) {
            gsupplicant_dict_add_string(builder, "phase2", buf->str);
        }
        g_string_free(buf, TRUE);

        gsupplicant_dict_add_string0(builder, "ca_cert2", ca_cert2);
        if (client_cert2) {
            if (np->private_key_file2 && np->private_key_file2[0]) {
                const char* private_key2 =
                    gsupplicant_check_blob_or_abs_path(np->private_key_file2,
                        blobs);
                if (private_key2) {
                    gsupplicant_dict_add_string(builder, "client_cert2",
                        client_cert2);
                    gsupplicant_dict_add_string(builder, "private_key2",
                        np->private_key_file2);
                    gsupplicant_dict_add_string_ne(builder,
                        "private_key_passwd2", np->private_key_passphrase2);
                }
            } else {
                GWARN("Missing private key for phase2");
            }
        }
        gsupplicant_dict_add_string_ne(builder, "subject_match2",
            np->subject_match2);
        gsupplicant_dict_add_string_ne(builder, "altsubject_match2",
            np->altsubject_match2);
        gsupplicant_dict_add_string_ne(builder, "domain_suffix_match2",
            np->domain_suffix_match2);
    }
}

static
void
gsupplicant_interface_add_network_args_security_eap(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np,
    GHashTable* blobs)
{
    guint found;
    const char* ca_cert =
        gsupplicant_check_blob_or_abs_path(np->ca_cert_file, blobs);
    const char* client_cert =
        gsupplicant_check_blob_or_abs_path(np->client_cert_file, blobs);
    const char* method = gsupplicant_eap_method_name(np->eap, &found);
    GASSERT(found == np->eap); /* Only one method should be specified */
    gsupplicant_dict_add_string_ne(builder, "eap", method);
    switch (np->eap) {
    case GSUPPLICANT_EAP_METHOD_NONE:
        GERR_("No EAP method specified!");
        return;
    case GSUPPLICANT_EAP_METHOD_PEAP:
    case GSUPPLICANT_EAP_METHOD_TTLS:
        gsupplicant_interface_add_network_args_security_peap(builder, np, blobs);
        break;
    case GSUPPLICANT_EAP_METHOD_TLS:
        break;
    default:
        GWARN_("Unsupported EAP method %s", method);
        break;
    }
    gsupplicant_dict_add_string_ne(builder, "identity", np->identity);
    gsupplicant_dict_add_string_ne(builder, "anonymous_identity",
        np->anonymous_identity);
    gsupplicant_dict_add_string_ne(builder, "password", np->passphrase);
    gsupplicant_dict_add_string0(builder, "ca_cert", ca_cert);
    if (client_cert) {
        if (np->private_key_file && np->private_key_file[0]) {
            const char* private_key =
                gsupplicant_check_blob_or_abs_path(np->private_key_file,
                    blobs);
            if (private_key) {
                gsupplicant_dict_add_string(builder, "client_cert",
                    client_cert);
                gsupplicant_dict_add_string(builder, "private_key",
                    private_key);
                gsupplicant_dict_add_string_ne(builder, "private_key_passwd",
                    np->private_key_passphrase);
            }
        } else {
            GWARN("Missing private key");
        }
    }
    gsupplicant_dict_add_string_ne(builder, "domain_match",
        np->domain_match);
    gsupplicant_dict_add_string_ne(builder, "subject_match",
        np->subject_match);
    gsupplicant_dict_add_string_ne(builder, "altsubject_match",
        np->altsubject_match);
    gsupplicant_dict_add_string_ne(builder, "domain_suffix_match",
        np->domain_suffix_match);
}

static
void
gsupplicant_interface_add_network_args_security_ciphers(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np)
{
    static const GSupNameIntPair ciphers [] = {
        { "CCMP",       GSUPPLICANT_CIPHER_CCMP },
        { "TKIP",       GSUPPLICANT_CIPHER_TKIP },
        { "WEP104",     GSUPPLICANT_CIPHER_WEP104 },
        { "WEP40",      GSUPPLICANT_CIPHER_WEP40 }
    };
    char* pairwise = gsupplicant_name_int_concat(np->pairwise, ' ',
        ciphers, G_N_ELEMENTS(ciphers));
    char* group = gsupplicant_name_int_concat(np->group, ' ',
        ciphers, G_N_ELEMENTS(ciphers));
    if (pairwise) {
        gsupplicant_dict_add_string(builder, "pairwise", pairwise);
        g_free(pairwise);
    }
    if (group) {
        gsupplicant_dict_add_string(builder, "group", group);
        g_free(group);
    }
}

static
void
gsupplicant_interface_add_network_args_security_proto(
    GVariantBuilder* builder,
    const GSupplicantNetworkParams* np)
{
    static const GSupNameIntPair protos [] = {
        { "RSN",        GSUPPLICANT_PROTOCOL_RSN },
        { "WPA",        GSUPPLICANT_PROTOCOL_WPA }
    };
    char* proto = gsupplicant_name_int_concat(np->protocol, ' ',
        protos, G_N_ELEMENTS(protos));
    if (proto) {
        gsupplicant_dict_add_string(builder, "proto", proto);
        g_free(proto);
    }
}

static
GVariant*
gsupplicant_interface_add_network_args_new(
    const GSupplicantNetworkParams* np,
    GHashTable* blobs)
{
    const char* key_mgmt = NULL;
    const char* auth_alg = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    gsupplicant_dict_add_bytes0(&builder, "ssid", np->ssid);
    if (np->frequency) {
        gsupplicant_dict_add_uint32(&builder, "frequency", np->frequency);
    }
    gsupplicant_dict_add_string_ne(&builder, "bgscan", np->bgscan);
    gsupplicant_dict_add_uint32(&builder, "scan_ssid", np->scan_ssid);
    gsupplicant_dict_add_uint32(&builder, "mode", np->mode);
    switch (np->security) {
    case GSUPPLICANT_SECURITY_NONE:
        GDEBUG_("no security");
        key_mgmt = "NONE";
        auth_alg = "OPEN";
        break;
    case GSUPPLICANT_SECURITY_WEP:
        GDEBUG_("WEP security");
        key_mgmt = "NONE";
        auth_alg = "OPEN SHARED";
        gsupplicant_interface_add_network_args_security_wep(&builder, np);
        gsupplicant_interface_add_network_args_security_ciphers(&builder, np);
        break;
    case GSUPPLICANT_SECURITY_PSK:
        GDEBUG_("PSK security");
        key_mgmt = "WPA-PSK";
        gsupplicant_interface_add_network_args_security_psk(&builder, np);
        gsupplicant_interface_add_network_args_security_proto(&builder, np);
        gsupplicant_interface_add_network_args_security_ciphers(&builder, np);
        break;
    case GSUPPLICANT_SECURITY_EAP:
        GDEBUG_("EAP security");
        key_mgmt = "WPA-EAP";
        gsupplicant_interface_add_network_args_security_eap(&builder, np, blobs);
        gsupplicant_interface_add_network_args_security_proto(&builder, np);
        gsupplicant_interface_add_network_args_security_ciphers(&builder, np);
        break;
    }
    gsupplicant_dict_add_string0(&builder, "auth_alg", auth_alg);
    gsupplicant_dict_add_string0(&builder, "key_mgmt", key_mgmt);
    return g_variant_ref_sink(g_variant_builder_end(&builder));
}

/*==========================================================================*
 * WPS Connect
 *==========================================================================*/

static
void
gsupplicant_interface_clear_wps_credentials(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    priv->wps_credentials.auth_types = GSUPPLICANT_AUTH_NONE;
    priv->wps_credentials.encr_types = GSUPPLICANT_WPS_ENCR_NONE;
    priv->wps_credentials.key_index = 0;
    if (priv->wps_credentials.bssid) {
        g_bytes_unref(priv->wps_credentials.bssid);
        priv->wps_credentials.bssid = NULL;
    }
    if (priv->wps_credentials.ssid) {
        g_bytes_unref(priv->wps_credentials.ssid);
        priv->wps_credentials.ssid = NULL;
    }
    if (priv->wps_credentials.key) {
        g_bytes_unref(priv->wps_credentials.key);
        priv->wps_credentials.key = NULL;
    }
    if (self->wps_credentials) {
        self->wps_credentials = NULL;
        priv->pending_signals |= SIGNAL_BIT(WPS_CREDENTIALS);
    }
}

static
void
gsupplicant_interface_wps_parse_creds(
    const char* name,
    GVariant* value,
    void* data)
{
    GSupplicantWPSCredentials* wps = data;
    if (!g_strcmp0(name, "BSSID")) {
        if (wps->bssid) g_bytes_unref(wps->bssid);
        wps->bssid = gsupplicant_variant_data_as_bytes(value);
        GVERBOSE("  %s: %s", name, gsupplicant_format_bytes(wps->bssid, TRUE));
    } else if (!g_strcmp0(name, "SSID")) {
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
            gsize length = 0;
            const char* ssid = g_variant_get_string(value, &length);
            if (ssid) {
                if (wps->ssid) g_bytes_unref(wps->ssid);
                wps->ssid = g_bytes_new(ssid, length);
                GVERBOSE("  %s: \"%.*s\"", name, (int)length, ssid);
            }
        }
    } else if (!g_strcmp0(name, "AuthType")) {
        static const GSupNameIntPair auth_types_map [] = {
            { "open",       GSUPPLICANT_AUTH_OPEN },
            { "shared",     GSUPPLICANT_AUTH_SHARED },
            { "wpa-psk",    GSUPPLICANT_AUTH_WPA_PSK },
            { "wpa-eap",    GSUPPLICANT_AUTH_WPA_EAP },
            { "wpa2-eap",   GSUPPLICANT_AUTH_WPA2_EAP },
            { "wpa2-psk",   GSUPPLICANT_AUTH_WPA2_PSK }
        };
        wps->auth_types = gsupplicant_parse_bits_array(0, name, value,
            auth_types_map, G_N_ELEMENTS(auth_types_map));
    } else if (!g_strcmp0(name, "EncrType")) {
        static const GSupNameIntPair encr_types_map [] = {
            { "none",       GSUPPLICANT_WPS_ENCR_NONE },
            { "wep",        GSUPPLICANT_WPS_ENCR_WEP },
            { "tkip",       GSUPPLICANT_WPS_ENCR_TKIP },
            { "aes",        GSUPPLICANT_WPS_ENCR_AES }
        };
        wps->encr_types = gsupplicant_parse_bits_array(0, name, value,
            encr_types_map, G_N_ELEMENTS(encr_types_map));
    } else if (!g_strcmp0(name, "Key")) {
        if (wps->key) g_bytes_unref(wps->key);
        wps->key = gsupplicant_variant_data_as_bytes(value);
        GVERBOSE("  %s: %s", name, gsupplicant_format_bytes(wps->key, TRUE));
    } else if (!g_strcmp0(name, "KeyIndex")) {
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
            wps->key_index = g_variant_get_uint32(value);
            GVERBOSE("  %s: %u", name, wps->key_index);
        }
    }
}

static
void
gsupplicant_interface_wps_connect_dispose(
    GSupplicantInterfaceWPSConnect* connect)
{
    /* May be invoked twice */
    if (connect->wps_proxy) {
        gutil_disconnect_handlers(connect->wps_proxy,
            connect->wps_proxy_handler_id,
            G_N_ELEMENTS(connect->wps_proxy_handler_id));
        g_object_unref(connect->wps_proxy);
        connect->wps_proxy = NULL;
    }
    if (connect->timeout_id) {
        g_source_remove(connect->timeout_id);
        connect->timeout_id = 0;
    }
}

static
void
gsupplicant_interface_wps_connect_free(
    GSupplicantInterfaceWPSConnect* connect)
{
    gsupplicant_interface_wps_connect_dispose(connect);
    if (connect->wps.bssid) g_bytes_unref(connect->wps.bssid);
    if (connect->wps.p2p_address) g_bytes_unref(connect->wps.p2p_address);
    gsupplicant_interface_unref(connect->iface);
    if (connect->cancel_id) {
        g_cancellable_disconnect(connect->cancel, connect->cancel_id);
    }
    g_object_unref(connect->cancel);
    g_free(connect->pin);
    g_free(connect->new_pin);
    if (connect->destroy) {
        connect->destroy(connect->data);
    }
    g_slice_free(GSupplicantInterfaceWPSConnect, connect);
}

static
void
gsupplicant_interface_wps_connect_free1(
    void* connect)
{
    gsupplicant_interface_wps_connect_free(connect);
}

static
void
gsupplicant_interface_wps_connect_cancelled(
    GCancellable* cancel,
    gpointer data)
{
    /*
     * Under GLib < 2.40 this function is invoked under the lock
     * protecting cancellable. We can't call g_cancellable_disconnect
     * because it wouild deadlock, it has to be invoked on a fresh stack.
     * That sucks.
     */
    GSupplicantInterfaceWPSConnect* connect = data;
    gsupplicant_interface_wps_connect_dispose(connect);
    gsupplicant_call_later(gsupplicant_interface_wps_connect_free1, connect);
}

static
void
gsupplicant_interface_wps_connect_ok(
    GSupplicantInterfaceWPSConnect* connect)
{
    GDEBUG("[%s] WPS connect OK", connect->iface->path);
    GASSERT(!g_cancellable_is_cancelled(connect->cancel));
    if (connect->fn) {
        if (connect->cancel_id) {
            /* In case if callback calls g_cancellable_cancel() */
            g_cancellable_disconnect(connect->cancel, connect->cancel_id);
            connect->cancel_id = 0;
        }
        connect->fn(connect->iface, connect->cancel, NULL, connect->new_pin,
            connect->data);
    }
    gsupplicant_interface_wps_connect_free(connect);
}

static
void
gsupplicant_interface_wps_connect_error_free(
    GSupplicantInterfaceWPSConnect* connect,
    const GError* error)
{
    GERR("Failed to start WPS: %s", GERRMSG(error));
    if (connect->fn && !g_cancellable_is_cancelled(connect->cancel)) {
        GError* tmp_error = NULL;
        if (connect->cancel_id) {
            /* In case if callback calls g_cancellable_cancel() */
            g_cancellable_disconnect(connect->cancel, connect->cancel_id);
            connect->cancel_id = 0;
        }
        if (!error) {
            error = tmp_error = g_error_new_literal(G_IO_ERROR,
                G_IO_ERROR_FAILED, "WPS connect failed");
        }
        connect->fn(connect->iface, connect->cancel, error, NULL,
            connect->data);
        if (tmp_error) g_error_free(tmp_error);
    }
    gsupplicant_interface_wps_connect_free(connect);
}

static
gboolean
gsupplicant_interface_wps_connect_timeout(
    gpointer data)
{
    GSupplicantInterfaceWPSConnect* connect = data;
    GDEBUG("WPS connect timed out");
    GASSERT(!g_cancellable_is_cancelled(connect->cancel));
    connect->timeout_id = 0;
    if (connect->fn) {
        GError* error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "WPS connect timed out");
        if (connect->cancel_id) {
            /* In case if callback calls g_cancellable_cancel() */
            g_cancellable_disconnect(connect->cancel, connect->cancel_id);
            connect->cancel_id = 0;
        }
        connect->fn(connect->iface, connect->cancel, error, NULL,
            connect->data);
        g_error_free(error);
    }
    gsupplicant_interface_wps_connect_free(connect);
    return G_SOURCE_REMOVE;
}

static
GSupplicantInterfaceWPSConnect*
gsupplicant_interface_wps_connect_new(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GSupplicantWPSParams* params,
    gint timeout_sec, /* 0 = default, negative = no timeout */
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    GSupplicantInterfaceWPSConnect* connect =
        g_slice_new0(GSupplicantInterfaceWPSConnect);
    connect->iface = gsupplicant_interface_ref(iface);
    connect->cancel = cancel ? g_object_ref(cancel) : g_cancellable_new();
    connect->wps.role = params->role;
    connect->wps.auth = params->auth;
    connect->wps.pin = connect->pin = g_strdup(params->pin);
    if (params->bssid) {
        connect->wps.bssid = g_bytes_ref(params->bssid);
    }
    if (params->p2p_address) {
        connect->wps.p2p_address = g_bytes_ref(params->p2p_address);
    }
    if (!timeout_sec) timeout_sec = WPS_DEFAULT_CONNECT_TIMEOUT_SEC;
    if (timeout_sec > 0) {
        connect->timeout_id = g_timeout_add_seconds(timeout_sec,
            gsupplicant_interface_wps_connect_timeout, connect);
    }
    connect->fn = fn;
    connect->destroy = destroy;
    connect->data = data;
    return connect;
}

static
void
gsupplicant_interface_wps_connect_proxy_credentials(
    FiW1Wpa_supplicant1InterfaceWPS* proxy,
    GVariant* args,
    gpointer data)
{
    GSupplicantInterfaceWPSConnect* connect = data;
    GSupplicantInterface* self = connect->iface;
    GSupplicantInterfacePriv* priv = self->priv;
    GSupplicantWPSCredentials* wps = &priv->wps_credentials;
    GDEBUG("[%s] WPS credentials received", priv->path);
    gsupplicant_interface_clear_wps_credentials(self);
    gsupplicant_dict_parse(args, gsupplicant_interface_wps_parse_creds, wps);
    self->wps_credentials = wps;
    priv->pending_signals |= SIGNAL_BIT(WPS_CREDENTIALS);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_wps_connect_proxy_event(
    FiW1Wpa_supplicant1InterfaceWPS* proxy,
    const char* type,
    GVariant* args,
    gpointer data)
{
    static const GSupNameIntPair event_types[] = {
        { "success",        WPS_CONNECT_SUCCESS },
        { "fail",           WPS_CONNECT_FAIL },
        { "m2d",            WPS_CONNECT_M2D },
        { "pbc-overlap",    WPS_CONNECT_PBC_OVERLAP }
    };
    GSupplicantInterfaceWPSConnect* connect = data;
    GDEBUG("[%s] WPS event \"%s\"", connect->iface->path, type);
    connect->state = gsupplicant_name_int_get_int(type, event_types,
        G_N_ELEMENTS(event_types), WPS_CONNECT_FAIL);
    if (connect->state == WPS_CONNECT_SUCCESS) {
        /*
         * If connect_id is non-zero, then Start() call has completed
         * and we have been waiting for this event. Otherwise, we will
         * wait for the Start() call to complete.
         */
        if (connect->cancel_id) {
            gsupplicant_interface_wps_connect_ok(connect);
        }
    } else {
        GError* error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED,
            "WPS connect failed (%s)", type);
        gsupplicant_interface_wps_connect_error_free(connect, error);
        g_error_free(error);
    }
}

static
GVariant*
gsupplicant_interface_wps_start_args_new(
    const GSupplicantWPSParams* wps)
{
    GVariantBuilder builder;
    const gboolean enrollee = (wps->role != GSUPPLICANT_WPS_ROLE_REGISTRAR);
    const char* role = enrollee ? "enrollee" : "registrar";
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    GVERBOSE_("Role: %s", role);
    gsupplicant_dict_add_string(&builder, "Role", role);
    if (enrollee) {
        const char* type = wps->auth == GSUPPLICANT_WPS_AUTH_PIN && wps->pin ?
            "pin" : "pbc";
        GVERBOSE_("Type: %s", type);
        gsupplicant_dict_add_string(&builder, "Type", type);
    }
    gsupplicant_dict_add_string0(&builder, "Pin", wps->pin);
    gsupplicant_dict_add_bytes0(&builder, "Bssid", wps->bssid);
    gsupplicant_dict_add_bytes0(&builder, "P2PDeviceAddress",
        wps->p2p_address);
    return g_variant_ref_sink(g_variant_builder_end(&builder));
}

static
void
gsupplicant_interface_wps_start_pin(
    const char* key,
    GVariant* value,
    void* data)
{
    if (!g_strcmp0(key, "Pin") &&
        g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
        const char** out = data;
        *out = g_variant_get_string(value, NULL);
        GDEBUG_("pin: %s", *out);
    }
}

/* fi_w1_wpa_supplicant1_interface_wps_call_start() completion */
static
void
gsupplicant_interface_wps_connect3(
    GObject* bus,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterfaceWPSConnect* connect = data;
    GSupplicantInterface* self = connect->iface;
    GSupplicantInterfacePriv* priv = self->priv;
    GVariant* out;
    GError* error = NULL;
    if (fi_w1_wpa_supplicant1_interface_wps_call_start_finish(
        connect->wps_proxy, &out, result, &error)) {
        const char* pin = NULL;
        gsupplicant_dict_parse(out, gsupplicant_interface_wps_start_pin, &pin);
        connect->new_pin = g_strdup(pin);
        if (connect->state == WPS_CONNECT_SUCCESS) {
            /* We have already received the WPS event */
            gsupplicant_interface_wps_connect_ok(connect);
        } else {
            /* Wait for the event */
            GDEBUG("[%s]: Waiting for WPS event", priv->path);
            GASSERT(!connect->cancel_id);
            connect->cancel_id = g_cancellable_connect(connect->cancel,
                G_CALLBACK(gsupplicant_interface_wps_connect_cancelled),
                connect, NULL);
        }
        g_variant_unref(out);
    } else {
        GDEBUG_("%s %s", priv->path, GERRMSG(error));
        gsupplicant_interface_wps_connect_error_free(connect, error);
        g_error_free(error);
    }
}

/* fi_w1_wpa_supplicant1_interface_wps_call_cancel() completion */
static
void
gsupplicant_interface_wps_connect2(
    GObject* object,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterfaceWPSConnect* connect = data;
    GSupplicantInterface* self = connect->iface;
    GSupplicantInterfacePriv* priv = self->priv;
    GError* error = NULL;
    gsupplicant_interface_clear_wps_credentials(self);
    gsupplicant_interface_emit_pending_signals(self);
    if (fi_w1_wpa_supplicant1_interface_wps_call_cancel_finish(
        connect->wps_proxy, result, &error)) {
        GVariant* args;

        /* Register to receive WPS events */
        connect->wps_proxy_handler_id[WPS_PROXY_EVENT] =
            g_signal_connect(connect->wps_proxy, "event",
            G_CALLBACK(gsupplicant_interface_wps_connect_proxy_event),
            connect);
        connect->wps_proxy_handler_id[WPS_PROXY_CREDENTIALS] =
            g_signal_connect(connect->wps_proxy, "credentials",
            G_CALLBACK(gsupplicant_interface_wps_connect_proxy_credentials),
            connect);

        /* Start WPS configuration */
        args = gsupplicant_interface_wps_start_args_new(&connect->wps);
        GDEBUG_("%s starting WPS configuration", priv->path);
        fi_w1_wpa_supplicant1_interface_wps_call_start(connect->wps_proxy,
            args, connect->cancel, gsupplicant_interface_wps_connect3,
            connect);
        g_variant_unref(args);
    } else {
        GDEBUG_("%s %s", priv->path, GERRMSG(error));
        gsupplicant_interface_wps_connect_error_free(connect, error);
        g_error_free(error);
    }
}

/* fi_w1_wpa_supplicant1_interface_wps_proxy_new() completion */
void
gsupplicant_interface_wps_connect1(
    GObject* object,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterfaceWPSConnect* connect = data;
    GSupplicantInterface* self = connect->iface;
    GSupplicantInterfacePriv* priv = self->priv;
    GError* error = NULL;

    GASSERT(!connect->wps_proxy);
    connect->wps_proxy = fi_w1_wpa_supplicant1_interface_wps_proxy_new_finish(
        result, &error);
    if (connect->wps_proxy) {
        /* Cancel ongoing WPS operation, if any */
        GVERBOSE_("%s cancelling ongoing WPS operation", priv->path);
        fi_w1_wpa_supplicant1_interface_wps_call_cancel(connect->wps_proxy,
            connect->cancel, gsupplicant_interface_wps_connect2, connect);
    } else {
        GDEBUG_("%s %s", priv->path, GERRMSG(error));
        gsupplicant_interface_wps_connect_error_free(connect, error);
        g_error_free(error);
    }
}

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
gsupplicant_interface_call_finished(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterfaceCall* call = data;
    if (!g_cancellable_is_cancelled(call->cancel)) {
        call->finish(call, result);
    } else {
        /* Generic cleanup */
        GVariant* var = g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy),
            result, NULL);
        if (var) {
            g_variant_unref(var);
        }
    }
    gsupplicant_interface_unref(call->iface);
    g_object_unref(call->cancel);
    if (call->destroy) {
        call->destroy(call->data);
    }
    g_slice_free(GSupplicantInterfaceCall, call);
}

static
GSupplicantInterfaceCall*
gsupplicant_interface_call_new(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    GSupplicantInterfaceCallFinishFunc finish,
    GCallback cb,
    GDestroyNotify destroy,
    void* data)
{
    GSupplicantInterfaceCall* call = g_slice_new0(GSupplicantInterfaceCall);
    call->cancel = cancel ? g_object_ref(cancel) : g_cancellable_new();
    call->iface = gsupplicant_interface_ref(iface);
    call->finish = finish;
    call->fn.cb = cb;
    call->destroy = destroy;
    call->data = data;
    return call;
}

static
void
gsupplicant_interface_add_network_call_dispose(
    GSupplicantInterfaceAddNetworkCall* call)
{
    /* May be invoked twice */
    if (call->network) {
        gsupplicant_network_remove_all_handlers(call->network,
            call->network_event_id);
        gsupplicant_network_unref(call->network);
        call->network = NULL;
    }
    if (call->args) {
        g_variant_unref(call->args);
        call->args = NULL;
   }
}

static
void
gsupplicant_interface_add_network_call_free(
    GSupplicantInterfaceAddNetworkCall* call)
{
    GASSERT(!call->pending);
    gsupplicant_interface_add_network_call_dispose(call);
    gsupplicant_interface_unref(call->iface);
    if (call->cancel_id) {
        g_cancellable_disconnect(call->cancel, call->cancel_id);
    }
    if (call->blobs) {
        g_hash_table_unref(call->blobs);
    }
    g_object_unref(call->cancel);
    g_free(call->path);
    if (call->destroy) {
        call->destroy(call->data);
    }
    g_slice_free(GSupplicantInterfaceAddNetworkCall, call);
}

static
void
gsupplicant_interface_add_network_call_free1(
    void* call)
{
    gsupplicant_interface_add_network_call_free(call);
}

static
void
gsupplicant_interface_call_add_network_finish(
    GSupplicantInterfaceAddNetworkCall* call,
    const GError* error)
{
    /*
     * If it's cancelled then gsupplicant_interface_add_network_call_free1
     * call has been scheduled and we don't have to do anything here.
     */
    if (!g_cancellable_is_cancelled(call->cancel)) {
        if (call->fn) {
            if (call->cancel_id) {
                /* In case if the callback calls g_cancellable_cancel() */
                g_cancellable_disconnect(call->cancel, call->cancel_id);
                call->cancel_id = 0;
            }
            call->fn(call->iface, call->cancel, error,
                error ? NULL : call->path, call->data);
        }
        gsupplicant_interface_add_network_call_free(call);
    }
}

static
void
gsupplicant_interface_call_add_network_finish_error(
    GSupplicantInterfaceAddNetworkCall* call)
{
    /*
     * If it's cancelled then gsupplicant_interface_add_network_call_free1
     * call has been scheduled and we don't have to do anything here.
     */
    if (!g_cancellable_is_cancelled(call->cancel)) {
        if (call->fn) {
            GError* error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED,
                "Failed to enable %s", call->path);
            if (call->cancel_id) {
                /* In case if the callback calls g_cancellable_cancel() */
                g_cancellable_disconnect(call->cancel, call->cancel_id);
                call->cancel_id = 0;
            }
            call->fn(call->iface, call->cancel, error, NULL, call->data);
            g_error_free(error);
        }
        gsupplicant_interface_add_network_call_free(call);
    }
}

static
void
gsupplicant_interface_add_network_call_cancelled(
    GCancellable* cancel,
    gpointer data)
{
    GSupplicantInterfaceAddNetworkCall* call = data;
    GASSERT(call->cancel == cancel);
    if (call->pending) {
        /*
         * An asynchrnous call is pending, we expect it to complete
         * with G_IO_ERROR_CANCELLED.
         */
        gsupplicant_interface_add_network_call_dispose(call);
    } else {
        /*
         * Under GLib < 2.40 this function is invoked under the lock
         * protecting cancellable. We can't call g_cancellable_disconnect
         * because it wouild deadlock, it has to be invoked on a fresh stack.
         * That sucks.
         */
        gsupplicant_call_later(gsupplicant_interface_add_network_call_free1,
            call);
    }
}

static
GSupplicantInterfaceAddNetworkCall*
gsupplicant_interface_add_network_call_new(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GSupplicantNetworkParams* np,
    guint flags,
    GHashTable* blobs,
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    GSupplicantInterfaceAddNetworkCall* call =
        g_slice_new0(GSupplicantInterfaceAddNetworkCall);
    call->cancel = cancel ? g_object_ref(cancel) : g_cancellable_new();
    call->cancel_id = g_cancellable_connect(call->cancel,
        G_CALLBACK(gsupplicant_interface_add_network_call_cancelled),
        call, NULL);
    if (blobs && g_hash_table_size(blobs)) {
        call->blobs = g_hash_table_ref(blobs);
        g_hash_table_iter_init(&call->iter, call->blobs);
    }
    call->args = gsupplicant_interface_add_network_args_new(np, call->blobs);
    call->iface = gsupplicant_interface_ref(iface);
    call->fn = fn;
    call->destroy = destroy;
    call->data = data;
    call->flags = flags;
    return call;
}

static
void
gsupplicant_interface_call_finish_void(
    GSupplicantInterfaceCall* call,
    GAsyncResult* result)
{
    GError* error = NULL;
    GDBusProxy* proxy = G_DBUS_PROXY(call->iface->priv->proxy);
    GVariant* var = g_dbus_proxy_call_finish(proxy, result, &error);
    if (var) {
        g_variant_unref(var);
    }
    if (call->fn.fn_void) {
        call->fn.fn_void(call->iface, call->cancel, error, call->data);
    }
    if (error) {
        g_error_free(error);
    }
}

static
void
gsupplicant_interface_call_finish_signal_poll(
    GSupplicantInterfaceCall* call,
    GAsyncResult* result)
{
    GError* error = NULL;
    GVariant* dict = NULL;
    GSupplicantSignalPoll info;
    const GSupplicantSignalPoll* info_ptr;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    if (fi_w1_wpa_supplicant1_interface_call_signal_poll_finish(proxy, &dict,
        result, &error) && call->fn.fn_signal_poll) {
        GVariantIter it;
        GVariant* entry;
        info_ptr = &info;
        memset(&info, 0, sizeof(info));
        if (g_variant_is_of_type(dict, G_VARIANT_TYPE_VARIANT)) {
            GVariant* tmp = g_variant_get_variant(dict);
            g_variant_unref(dict);
            dict = tmp;
        }
        for (g_variant_iter_init(&it, dict);
             (entry = g_variant_iter_next_value(&it)) != NULL;
             g_variant_unref(entry)) {
            GVariant* key = g_variant_get_child_value(entry, 0);
            GVariant* value = g_variant_get_child_value(entry, 1);
            const char* name = g_variant_get_string(key, NULL);
            if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
                GVariant* tmp = g_variant_get_variant(value);
                g_variant_unref(value);
                value = tmp;
            }
            if (!g_strcmp0(name, "linkspeed")) {
                info.linkspeed = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_LINKSPEED;
            } else if (!g_strcmp0(name, "noise")) {
                info.noise = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_NOISE;
            } else if (!g_strcmp0(name, "frequency")) {
                info.frequency = g_variant_get_uint32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_FREQUENCY;
            } else if (!g_strcmp0(name, "rssi")) {
                info.rssi = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_RSSI;
            } else if (!g_strcmp0(name, "avg-rssi")) {
                info.avg_rssi = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_AVG_RSSI;
            } else if (!g_strcmp0(name, "center-frq1")) {
                info.center_frq1 = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ1;
            } else if (!g_strcmp0(name, "center-frq2")) {
                info.center_frq2 = g_variant_get_int32(value);
                info.flags |= GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ2;
            }
            g_variant_unref(key);
            g_variant_unref(value);
        }
        g_variant_unref(dict);
    } else {
        info_ptr = NULL;
    }
    if (call->fn.fn_signal_poll) {
        call->fn.fn_signal_poll(call->iface, call->cancel, error, info_ptr,
            call->data);
    }
    if (error) {
        g_error_free(error);
    }
}

static
GCancellable*
gsupplicant_interface_call_void_void(
    GSupplicantInterface* self,
    GCancellable* cancel,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify destroy,
    void* data,
    void (*submit)(FiW1Wpa_supplicant1Interface* proxy,
        GCancellable* cancel, GAsyncReadyCallback cb, gpointer data))
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            cancel, gsupplicant_interface_call_finish_void, G_CALLBACK(fn),
            destroy, data);
        submit(priv->proxy, call->cancel, gsupplicant_interface_call_finished,
            call);
        return call->cancel;
    }
    return NULL;
}

static
GCancellable*
gsupplicant_interface_call_string_void(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* arg,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify destroy,
    void* data,
    void (*submit)(FiW1Wpa_supplicant1Interface* proxy, const gchar *arg,
        GCancellable* cancel, GAsyncReadyCallback cb, gpointer data))
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            cancel, gsupplicant_interface_call_finish_void, G_CALLBACK(fn),
            destroy, data);
        submit(priv->proxy, arg, call->cancel,
            gsupplicant_interface_call_finished, call);
        return call->cancel;
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

static
void
gsupplicant_interface_update_valid(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const gboolean valid = priv->proxy && self->supplicant->valid;
    if (self->valid != valid) {
        self->valid = valid;
        GDEBUG("Interface %s is %svalid", priv->path, valid ? "" : "in");
        priv->pending_signals |= SIGNAL_BIT(VALID);
    }
}

static
void
gsupplicant_interface_update_present(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const gboolean present = priv->proxy && self->supplicant->valid &&
        gutil_strv_contains(self->supplicant->interfaces, priv->path);
    if (self->present != present) {
        self->present = present;
        GDEBUG("interface %s is %spresent", priv->path, present ? "" : "not ");
        priv->pending_signals |= SIGNAL_BIT(PRESENT);
    }
}

static
void
gsupplicant_interface_parse_cap(
    const char* name,
    GVariant* value,
    void* data)
{
    GSupplicantInterfaceCaps* caps = data;
    if (!g_strcmp0(name, "Pairwise")) {
        static const GSupNameIntPair pairwise_map [] = {
            { "ccmp",           GSUPPLICANT_CIPHER_CCMP },
            { "tkip",           GSUPPLICANT_CIPHER_TKIP },
            { "none",           GSUPPLICANT_CIPHER_NONE }
        };
        caps->pairwise = gsupplicant_parse_bits_array(0, name, value,
            pairwise_map, G_N_ELEMENTS(pairwise_map));
    } else if (!g_strcmp0(name, "Group")) {
        static const GSupNameIntPair group_map [] = {
            { "ccmp",           GSUPPLICANT_CIPHER_CCMP },
            { "tkip",           GSUPPLICANT_CIPHER_TKIP },
            { "wep104",         GSUPPLICANT_CIPHER_WEP104 },
            { "wep40",          GSUPPLICANT_CIPHER_WEP40 }
        };
        caps->group = gsupplicant_parse_bits_array(0, name, value,
            group_map, G_N_ELEMENTS(group_map));
    } else if (!g_strcmp0(name, "KeyMgmt")) {
        static const GSupNameIntPair keymgmt_map [] = {
            { "wpa-psk",        GSUPPLICANT_KEYMGMT_WPA_PSK },
            { "wpa-ft-psk",     GSUPPLICANT_KEYMGMT_WPA_FT_PSK },
            { "wpa-psk-sha256", GSUPPLICANT_KEYMGMT_WPA_PSK_SHA256 },
            { "wpa-eap",        GSUPPLICANT_KEYMGMT_WPA_EAP },
            { "wpa-ft-eap",     GSUPPLICANT_KEYMGMT_WPA_FT_EAP },
            { "wpa-eap-sha256", GSUPPLICANT_KEYMGMT_WPA_EAP_SHA256 },
            { "ieee8021x",      GSUPPLICANT_KEYMGMT_IEEE8021X },
            { "wpa-none",       GSUPPLICANT_KEYMGMT_WPA_NONE },
            { "wps",            GSUPPLICANT_KEYMGMT_WPS },
            { "none",           GSUPPLICANT_KEYMGMT_NONE }
        };
        caps->keymgmt = gsupplicant_parse_bits_array(0, name, value,
            keymgmt_map, G_N_ELEMENTS(keymgmt_map));
    } else if (!g_strcmp0(name, "Protocol")) {
        static const GSupNameIntPair protocol_map [] = {
            { "rsn",            GSUPPLICANT_PROTOCOL_RSN },
            { "wpa",            GSUPPLICANT_PROTOCOL_WPA }
        };
        caps->protocol = gsupplicant_parse_bits_array(0, name, value,
            protocol_map, G_N_ELEMENTS(protocol_map));
    } else if (!g_strcmp0(name, "AuthAlg")) {
        static const GSupNameIntPair auth_alg_map [] = {
            { "open",           GSUPPLICANT_AUTH_OPEN },
            { "shared",         GSUPPLICANT_AUTH_SHARED },
            { "leap",           GSUPPLICANT_AUTH_LEAP }
        };
        caps->auth_alg = gsupplicant_parse_bits_array(0, name, value,
            auth_alg_map, G_N_ELEMENTS(auth_alg_map));
    } else if (!g_strcmp0(name, "Scan")) {
        static const GSupNameIntPair scan_map [] = {
            { "active",         GSUPPLICANT_INTERFACE_CAPS_SCAN_ACTIVE },
            { "passive",        GSUPPLICANT_INTERFACE_CAPS_SCAN_PASSIVE },
            { "ssid",           GSUPPLICANT_INTERFACE_CAPS_SCAN_SSID }
        };
        caps->scan = gsupplicant_parse_bits_array(0, name, value,
            scan_map, G_N_ELEMENTS(scan_map));
    } else if (!g_strcmp0(name, "Modes")) {
        static const GSupNameIntPair modes_map [] = {
            { "infrastructure", GSUPPLICANT_INTERFACE_CAPS_MODES_INFRA},
            { "ad-hoc",         GSUPPLICANT_INTERFACE_CAPS_MODES_AD_HOC },
            { "ap",             GSUPPLICANT_INTERFACE_CAPS_MODES_AP },
            { "p2p",            GSUPPLICANT_INTERFACE_CAPS_MODES_P2P }
        };
        caps->modes = gsupplicant_parse_bits_array(0, name, value,
            modes_map, G_N_ELEMENTS(modes_map));
    } else if (!g_strcmp0(name, "MaxScanSSID")) {
        caps->max_scan_ssid = g_variant_get_int32(value);
        GVERBOSE("  %s: %d", name, caps->max_scan_ssid);
    } else {
        GWARN("Unexpected interface capability key %s", name);
    }
}

static
void
gsupplicant_interface_update_caps(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const GSupplicantInterfaceCaps caps = self->caps;
    GVariant* dict;
    memset(&self->caps, 0, sizeof(self->caps));
    GVERBOSE("[%s] Capabilities:", priv->path);
    dict = fi_w1_wpa_supplicant1_interface_get_capabilities(priv->proxy);
    gsupplicant_dict_parse(dict, gsupplicant_interface_parse_cap, &self->caps);
    if (memcmp(&caps, &self->caps, sizeof(caps))) {
        priv->pending_signals |= SIGNAL_BIT(CAPS);
    }
}

static
void
gsupplicant_interface_update_state(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const GSUPPLICANT_INTERFACE_STATE state = gsupplicant_name_int_get_int(
        fi_w1_wpa_supplicant1_interface_get_state(priv->proxy),
        gsupplicant_interface_states,
        G_N_ELEMENTS(gsupplicant_interface_states),
        GSUPPLICANT_INTERFACE_STATE_UNKNOWN);
    if (self->state != state) {
        self->state = state;
        priv->pending_signals |= SIGNAL_BIT(STATE);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_STATE,
            gsupplicant_interface_state_name(state));
    }
}

static
void
gsupplicant_interface_update_scanning(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    gboolean b = fi_w1_wpa_supplicant1_interface_get_scanning(priv->proxy);
    if (self->scanning != b) {
        self->scanning = b;
        priv->pending_signals |= SIGNAL_BIT(SCANNING);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_SCANNING,
            b ? "true" : "false");
    }
}

static
void
gsupplicant_interface_update_ap_scan(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const guint val = fi_w1_wpa_supplicant1_interface_get_ap_scan(priv->proxy);
    if (self->ap_scan != val) {
        self->ap_scan = val;
        priv->pending_signals |= SIGNAL_BIT(AP_SCAN);
        GVERBOSE("[%s] %s: %u", priv->path, PROXY_PROPERTY_NAME_AP_SCAN, val);
    }
}

static
void
gsupplicant_interface_update_scan_interval(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    gint b = fi_w1_wpa_supplicant1_interface_get_scan_interval(priv->proxy);
    if (self->scan_interval != b) {
        self->scan_interval = b;
        priv->pending_signals |= SIGNAL_BIT(SCAN_INTERVAL);
        GVERBOSE("[%s] %s: %d", priv->path,
            PROXY_PROPERTY_NAME_SCAN_INTERVAL, b);
    }
}

static
void
gsupplicant_interface_update_country(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* country =
        fi_w1_wpa_supplicant1_interface_get_country(priv->proxy);
    if (g_strcmp0(priv->country, country)) {
        g_free(priv->country);
        self->country = priv->country = g_strdup(country);
        priv->pending_signals |= SIGNAL_BIT(COUNTRY);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_COUNTRY,
            self->country);
    }
}

static
void
gsupplicant_interface_update_driver(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* driver =
        fi_w1_wpa_supplicant1_interface_get_driver(priv->proxy);
    if (g_strcmp0(priv->driver, driver)) {
        g_free(priv->driver);
        self->driver = priv->driver = g_strdup(driver);
        priv->pending_signals |= SIGNAL_BIT(DRIVER);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_DRIVER,
            self->driver);
    }
}

static
void
gsupplicant_interface_update_ifname(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* ifname =
        fi_w1_wpa_supplicant1_interface_get_ifname(priv->proxy);
    if (g_strcmp0(priv->ifname, ifname)) {
        g_free(priv->ifname);
        self->ifname = priv->ifname = g_strdup(ifname);
        priv->pending_signals |= SIGNAL_BIT(IFNAME);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_IFNAME,
            self->ifname);
    }
}

static
void
gsupplicant_interface_update_bridge_ifname(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* ifname =
        fi_w1_wpa_supplicant1_interface_get_bridge_ifname(priv->proxy);
    if (g_strcmp0(priv->bridge_ifname, ifname)) {
        g_free(priv->bridge_ifname);
        self->bridge_ifname = priv->bridge_ifname = g_strdup(ifname);
        priv->pending_signals |= SIGNAL_BIT(BRIDGE_IFNAME);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_BRIDGE_IFNAME,
            self->bridge_ifname);
    }
}

static
void
gsupplicant_interface_update_current_bss(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* bss =
        gsupplicant_interface_association_path_filter(
            fi_w1_wpa_supplicant1_interface_get_current_bss(priv->proxy));
    if (g_strcmp0(priv->current_bss, bss)) {
        g_free(priv->current_bss);
        self->current_bss = priv->current_bss = g_strdup(bss);
        priv->pending_signals |= SIGNAL_BIT(CURRENT_BSS);
        GVERBOSE("[%s] %s: %s", priv->path, PROXY_PROPERTY_NAME_CURRENT_BSS,
            self->current_bss);
    }
}

static
void
gsupplicant_interface_update_current_network(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    const char* network =
        gsupplicant_interface_association_path_filter(
            fi_w1_wpa_supplicant1_interface_get_current_network(priv->proxy));
    if (g_strcmp0(priv->current_network, network)) {
        g_free(priv->current_network);
        self->current_network = priv->current_network = g_strdup(network);
        priv->pending_signals |= SIGNAL_BIT(CURRENT_NETWORK);
        GVERBOSE("[%s] %s: %s", priv->path,
            PROXY_PROPERTY_NAME_CURRENT_NETWORK, self->current_network);
    }
}

static
void
gsupplicant_interface_update_bsss(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    gchar** bsss = (char**)(self->valid ?
        fi_w1_wpa_supplicant1_interface_get_bsss(priv->proxy) : NULL);
    /* If the stub is generated by gdbus-codegen < 2.56 then the getting
     * returns shallow copy, i.e. the return result should be released
     * with g_free(), but the individual strings must not be modified. */
    if (!gutil_strv_equal((const GStrV*)bsss, priv->bsss)) {
#if STRV_GETTERS_RETURN_SHALLOW_COPY
        GStrV* ptr;
        for (ptr = bsss; *ptr; ptr++) {
            *ptr = g_strdup(*ptr);
        }
#else
        bsss = g_strdupv(bsss);
#endif
        g_strfreev(priv->bsss);
        self->bsss = priv->bsss = bsss;
        priv->pending_signals |= SIGNAL_BIT(BSSS);
    }
#if STRV_GETTERS_RETURN_SHALLOW_COPY
    else {
        g_free(bsss);
    }
#endif
}

static
void
gsupplicant_interface_update_networks(
    GSupplicantInterface* self)
{
    GSupplicantInterfacePriv* priv = self->priv;
    gchar** networks = (char**)(self->valid ?
        fi_w1_wpa_supplicant1_interface_get_networks(priv->proxy) : NULL);
    /* If the stub is generated by gdbus-codegen < 2.56 then the getting
     * returns shallow copy, i.e. the return result should be released
     * with g_free(), but the individual strings must not be modified. */
    if (!gutil_strv_equal((const GStrV*)networks, priv->networks)) {
#if STRV_GETTERS_RETURN_SHALLOW_COPY
        GStrV* ptr;
        for (ptr = networks; *ptr; ptr++) {
            *ptr = g_strdup(*ptr);
        }
#else
        networks = g_strdupv(networks);
#endif
        g_strfreev(priv->networks);
        self->networks = priv->networks = networks;
        priv->pending_signals |= SIGNAL_BIT(NETWORKS);
    }
#if STRV_GETTERS_RETURN_SHALLOW_COPY
    else {
        g_free(networks);
    }
#endif
}

static
void
gsupplicant_interface_notify_state(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_state(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_scanning(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_scanning(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_ap_scan(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_ap_scan(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_scan_interval(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_scan_interval(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_caps(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_caps(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_country(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_country(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_driver(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_driver(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_ifname(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_ifname(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_bridge_ifname(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_bridge_ifname(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_current_bss(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_current_bss(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_current_network(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_current_network(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_bsss(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_bsss(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_notify_networks(
    FiW1Wpa_supplicant1Interface* proxy,
    GParamSpec* param,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    gsupplicant_interface_update_networks(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_proxy_bss_added(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* path,
    GVariant* properties,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GDEBUG("BSS added: %s", path);
    if (!gutil_strv_contains(priv->bsss, path)) {
        self->bsss = priv->bsss = gutil_strv_add(priv->bsss, path);
        priv->pending_signals |= SIGNAL_BIT(BSSS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_bss_removed(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* path,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    const int pos = gutil_strv_find(priv->bsss, path);
    GDEBUG("BSS removed: %s", path);
    if (pos >= 0) {
        self->bsss = priv->bsss = gutil_strv_remove_at(priv->bsss, pos, TRUE);
        priv->pending_signals |= SIGNAL_BIT(BSSS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_network_added(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* path,
    GVariant* properties,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GDEBUG("Network added: %s", path);
    if (!gutil_strv_contains(priv->networks, path)) {
        self->networks = priv->networks = gutil_strv_add(priv->networks, path);
        priv->pending_signals |= SIGNAL_BIT(NETWORKS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_network_removed(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* path,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    const int pos = gutil_strv_find(priv->networks, path);
    GDEBUG("Network removed: %s", path);
    if (pos >= 0) {
        self->networks = priv->networks =
            gutil_strv_remove_at(priv->networks, pos, TRUE);
        priv->pending_signals |= SIGNAL_BIT(NETWORKS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_network_selected(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* path,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GDEBUG("Network selected: %s", path);
    if (g_strcmp0(priv->current_network, path)) {
        g_free(priv->current_network);
        self->current_network = priv->current_network = g_strdup(path);
        priv->pending_signals |= SIGNAL_BIT(CURRENT_NETWORK);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_sta_authorized(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* mac,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GDEBUG("Station authorized: %s", mac);
    if (!gutil_strv_contains(priv->stations, mac)) {
        self->stations = priv->stations = gutil_strv_add(priv->stations, mac);
        priv->pending_signals |= SIGNAL_BIT(STATIONS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_proxy_sta_deauthorized(
    FiW1Wpa_supplicant1Interface* proxy,
    const char* mac,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    const int pos = gutil_strv_find(priv->stations, mac);
    GDEBUG("Station deauthorized: %s", mac);
    if (pos >= 0) {
        self->stations = priv->stations =
            gutil_strv_remove_at(priv->stations, pos, TRUE);
        priv->pending_signals |= SIGNAL_BIT(STATIONS);
        gsupplicant_interface_emit_pending_signals(self);
    }
}

static
void
gsupplicant_interface_supplicant_valid_changed(
    GSupplicant* supplicant,
    void* data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GASSERT(self->supplicant == supplicant);
    gsupplicant_interface_update_valid(self);
    gsupplicant_interface_update_present(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_supplicant_interfaces_changed(
    GSupplicant* supplicant,
    void* data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GASSERT(self->supplicant == supplicant);
    gsupplicant_interface_update_present(self);
    gsupplicant_interface_emit_pending_signals(self);
}

static
void
gsupplicant_interface_create2(
    GObject* bus,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GError* error = NULL;
    GASSERT(!self->valid);
    GASSERT(!priv->proxy);
    priv->proxy = fi_w1_wpa_supplicant1_interface_proxy_new_for_bus_finish(
        result, &error);
    if (priv->proxy) {
        priv->proxy_handler_id[PROXY_BSS_ADDED] =
            g_signal_connect(priv->proxy, "bssadded",
            G_CALLBACK(gsupplicant_interface_proxy_bss_added), self);
        priv->proxy_handler_id[PROXY_BSS_REMOVED] =
            g_signal_connect(priv->proxy, "bssremoved",
            G_CALLBACK(gsupplicant_interface_proxy_bss_removed), self);
        priv->proxy_handler_id[PROXY_NETWORK_ADDED] =
            g_signal_connect(priv->proxy, "network-added",
            G_CALLBACK(gsupplicant_interface_proxy_network_added), self);
        priv->proxy_handler_id[PROXY_NETWORK_REMOVED] =
            g_signal_connect(priv->proxy, "network-removed",
            G_CALLBACK(gsupplicant_interface_proxy_network_removed), self);
        priv->proxy_handler_id[PROXY_NETWORK_SELECTED] =
            g_signal_connect(priv->proxy, "network-selected",
            G_CALLBACK(gsupplicant_interface_proxy_network_selected), self);
        priv->proxy_handler_id[PROXY_STA_AUTHORIZED] =
            g_signal_connect(priv->proxy, "sta-authorized",
            G_CALLBACK(gsupplicant_interface_proxy_sta_authorized), self);
        priv->proxy_handler_id[PROXY_STA_DEAUTHORIZED] =
            g_signal_connect(priv->proxy, "sta-deauthorized",
            G_CALLBACK(gsupplicant_interface_proxy_sta_deauthorized), self);

        priv->proxy_handler_id[PROXY_NOTIFY_STATE] =
            g_signal_connect(priv->proxy, "notify::state",
            G_CALLBACK(gsupplicant_interface_notify_state), self);
        priv->proxy_handler_id[PROXY_NOTIFY_SCANNING] =
            g_signal_connect(priv->proxy, "notify::scanning",
            G_CALLBACK(gsupplicant_interface_notify_scanning), self);
        priv->proxy_handler_id[PROXY_NOTIFY_AP_SCAN] =
            g_signal_connect(priv->proxy, "notify::ap-scan",
            G_CALLBACK(gsupplicant_interface_notify_ap_scan), self);
        priv->proxy_handler_id[PROXY_NOTIFY_SCAN_INTERVAL] =
            g_signal_connect(priv->proxy, "notify::scan-interval",
            G_CALLBACK(gsupplicant_interface_notify_scan_interval), self);
        priv->proxy_handler_id[PROXY_NOTIFY_CAPABILITIES] =
            g_signal_connect(priv->proxy, "notify::capabilities",
            G_CALLBACK(gsupplicant_interface_notify_caps), self);
        priv->proxy_handler_id[PROXY_NOTIFY_COUNTRY] =
            g_signal_connect(priv->proxy, "notify::country",
            G_CALLBACK(gsupplicant_interface_notify_country), self);
        priv->proxy_handler_id[PROXY_NOTIFY_DRIVER] =
            g_signal_connect(priv->proxy, "notify::driver",
            G_CALLBACK(gsupplicant_interface_notify_driver), self);
        priv->proxy_handler_id[PROXY_NOTIFY_IFNAME] =
            g_signal_connect(priv->proxy, "notify::ifname",
            G_CALLBACK(gsupplicant_interface_notify_ifname), self);
        priv->proxy_handler_id[PROXY_NOTIFY_BRIDGE_IFNAME] =
            g_signal_connect(priv->proxy, "notify::bridge-ifname",
            G_CALLBACK(gsupplicant_interface_notify_bridge_ifname), self);
        priv->proxy_handler_id[PROXY_NOTIFY_CURRENT_BSS] =
            g_signal_connect(priv->proxy, "notify::current-bss",
            G_CALLBACK(gsupplicant_interface_notify_current_bss), self);
        priv->proxy_handler_id[PROXY_NOTIFY_CURRENT_NETWORK] =
            g_signal_connect(priv->proxy, "notify::current-network",
            G_CALLBACK(gsupplicant_interface_notify_current_network), self);
        priv->proxy_handler_id[PROXY_NOTIFY_BSSS] =
            g_signal_connect(priv->proxy, "notify::bsss",
            G_CALLBACK(gsupplicant_interface_notify_bsss), self);
        priv->proxy_handler_id[PROXY_NOTIFY_NETWORKS] =
            g_signal_connect(priv->proxy, "notify::networks",
            G_CALLBACK(gsupplicant_interface_notify_networks), self);

        priv->supplicant_handler_id[SUPPLICANT_VALID_CHANGED] =
            gsupplicant_add_handler(self->supplicant,
                GSUPPLICANT_PROPERTY_VALID,
                gsupplicant_interface_supplicant_valid_changed, self);
        priv->supplicant_handler_id[SUPPLICANT_INTERFACES_CHANGED] =
            gsupplicant_add_handler(self->supplicant,
                GSUPPLICANT_PROPERTY_INTERFACES,
                gsupplicant_interface_supplicant_interfaces_changed, self);

        gsupplicant_interface_update_valid(self);
        gsupplicant_interface_update_present(self);
        gsupplicant_interface_update_caps(self);
        gsupplicant_interface_update_state(self);
        gsupplicant_interface_update_scanning(self);
        gsupplicant_interface_update_ap_scan(self);
        gsupplicant_interface_update_scan_interval(self);
        gsupplicant_interface_update_country(self);
        gsupplicant_interface_update_driver(self);
        gsupplicant_interface_update_ifname(self);
        gsupplicant_interface_update_bridge_ifname(self);
        gsupplicant_interface_update_current_bss(self);
        gsupplicant_interface_update_current_network(self);
        gsupplicant_interface_update_bsss(self);
        gsupplicant_interface_update_networks(self);

        gsupplicant_interface_emit_pending_signals(self);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    gsupplicant_interface_unref(self);
}

static
void
gsupplicant_interface_create1(
    GObject* bus,
    GAsyncResult* result,
    gpointer data)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(data);
    GSupplicantInterfacePriv* priv = self->priv;
    GError* error = NULL;
    priv->bus = g_bus_get_finish(result, &error);
    if (priv->bus) {
        fi_w1_wpa_supplicant1_interface_proxy_new(priv->bus,
            G_DBUS_PROXY_FLAGS_NONE, GSUPPLICANT_SERVICE, priv->path, NULL,
            gsupplicant_interface_create2, gsupplicant_interface_ref(self));
    } else {
        GERR("[%s] %s", priv->path, error->message);
        g_error_free(error);
    }
    gsupplicant_interface_unref(self);
}

static
void
gsupplicant_interface_destroyed(
    gpointer key,
    GObject* dead)
{
    GVERBOSE_("%s", (char*)key);
    GASSERT(gsupplicant_interface_table);
    if (gsupplicant_interface_table) {
        GASSERT(g_hash_table_lookup(gsupplicant_interface_table, key) == dead);
        g_hash_table_remove(gsupplicant_interface_table, key);
        if (g_hash_table_size(gsupplicant_interface_table) == 0) {
            g_hash_table_unref(gsupplicant_interface_table);
            gsupplicant_interface_table = NULL;
        }
    }
}

static
GSupplicantInterface*
gsupplicant_interface_create(
    const char* path)
{
    GSupplicantInterface* self = g_object_new(GSUPPLICANT_INTERFACE_TYPE,NULL);
    GSupplicantInterfacePriv* priv = self->priv;
    self->supplicant = gsupplicant_new();
    self->path = priv->path = g_strdup(path);
    g_bus_get(GSUPPLICANT_BUS_TYPE, NULL, gsupplicant_interface_create1,
        gsupplicant_interface_ref(self));
    return self;
}

/*==========================================================================*
 * API
 *==========================================================================*/

GSupplicantInterface*
gsupplicant_interface_new(
    const char* path)
{
    GSupplicantInterface* self = NULL;
    if (G_LIKELY(path)) {
        self = gsupplicant_interface_table ?
            gsupplicant_interface_ref(g_hash_table_lookup(
                gsupplicant_interface_table, path)) : NULL;
        if (!self) {
            gpointer key = g_strdup(path);
            self = gsupplicant_interface_create(path);
            if (!gsupplicant_interface_table) {
                gsupplicant_interface_table =
                    g_hash_table_new_full(g_str_hash, g_str_equal,
                        g_free, NULL);
            }
            g_hash_table_replace(gsupplicant_interface_table, key, self);
            g_object_weak_ref(G_OBJECT(self), gsupplicant_interface_destroyed,
                key);
        }
    }
    return self;
}

GSupplicantInterface*
gsupplicant_interface_ref(
    GSupplicantInterface* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(GSUPPLICANT_INTERFACE(self));
        return self;
    } else {
        return NULL;
    }
}

void
gsupplicant_interface_unref(
    GSupplicantInterface* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(GSUPPLICANT_INTERFACE(self));
    }
}

gulong
gsupplicant_interface_add_property_changed_handler(
    GSupplicantInterface* self,
    GSUPPLICANT_INTERFACE_PROPERTY property,
    GSupplicantInterfacePropertyFunc fn,
    void* data)
{
    if (G_LIKELY(self) && G_LIKELY(fn)) {
        const char* signal_name;
        char buf[sizeof(SIGNAL_PROPERTY_CHANGED_NAME) + 2 +
            SIGNAL_PROPERTY_CHANGED_DETAIL_MAX_LEN];
        if (property) {
            snprintf(buf, sizeof(buf), SIGNAL_PROPERTY_CHANGED_NAME "::"
                SIGNAL_PROPERTY_CHANGED_DETAIL, property);
            buf[sizeof(buf)-1] = 0;
            signal_name = buf;
        } else {
            signal_name = SIGNAL_PROPERTY_CHANGED_NAME;
        }
        return g_signal_connect(self, signal_name, G_CALLBACK(fn), data);
    }
    return 0;
}

gulong
gsupplicant_interface_add_handler(
    GSupplicantInterface* self,
    GSUPPLICANT_INTERFACE_PROPERTY prop,
    GSupplicantInterfaceFunc fn,
    void* data)
{
    if (G_LIKELY(self) && G_LIKELY(fn)) {
        const char* signame;
        switch (prop) {
#define SIGNAL_NAME_(P,p) case GSUPPLICANT_INTERFACE_PROPERTY_##P: \
            signame = gsupplicant_interface_signame[SIGNAL_##P##_CHANGED]; \
            break;
            GSUPPLICANT_INTERFACE_PROPERTIES_(SIGNAL_NAME_)
        default:
            signame = NULL;
            break;
        }
        if (G_LIKELY(signame)) {
            return g_signal_connect(self, signame, G_CALLBACK(fn), data);
        }
    }
    return 0;
}

gboolean
gsupplicant_interface_set_ap_scan(
    GSupplicantInterface* self,
    guint ap_scan)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        fi_w1_wpa_supplicant1_interface_set_ap_scan(priv->proxy, ap_scan);
        return TRUE;
    }
    return FALSE;
}

gboolean
gsupplicant_interface_set_country(
    GSupplicantInterface* self,
    const char* country)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        if (!country) country = "";
        fi_w1_wpa_supplicant1_interface_set_country(priv->proxy, country);
        return TRUE;
    }
    return FALSE;
}

void
gsupplicant_interface_remove_handler(
    GSupplicantInterface* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
gsupplicant_interface_remove_handlers(
    GSupplicantInterface* self,
    gulong* ids,
    guint count)
{
    gutil_disconnect_handlers(self, ids, count);
}

GCancellable*
gsupplicant_interface_disconnect(
    GSupplicantInterface* self,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_call_void_void(self, NULL, fn, NULL, data,
        fi_w1_wpa_supplicant1_interface_call_disconnect);
}

GCancellable*
gsupplicant_interface_reassociate(
    GSupplicantInterface* self,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_call_void_void(self, NULL, fn, NULL, data,
        fi_w1_wpa_supplicant1_interface_call_reassociate);
}

GCancellable*
gsupplicant_interface_reconnect(
    GSupplicantInterface* self,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_call_void_void(self, NULL, fn, NULL, data,
        fi_w1_wpa_supplicant1_interface_call_reconnect);
}

GCancellable*
gsupplicant_interface_reattach(
    GSupplicantInterface* self,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_call_void_void(self, NULL, fn, NULL, data,
        fi_w1_wpa_supplicant1_interface_call_reattach);
}

static /* should be public? */
GCancellable*
gsupplicant_interface_add_blob_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* name,
    GBytes* blob,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify free,
    void* data)
{
    if (G_LIKELY(self) && self->valid && name && blob) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            cancel, gsupplicant_interface_call_finish_void, G_CALLBACK(fn),
            free, data);
        gsize size = 0;
        const guint8* data = g_bytes_get_data(blob, &size);
        fi_w1_wpa_supplicant1_interface_call_add_blob(
            priv->proxy,
            name,
            g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE, data, size, 1),
            call->cancel,
            gsupplicant_interface_call_finished, call);
        return call->cancel;
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_add_blob(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* name,
    GBytes* blob,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_add_blob_full(self, cancel, name, blob,
        fn, NULL, data);
}

static /* should be public? */
GCancellable*
gsupplicant_interface_remove_blob_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* name,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify free,
    void* data)
{
    if (name) {
        return gsupplicant_interface_call_string_void(self, cancel, name, fn,
            free, data, fi_w1_wpa_supplicant1_interface_call_remove_blob);
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_remove_blob(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* name,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_remove_blob_full(self, cancel, name,
        fn, NULL, data);
}

static /* should be public? */
GCancellable*
gsupplicant_interface_select_network_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify free,
    void* data)
{
    if (path && g_variant_is_object_path(path)) {
        return gsupplicant_interface_call_string_void(self, cancel, path, fn,
            free, data,fi_w1_wpa_supplicant1_interface_call_select_network);
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_select_network(
    GSupplicantInterface* self,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_select_network_full(self, NULL, path, fn,
        NULL, data);
}

static /* should be public? */
GCancellable*
gsupplicant_interface_remove_network_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify free,
    void* data)
{
    if (path && g_variant_is_object_path(path)) {
        return gsupplicant_interface_call_string_void(self, cancel, path, fn,
            free, data, fi_w1_wpa_supplicant1_interface_call_remove_network);
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_remove_network(
    GSupplicantInterface* self,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_remove_network_full(self, NULL, path, fn,
        NULL, data);
}

GCancellable*
gsupplicant_interface_remove_all_networks(
    GSupplicantInterface* self,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_remove_all_networks_full(self, NULL, fn,
        NULL, data);
}

GCancellable*
gsupplicant_interface_remove_all_networks_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    return gsupplicant_interface_call_void_void(self, cancel, fn, destroy,
        data, fi_w1_wpa_supplicant1_interface_call_remove_all_networks);
}

GCancellable*
gsupplicant_interface_scan(
    GSupplicantInterface* self,
    const GSupplicantScanParams* params,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantScanParams default_params;
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            NULL, gsupplicant_interface_call_finish_void, G_CALLBACK(fn),
            NULL, data);
        GVariantBuilder builder;
        GVariant* dict;

        /* Do passive scan by default */
        if (!params) {
            memset(&default_params, 0, sizeof(default_params));
            default_params.type = GSUPPLICANT_SCAN_TYPE_PASSIVE;
            params = &default_params;
        }

        /* Prepare scan parameters */
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        gsupplicant_dict_add_string(&builder, "Type",
            params->type == GSUPPLICANT_SCAN_TYPE_ACTIVE ?
            "active" : "passive");
        if (params->ssids) {
            gsupplicant_dict_add_value(&builder, "SSIDs",
                gsupplicant_variant_new_ayy(params->ssids));
        }
        if (params->ies) {
            gsupplicant_dict_add_value(&builder, "IEs",
                gsupplicant_variant_new_ayy(params->ies));
        }
        if (params->channels) {
            guint i;
            const GSupplicantScanFrequency* freq = params->channels->freq;
            GVariantBuilder auu;
            g_variant_builder_init(&auu, G_VARIANT_TYPE("a(uu)"));
            for (i=0; i<params->channels->count; i++, freq++) {
                g_variant_builder_add(&auu, "(uu)", freq->center, freq->width);
            }
            gsupplicant_dict_add_value(&builder, "Channels",
                g_variant_builder_end(&auu));
        }
        if (params->flags & GSUPPLICANT_SCAN_PARAM_ALLOW_ROAM) {
            gsupplicant_dict_add_boolean(&builder, "AllowRoam",
                params->allow_roam);
        }

        /* Submit the call */
        dict = g_variant_ref_sink(g_variant_builder_end(&builder));
        fi_w1_wpa_supplicant1_interface_call_scan(priv->proxy, dict,
            call->cancel, gsupplicant_interface_call_finished, call);
        g_variant_unref(dict);
        return call->cancel;
    }
    return NULL;
}

static /* should be public? */
GCancellable*
gsupplicant_interface_auto_scan_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const char* param,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    if (!param) param = "";
    return gsupplicant_interface_call_string_void(self, cancel, param, fn,
        destroy, data, fi_w1_wpa_supplicant1_interface_call_auto_scan);
}

GCancellable*
gsupplicant_interface_auto_scan(
    GSupplicantInterface* self,
    const char* param,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    return gsupplicant_interface_auto_scan_full(self, NULL, param, fn,
        NULL, data);
}

GCancellable*
gsupplicant_interface_remove_flush_bss(
    GSupplicantInterface* self,
    guint age,
    GSupplicantInterfaceResultFunc fn,
    void* data)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            NULL, gsupplicant_interface_call_finish_void, G_CALLBACK(fn),
            NULL, data);
        fi_w1_wpa_supplicant1_interface_call_flush_bss(priv->proxy, age,
            call->cancel, gsupplicant_interface_call_finished, call);
        return call->cancel;
    }
    return NULL;
}

GCancellable*
gsupplicant_interface_signal_poll(
    GSupplicantInterface* self,
    GSupplicantInterfaceSignalPollResultFunc fn,
    void* data)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceCall* call = gsupplicant_interface_call_new(self,
            NULL, gsupplicant_interface_call_finish_signal_poll,
            G_CALLBACK(fn), NULL, data);
        fi_w1_wpa_supplicant1_interface_call_signal_poll(priv->proxy,
            call->cancel, gsupplicant_interface_call_finished, call);
        return call->cancel;
    }
    return NULL;
}

static
void
gsupplicant_interface_add_network6(
    GSupplicantNetwork* network,
    void* data)
{
    GSupplicantInterfaceAddNetworkCall* call = data;
    if (network->enabled) {
        GVERBOSE_("enabled %s", call->path);
        gsupplicant_interface_call_add_network_finish(call, NULL);
    }
}

static
gboolean
gsupplicant_interface_add_network5(
    GSupplicantInterfaceAddNetworkCall* call,
    GError** error)
{
    gboolean done = TRUE;
    GASSERT(call->network->valid);
    if (!call->network->enabled) {
        /* Have to wait for the network to become enabled */
        if (!call->network_event_id[ADD_NETWORK_ENABLED_CHANGED]) {
            call->network_event_id[ADD_NETWORK_ENABLED_CHANGED] =
                gsupplicant_network_add_handler(call->network,
                    GSUPPLICANT_NETWORK_PROPERTY_ENABLED,
                    gsupplicant_interface_add_network6, call);
            if (gsupplicant_network_set_enabled(call->network, TRUE)) {
                GVERBOSE_("waiting for %s to become enabled", call->path);
                done = FALSE;
            } else {
                g_propagate_error(error, g_error_new(G_IO_ERROR,
                    G_IO_ERROR_FAILED, "Failed to enable %s", call->path));
            }
        }
    }
    return done;
}

static
void
gsupplicant_interface_add_network4(
    GSupplicantNetwork* network,
    void* data)
{
    GSupplicantInterfaceAddNetworkCall* call = data;
    GVERBOSE_("%s has become %svalid", call->path, network->valid ? "" : "in");
    if (network->valid) {
        GError* error = NULL;
        gboolean done = gsupplicant_interface_add_network5(call, &error);
        if (done) {
            gsupplicant_interface_call_add_network_finish(call, error);
        }
        if (error) {
            g_error_free(error);
        }
    } else {
        gsupplicant_interface_call_add_network_finish_error(call);
    }
}

static
gboolean
gsupplicant_interface_add_network3(
    GSupplicantInterfaceAddNetworkCall* call,
    GError** error)
{
    if (!call->network->valid) {
        /* Have to wait for the network to initialize */
        GVERBOSE_("waiting for %s to initialize", call->path);
        if (!call->network_event_id[ADD_NETWORK_VALID_CHANGED]) {
            call->network_event_id[ADD_NETWORK_VALID_CHANGED] =
                gsupplicant_network_add_handler(call->network,
                    GSUPPLICANT_NETWORK_PROPERTY_VALID,
                    gsupplicant_interface_add_network4, call);
        }
        return FALSE;
    } else {
        return gsupplicant_interface_add_network5(call, error);
    }
}

static
void
gsupplicant_interface_add_network2(
    GObject* obj,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    gboolean done = TRUE;
    GSupplicantInterfaceAddNetworkCall* call = data;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    call->pending = FALSE;
    GASSERT(proxy == FI_W1_WPA_SUPPLICANT1_INTERFACE(obj));
    if (fi_w1_wpa_supplicant1_interface_call_select_network_finish(proxy,
        result, &error)) {
        /* Network has been successfully selected */
        GVERBOSE_("selected %s", call->path);
        if (call->flags & GSUPPLICANT_ADD_NETWORK_ENABLE) {
            done = gsupplicant_interface_add_network3(call, &error);
        }
    }
    if (done) {
        gsupplicant_interface_call_add_network_finish(call, error);
    }
    if (error) {
        g_error_free(error);
    }
}

static
void
gsupplicant_interface_add_network1(
    GObject* obj,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    gboolean done = TRUE;
    GSupplicantInterfaceAddNetworkCall* call = data;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    call->pending = FALSE;
    GASSERT(proxy == FI_W1_WPA_SUPPLICANT1_INTERFACE(obj));
    if (fi_w1_wpa_supplicant1_interface_call_add_network_finish(proxy,
        &call->path, result, &error)) {
        GVERBOSE_("added %s", call->path);
        if (call->flags & GSUPPLICANT_ADD_NETWORK_ENABLE) {
            /* We will need GSupplicantNetwork */
            call->network = gsupplicant_network_new(call->path);
        }
        if (call->flags & GSUPPLICANT_ADD_NETWORK_SELECT) {
            /*
             * Select the network first. Also, while it's being selected,
             * the GSupplicantNetwork will become valid.
             */
            call->pending = TRUE;
            fi_w1_wpa_supplicant1_interface_call_select_network(proxy,
                call->path, call->cancel,
                gsupplicant_interface_add_network2, call);
            done = FALSE;
        } else if (call->flags & GSUPPLICANT_ADD_NETWORK_ENABLE) {
            /* Need to enable the network without selecting it */
            done = gsupplicant_interface_add_network3(call, &error);
        }
    }
    if (done) {
        gsupplicant_interface_call_add_network_finish(call, error);
    }
    if (error) {
        g_error_free(error);
    }
}

static
void
gsupplicant_interface_add_network_pre1(
    GObject* obj,
    GAsyncResult* result,
    gpointer data) {

    GError* error = NULL;
    GSupplicantInterfaceAddNetworkCall* call = data;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    GASSERT(proxy == FI_W1_WPA_SUPPLICANT1_INTERFACE(obj));

    if (fi_w1_wpa_supplicant1_interface_call_add_blob_finish(proxy,
        result, &error)) {
        gpointer name, blob;

        if (g_hash_table_iter_next(&call->iter, &name, &blob)) {
            gsize size = 0;
            const guint8* data = g_bytes_get_data(blob, &size);

            fi_w1_wpa_supplicant1_interface_call_add_blob(
                    proxy,
                    name,
                    g_variant_new_fixed_array(
                        G_VARIANT_TYPE_BYTE, data, size, 1),
                    call->cancel,
                    gsupplicant_interface_add_network_pre1, call);
        } else {
            g_hash_table_unref(call->blobs);
            call->blobs = NULL;
            fi_w1_wpa_supplicant1_interface_call_add_network(proxy,
                    call->args, call->cancel, gsupplicant_interface_add_network1,
                    call);
            g_variant_unref(call->args);
            call->args = NULL;
        }
    }
    if (error) {
        g_error_free(error);
    }
}
static
void
gsupplicant_interface_add_network0(
    GObject* obj,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    GSupplicantInterfaceAddNetworkCall* call = data;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    GASSERT(proxy == FI_W1_WPA_SUPPLICANT1_INTERFACE(obj));
    if (fi_w1_wpa_supplicant1_interface_call_remove_all_networks_finish(proxy,
        result, &error)) {
        GVERBOSE_("removed all networks");
        call->pending = TRUE;
        if (call->blobs) {
            gpointer name, blob;
            gsize size = 0;
            const guint8* data;

            g_hash_table_iter_next(&call->iter, &name, &blob);
            data = g_bytes_get_data(blob, &size);

            fi_w1_wpa_supplicant1_interface_call_add_blob(
                proxy,
                name,
                g_variant_new_fixed_array(
                    G_VARIANT_TYPE_BYTE, data, size, 1),
                call->cancel,
                gsupplicant_interface_add_network_pre1, call);
        } else {
            fi_w1_wpa_supplicant1_interface_call_add_network(proxy, call->args,
                call->cancel, gsupplicant_interface_add_network1, call);
            g_variant_unref(call->args);
            call->args = NULL;
        }
    } else {
        call->pending = FALSE;
        gsupplicant_interface_call_add_network_finish(call, error);
    }
    if (error) {
        g_error_free(error);
    }
}

static
void
gsupplicant_interface_add_network_pre0(
    GObject* obj,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    GSupplicantInterfaceAddNetworkCall* call = data;
    FiW1Wpa_supplicant1Interface* proxy = call->iface->priv->proxy;
    GASSERT(proxy == FI_W1_WPA_SUPPLICANT1_INTERFACE(obj));
    if (fi_w1_wpa_supplicant1_interface_call_remove_blob_finish(proxy,
            result, &error) ||
        gsupplicant_is_error(error, GSUPPLICANT_ERROR_BLOB_UNKNOWN)) {
            const gchar *name;
            if (g_hash_table_iter_next(&call->iter, (gpointer*)&name, NULL)) {
                fi_w1_wpa_supplicant1_interface_call_remove_blob(
                    proxy, name,
                    call->cancel, gsupplicant_interface_add_network_pre0,
                    call);
            } else {
                g_hash_table_iter_init(&call->iter, call->blobs);
                fi_w1_wpa_supplicant1_interface_call_remove_all_networks(
                    proxy, call->cancel, gsupplicant_interface_add_network0,
                    call);
            }
    } else {
        call->pending = FALSE;
        gsupplicant_interface_call_add_network_finish(call, error);
    }
    g_clear_error(&error);
}

GCancellable*
gsupplicant_interface_add_network_full2(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const GSupplicantNetworkParams* np,
    guint flags,
    GHashTable* blobs,
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    if (G_LIKELY(self) && self->valid && np) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceAddNetworkCall* call =
            gsupplicant_interface_add_network_call_new(self, cancel, np,
                flags, blobs, fn, destroy, data);
        call->pending = TRUE;

        if (flags & GSUPPLICANT_ADD_NETWORK_DELETE_OTHER) {
            const gchar *name;
            if (blobs && g_hash_table_iter_next(&call->iter, (gpointer*)&name, NULL)) {
                fi_w1_wpa_supplicant1_interface_call_remove_blob(
                    priv->proxy, name,
                    call->cancel, gsupplicant_interface_add_network_pre0,
                    call);
            } else {
                fi_w1_wpa_supplicant1_interface_call_remove_all_networks(
                    priv->proxy, call->cancel, gsupplicant_interface_add_network0,
                    call);
            }
        } else {
            if (call->blobs) {
                gpointer name, blob;
                gsize size = 0;
                const guint8* data;

                g_hash_table_iter_next(&call->iter, &name, &blob);
                data = g_bytes_get_data(blob, &size);

                fi_w1_wpa_supplicant1_interface_call_add_blob(
                    priv->proxy,
                    name,
                    g_variant_new_fixed_array(
                        G_VARIANT_TYPE_BYTE, data, size, 1),
                    call->cancel,
                    gsupplicant_interface_add_network_pre1, call);
            } else {
                fi_w1_wpa_supplicant1_interface_call_add_network(priv->proxy,
                    call->args, call->cancel, gsupplicant_interface_add_network1,
                    call);
                g_variant_unref(call->args);
                call->args = NULL;
            }
        }
        return call->cancel;
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_add_network_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const GSupplicantNetworkParams* np,
    guint flags,
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    return gsupplicant_interface_add_network_full2(self, cancel, np, flags,
        NULL, fn, destroy, data);
}

GCancellable*
gsupplicant_interface_add_network(
    GSupplicantInterface* self,
    const GSupplicantNetworkParams* np,
    guint flags,
    GSupplicantInterfaceStringResultFunc fn,
    void* data)
{
    return gsupplicant_interface_add_network_full(self, NULL, np, flags, fn,
        NULL, data);
}

GCancellable*
gsupplicant_interface_wps_connect_full(
    GSupplicantInterface* self,
    GCancellable* cancel,
    const GSupplicantWPSParams* params,
    gint timeout_sec, /* 0 = default, negative = no timeout */
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data)
{
    if (G_LIKELY(self) && self->valid && params) {
        GSupplicantInterfacePriv* priv = self->priv;
        GSupplicantInterfaceWPSConnect* connect =
            gsupplicant_interface_wps_connect_new(self, cancel, params,
                timeout_sec, fn, destroy, data);
        GVERBOSE_("%s creating WPS proxy", priv->path);
        fi_w1_wpa_supplicant1_interface_wps_proxy_new(priv->bus,
            G_DBUS_PROXY_FLAGS_NONE, GSUPPLICANT_SERVICE, priv->path,
            connect->cancel, gsupplicant_interface_wps_connect1, connect);
        return connect->cancel;
    }
    gsupplicant_cancel_later(cancel);
    return NULL;
}

GCancellable*
gsupplicant_interface_wps_connect(
    GSupplicantInterface* self,
    const GSupplicantWPSParams* params,
    GSupplicantInterfaceStringResultFunc fn,
    void* data)
{
    return gsupplicant_interface_wps_connect_full(self, NULL, params, 0, fn,
        NULL, data);
}

const char*
gsupplicant_interface_state_name(
    GSUPPLICANT_INTERFACE_STATE state)
{
    return gsupplicant_name_int_find_int(state, gsupplicant_interface_states,
        G_N_ELEMENTS(gsupplicant_interface_states));
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
gsupplicant_interface_init(
    GSupplicantInterface* self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, GSUPPLICANT_INTERFACE_TYPE,
        GSupplicantInterfacePriv);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
gsupplicant_interface_dispose(
    GObject* object)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(object);
    GSupplicantInterfacePriv* priv = self->priv;
    gsupplicant_interface_clear_wps_credentials(self);
    if (priv->proxy) {
        gutil_disconnect_handlers(priv->proxy, priv->proxy_handler_id,
            G_N_ELEMENTS(priv->proxy_handler_id));
        g_object_unref(priv->proxy);
        priv->proxy = NULL;
    }
    gsupplicant_remove_handlers(self->supplicant, priv->supplicant_handler_id,
        G_N_ELEMENTS(priv->supplicant_handler_id));
    if (priv->bus) {
        g_object_unref(priv->bus);
        priv->bus = NULL;
    }
    G_OBJECT_CLASS(SUPER_CLASS)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
gsupplicant_interface_finalize(
    GObject* object)
{
    GSupplicantInterface* self = GSUPPLICANT_INTERFACE(object);
    GSupplicantInterfacePriv* priv = self->priv;
    GASSERT(!priv->bus);
    GASSERT(!priv->proxy);
    g_strfreev(priv->bsss);
    g_strfreev(priv->networks);
    g_strfreev(priv->stations);
    g_free(priv->path);
    g_free(priv->country);
    g_free(priv->driver);
    g_free(priv->ifname);
    g_free(priv->bridge_ifname);
    g_free(priv->current_bss);
    g_free(priv->current_network);
    gsupplicant_unref(self->supplicant);
    G_OBJECT_CLASS(SUPER_CLASS)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
gsupplicant_interface_class_init(
    GSupplicantInterfaceClass* klass)
{
    int i;
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = gsupplicant_interface_dispose;
    object_class->finalize = gsupplicant_interface_finalize;
    g_type_class_add_private(klass, sizeof(GSupplicantInterfacePriv));
    for (i=0; i<SIGNAL_PROPERTY_CHANGED; i++) {
        gsupplicant_interface_signals[i] =  g_signal_new(
            gsupplicant_interface_signame[i], G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    }
    gsupplicant_interface_signals[SIGNAL_PROPERTY_CHANGED] =
        g_signal_new(SIGNAL_PROPERTY_CHANGED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_UINT);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
