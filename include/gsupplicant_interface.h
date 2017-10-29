/*
 * Copyright (C) 2015-2017 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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
 *   3. Neither the name of Jolla Ltd nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
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

#ifndef GSUPPLICANT_INTERFACE_H
#define GSUPPLICANT_INTERFACE_H

#include <gsupplicant_types.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum gsupplicant_interface_state {
    GSUPPLICANT_INTERFACE_STATE_UNKNOWN,
    GSUPPLICANT_INTERFACE_STATE_DISCONNECTED,
    GSUPPLICANT_INTERFACE_STATE_INACTIVE,
    GSUPPLICANT_INTERFACE_STATE_SCANNING,
    GSUPPLICANT_INTERFACE_STATE_AUTHENTICATING,
    GSUPPLICANT_INTERFACE_STATE_ASSOCIATING,
    GSUPPLICANT_INTERFACE_STATE_ASSOCIATED,
    GSUPPLICANT_INTERFACE_STATE_4WAY_HANDSHAKE,
    GSUPPLICANT_INTERFACE_STATE_GROUP_HANDSHAKE,
    GSUPPLICANT_INTERFACE_STATE_COMPLETED
} GSUPPLICANT_INTERFACE_STATE;

typedef enum gsupplicant_interface_property {
    GSUPPLICANT_INTERFACE_PROPERTY_ANY,
    GSUPPLICANT_INTERFACE_PROPERTY_VALID,
    GSUPPLICANT_INTERFACE_PROPERTY_PRESENT,
    GSUPPLICANT_INTERFACE_PROPERTY_CAPS,
    GSUPPLICANT_INTERFACE_PROPERTY_STATE,
    GSUPPLICANT_INTERFACE_PROPERTY_WPS_CREDENTIALS,
    GSUPPLICANT_INTERFACE_PROPERTY_SCANNING,
    GSUPPLICANT_INTERFACE_PROPERTY_AP_SCAN,
    GSUPPLICANT_INTERFACE_PROPERTY_COUNTRY,
    GSUPPLICANT_INTERFACE_PROPERTY_DRIVER,
    GSUPPLICANT_INTERFACE_PROPERTY_IFNAME,
    GSUPPLICANT_INTERFACE_PROPERTY_BRIDGE_IFNAME,
    GSUPPLICANT_INTERFACE_PROPERTY_CURRENT_BSS,
    GSUPPLICANT_INTERFACE_PROPERTY_CURRENT_NETWORK,
    GSUPPLICANT_INTERFACE_PROPERTY_BSSS,
    GSUPPLICANT_INTERFACE_PROPERTY_NETWORKS,
    GSUPPLICANT_INTERFACE_PROPERTY_SCAN_INTERVAL,
    GSUPPLICANT_INTERFACE_PROPERTY_STATIONS,        /* Since 1.0.7 */
    GSUPPLICANT_INTERFACE_PROPERTY_COUNT
} GSUPPLICANT_INTERFACE_PROPERTY;

typedef struct gsupplicant_interface_caps {
    GSUPPLICANT_KEYMGMT keymgmt;
    GSUPPLICANT_CIPHER pairwise;
    GSUPPLICANT_CIPHER group;
    GSUPPLICANT_PROTOCOL protocol;
    GSUPPLICANT_AUTH auth_alg;
    guint scan;

#define GSUPPLICANT_INTERFACE_CAPS_SCAN_ACTIVE      (0x00000001)
#define GSUPPLICANT_INTERFACE_CAPS_SCAN_PASSIVE     (0x00000002)
#define GSUPPLICANT_INTERFACE_CAPS_SCAN_SSID        (0x00000004)

    guint modes;

#define GSUPPLICANT_INTERFACE_CAPS_MODES_INFRA      (0x00000001)
#define GSUPPLICANT_INTERFACE_CAPS_MODES_AD_HOC     (0x00000002)
#define GSUPPLICANT_INTERFACE_CAPS_MODES_AP         (0x00000004)
#define GSUPPLICANT_INTERFACE_CAPS_MODES_P2P        (0x00000008)

    gint max_scan_ssid;
    guint caps_reserved[2];
} GSupplicantInterfaceCaps;

