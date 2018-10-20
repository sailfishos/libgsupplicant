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

#include "gsupplicant_network.h"
#include "gsupplicant_interface.h"
#include "gsupplicant_dbus.h"
#include "gsupplicant_log.h"

#include <gutil_misc.h>
#include <gutil_strv.h>

/* Generated headers */
#include "fi.w1.wpa_supplicant1.Network.h"

/* Object definition */
enum supplicant_network_proxy_handler_id {
    PROXY_GPROPERTIES_CHANGED,
    PROXY_PROPERTIES_CHANGED,
    PROXY_HANDLER_COUNT
};

enum supplicant_interface_handler_id {
    INTERFACE_VALID_CHANGED,
    INTERFACE_NETWORKS_CHANGED,
    INTERFACE_HANDLER_COUNT
};

struct gsupplicant_network_priv {
    FiW1Wpa_supplicant1Network* proxy;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    gulong iface_handler_id[INTERFACE_HANDLER_COUNT];
    char* path;
    guint32 pending_signals;
};

typedef GObjectClass GSupplicantNetworkClass;
G_DEFINE_TYPE(GSupplicantNetwork, gsupplicant_network, G_TYPE_OBJECT)
#define GSUPPLICANT_NETWORK_TYPE (gsupplicant_network_get_type())
#define GSUPPLICANT_NETWORK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        GSUPPLICANT_NETWORK_TYPE, GSupplicantNetwork))
#define SUPER_CLASS gsupplicant_network_parent_class

/* Network properties */
#define GSUPPLICANT_NETWORK_PROPERTIES_(p) \
    p(VALID,valid) \
    p(PRESENT,present) \
    p(PROPERTIES,properties) \
    p(ENABLED,enabled)

typedef enum gsupplicant_network_signal {
#define SIGNAL_ENUM_(P,p) SIGNAL_##P##_CHANGED,
    GSUPPLICANT_NETWORK_PROPERTIES_(SIGNAL_ENUM_)
#undef SIGNAL_ENUM_
    SIGNAL_PROPERTY_CHANGED,
    SIGNAL_COUNT
} GSUPPLICANT_NETWORK_SIGNAL;

#define SIGNAL_BIT(name) (1 << SIGNAL_##name##_CHANGED)

/*
 * The code assumes that VALID is the first one and that their number
 * doesn't exceed the number of bits in pending_signals (currently, 32)
 */
G_STATIC_ASSERT(SIGNAL_VALID_CHANGED == 0);
G_STATIC_ASSERT(SIGNAL_PROPERTY_CHANGED <= 32);

/* Assert that we have covered all publicly defined properties */
G_STATIC_ASSERT((int)SIGNAL_PROPERTY_CHANGED ==
               ((int)GSUPPLICANT_NETWORK_PROPERTY_COUNT-1));

#define SIGNAL_PROPERTY_CHANGED_NAME            "property-changed"
#define SIGNAL_PROPERTY_CHANGED_DETAIL          "%x"
#define SIGNAL_PROPERTY_CHANGED_DETAIL_MAX_LEN  (8)

static GQuark gsupplicant_network_property_quarks[SIGNAL_PROPERTY_CHANGED];
static guint gsupplicant_network_signals[SIGNAL_COUNT];
static const char* gsupplicant_network_signame[] = {
#define SIGNAL_NAME_(P,p) #p "-changed",
    GSUPPLICANT_NETWORK_PROPERTIES_(SIGNAL_NAME_)
#undef SIGNAL_NAME_
    SIGNAL_PROPERTY_CHANGED_NAME
};

G_STATIC_ASSERT(G_N_ELEMENTS(gsupplicant_network_signame) == SIGNAL_COUNT);

/* Proxy properties */
#define PROXY_PROPERTY_NAME_ENABLED        "Enabled"
#define PROXY_PROPERTY_NAME_PROPERTIES     "Properties"

/* Weak references to the instances of GSupplicantNetwork */
static GHashTable* gsupplicant_network_table = NULL;

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static inline
GSUPPLICANT_NETWORK_PROPERTY
gsupplicant_network_property_from_signal(
    GSUPPLICANT_NETWORK_SIGNAL sig)
{
    switch (sig) {
#define SIGNAL_PROPERTY_MAP_(P,p) \
    case SIGNAL_##P##_CHANGED: return GSUPPLICANT_NETWORK_PROPERTY_##P;
    GSUPPLICANT_NETWORK_PROPERTIES_(SIGNAL_PROPERTY_MAP_)
#undef SIGNAL_PROPERTY_MAP_
    default: /* unreachable */ return GSUPPLICANT_NETWORK_PROPERTY_ANY;
    }
}

