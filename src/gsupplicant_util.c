/*
 * Copyright (C) 2015-2021 Jolla Ltd.
 * Copyright (C) 2015-2021 Slava Monich <slava.monich@jolla.com>
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

#include "gsupplicant_util_p.h"
#include "gsupplicant_log.h"

#include <ctype.h>

const char*
gsupplicant_name_int_find_bit(
    guint value,
    guint* bit,
    const GSupNameIntPair* list,
    gsize count)
{
    if (value) {
        gsize i;
        for (i=0; i<count; i++) {
            if (list[i].value & value) {
                if (bit) *bit = list[i].value;
                return list[i].name;
            }
        }
    }
    if (bit) *bit = 0;
    return NULL;
}

const char*
gsupplicant_name_int_find_int(
    guint value,
    const GSupNameIntPair* list,
    gsize count)
{
    gsize i;
    for (i=0; i<count; i++) {
        if (list[i].value == value) {
            return list[i].name;
        }
    }
    return NULL;
}

guint
gsupplicant_name_int_get_int(
    const char* name,
    const GSupNameIntPair* list,
    gsize n,
    guint default_value)
{
    const GSupNameIntPair* pair = gsupplicant_name_int_find_name(name, list, n);
    return pair ? pair->value : default_value;
}

static
const GSupNameIntPair*
gsupplicant_name_int_find_name_impl(
    const char* name,
    int (*cmp)(const char* s1, const char* s2),
    const GSupNameIntPair* list,
    gsize count)
{
    if (name) {
        gsize i;
        for (i=0; i<count; i++) {
            if (!cmp(list[i].name, name)) {
                return list + i;
            }
        }
    }
    return NULL;
}

const GSupNameIntPair*
gsupplicant_name_int_find_name(
    const char* name,
    const GSupNameIntPair* list,
    gsize count)
{
    return gsupplicant_name_int_find_name_impl(name, strcmp, list, count);
}

const GSupNameIntPair*
gsupplicant_name_int_find_name_i(
    const char* name,
    const GSupNameIntPair* list,
    gsize count)
{
    return gsupplicant_name_int_find_name_impl(name, strcasecmp, list, count);
}

gboolean
gsupplicant_name_int_set_bits(
    guint* bitmask,
    const char* name,
    const GSupNameIntPair* list,
    gsize n)
{
    const GSupNameIntPair* pair = gsupplicant_name_int_find_name(name, list, n);
    if (pair) {
        if (bitmask) *bitmask |= pair->value;
        return TRUE;
    } else {
        return FALSE;
    }
}

char*
gsupplicant_name_int_concat(
    guint value,
    char separator,
    const GSupNameIntPair* list,
    gsize count)
{
    GString* buf = NULL;
    if (value) {
        gsize i;
        for (i=0; i<count && value; i++) {
            if ((list[i].value & value) && list[i].name[0]) {
                if (!buf) {
                    buf = g_string_new(NULL);
                    if (!separator) separator = ',';
                } else {
                    g_string_append_c(buf, separator);
                }
                g_string_append(buf, list[i].name);
            }
        }
    }
    return buf ? g_string_free(buf, FALSE) : NULL;
}

guint32
gsupplicant_parse_bits_array(
    guint32 mask,
    const char* name,
    GVariant* value,
    const GSupNameIntPair* map,
    gsize count)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE("as"))) {
        GVariantIter it;
        char* str = NULL;
#if GUTIL_LOG_VERBOSE
        GString* buf = GLOG_ENABLED(GUTIL_LOG_VERBOSE) ?
            g_string_new(NULL) : NULL;
#endif
        g_variant_iter_init(&it, value);
        while (g_variant_iter_loop(&it, "s", &str)) {
            if (gsupplicant_name_int_set_bits(&mask, str, map, count)) {
#if GUTIL_LOG_VERBOSE
                if (buf) {
                    if (buf->len) g_string_append_c(buf, ',');
                    g_string_append(buf, str);
                }
#endif
            } else if (g_strcmp0(name, "KeyMgmt") || g_strcmp0(str, "sae")) {
                /* FIXME: just silencing the SAE case for now as it's known but not supported.
                   Should implement proper mapping and support */
                GWARN("Unexpected %s value %s", name, str);
            }
        }
#if GUTIL_LOG_VERBOSE
        if (buf) {
            GVERBOSE("  %s: %s", name, buf->str);
            g_string_free(buf, TRUE);
        }
#endif
    } else {
        GWARN("Unexpected value type for %s", name);
    }
    return mask;
}

static
gboolean
gsupplicant_idle_cb(
    gpointer data)
{
    return G_SOURCE_REMOVE;
}

const char*
gsupplicant_format_bytes(
    GBytes* bytes,
    gboolean append_length)
{
    if (bytes) {
        char* str;
        gsize i, size = 0;
        const guint8* data = g_bytes_get_data(bytes, &size);
        GString* buf = g_string_sized_new(3*size + (append_length ? 8 : 0));
        for (i=0; i<size; i++) {
            if (i > 0) g_string_append_c(buf, ':');
            g_string_append_printf(buf, "%02x", data[i]);
        }
        if (append_length) {
            if (size > 0) g_string_append_c(buf, ' ');
            g_string_append_printf(buf, "(%u)", (guint)size);
        }
        str = g_string_free(buf, FALSE);
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, gsupplicant_idle_cb,
            str, g_free);
        return str;
    } else {
        return "(null)";
    }
}

