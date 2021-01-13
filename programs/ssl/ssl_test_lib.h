/*
 *  Common code for SSL test programs
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H
#define MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_calloc     calloc
#define mbedtls_free       free
#define mbedtls_time       time
#define mbedtls_time_t     time_t
#define mbedtls_printf     printf
#define mbedtls_fprintf    fprintf
#define mbedtls_snprintf   snprintf
#define mbedtls_exit            exit
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif

#if !defined(MBEDTLS_ENTROPY_C) ||                          \
    !defined(MBEDTLS_NET_C) ||                              \
    !defined(MBEDTLS_SSL_TLS_C) ||                          \
    defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)
#define MBEDTLS_SSL_TEST_IMPOSSIBLE                             \
    "MBEDTLS_ENTROPY_C and/or "                                 \
    "MBEDTLS_NET_C and/or "                                     \
    "MBEDTLS_SSL_TLS_C not defined, "                           \
    "and/or MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER defined.\n"
#elif !( defined(MBEDTLS_CTR_DRBG_C) ||                                 \
         defined(MBEDTLS_HMAC_DRBG_C) && ( defined(MBEDTLS_SHA256_C) || \
                                           defined(MBEDTLS_SHA512_C) ) )
#define MBEDTLS_SSL_TEST_IMPOSSIBLE                                     \
    "Neither MBEDTLS_CTR_DRBG_C, nor MBEDTLS_HMAC_DRBG_C and a supported hash defined.\n"
#else
#undef MBEDTLS_SSL_TEST_IMPOSSIBLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"
#include "mbedtls/base64.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#include "mbedtls/psa_util.h"
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif

#include <test/helpers.h>

#include "../test/query_config.h"

#if defined(MBEDTLS_SSL_EXPORT_KEYS)

typedef struct eap_tls_keys
{
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} eap_tls_keys;

#if defined( MBEDTLS_SSL_DTLS_SRTP )

/* Supported SRTP mode needs a maximum of :
 * - 16 bytes for key (AES-128)
 * - 14 bytes SALT
 * One for sender, one for receiver context
 */
#define MBEDTLS_TLS_SRTP_MAX_KEY_MATERIAL_LENGTH    60

typedef struct dtls_srtp_keys
{
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} dtls_srtp_keys;

#endif /* MBEDTLS_SSL_DTLS_SRTP */

#endif /* MBEDTLS_SSL_EXPORT_KEYS */

typedef struct
{
    mbedtls_ssl_context *ssl;
    mbedtls_net_context *net;
} io_ctx_t;

void my_debug( void *ctx, int level,
               const char *file, int line,
               const char *str );

mbedtls_time_t dummy_constant_time( mbedtls_time_t* time );

/** A context for random number generation (RNG).
 */
typedef struct
{
    mbedtls_entropy_context entropy;
#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_context drbg;
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_context drbg;
#else
#error "No DRBG available"
#endif
} rng_context_t;

/** Initialize the RNG.
 *
 * This function only initializes the memory used by the RNG context.
 * Before using the RNG, it must be seeded with rng_seed().
 */
void rng_init( rng_context_t *rng );

/* Seed the random number generator.
 *
 * \param rng           The RNG context to use. It must have been initialized
 *                      with rng_init().
 * \param reproducible  If zero, seed the RNG from entropy.
 *                      If nonzero, use a fixed seed, so that the program
 *                      will produce the same sequence of random numbers
 *                      each time it is invoked.
 * \param pers          A null-terminated string. Different values for this
 *                      string cause the RNG to emit different output for
 *                      the same seed.
 *
 * return 0 on success, a negative value on error.
 */
int rng_seed( rng_context_t *rng, int reproducible, const char *pers );

/** Deinitialize the RNG. Free any embedded resource.
 *
 * \param rng           The RNG context to deinitialize. It must have been
 *                      initialized with rng_init().
 */
void rng_free( rng_context_t *rng );

/** Generate random data.
 *
 * This function is suitable for use as the \c f_rng argument to Mbed TLS
 * library functions.
 *
 * \param p_rng         The CTR_DRBG context. This must be a pointer to a
 *                      #rng_context_t structure.
 * \param output        The buffer to fill.
 * \param output_len    The length of the buffer in bytes.
 *
 * \return              \c 0 on success.
 * \return              An Mbed TLS error code on error.
 */
int rng_get( void *p_rng, unsigned char *output, size_t output_len );

#if defined(MBEDTLS_USE_PSA_CRYPTO) && defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
/* The test implementation of the PSA external RNG is insecure. When
 * MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG is enabled, before using any PSA crypto
 * function that makes use of an RNG, you must call
 * mbedtls_test_enable_insecure_external_rng(). */
#include <test/psa_crypto_helpers.h>
#endif

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
int ca_callback( void *data, mbedtls_x509_crt const *child,
                 mbedtls_x509_crt **candidates );
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

/*
 * Test recv/send functions that make sure each try returns
 * WANT_READ/WANT_WRITE at least once before sucesseding
 */
int delayed_recv( void *ctx, unsigned char *buf, size_t len );
int delayed_send( void *ctx, const unsigned char *buf, size_t len );

/*
 * Wait for an event from the underlying transport or the timer
 * (Used in event-driven IO mode).
 */
int idle( mbedtls_net_context *fd,
#if defined(MBEDTLS_TIMING_C)
          mbedtls_timing_delay_context *timer,
#endif
          int idle_reason );

#endif /* MBEDTLS_SSL_TEST_IMPOSSIBLE conditions: else */
#endif /* MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H */