static
void
gsupplicant_network_signal_property_change(
    GSupplicantNetwork* self,
    GSUPPLICANT_NETWORK_SIGNAL sig,
    GSUPPLICANT_NETWORK_PROPERTY prop)
{
    GSupplicantNetworkPriv* priv = self->priv;
    GASSERT(prop > GSUPPLICANT_NETWORK_PROPERTY_ANY);
    GASSERT(prop < GSUPPLICANT_NETWORK_PROPERTY_COUNT);
    GASSERT(sig < G_N_ELEMENTS(gsupplicant_network_property_quarks));
    if (!gsupplicant_network_property_quarks[sig]) {
        char buf[SIGNAL_PROPERTY_CHANGED_DETAIL_MAX_LEN + 1];
        snprintf(buf, sizeof(buf), SIGNAL_PROPERTY_CHANGED_DETAIL, prop);
        buf[sizeof(buf)-1] = 0;
        gsupplicant_network_property_quarks[sig] = g_quark_from_string(buf);
    }
    priv->pending_signals &= ~(1 << sig);
    g_signal_emit(self, gsupplicant_network_signals[sig], 0);
    g_signal_emit(self, gsupplicant_network_signals[SIGNAL_PROPERTY_CHANGED],
        gsupplicant_network_property_quarks[sig], prop);
}

static
void
gsupplicant_network_emit_pending_signals(
    GSupplicantNetwork* self)
{
    GSupplicantNetworkPriv* priv = self->priv;
    GSUPPLICANT_NETWORK_SIGNAL sig;
    gboolean valid_changed;

    /* Handlers could drops their references to us */
    gsupplicant_network_ref(self);

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
         sig < SIGNAL_COUNT && priv->pending_signals;
         sig++) {
        if (priv->pending_signals & (1 << sig)) {
            gsupplicant_network_signal_property_change(self, sig,
                gsupplicant_network_property_from_signal(sig));
        }
    }

    /* Then emit VALID if valid has become TRUE */
    if (valid_changed) {
        gsupplicant_network_signal_property_change(self, SIGNAL_VALID_CHANGED,
            GSUPPLICANT_NETWORK_PROPERTY_VALID);
    }

    /* And release the temporary reference */
    gsupplicant_network_unref(self);
}

static
void
gsupplicant_network_update_valid(
    GSupplicantNetwork* self)
{
    GSupplicantNetworkPriv* priv = self->priv;
    const gboolean valid = priv->proxy && self->iface->valid;
    if (self->valid != valid) {
        self->valid = valid;
        GDEBUG("Network %s is %svalid", priv->path, valid ? "" : "in");
        priv->pending_signals |= SIGNAL_BIT(VALID);
    }
}

static
void
gsupplicant_network_update_present(
    GSupplicantNetwork* self)
{
    GSupplicantNetworkPriv* priv = self->priv;
    const gboolean present = priv->proxy && self->iface->valid &&
        gutil_strv_contains(self->iface->networks, priv->path);
    if (self->present != present) {
        self->present = present;
        GDEBUG("Network %s is %spresent", priv->path, present ? "" : "not ");
        priv->pending_signals |= SIGNAL_BIT(PRESENT);
    }
}

static
GHashTable*
gsupplicant_network_get_properties(
    GSupplicantNetwork* self)
{
    GHashTable* props = NULL;
    GSupplicantNetworkPriv* priv = self->priv;
    GVariant* dict = fi_w1_wpa_supplicant1_network_get_properties(priv->proxy);
    if (dict) {
        GVariantIter it;
        GVariant* entry;
        if (g_variant_is_of_type(dict, G_VARIANT_TYPE_VARIANT)) {
            GVariant* tmp = g_variant_get_variant(dict);
            g_variant_unref(dict);
            dict = tmp;
        }
        props = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        for (g_variant_iter_init(&it, dict);
             (entry = g_variant_iter_next_value(&it)) != NULL;
             g_variant_unref(entry)) {
            const guint num = g_variant_n_children(entry);
            if (G_LIKELY(num == 2)) {
                GVariant* key = g_variant_get_child_value(entry, 0);
                GVariant* value = g_variant_get_child_value(entry, 1);
                if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
                    GVariant* tmp = g_variant_get_variant(value);
                    g_variant_unref(value);
                    value = tmp;
                }
                if (g_variant_is_of_type(key, G_VARIANT_TYPE_STRING) &&
                    g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
                    g_hash_table_replace(props,
                        g_strdup(g_variant_get_string(key, NULL)),
                        g_strdup(g_variant_get_string(value, NULL)));
                }
                g_variant_unref(key);
                g_variant_unref(value);
            }
        }
    }
    return props;
}

