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

#ifndef GSUPPLICANT_NETWORK_H
#define GSUPPLICANT_NETWORK_H

#include <gsupplicant_types.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum gsupplicant_network_property {
    GSUPPLICANT_NETWORK_PROPERTY_ANY,
    GSUPPLICANT_NETWORK_PROPERTY_VALID,
    GSUPPLICANT_NETWORK_PROPERTY_PRESENT,
    GSUPPLICANT_NETWORK_PROPERTY_ENABLED,
    GSUPPLICANT_NETWORK_PROPERTY_PROPERTIES,
    GSUPPLICANT_NETWORK_PROPERTY_COUNT
} GSUPPLICANT_NETWORK_PROPERTY;

typedef struct gsupplicant_network_priv GSupplicantNetworkPriv;

struct gsupplicant_network {
    GObject object;
    GSupplicantNetworkPriv* priv;
    GSupplicantInterface* iface;
    const char* path;
    gboolean valid;
    gboolean present;
    GHashTable* properties;
    gboolean enabled;
};

typedef
void
(*GSupplicantNetworkFunc)(
    GSupplicantNetwork* network,
    void* data);

typedef
void
(*GSupplicantNetworkPropertyFunc)(
    GSupplicantNetwork* network,
    GSUPPLICANT_NETWORK_PROPERTY property,
    void* data);

GSupplicantNetwork*
gsupplicant_network_new(
    const char* path);

GSupplicantNetwork*
gsupplicant_network_ref(
    GSupplicantNetwork* network);

void
gsupplicant_network_unref(
    GSupplicantNetwork* network);

gulong
gsupplicant_network_add_handler(
    GSupplicantNetwork* network,
    GSUPPLICANT_NETWORK_PROPERTY property,
    GSupplicantNetworkFunc fn,
    void* data);

gulong
gsupplicant_network_add_property_changed_handler(
    GSupplicantNetwork* network,
    GSUPPLICANT_NETWORK_PROPERTY property,
    GSupplicantNetworkPropertyFunc fn,
    void* data);

void
gsupplicant_network_remove_handler(
    GSupplicantNetwork* network,
    gulong id);

void
gsupplicant_network_remove_handlers(
    GSupplicantNetwork* network,
    gulong* ids,
    guint count);

gboolean
gsupplicant_network_set_enabled(
    GSupplicantNetwork* network,
    gboolean enabled);

#define gsupplicant_network_remove_all_handlers(network, ids) \
    gsupplicant_network_remove_handlers(network, ids, G_N_ELEMENTS(ids))

G_END_DECLS

#endif /* GSUPPLICANT_NETWORK_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
