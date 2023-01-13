/*
 * Copyright (C) 2015-2020 Jolla Ltd.
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

#ifndef GSUPPLICANT_TYPES_H
#define GSUPPLICANT_TYPES_H

#include <gutil_types.h>

G_BEGIN_DECLS

#define GSUPPLICANT_INLINE static inline
#define GSUPPLICANT_LOG_MODULE gsupplicant_log

typedef struct gsupplicant              GSupplicant;
typedef struct gsupplicant_bss          GSupplicantBSS;
typedef struct gsupplicant_network      GSupplicantNetwork;
typedef struct gsupplicant_interface    GSupplicantInterface;

typedef enum gsupplicant_cipher {
    GSUPPLICANT_CIPHER_INVALID          = (0x00000000),
    GSUPPLICANT_CIPHER_NONE             = (0x00000001),
    GSUPPLICANT_CIPHER_CCMP             = (0x00000002),
    GSUPPLICANT_CIPHER_TKIP             = (0x00000004),
    GSUPPLICANT_CIPHER_WEP104           = (0x00000008),
    GSUPPLICANT_CIPHER_WEP40            = (0x00000010),
    GSUPPLICANT_CIPHER_AES128_CMAC      = (0x00000020)
} GSUPPLICANT_CIPHER;

typedef enum gsupplicant_keymgmt {
    GSUPPLICANT_KEYMGMT_INVALID         = (0x00000000),
    GSUPPLICANT_KEYMGMT_NONE            = (0x00000001),
    GSUPPLICANT_KEYMGMT_WPA_PSK         = (0x00000002),
    GSUPPLICANT_KEYMGMT_WPA_FT_PSK      = (0x00000004),
    GSUPPLICANT_KEYMGMT_WPA_PSK_SHA256  = (0x00000008),
    GSUPPLICANT_KEYMGMT_WPA_EAP         = (0x00000010),
    GSUPPLICANT_KEYMGMT_WPA_FT_EAP      = (0x00000020),
    GSUPPLICANT_KEYMGMT_WPA_EAP_SHA256  = (0x00000040),
    GSUPPLICANT_KEYMGMT_IEEE8021X       = (0x00000080),
    GSUPPLICANT_KEYMGMT_WPA_NONE        = (0x00000100),
    GSUPPLICANT_KEYMGMT_WPS             = (0x00000200),
    GSUPPLICANT_KEYMGMT_SAE             = (0x00000400), /* Since 1.0.27 */
    GSUPPLICANT_KEYMGMT_SAE_EXT_KEY     = (0x00000800), /* Since 1.0.27 */
    GSUPPLICANT_KEYMGMT_FT_SAE          = (0x00001000), /* Since 1.0.27 */
    GSUPPLICANT_KEYMGMT_FT_SAE_EXT_KEY  = (0x00002000)  /* Since 1.0.27 */
} GSUPPLICANT_KEYMGMT;

typedef enum gsupplicant_protocol {
    GSUPPLICANT_PROTOCOL_NONE           = (0x00000000),
    GSUPPLICANT_PROTOCOL_RSN            = (0x00000001),
    GSUPPLICANT_PROTOCOL_WPA            = (0x00000002)
} GSUPPLICANT_PROTOCOL;

typedef enum gsupplicant_auth {
    GSUPPLICANT_AUTH_NONE               = (0x00000000),
    GSUPPLICANT_AUTH_OPEN               = (0x00000001),
    GSUPPLICANT_AUTH_SHARED             = (0x00000002),
    GSUPPLICANT_AUTH_LEAP               = (0x00000004),
    GSUPPLICANT_AUTH_WPA_PSK            = (0x00000010),
    GSUPPLICANT_AUTH_WPA_EAP            = (0x00000020),
    GSUPPLICANT_AUTH_WPA2_EAP           = (0x00000040),
    GSUPPLICANT_AUTH_WPA2_PSK           = (0x00000080)
} GSUPPLICANT_AUTH;

typedef enum gsupplicant_wps_caps {
    GSUPPLICANT_WPS_NONE                = (0x00000000),
    GSUPPLICANT_WPS_SUPPORTED           = (0x00000001),
    GSUPPLICANT_WPS_CONFIGURED          = (0x00000002),
    GSUPPLICANT_WPS_PUSH_BUTTON         = (0x00000004),
    GSUPPLICANT_WPS_PIN                 = (0x00000008),
    GSUPPLICANT_WPS_REGISTRAR           = (0x00000010)
} GSUPPLICANT_WPS_CAPS;