typedef struct gsupplicant_signal_poll {
    guint flags;                /* Fields validity flags: */

#define GSUPPLICANT_SIGNAL_POLL_LINKSPEED   (0x01)
#define GSUPPLICANT_SIGNAL_POLL_NOISE       (0x02)
#define GSUPPLICANT_SIGNAL_POLL_FREQUENCY   (0x04)
#define GSUPPLICANT_SIGNAL_POLL_RSSI        (0x08)
#define GSUPPLICANT_SIGNAL_POLL_AVG_RSSI    (0x10)
#define GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ1 (0x20)
#define GSUPPLICANT_SIGNAL_POLL_CENTER_FRQ2 (0x40)

    gint linkspeed;             /* Link speed (Mbps) */
    gint noise;                 /* Noise (dBm) */
    guint frequency;            /* Frequency (MHz) */
    gint rssi;                  /* RSSI (dBm) */
    gint avg_rssi;              /* Average RSSI (dBm) */
    gint center_frq1;           /* VHT segment 1 frequency (MHz) */
    gint center_frq2;           /* VHT segment 2 frequency (MHz) */
} GSupplicantSignalPoll;

typedef enum gsupplicant_scan_type {
    GSUPPLICANT_SCAN_TYPE_PASSIVE,
    GSUPPLICANT_SCAN_TYPE_ACTIVE
} GSUPPLICANT_SCAN_TYPE;

typedef struct gsupplicant_scan_frequency {
    guint center;
    guint width;
} GSupplicantScanFrequency;

typedef struct gsupplicant_scan_frequencies {
    const GSupplicantScanFrequency* freq;
    guint count;
} GSupplicantScanFrequencies;

typedef struct gsupplicant_scan_params {
    guint flags;

#define GSUPPLICANT_SCAN_PARAM_ALLOW_ROAM   (0x01)

    GSUPPLICANT_SCAN_TYPE type;
    GBytes** ssids;
    GBytes** ies;
    const GSupplicantScanFrequencies* channels;
    gboolean allow_roam;
} GSupplicantScanParams;

typedef struct gsupplicant_network_params {
    guint flags;  /* Should be zero */
    GSUPPLICANT_AUTH_FLAGS auth_flags;
    GBytes* ssid;
    GSUPPLICANT_OP_MODE mode;
    GSUPPLICANT_EAP_METHOD eap;
    guint scan_ssid;
    guint frequency;
    GSUPPLICANT_SECURITY security;
    GSUPPLICANT_PROTOCOL protocol;
    GSUPPLICANT_CIPHER pairwise;
    GSUPPLICANT_CIPHER group;
    const char* bgscan;
    const char* passphrase;
    /* EAP */
    const char* identity;
    const char* anonymous_identity;
    const char* ca_cert_file;
    const char* client_cert_file;
    const char* private_key_file;
    const char* private_key_passphrase;
    const char* subject_match;
    const char* altsubject_match;
    const char* domain_suffix_match;
    const char* domain_match;
    GSUPPLICANT_EAP_METHOD phase2;
    const char* ca_cert_file2;
    const char* client_cert_file2;
    const char* private_key_file2;
    const char* private_key_passphrase2;
    const char* subject_match2;
    const char* altsubject_match2;
    const char* domain_suffix_match2;
} GSupplicantNetworkParams;

typedef struct gsupplicant_wps_params {
    GSUPPLICANT_WPS_ROLE role;
    GSUPPLICANT_WPS_AUTH auth;
    const char* pin;
    GBytes* bssid;
    GBytes* p2p_address;
} GSupplicantWPSParams;

typedef struct gsupplicant_wps_credentials {
    GBytes* bssid;
    GBytes* ssid;
    GSUPPLICANT_AUTH auth_types;
    GSUPPLICANT_WPS_ENCR encr_types;
    GBytes* key;
    guint key_index;
} GSupplicantWPSCredentials;

typedef struct gsupplicant_interface_priv GSupplicantInterfacePriv;

struct gsupplicant_interface {
    GObject object;
    GSupplicantInterfacePriv* priv;
    GSupplicant* supplicant;
    const char* path;
    gboolean valid;
    gboolean present;
    GSupplicantInterfaceCaps caps;
    GSUPPLICANT_INTERFACE_STATE state;
    const GSupplicantWPSCredentials* wps_credentials;
    gboolean scanning;
    guint ap_scan;
    gint scan_interval;
    const char* country;
    const char* driver;
    const char* ifname;
    const char* bridge_ifname;
    const char* current_bss;
    const char* current_network;
    const GStrV* bsss;
    const GStrV* networks;
    const GStrV* stations;          /* Since 1.0.7 */
};

