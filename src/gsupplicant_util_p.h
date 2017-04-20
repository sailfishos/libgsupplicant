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

#ifndef GSUPPLICANT_UTIL_PRIVATE_H
#define GSUPPLICANT_UTIL_PRIVATE_H

#include <gsupplicant_util.h>
#include <gio/gio.h>

typedef struct gsupplicant_name_int_pair {
    const char* name;
    guint value;
} GSupNameIntPair;

typedef
void
(*GSupplicantDictStrFunc)(
    const char* key,
    GVariant* value,
    void* data);

guint32
gsupplicant_parse_bits_array(
    guint32 mask,
    const char* name,
    GVariant* value,
    const GSupNameIntPair* map,
    gsize count);

const char*
gsupplicant_name_int_find_bit(
    guint value,
    guint* bit,
    const GSupNameIntPair* list,
    gsize count);

const char*
gsupplicant_name_int_find_int(
    guint value,
    const GSupNameIntPair* list,
    gsize count);

guint
gsupplicant_name_int_get_int(
    const char* name,
    const GSupNameIntPair* list,
    gsize count,
    guint default_value);

gboolean
gsupplicant_name_int_set_bits(
    guint* bitmask,
    const char* name,
    const GSupNameIntPair* list,
    gsize count);

const GSupNameIntPair*
gsupplicant_name_int_find_name(
    const char* name,
    const GSupNameIntPair* list,
    gsize count);

const GSupNameIntPair*
gsupplicant_name_int_find_name_i(
    const char* name,
    const GSupNameIntPair* list,
    gsize count);

char*
gsupplicant_name_int_concat(
    guint value,
    char separator,
    const GSupNameIntPair* list,
    gsize count);

const char*
gsupplicant_format_bytes(
    GBytes* bytes,
    gboolean append_length);

guint
gsupplicant_call_later(
    GDestroyNotify notify,
    void* data);

guint
gsupplicant_cancel_later(
    GCancellable* cancel);

const char*
gsupplicant_check_abs_path(
    const char* path);

int
gsupplicant_dict_parse(
    GVariant* dict,
    GSupplicantDictStrFunc fn,
    void* data);

void
gsupplicant_dict_add_value(
    GVariantBuilder* builder,
    const char* name,
    GVariant* value);

void
gsupplicant_dict_add_boolean(
    GVariantBuilder* builder,
    const char* name,
    gboolean value);

void
gsupplicant_dict_add_uint32(
    GVariantBuilder* builder,
    const char* name,
    guint32 value);

void
gsupplicant_dict_add_string(
    GVariantBuilder* builder,
    const char* name,
    const char* value);

void
gsupplicant_dict_add_string0(
    GVariantBuilder* builder,
    const char* name,
    const char* value);

void
gsupplicant_dict_add_string_ne(
    GVariantBuilder* builder,
    const char* name,
    const char* value);

void
gsupplicant_dict_add_bytes(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value);

void
gsupplicant_dict_add_bytes0(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value);

GVariant*
gsupplicant_variant_new_ayy(
    GBytes** bytes);

GBytes *
gsupplicant_variant_data_as_bytes(
   GVariant *value);

#endif /* GSUPPLICANT_UTIL_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
