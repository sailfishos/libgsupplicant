/*
 * Copyright (C) 2015-2021 Jolla Ltd.
 * Copyright (C) 2023 Slava Monich <slava@monich.com>
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

#ifndef GSUPPLICANT_UTIL_PRIVATE_H
#define GSUPPLICANT_UTIL_PRIVATE_H

#include "gsupplicant_types_p.h"
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
gsupplicant_parse_bit_value(
    const char* name,
    GVariant* value,
    const GSupNameIntPair* map,
    gsize count)
    GSUPPLICANT_INTERNAL;

guint32
gsupplicant_parse_bits_array(
    guint32 mask,
    const char* name,
    GVariant* value,
    const GSupNameIntPair* map,
    gsize count)
    GSUPPLICANT_INTERNAL;

const char*
gsupplicant_name_int_find_bit(
    guint value,
    guint* bit,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

const char*
gsupplicant_name_int_find_int(
    guint value,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

guint
gsupplicant_name_int_get_int(
    const char* name,
    const GSupNameIntPair* list,
    gsize count,
    guint default_value)
    GSUPPLICANT_INTERNAL;

gboolean
gsupplicant_name_int_set_bits(
    guint* bitmask,
    const char* name,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

const GSupNameIntPair*
gsupplicant_name_int_find_name(
    const char* name,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

const GSupNameIntPair*
gsupplicant_name_int_find_name_i(
    const char* name,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

char*
gsupplicant_name_int_concat(
    guint value,
    char separator,
    const GSupNameIntPair* list,
    gsize count)
    GSUPPLICANT_INTERNAL;

const char*
gsupplicant_format_bytes(
    GBytes* bytes,
    gboolean append_length)
    GSUPPLICANT_INTERNAL;

guint
gsupplicant_cancel_later(
    GCancellable* cancel)
    GSUPPLICANT_INTERNAL;

const char*
gsupplicant_check_abs_path(
    const char* path)
    GSUPPLICANT_INTERNAL;

const char*
gsupplicant_check_blob_or_abs_path(
    const char* path,
    GHashTable* blobs)
    GSUPPLICANT_INTERNAL;

int
gsupplicant_dict_parse(
    GVariant* dict,
    GSupplicantDictStrFunc fn,
    void* data)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_value(
    GVariantBuilder* builder,
    const char* name,
    GVariant* value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_boolean(
    GVariantBuilder* builder,
    const char* name,
    gboolean value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_uint32(
    GVariantBuilder* builder,
    const char* name,
    guint32 value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_string(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_string0(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_string_ne(
    GVariantBuilder* builder,
    const char* name,
    const char* value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_bytes(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value)
    GSUPPLICANT_INTERNAL;

void
gsupplicant_dict_add_bytes0(
    GVariantBuilder* builder,
    const char* name,
    GBytes* value)
    GSUPPLICANT_INTERNAL;

GVariant*
gsupplicant_variant_new_ayy(
    GBytes** bytes)
    GSUPPLICANT_INTERNAL;

GBytes*
gsupplicant_variant_data_as_bytes(
    GVariant* value)
    GSUPPLICANT_INTERNAL;

#endif /* GSUPPLICANT_UTIL_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
