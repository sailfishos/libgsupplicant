/*
 * Copyright (C) 2017 Jolla Ltd.
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

#include "test_common.h"

#include "gsupplicant_util_p.h"

#include <gutil_log.h>
#include <glib/gstdio.h>

#define TEST_PREFIX "/util/"

#define BIT_FOO     0x01
#define BIT_BAR     0x02
#define BIT_EMPTY   0x04
#define BIT_XXX     0x08
#define BIT_MISSING 0x10

#define INDEX_FOO 0
#define INDEX_BAR 1

static const GSupNameIntPair test_map [] = {
        { "foo",  BIT_FOO },
        { "bar",  BIT_BAR },
        { "",     BIT_EMPTY },
        { "xxx",  BIT_XXX }
};

static TestOpt test_opt;

static
gboolean
test_loop_quit(
    gpointer data)
{
    g_main_loop_quit(data);
    return G_SOURCE_REMOVE;
}

static
void
test_variant_unref(
    void* data)
{
    g_variant_unref(data);
}

/*==========================================================================*
 * name_int
 *==========================================================================*/

static
void
test_util_name_int(
    void)
{
    guint bit;
    char* str;

    /* Nothing is ever found in the empty maps */
    bit = 1;
    g_assert(!gsupplicant_name_int_find_bit(BIT_BAR, &bit, test_map, 0));
    g_assert(!bit);
    g_assert(!gsupplicant_name_int_find_int(BIT_BAR, test_map, 0));

    /* Find bits */
    bit = 1;
    g_assert(!gsupplicant_name_int_find_bit(0, NULL,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_bit(BIT_MISSING, NULL,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_bit(BIT_MISSING, &bit,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!bit);

    bit = 0;
    g_assert(!g_strcmp0(gsupplicant_name_int_find_bit(BIT_BAR, NULL,
        test_map, G_N_ELEMENTS(test_map)), test_map[INDEX_BAR].name));
    g_assert(!g_strcmp0(gsupplicant_name_int_find_bit(BIT_BAR, &bit,
        test_map, G_N_ELEMENTS(test_map)), test_map[INDEX_BAR].name));
    g_assert(bit == BIT_BAR);

    /* With two bits set, the first one will be found */
    bit = 0;
    g_assert(!g_strcmp0(gsupplicant_name_int_find_bit(BIT_FOO|BIT_BAR, NULL,
        test_map, G_N_ELEMENTS(test_map)), test_map[INDEX_FOO].name));
    g_assert(!g_strcmp0(gsupplicant_name_int_find_bit(BIT_FOO|BIT_BAR, &bit,
        test_map, G_N_ELEMENTS(test_map)), test_map[INDEX_FOO].name));
    g_assert(bit == BIT_FOO);

    /* Same but searching for value */
    g_assert(!gsupplicant_name_int_find_int(BIT_MISSING,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!g_strcmp0(gsupplicant_name_int_find_int(BIT_BAR,
        test_map, G_N_ELEMENTS(test_map)), test_map[INDEX_BAR].name));

    /* Search for name */
    g_assert(gsupplicant_name_int_get_int("non-existent",
        test_map, G_N_ELEMENTS(test_map), BIT_MISSING) == BIT_MISSING);
    g_assert(gsupplicant_name_int_get_int(test_map[INDEX_BAR].name,
        test_map, G_N_ELEMENTS(test_map), BIT_MISSING) == BIT_BAR);
    g_assert(!gsupplicant_name_int_find_name(NULL,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_name("non-existent",
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_name_i(NULL,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_name_i("non-existent",
        test_map, G_N_ELEMENTS(test_map)));

    g_assert(gsupplicant_name_int_find_name(test_map[INDEX_BAR].name,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(gsupplicant_name_int_find_name(test_map[INDEX_BAR].name,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_find_name("Foo",
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(gsupplicant_name_int_find_name_i("Foo",
        test_map, G_N_ELEMENTS(test_map)));

    /* Set bits */
    bit = 0;
    g_assert(!gsupplicant_name_int_set_bits(NULL, NULL, NULL, 0));
    g_assert(!gsupplicant_name_int_set_bits(NULL, "non-existent",
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(gsupplicant_name_int_set_bits(NULL, test_map[INDEX_FOO].name,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(gsupplicant_name_int_set_bits(&bit, test_map[INDEX_FOO].name,
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(bit == BIT_FOO);

    /* Concatenate strings */
    g_assert(!gsupplicant_name_int_concat(0, ':',
        test_map, G_N_ELEMENTS(test_map)));
    g_assert(!gsupplicant_name_int_concat(BIT_MISSING, ':',
        test_map, G_N_ELEMENTS(test_map)));

    str = gsupplicant_name_int_concat(BIT_FOO|BIT_BAR, 0,
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "foo,bar")); /* Default separator is used */
    g_free(str);

    str = gsupplicant_name_int_concat(BIT_FOO|BIT_BAR, ':',
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "foo:bar"));
    g_free(str);

    str = gsupplicant_name_int_concat(BIT_FOO, 0,
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "foo"));
    g_free(str);

    str = gsupplicant_name_int_concat(BIT_EMPTY|BIT_XXX, 0,
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "xxx"));
    g_free(str);

    str = gsupplicant_name_int_concat(BIT_FOO|BIT_EMPTY, 0,
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "foo"));
    g_free(str);

    str = gsupplicant_name_int_concat(BIT_FOO|BIT_EMPTY|BIT_XXX, ':',
        test_map, G_N_ELEMENTS(test_map));
    g_assert(!g_strcmp0(str, "foo:xxx"));
    g_free(str);
}

/*==========================================================================*
 * parse_bits_array
 *==========================================================================*/

static
void
test_util_parse_bits_array(
    void)
{
    GVariantBuilder builder;
    GVariant* var;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("ai"));
    g_variant_builder_add_value(&builder, g_variant_new_int32(0));
    var = g_variant_ref_sink(g_variant_builder_end(&builder));

    g_assert(!gsupplicant_parse_bits_array(0, "test", var, 
        test_map, G_N_ELEMENTS(test_map)));
    g_variant_unref(var);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    g_variant_builder_add_value(&builder, g_variant_new_string("foo"));
    g_variant_builder_add_value(&builder, g_variant_new_string("unknown"));
    var = g_variant_ref_sink(g_variant_builder_end(&builder));

    g_assert(gsupplicant_parse_bits_array(0, "test", var, 
        test_map, G_N_ELEMENTS(test_map)) == BIT_FOO);
    g_variant_unref(var);
}

/*==========================================================================*
 * format_bytes
 *==========================================================================*/

static
void
test_util_format_bytes(
    void)
{
    static const guint8 data[] = { 0x01, 0x02, 0x03 };
    static const char* data_str1 = "01:02:03";
    static const char* data_str2 = "01:02:03 (3)";
    GMainLoop* loop;
    GBytes* bytes;

    g_assert(!g_strcmp0("(null)", gsupplicant_format_bytes(NULL, FALSE)));

    bytes = g_bytes_new_static(data, 0);
    g_assert(!g_strcmp0(gsupplicant_format_bytes(bytes, FALSE), ""));
    g_assert(!g_strcmp0(gsupplicant_format_bytes(bytes, TRUE), "(0)"));
    g_bytes_unref(bytes);

    bytes = g_bytes_new_static(data, sizeof(data));
    g_assert(!g_strcmp0(gsupplicant_format_bytes(bytes, FALSE), data_str1));
    g_assert(!g_strcmp0(gsupplicant_format_bytes(bytes, TRUE), data_str2));
    g_bytes_unref(bytes);

    /* Strings allocated by the above calls are deallocated by idle callback */
    loop = g_main_loop_new(NULL, TRUE);
    g_idle_add(test_loop_quit, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * call_later
 *==========================================================================*/

void
test_util_call_later_cb(
    void* data)
{
    (*((int*)data))++;
}

static
void
test_util_call_later(
    void)
{
    GMainLoop* loop;
    int count = 0;

    /* The callback is invoked even if the source gets removed */
    g_source_remove(gsupplicant_call_later(test_util_call_later_cb, &count));
    g_assert(count == 1);

    /* Or during the next idle loop if it doesn't get removed */
    gsupplicant_call_later(test_util_call_later_cb, &count);
    loop = g_main_loop_new(NULL, TRUE);
    g_idle_add(test_loop_quit, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    g_assert(count == 2);
}

/*==========================================================================*
 * cancel_later
 *==========================================================================*/

static
void
test_util_cancel_later(
    void)
{
    GMainLoop* loop;
    GCancellable* cancel;

    /* NULL is ignored */
    g_assert(!gsupplicant_cancel_later(NULL));

    /* It doesn't get cancelled if the source gets removed */
    cancel = g_cancellable_new();
    g_source_remove(gsupplicant_cancel_later(cancel));
    g_assert(!g_cancellable_is_cancelled(cancel));
    g_object_unref(cancel);

    /* But it does when we run the loop once */
    cancel = g_cancellable_new();
    gsupplicant_cancel_later(cancel);
    loop = g_main_loop_new(NULL, TRUE);
    g_idle_add(test_loop_quit, loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    g_assert(g_cancellable_is_cancelled(cancel));
    g_object_unref(cancel);
}

/*==========================================================================*
 * abs_path
 *==========================================================================*/

static
void
test_util_abs_path(
    void)
{
    char* name = NULL;
    int fd;

    g_assert(!gsupplicant_check_abs_path(NULL));
    g_assert(!gsupplicant_check_abs_path(""));
    g_assert(!gsupplicant_check_abs_path("foo"));

    fd = g_file_open_tmp("test-abspath-XXXXXX", &name, NULL);
    g_assert(fd >= 0);
    GDEBUG_("%s", name);
    g_assert(close(fd) == 0);

    /* Now a file should exist (albeit empty), the check should pass */
    g_assert(gsupplicant_check_abs_path(name));

    /* When the file is missing, the check should fail */
    g_assert(g_unlink(name) == 0);
    g_assert(!gsupplicant_check_abs_path(name));
    g_free(name);
}

/*==========================================================================*
 * dict_parse
 *==========================================================================*/

static
void
test_util_dict_cb(
    const char* key,
    GVariant* value,
    void* data)
{
    GHashTable* values = data;
    g_assert(!g_hash_table_contains(values, key));
    g_hash_table_insert(values, g_strdup(key), g_variant_ref(value));
}

static
void
test_util_dict_parse(
    void)
{
    GVariant* dict;
    GVariant* var;
    GVariantBuilder builder;
    static const guint test_data[] = {0x01, 0x02, 0x03};
    GBytes* test_bytes2[2];
    GBytes* test_bytes = g_bytes_new_static(test_data, sizeof(test_data));
    GBytes* bytes_value;
    GHashTable* values = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        test_variant_unref);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    gsupplicant_dict_add_boolean(&builder, "true", TRUE);
    gsupplicant_dict_add_uint32(&builder, "one", 1);
    gsupplicant_dict_add_string0(&builder, NULL, NULL);
    gsupplicant_dict_add_string0(&builder, NULL, "ignored");
    gsupplicant_dict_add_string0(&builder, "string", NULL);
    gsupplicant_dict_add_string0(&builder, "string", "string");
    gsupplicant_dict_add_string_ne(&builder, NULL, NULL);
    gsupplicant_dict_add_string_ne(&builder, NULL, "ignored");
    gsupplicant_dict_add_string_ne(&builder, "non-empty", NULL);
    gsupplicant_dict_add_string_ne(&builder, "non-empty", "");
    gsupplicant_dict_add_string_ne(&builder, "non-empty", "non-empty");
    gsupplicant_dict_add_bytes0(&builder, NULL, NULL);
    gsupplicant_dict_add_bytes0(&builder, NULL, test_bytes);
    gsupplicant_dict_add_bytes0(&builder, "bytes", NULL);
    gsupplicant_dict_add_bytes0(&builder, "bytes", test_bytes);
    dict = g_variant_ref_sink(g_variant_builder_end(&builder));
    GDEBUG_("%u", (guint)g_variant_n_children(dict));
    g_assert(g_variant_n_children(dict) == 5);

    g_assert(!gsupplicant_dict_parse(NULL, test_util_dict_cb, values));
    g_assert(!g_hash_table_size(values));
    g_assert(gsupplicant_dict_parse(dict, test_util_dict_cb, values) == 5);
    g_assert(g_variant_get_boolean(g_hash_table_lookup(values, "true")));
    g_assert(g_variant_get_uint32(g_hash_table_lookup(values, "one")) == 1);
    g_assert(!g_strcmp0(g_variant_get_string(g_hash_table_lookup(values,
        "string"), NULL), "string"));
    g_assert(!g_strcmp0(g_variant_get_string(g_hash_table_lookup(values,
        "non-empty"), NULL), "non-empty"));
    g_assert(!gsupplicant_variant_data_as_bytes(NULL));
    bytes_value = gsupplicant_variant_data_as_bytes(g_hash_table_lookup(values,
        "bytes"));
    g_assert(g_bytes_equal(bytes_value, test_bytes));
    g_bytes_unref(bytes_value);
    g_variant_unref(dict);
    g_hash_table_remove_all(values);

    /* Test it on a non-dictionary collection */
    var = gsupplicant_variant_new_ayy(NULL);
    g_assert(var && !g_variant_n_children(var));
    g_variant_unref(var);

    test_bytes2[0] = test_bytes;
    test_bytes2[1] = NULL;
    var = gsupplicant_variant_new_ayy(test_bytes2);
    g_assert(g_variant_n_children(var) == 1);
    g_assert(!gsupplicant_dict_parse(var, test_util_dict_cb, values));
    g_assert(!g_hash_table_size(values));
    g_variant_unref(var);

    g_hash_table_unref(values);
    g_bytes_unref(test_bytes);
}

/*==========================================================================*
 * utf8_from_bytes
 *==========================================================================*/

typedef struct test_util_utf8_data {
    const char* name;
    const void* in;
    gsize in_size;
    const guint32* ucs4;
    glong ucs4_len;
} TestUTF8Data;

static const guint8 test_util_utf8_data_1_in[] = {
    0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82
};
static const guint32 test_util_utf8_data_1_ucs4[] = {
    0x0442, 0x0435, 0x0441, 0x0442
};
static const guint8 test_util_utf8_data_2_in[] = {
    0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82, 0x81
};
static const guint32 test_util_utf8_data_2_ucs4[] = {
    0x0442, 0x0435, 0x0441, 0x0442, 0xfffd
};
static const guint8 test_util_utf8_data_3_in[] = {
    0xf0, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82
};
static const guint32 test_util_utf8_data_3_ucs4[] = {
    0xfffd, 0x0442, 0x0435, 0x0441, 0x0442
};
static const TestUTF8Data test_util_utf8_data[] = {
    {
        TEST_PREFIX "utf8_from_bytes1",
        test_util_utf8_data_1_in, G_N_ELEMENTS(test_util_utf8_data_1_in),
        test_util_utf8_data_1_ucs4, G_N_ELEMENTS(test_util_utf8_data_1_ucs4)
    },{
        TEST_PREFIX "utf8_from_bytes2",
        test_util_utf8_data_2_in, G_N_ELEMENTS(test_util_utf8_data_2_in),
        test_util_utf8_data_2_ucs4, G_N_ELEMENTS(test_util_utf8_data_2_ucs4)
    },{
        TEST_PREFIX "utf8_from_bytes3",
        test_util_utf8_data_3_in, G_N_ELEMENTS(test_util_utf8_data_3_in),
        test_util_utf8_data_3_ucs4, G_N_ELEMENTS(test_util_utf8_data_3_ucs4)
    }
};

static
void
test_util_utf8_from_bytes(
    void)
{
    GBytes* nothing = g_bytes_new_static(NULL, 0);
    char* empty = gsupplicant_utf8_from_bytes(nothing);
    g_assert(!g_strcmp0(empty, ""));
    g_assert(!gsupplicant_utf8_from_bytes(NULL));
    g_bytes_unref(nothing);
    g_free(empty);
}

static
void
test_util_utf8_from_bytes1(
    gconstpointer data)
{
    const TestUTF8Data* test = data;
    GBytes* bytes = g_bytes_new_static(test->in, test->in_size);
    char* utf8 = gsupplicant_utf8_from_bytes(bytes);
    if (utf8) {
        GError* error = NULL;
        glong len = 0;
        gunichar* ucs4 = g_utf8_to_ucs4(utf8, -1, NULL, &len, &error);
        g_assert(!error);
        g_assert(test->ucs4);
        g_assert(test->ucs4_len == len);
        g_assert(!memcmp(ucs4, test->ucs4, sizeof(guint32)*len));
        g_free(utf8);
        g_free(ucs4);
    } else {
        g_assert(!test->ucs4);
    }
    g_bytes_unref(bytes);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

int main(int argc, char* argv[])
{
    int i;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    g_type_init();
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_PREFIX "name_int", test_util_name_int);
    g_test_add_func(TEST_PREFIX "parse_bits_array", test_util_parse_bits_array);
    g_test_add_func(TEST_PREFIX "format_bytes", test_util_format_bytes);
    g_test_add_func(TEST_PREFIX "call_later", test_util_call_later);
    g_test_add_func(TEST_PREFIX "cancel_later", test_util_cancel_later);
    g_test_add_func(TEST_PREFIX "abs_path", test_util_abs_path);
    g_test_add_func(TEST_PREFIX "dict_parse", test_util_dict_parse);
    g_test_add_func(TEST_PREFIX "utf8_from_bytes", test_util_utf8_from_bytes);
    for (i = 0; i < G_N_ELEMENTS(test_util_utf8_data); i++) {
        const TestUTF8Data* test = test_util_utf8_data + i;
        g_test_add_data_func(test->name, test, test_util_utf8_from_bytes1);
    }
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