static
gboolean
gsupplicant_network_properties_equal(
    GHashTable* p1,
    GHashTable* p2)
{
    if (p1 && p2) {
        const gsize size = g_hash_table_size(p1);
        if (g_hash_table_size(p2) == size) {
            if (size) {
                GHashTableIter it;
                gpointer key, value;
                gboolean equal = TRUE;
                g_hash_table_iter_init(&it, p1);
                while (g_hash_table_iter_next(&it, &key, &value)) {
                    const char* value2 = g_hash_table_lookup(p2, key);
                    if (g_strcmp0(value, value2)) {
                        equal = FALSE;
                        break;
                    }
                }
                return equal;
            }
            return TRUE;
        }
        return FALSE;
    } else {
        return !p1 && !p2;
    }
}

static
void
gsupplicant_network_update_properties(
    GSupplicantNetwork* self)
{
    GSupplicantNetworkPriv* priv = self->priv;
    GHashTable* props = gsupplicant_network_get_properties(self);
    if (!gsupplicant_network_properties_equal(props, self->properties)) {
        if (self->properties) {
            g_hash_table_unref(self->properties);
        }
        self->properties = props;
        priv->pending_signals |= SIGNAL_BIT(PROPERTIES);
#if GUTIL_LOG_VERBOSE
        if (GLOG_ENABLED(GUTIL_LOG_VERBOSE)) {
            if (props) {
                GHashTableIter it;
                gpointer key;
                char** keys = g_new(char*, g_hash_table_size(props) + 1);
                char** ptr = keys;
                g_hash_table_iter_init(&it, props);
                while (g_hash_table_iter_next(&it, &key, NULL)) *ptr++ = key;
                *ptr++ = NULL;
                GVERBOSE("[%s] Properties:", self->path);
                gutil_strv_sort(keys, TRUE);
                for (ptr = keys; *ptr; ptr++) {
                    const char* key = *ptr;
                    const char* value = g_hash_table_lookup(props, key);
                    GVERBOSE("  %s: %s", key, value);
                }
                g_free(keys);
            } else {
                GVERBOSE("[%s] Properties: (null)", self->path);
            }
        }
#endif
    } else if (props) {
        g_hash_table_unref(props);
    }
}

static
void
gsupplicant_network_update_enabled(
    GSupplicantNetwork* self)
{
    GSupplicantNetworkPriv* priv = self->priv;
    gboolean b = fi_w1_wpa_supplicant1_network_get_enabled(priv->proxy);
    if (self->enabled != b) {
        self->enabled = b;
        priv->pending_signals |= SIGNAL_BIT(ENABLED);
        GVERBOSE("[%s] %s: %s", self->path, PROXY_PROPERTY_NAME_ENABLED,
            b ? "true" : "false");
    }
}

static
void
gsupplicant_network_proxy_gproperties_changed(
    GDBusProxy* proxy,
    GVariant* changed,
    GStrv invalidated,
    gpointer data)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(data);
    GSupplicantNetworkPriv* priv = self->priv;
    if (invalidated) {
        char** ptr;
        for (ptr = invalidated; *ptr; ptr++) {
            const char* name = *ptr;
            if (!strcmp(name, PROXY_PROPERTY_NAME_PROPERTIES)) {
                if (self->properties) {
                    g_hash_table_unref(self->properties);
                    self->properties = NULL;
                    priv->pending_signals |= SIGNAL_BIT(PROPERTIES);
                }
            } else if (!strcmp(name, PROXY_PROPERTY_NAME_ENABLED)) {
                if (self->enabled) {
                    self->enabled = FALSE;
                    priv->pending_signals |= SIGNAL_BIT(ENABLED);
                }
            }
        }
    }
    if (changed) {
        GVariantIter it;
        GVariant* value;
        const char* name;
        g_variant_iter_init(&it, changed);
        while (g_variant_iter_next(&it, "{&sv}", &name, &value)) {
            if (!strcmp(name, PROXY_PROPERTY_NAME_PROPERTIES)) {
                gsupplicant_network_update_properties(self);
            } else if (!strcmp(name, PROXY_PROPERTY_NAME_ENABLED)) {
                gsupplicant_network_update_enabled(self);
            }
            g_variant_unref(value);
        }
    }
    gsupplicant_network_emit_pending_signals(self);
}