static
gboolean
gsupplicant_cancel_later_cb(
    gpointer cancel)
{
    g_cancellable_cancel(cancel);
    return G_SOURCE_REMOVE;
}

guint
gsupplicant_cancel_later(
    GCancellable* cancel)
{
    if (cancel) {
        return g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
            gsupplicant_cancel_later_cb, g_object_ref(cancel),
            g_object_unref);
    }
    return 0;
}

const char*
gsupplicant_check_abs_path(
    const char* path)
{
    if (path && path[0]) {
        if (!g_path_is_absolute(path)) {
            GWARN("Not an absolute path: %s", path);
            return NULL;
        } else if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
            GWARN("No such file: %s", path);
            return NULL;
        } else {
            return path;
        }
    }
    return NULL;
}

const char*
gsupplicant_check_blob_or_abs_path(
    const char* path,
    GHashTable* blobs)
{
    if (path && path[0]) {
        const char* blob_prefix = "blob://";
        if (!g_str_has_prefix(path, blob_prefix)) {
            return gsupplicant_check_abs_path(path);
        } else if (blobs) {
            if (g_hash_table_contains(blobs, path + strlen(blob_prefix))) {
                return path;
            } else {
                GWARN("No such blob: %s", path);
                return NULL;
            }
        }
    }
    return NULL;
}

int
gsupplicant_dict_parse(
    GVariant* dict,
    GSupplicantDictStrFunc fn,
    void* data)
{
    int count = 0;
    if (dict) {
        GVariantIter it;
        GVariant* entry;
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
                if (g_variant_is_of_type(key, G_VARIANT_TYPE_STRING)) {
                    fn(g_variant_get_string(key, NULL), value, data);
                    count++;
                }
                g_variant_unref(key);
                g_variant_unref(value);
            }
        }
    }
    return count;
}

void
gsupplicant_dict_add_value(
    GVariantBuilder* builder,
    const char* name,
    GVariant* value)
{
    g_variant_builder_add(builder, "{sv}", name, value);
}

void
gsupplicant_dict_add_boolean(
    GVariantBuilder* builder,
    const char* name,
    gboolean value)
{
    gsupplicant_dict_add_value(builder, name, g_variant_new_boolean(value));
}

void
gsupplicant_dict_add_uint32(
    GVariantBuilder* builder,
    const char* name,
    guint32 value)
{
    gsupplicant_dict_add_value(builder, name, g_variant_new_uint32(value));
}

void
gsupplicant_dict_add_string(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
{
    gsupplicant_dict_add_value(builder, name, g_variant_new_string(value));
}

void
gsupplicant_dict_add_string0(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
{
    if (name && value) {
        gsupplicant_dict_add_string(builder, name, value);
    }
}

void
gsupplicant_dict_add_string_ne(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
{
    if (name && value && value[0]) {
        gsupplicant_dict_add_string(builder, name, value);
    }
}

void
gsupplicant_dict_add_bytes(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value)
{
    gsize size = 0;
    const void* data = g_bytes_get_data(value, &size);
    gsupplicant_dict_add_value(builder, name, g_variant_new_fixed_array(
        G_VARIANT_TYPE_BYTE, data, size, 1));
}

void
gsupplicant_dict_add_bytes0(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value)
{
    if (name && value) {
        gsupplicant_dict_add_bytes(builder, name, value);
    }
}

GVariant*
gsupplicant_variant_new_ayy(
    GBytes** bytes)
{
    GVariantBuilder ayy;
    g_variant_builder_init(&ayy, G_VARIANT_TYPE("aay"));
    if (bytes) {
        while (*bytes) {
            gsize size = 0;
            const void* data = g_bytes_get_data(*bytes, &size);
            g_variant_builder_add_value(&ayy, g_variant_new_fixed_array(
                G_VARIANT_TYPE_BYTE, data, size, 1));
            bytes++;
        }
    }
    return g_variant_builder_end(&ayy);
}

GBytes*
gsupplicant_variant_data_as_bytes(
    GVariant* value)
{
    if (value) {
#if GLIB_CHECK_VERSION(2,36,0)
        /* This is more efficient */
        return g_variant_get_data_as_bytes(value);
#else
        return g_bytes_new(g_variant_get_data(value),
            g_variant_get_size(value));
#endif
    }
    return NULL;
}

char*
gsupplicant_utf8_from_bytes(
    GBytes* bytes)
{
    if (bytes) {
        gsize i, size = 0;
        gboolean empty = TRUE;
        const char* ptr = g_bytes_get_data(bytes, &size);
        for (i=0; i<size && empty; i++) {
            if (ptr[i]) {
                empty = FALSE;
            }
        }
        if (!empty) {
            GString* buf = g_string_sized_new(size);
            while (size) {
                const gchar* invalid = NULL;
		if (g_utf8_validate(ptr, size, &invalid)) {
                    g_string_append_len(buf, ptr, size);
                    break;
		} else {
                    gsize valid = invalid - ptr;
                    /* U+FFFD is REPLACEMENT CHARACTER */
                    g_string_append_len(buf, ptr, valid);
                    g_string_append(buf, "\357\277\275");
                    ptr = invalid + 1;
                    size -= valid + 1;
                }
            }
            return g_string_free(buf, FALSE);
        }
        return g_strdup("");
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
