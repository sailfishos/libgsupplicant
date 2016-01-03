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

#include "gsupplicant_error.h"
#include "gsupplicant_dbus.h"

#define GSUPPLICANT_ERROR_ENTRY(E,e) \
    {GSUPPLICANT_ERROR_##E, GSUPPLICANT_SERVICE "." e},

static const GDBusErrorEntry gsupplicant_errors[] = {
    GSUPPLICANT_ERRORS(GSUPPLICANT_ERROR_ENTRY)
};

GQuark
gsupplicant_error_quark()
{
    static volatile gsize gsupplicant_error_quark_value = 0;
    g_dbus_error_register_error_domain("gsupplicant-error-quark",
        &gsupplicant_error_quark_value, gsupplicant_errors,
        G_N_ELEMENTS(gsupplicant_errors));
    return (GQuark)gsupplicant_error_quark_value;
}

gboolean
gsupplicant_is_error(
    const GError* error,
    GSUPPLICANT_ERROR_CODE code)
{
    return error && error->domain == GSUPPLICANT_ERROR && error->code == code;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