static
void
gsupplicant_network_proxy_properties_changed(
    GDBusProxy* proxy,
    GVariant* change,
    gpointer data)
{
    gsupplicant_network_proxy_gproperties_changed(proxy, change, NULL, data);
}

static
void
gsupplicant_network_interface_valid_changed(
    GSupplicantInterface* iface,
    void* data)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(data);
    GASSERT(self->iface == iface);
    gsupplicant_network_update_valid(self);
    gsupplicant_network_update_present(self);
    gsupplicant_network_emit_pending_signals(self);
}

static
void
gsupplicant_network_interface_networks_changed(
    GSupplicantInterface* iface,
    void* data)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(data);
    GASSERT(self->iface == iface);
    gsupplicant_network_update_present(self);
    gsupplicant_network_emit_pending_signals(self);
}

static
void
gsupplicant_network_proxy_created(
    GObject* bus,
    GAsyncResult* res,
    gpointer data)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(data);
    GSupplicantNetworkPriv* priv = self->priv;
    GError* error = NULL;
    GASSERT(!self->valid);
    GASSERT(!priv->proxy);
    priv->proxy = fi_w1_wpa_supplicant1_network_proxy_new_for_bus_finish(res,
        &error);
    if (priv->proxy) {
        priv->proxy_handler_id[PROXY_GPROPERTIES_CHANGED] =
            g_signal_connect(priv->proxy, "g-properties-changed",
            G_CALLBACK(gsupplicant_network_proxy_gproperties_changed), self);
        priv->proxy_handler_id[PROXY_PROPERTIES_CHANGED] =
            g_signal_connect(priv->proxy, "properties-changed",
            G_CALLBACK(gsupplicant_network_proxy_properties_changed), self);

        priv->iface_handler_id[INTERFACE_VALID_CHANGED] =
            gsupplicant_interface_add_handler(self->iface,
                GSUPPLICANT_INTERFACE_PROPERTY_VALID,
                gsupplicant_network_interface_valid_changed, self);
        priv->iface_handler_id[INTERFACE_NETWORKS_CHANGED] =
            gsupplicant_interface_add_handler(self->iface,
                GSUPPLICANT_INTERFACE_PROPERTY_NETWORKS,
                gsupplicant_network_interface_networks_changed, self);

        gsupplicant_network_update_valid(self);
        gsupplicant_network_update_present(self);
        gsupplicant_network_update_properties(self);
        gsupplicant_network_update_enabled(self);

        gsupplicant_network_emit_pending_signals(self);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    gsupplicant_network_unref(self);
}

static
void
gsupplicant_network_destroyed(
    gpointer key,
    GObject* dead)
{
    GVERBOSE_("%s", (char*)key);
    GASSERT(gsupplicant_network_table);
    if (gsupplicant_network_table) {
        GASSERT(g_hash_table_lookup(gsupplicant_network_table, key) == dead);
        g_hash_table_remove(gsupplicant_network_table, key);
        if (g_hash_table_size(gsupplicant_network_table) == 0) {
            g_hash_table_unref(gsupplicant_network_table);
            gsupplicant_network_table = NULL;
        }
    }
}

static
GSupplicantNetwork*
gsupplicant_network_create(
    const char* path)
{
    /*
     * Let's assume that the network path has the following format:
     *
     *   /fi/w1/wpa_supplicant1/Interfaces/xxx/Networks/yyy
     *
     * and we just have to strip the last two elements of the path to get
     * the interface path.
     */
    int slash_count = 0;
    const char* ptr;
    for (ptr = path + strlen(path); ptr > path; ptr--) {
        if (ptr[0] == '/') {
            slash_count++;
            if (slash_count == 2) {
                break;
            }
        }
    }
    if (ptr > path) {
        GSupplicantInterface* iface;
        /* Temporarily shorten the path to lookup the interface */
        char* path2 = g_strdup(path);
        const gsize slash_index = ptr - path;
        path2[slash_index] = 0;
        GDEBUG_("%s -> %s", path, path2);
        iface = gsupplicant_interface_new(path2);
        if (iface) {
            GSupplicantNetwork* self =
                g_object_new(GSUPPLICANT_NETWORK_TYPE,NULL);
            GSupplicantNetworkPriv* priv = self->priv;
            /* Path is already allocated (but truncated) */
            path2[slash_index] = '/';
            self->path = priv->path = path2;
            self->iface = iface;
            fi_w1_wpa_supplicant1_network_proxy_new_for_bus(
                GSUPPLICANT_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
                GSUPPLICANT_SERVICE, self->path, NULL,
                gsupplicant_network_proxy_created,
                gsupplicant_network_ref(self));
            return self;
        }
        g_free(path2);
    }
    return NULL;
}