typedef
void
(*GSupplicantInterfaceFunc)(
    GSupplicantInterface* iface,
    void* data);

typedef
void
(*GSupplicantInterfacePropertyFunc)(
    GSupplicantInterface* iface,
    GSUPPLICANT_INTERFACE_PROPERTY property,
    void* data);

typedef
void
(*GSupplicantInterfaceResultFunc)(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GError* error,
    void* data);

typedef
void
(*GSupplicantInterfaceStringResultFunc)(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GError* error,
    const char* result,
    void* data);

typedef
void
(*GSupplicantInterfaceSignalPollResultFunc)(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GError* error,
    const GSupplicantSignalPoll* result,
    void* data);

GSupplicantInterface*
gsupplicant_interface_new(
    const char* path);

GSupplicantInterface*
gsupplicant_interface_ref(
    GSupplicantInterface* iface);

void
gsupplicant_interface_unref(
    GSupplicantInterface* iface);

gulong
gsupplicant_interface_add_handler(
    GSupplicantInterface* iface,
    GSUPPLICANT_INTERFACE_PROPERTY property,
    GSupplicantInterfaceFunc fn,
    void* data);

gulong
gsupplicant_interface_add_property_changed_handler(
    GSupplicantInterface* iface,
    GSUPPLICANT_INTERFACE_PROPERTY property,
    GSupplicantInterfacePropertyFunc fn,
    void* data);

gboolean
gsupplicant_interface_set_ap_scan(
    GSupplicantInterface* iface,
    guint ap_scan);

gboolean
gsupplicant_interface_set_country(
    GSupplicantInterface* iface,
    const char* country);

void
gsupplicant_interface_remove_handler(
    GSupplicantInterface* iface,
    gulong id);

void
gsupplicant_interface_remove_handlers(
    GSupplicantInterface* iface,
    gulong* ids,
    guint count);

GCancellable*
gsupplicant_interface_disconnect(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_reassociate(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_reconnect(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_reattach(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

#define GSUPPLICANT_ADD_NETWORK_DELETE_OTHER  (0x01)
#define GSUPPLICANT_ADD_NETWORK_SELECT        (0x02)
#define GSUPPLICANT_ADD_NETWORK_ENABLE        (0x04)

GCancellable*
gsupplicant_interface_add_network(
    GSupplicantInterface* iface,
    const GSupplicantNetworkParams* params,
    guint flags, /* See above */
    GSupplicantInterfaceStringResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_add_network_full(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GSupplicantNetworkParams* params,
    guint flags, /* See above */
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data);

GCancellable*
gsupplicant_interface_select_network(
    GSupplicantInterface* iface,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_remove_network(
    GSupplicantInterface* iface,
    const char* path,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_remove_all_networks(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_remove_all_networks_full(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    GSupplicantInterfaceResultFunc fn,
    GDestroyNotify destroy,
    void* data);

GCancellable*
gsupplicant_interface_wps_connect(
    GSupplicantInterface* iface,
    const GSupplicantWPSParams* params,
    GSupplicantInterfaceStringResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_wps_connect_full(
    GSupplicantInterface* iface,
    GCancellable* cancel,
    const GSupplicantWPSParams* params,
    gint timeout_sec, /* 0 = default, negative = no timeout */
    GSupplicantInterfaceStringResultFunc fn,
    GDestroyNotify destroy,
    void* data);

GCancellable*
gsupplicant_interface_wps_cancel(
    GSupplicantInterface* iface,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_scan(
    GSupplicantInterface* iface,
    const GSupplicantScanParams* params,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_auto_scan(
    GSupplicantInterface* iface,
    const char* param,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_remove_flush_bss(
    GSupplicantInterface* iface,
    guint age,
    GSupplicantInterfaceResultFunc fn,
    void* data);

GCancellable*
gsupplicant_interface_signal_poll(
    GSupplicantInterface* iface,
    GSupplicantInterfaceSignalPollResultFunc fn,
    void* data);

const char*
gsupplicant_interface_state_name(
    GSUPPLICANT_INTERFACE_STATE state);

G_INLINE_FUNC const char*
gsupplicant_interface_get_state_name(GSupplicantInterface* iface)
    { return iface ? gsupplicant_interface_state_name(iface->state) : NULL; }

G_END_DECLS

#endif /* GSUPPLICANT_INTERFACE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