typedef enum gsupplicant_wps_role {
    GSUPPLICANT_WPS_ROLE_NONE           = (0x00000000),
    GSUPPLICANT_WPS_ROLE_ENROLLEE       = (0x00000001),
    GSUPPLICANT_WPS_ROLE_REGISTRAR      = (0x00000002)
} GSUPPLICANT_WPS_ROLE;

typedef enum gsupplicant_wps_auth {
    GSUPPLICANT_WPS_AUTH_NONE           = (0x00000000),
    GSUPPLICANT_WPS_AUTH_PUSH_BUTTON    = (0x00000001),
    GSUPPLICANT_WPS_AUTH_PIN            = (0x00000002)
} GSUPPLICANT_WPS_AUTH;

typedef enum gsupplicant_wps_encr {
    GSUPPLICANT_WPS_ENCR_NONE           = (0x00000001),
    GSUPPLICANT_WPS_ENCR_WEP            = (0x00000002),
    GSUPPLICANT_WPS_ENCR_TKIP           = (0x00000004),
    GSUPPLICANT_WPS_ENCR_AES            = (0x00000008)
} GSUPPLICANT_WPS_ENCR;

typedef enum gsupplicant_eap_method {
    GSUPPLICANT_EAP_METHOD_NONE         = (0x00000000),
    GSUPPLICANT_EAP_METHOD_PEAP         = (0x00000001),
    GSUPPLICANT_EAP_METHOD_TTLS         = (0x00000002),
    GSUPPLICANT_EAP_METHOD_TLS          = (0x00000004),
    GSUPPLICANT_EAP_METHOD_MSCHAPV2     = (0x00000008),
    GSUPPLICANT_EAP_METHOD_MD5          = (0x00000010),
    GSUPPLICANT_EAP_METHOD_GTC          = (0x00000020),
    GSUPPLICANT_EAP_METHOD_OTP          = (0x00000040),
    GSUPPLICANT_EAP_METHOD_SIM          = (0x00000080),
    GSUPPLICANT_EAP_METHOD_LEAP         = (0x00000100),
    GSUPPLICANT_EAP_METHOD_PSK          = (0x00000200),
    GSUPPLICANT_EAP_METHOD_AKA          = (0x00000400),
    GSUPPLICANT_EAP_METHOD_FAST         = (0x00000800),
    GSUPPLICANT_EAP_METHOD_PAX          = (0x00001000),
    GSUPPLICANT_EAP_METHOD_SAKE         = (0x00002000),
    GSUPPLICANT_EAP_METHOD_GPSK         = (0x00004000),
    GSUPPLICANT_EAP_METHOD_WSC          = (0x00008000),
    GSUPPLICANT_EAP_METHOD_IKEV2        = (0x00010000),
    GSUPPLICANT_EAP_METHOD_TNC          = (0x00020000),
    GSUPPLICANT_EAP_METHOD_PWD          = (0x00040000)
} GSUPPLICANT_EAP_METHOD;

typedef enum gsupplicant_auth_flags {
    GSUPPLICANT_AUTH_DEFAULT            = (0x00000000),
    GSUPPLICANT_AUTH_PHASE2_AUTHEAP     = (0x00000001),
    GSUPPLICANT_AUTH_PHASE1_PEAPV0      = (0x00000002), /* Since 1.0.12 */
    GSUPPLICANT_AUTH_PHASE1_PEAPV1      = (0x00000004)  /* Since 1.0.12 */
} GSUPPLICANT_AUTH_FLAGS;

typedef enum gsupplicant_op_mode {
    GSUPPLICANT_OP_MODE_INFRA,
    GSUPPLICANT_OP_MODE_IBSS,
    GSUPPLICANT_OP_MODE_AP
} GSUPPLICANT_OP_MODE;

typedef enum gsupplicant_security {
    GSUPPLICANT_SECURITY_NONE,
    GSUPPLICANT_SECURITY_WEP,
    GSUPPLICANT_SECURITY_PSK,
    GSUPPLICANT_SECURITY_EAP,
} GSUPPLICANT_SECURITY;

typedef struct gsupplicant_uint_array {
    const guint* values;
    guint count;
} GSupplicantUIntArray;

extern GLogModule GSUPPLICANT_LOG_MODULE;

G_END_DECLS

#endif /* GSUPPLICANT_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