/*==========================================================================*
 * API
 *==========================================================================*/

GSupplicantNetwork*
gsupplicant_network_new(
    const char* path)
{
    GSupplicantNetwork* self = NULL;
    if (G_LIKELY(path)) {
        self = gsupplicant_network_table ? gsupplicant_network_ref(
            g_hash_table_lookup(gsupplicant_network_table, path)) : NULL;
        if (!self) {
            self = gsupplicant_network_create(path);
            if (self) {
                gpointer key = g_strdup(path);
                if (!gsupplicant_network_table) {
                    gsupplicant_network_table =
                        g_hash_table_new_full(g_str_hash, g_str_equal,
                            g_free, NULL);
                }
                g_hash_table_replace(gsupplicant_network_table, key, self);
                g_object_weak_ref(G_OBJECT(self),
                    gsupplicant_network_destroyed, key);
            }
        }
    }
    return self;
}

GSupplicantNetwork*
gsupplicant_network_ref(
    GSupplicantNetwork* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(GSUPPLICANT_NETWORK(self));
        return self;
    } else {
        return NULL;
    }
}

void
gsupplicant_network_unref(
    GSupplicantNetwork* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(GSUPPLICANT_NETWORK(self));
    }
}

gulong
gsupplicant_network_add_property_changed_handler(
    GSupplicantNetwork* self,
    GSUPPLICANT_NETWORK_PROPERTY property,
    GSupplicantNetworkPropertyFunc fn,
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
gsupplicant_network_add_handler(
    GSupplicantNetwork* self,
    GSUPPLICANT_NETWORK_PROPERTY prop,
    GSupplicantNetworkFunc fn,
    void* data)
{
    if (G_LIKELY(self) && G_LIKELY(fn)) {
        const char* signame;
        switch (prop) {
#define SIGNAL_NAME_(P,p) case GSUPPLICANT_NETWORK_PROPERTY_##P: \
            signame = gsupplicant_network_signame[SIGNAL_##P##_CHANGED]; \
            break;
            GSUPPLICANT_NETWORK_PROPERTIES_(SIGNAL_NAME_)
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

void
gsupplicant_network_remove_handler(
    GSupplicantNetwork* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
gsupplicant_network_remove_handlers(
    GSupplicantNetwork* self,
    gulong* ids,
    guint count)
{
    gutil_disconnect_handlers(self, ids, count);
}

gboolean
gsupplicant_network_set_enabled(
    GSupplicantNetwork* self,
    gboolean enabled)
{
    if (G_LIKELY(self) && self->valid) {
        GSupplicantNetworkPriv* priv = self->priv;
        fi_w1_wpa_supplicant1_network_set_enabled(priv->proxy, enabled);
        return TRUE;
    }
    return FALSE;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
gsupplicant_network_init(
    GSupplicantNetwork* self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, GSUPPLICANT_NETWORK_TYPE,
        GSupplicantNetworkPriv);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
gsupplicant_network_dispose(
    GObject* object)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(object);
    GSupplicantNetworkPriv* priv = self->priv;
    if (priv->proxy) {
        gutil_disconnect_handlers(priv->proxy, priv->proxy_handler_id,
            G_N_ELEMENTS(priv->proxy_handler_id));
        g_object_unref(priv->proxy);
        priv->proxy = NULL;
    }
    gsupplicant_interface_remove_all_handlers(self->iface,
        priv->iface_handler_id);
    G_OBJECT_CLASS(SUPER_CLASS)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
gsupplicant_network_finalize(
    GObject* object)
{
    GSupplicantNetwork* self = GSUPPLICANT_NETWORK(object);
    GSupplicantNetworkPriv* priv = self->priv;
    GASSERT(!priv->proxy);
    g_free(priv->path);
    gsupplicant_interface_unref(self->iface);
    if (self->properties) {
        g_hash_table_unref(self->properties);
    }
    G_OBJECT_CLASS(SUPER_CLASS)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
gsupplicant_network_class_init(
    GSupplicantNetworkClass* klass)
{
    int i;
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = gsupplicant_network_dispose;
    object_class->finalize = gsupplicant_network_finalize;
    g_type_class_add_private(klass, sizeof(GSupplicantNetworkPriv));
    for (i=0; i<SIGNAL_PROPERTY_CHANGED; i++) {
        gsupplicant_network_signals[i] =  g_signal_new(
            gsupplicant_network_signame[i], G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    }
    gsupplicant_network_signals[SIGNAL_PROPERTY_CHANGED] =
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
