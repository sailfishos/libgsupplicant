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

#ifndef GSUPPLICANT_ERROR_H
#define GSUPPLICANT_ERROR_H

#include <gsupplicant_types.h>

G_BEGIN_DECLS

#define GSUPPLICANT_ERROR (gsupplicant_error_quark())
GQuark gsupplicant_error_quark(void);

#define GSUPPLICANT_ERRORS(e)                    \
    e(UNKNOWN_ERROR,        "UnknownError")      \
    e(INVALID_ARGS,         "InvalidArgs")       \
    e(NO_MEMORY,            "NoMemory")          \
    e(NOT_CONNECTED,        "NotConnected")      \
    e(NETWORK_UNKNOWN,      "NetworkUnknown")    \
    e(INTERFACE_UNKNOWN,    "InterfaceUnknown")  \
    e(INTERFACE_DISABLED,   "InterfaceDisabled") \
    e(BLOB_UNKNOWN,         "BlobUnknown")       \
    e(BLOB_EXISTS,          "BlobExists")        \
    e(NO_SUBSCRIPTION,      "NoSubscription")    \
    e(SUBSCRIPTION_IN_USE,  "SubscriptionInUse") \
    e(SUBSCRIPTION_NOT_YOU, "SubscriptionNotYou")

typedef enum gsupplicant_error_code {
#define GSUPPLICANT_ERROR_ENUM_(E,e) GSUPPLICANT_ERROR_##E,
    GSUPPLICANT_ERRORS(GSUPPLICANT_ERROR_ENUM_)
#undef GSUPPLICANT_ERROR_ENUM_
} GSUPPLICANT_ERROR_CODE;

gboolean
gsupplicant_is_error(
    const GError* error,
    GSUPPLICANT_ERROR_CODE code);

G_END_DECLS

#endif /* GSUPPLICANT_ERROR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
