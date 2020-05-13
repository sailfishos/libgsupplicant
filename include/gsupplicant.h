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

#ifndef GSUPPLICANT_H
#define GSUPPLICANT_H

#include <gsupplicant_types.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum gsupplicant_property {
    GSUPPLICANT_PROPERTY_ANY,
    GSUPPLICANT_PROPERTY_VALID,
    GSUPPLICANT_PROPERTY_CAPABILITIES,
    GSUPPLICANT_PROPERTY_EAP_METHODS,
    GSUPPLICANT_PROPERTY_INTERFACES,
    GSUPPLICANT_PROPERTY_COUNT
} GSUPPLICANT_PROPERTY;

typedef struct gsupplicant_priv GSupplicantPriv;

struct gsupplicant {
    GObject object;
    GSupplicantPriv* priv;
    gboolean valid;
    gboolean failed;
    const GStrV* interfaces;
    GSUPPLICANT_EAP_METHOD eap_methods;
    guint32 caps;

#define GSUPPLICANT_CAPS_AP             (0x00000001)
#define GSUPPLICANT_CAPS_IBSS_RSN       (0x00000002)
#define GSUPPLICANT_CAPS_P2P            (0x00000004)
#define GSUPPLICANT_CAPS_INTERWORKING   (0x00000008)
};

typedef struct gsupplicant_create_interface_params {
    const char* ifname;
    const char* bridge_ifname;
    const char* driver;
    const char* config_file;
} GSupplicantCreateInterfaceParams;

typedef
void
(*GSupplicantFunc)(
    GSupplicant* supplicant,
    void* data);

typedef
void
(*GSupplicantPropertyFunc)(
    GSupplicant* supplicant,
    GSUPPLICANT_PROPERTY property,
    void* data);

typedef
void
(*GSupplicantResultFunc)(
    GSupplicant* supplicant,
    GCancellable* cancel,
    const GError* error,
    void* data);

typedef
void
(*GSupplicantStringResultFunc)(
    GSupplicant* supplicant,
    GCancellable* cancel,
    const GError* error,
    const char* result,
    void* data);

GSupplicant*
gsupplicant_new(
    void);

GSupplicant*
gsupplicant_ref(
    GSupplicant* supplicant);

void
gsupplicant_unref(
    GSupplicant* supplicant);

gulong
gsupplicant_add_handler(
    GSupplicant* supplicant,
    GSUPPLICANT_PROPERTY prop,
    GSupplicantFunc fn,
    void* data);

gulong
gsupplicant_add_property_changed_handler(
    GSupplicant* supplicant,
    GSUPPLICANT_PROPERTY property,
    GSupplicantPropertyFunc fn,
    void* data);

void
gsupplicant_remove_handler(
    GSupplicant* supplicant,
    gulong id);

void
gsupplicant_remove_handlers(
    GSupplicant* supplicant,
    gulong* ids,
    guint count);

GCancellable*
gsupplicant_create_interface(
    GSupplicant* supplicant,
    const GSupplicantCreateInterfaceParams* params,
    GSupplicantStringResultFunc fn,
    void* data);

GCancellable*
gsupplicant_remove_interface(
    GSupplicant* supplicant,
    const char* path,
    GSupplicantResultFunc fn,
    void* data);

GCancellable*
gsupplicant_get_interface(
    GSupplicant* supplicant,
    const char* ifname,
    GSupplicantStringResultFunc fn,
    void* data);

const char*
gsupplicant_caps_name(
    guint caps,
    guint* cap);

const char*
gsupplicant_eap_method_name(
    guint methods,
    guint* method);

const char*
gsupplicant_cipher_suite_name(
    guint cipher_suites,
    guint* cipher_suite);

const char*
gsupplicant_keymgmt_suite_name(
    guint keymgmt_suites,
    guint* keymgmt_suite);

#define gsupplicant_remove_all_handlers(supplicant, ids) \
    gsupplicant_remove_handlers(supplicant, ids, G_N_ELEMENTS(ids))

G_END_DECLS

#endif /* GSUPPLICANT_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
