/**
 * @file secSocket_openssl.c
 *
 * This file implements a set of networking functions to manage secure TCP/UDP sockets using
 * OpenSSL library.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include "legato.h"
#include "interfaces.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "secSocket.h"
#include "le_socketLib.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Magic number used in OpenSSL structure to check the structure validity
 */
//--------------------------------------------------------------------------------------------------
#define OPENSSL_MAGIC_NUMBER        0x4F50454E

//--------------------------------------------------------------------------------------------------
/**
 * Port maximum length
 */
//--------------------------------------------------------------------------------------------------
#define PORT_STR_LEN                6

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of a single ALPN protocol name string
 */
//--------------------------------------------------------------------------------------------------
#define ALPN_PROTO_NAME_MAX_LEN     32

//--------------------------------------------------------------------------------------------------
/**
 * OpenSSL global context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t                 magicNb;           ///< Magic number to check structure validity
    BIO*                     bioPtr;            ///< I/O stream abstraction pointer
    SSL_CTX*                 sslCtxPtr;         ///< SSL internal context pointer
    bool                     isInit;            ///< TRUE if the secure socket context is initialized
    int                      openssl_errcode;   ///< OpenSSL error codes.
    ProtoRoleType_t          role;              ///< Protocol role (server or client)
    uint8_t*                 alpnProtosBuf;     ///< Encoded ALPN protocol list (length-prefixed)
    size_t                   alpnProtosBufLen;  ///< Byte length of alpnProtosBuf
}
OpensslCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool for OpenSSL sockets context.
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(SocketCtxPool, MAX_SOCKET_NB, sizeof(OpensslCtx_t));

//--------------------------------------------------------------------------------------------------
// Internal variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool reference for the OpenSSL context pool
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t SocketCtxPoolRef = NULL;

//--------------------------------------------------------------------------------------------------
// Static functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Cast secure socket context into OpenSSL socket context and check its validity
 *
 * @return
 *  - OpenSSL socket context pointer
 */
//--------------------------------------------------------------------------------------------------
static OpensslCtx_t* GetContext
(
    secSocket_Ctx_t*  ctxPtr   ///< [IN] Secure socket context pointer
)
{
    if (!ctxPtr)
    {
        return NULL;
    }

    OpensslCtx_t* sslSocketCtx = (secSocket_Ctx_t*)ctxPtr;

    if (sslSocketCtx->magicNb == OPENSSL_MAGIC_NUMBER)
    {
        return sslSocketCtx;
    }
    else
    {
        LE_ERROR("Unrecognized context provided");
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Initialize a secure socket using the input certificate.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Init
(
    ProtoRoleType_t    role,        ///< [IN] Protocol role type
    secSocket_Ctx_t**  ctxPtr       ///< [INOUT] Secure socket context pointer
)
{
    // Check input parameter
    if (!ctxPtr)
    {
        LE_ERROR("Null pointer provided");
        return LE_BAD_PARAMETER;
    }

    // Check if the socket is already initialized
    OpensslCtx_t* contextPtr = GetContext(*ctxPtr);
    if ((contextPtr) && (contextPtr->isInit))
    {
        LE_ERROR("Socket context already initialized");
        return LE_FAULT;
    }

    // Alloc memory from pool
    contextPtr = le_mem_ForceAlloc(SocketCtxPoolRef);
    if (NULL == contextPtr)
    {
        LE_ERROR("Unable to allocate a socket context from pool");
        return LE_FAULT;
    }

    // Set the magic number
    contextPtr->magicNb = OPENSSL_MAGIC_NUMBER;

    // Initialize OpenSSL library and setup SSL pointers
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();

    if (role == PROTO_ROLE_CLIENT)
    {
        contextPtr->sslCtxPtr = SSL_CTX_new(TLSv1_client_method());
    }
    else
    {
        contextPtr->sslCtxPtr = SSL_CTX_new(TLSv1_server_method());
    }
#else
    OPENSSL_init_ssl(0, NULL);

    if (role == PROTO_ROLE_CLIENT)
    {
        contextPtr->sslCtxPtr = SSL_CTX_new(TLS_client_method());
    }
    else
    {
        contextPtr->sslCtxPtr = SSL_CTX_new(TLS_server_method());
    }
#endif

    SSL_CTX_set_min_proto_version(contextPtr->sslCtxPtr, TLS1_2_VERSION);
    SSL_CTX_clear_options(contextPtr->sslCtxPtr, SSL_OP_LEGACY_SERVER_CONNECT);
    SSL_CTX_set_options(contextPtr->sslCtxPtr, SSL_OP_NO_COMPRESSION);

    contextPtr->isInit          = true;
    contextPtr->openssl_errcode = 0;
    contextPtr->role            = role;
    contextPtr->alpnProtosBuf   = NULL;
    contextPtr->alpnProtosBufLen = 0;
    *ctxPtr = (secSocket_Ctx_t*)contextPtr;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add one or more certificates to the secure socket context.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FORMAT_ERROR  Invalid certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddCertificate
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const uint8_t*    certificatePtr,   ///< [IN] Certificate Pointer
    size_t            certificateLen    ///< [IN] Certificate Length
)
{
    X509_STORE *store = NULL;
    X509 *cert = NULL;
    BIO *bio = NULL;
    le_result_t status = LE_FAULT;
    le_clk_Time_t currentTime;

    // Check input parameters
    if ((!ctxPtr) || (!certificatePtr) || (!certificateLen))
    {
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    LE_INFO("Certificate: %p Len:%"PRIuS, certificatePtr, certificateLen);

    // Get a BIO abstraction pointer
    bio = BIO_new_mem_buf((void*)certificatePtr, certificateLen);
    if (!bio)
    {
        LE_ERROR("Unable to allocate BIO pointer");
        contextPtr->openssl_errcode = ERR_R_MALLOC_FAILURE;
        goto end;
    }

    // Read the DER formatted certificate from memory into an X509 structure
    cert = d2i_X509(NULL, &certificatePtr, certificateLen);
    if (!cert)
    {
        contextPtr->openssl_errcode = ERR_R_X509_LIB;
        LE_ERROR("Unable to read certificate");
        goto end;
    }

    // Check certificate validity
    currentTime = le_clk_GetAbsoluteTime();

    if ((X509_cmp_time(X509_get_notBefore(cert), &currentTime.sec) >= 0)  ||
        (X509_cmp_time(X509_get_notAfter(cert), &currentTime.sec) <= 0))
    {
        contextPtr->openssl_errcode = X509_V_ERR_CERT_HAS_EXPIRED;
        LE_ERROR("Current certificate expired, please add a valid certificate");
        status = LE_FORMAT_ERROR;
        goto end;
    }

    // Get a pointer to the current certificate verification pool
    store = SSL_CTX_get_cert_store(contextPtr->sslCtxPtr);
    if (!store)
    {
        contextPtr->openssl_errcode = X509_V_ERR_UNABLE_TO_GET_CRL;
        LE_ERROR("Unable to get a pointer to the X509 certificate");
        goto end;
    }

    // Add certificate to the verification pool
    if (!X509_STORE_add_cert(store, cert))
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("Unable to add certificate to pool");
        goto end;
    }

    status = LE_OK;

end:
    if (cert)
    {
        X509_free(cert);
    }

    if (bio)
    {
        BIO_free(bio);
    }

    return status;
}

// Search for PEM header in first up-to-2KB bytes
// BEGIN CERTIFICATE / BEGIN PRIVATE KEY / BEGIN RSA PRIVATE KEY
static bool HasPemHeader
(
    const uint8_t*  ptr,
    size_t          len
)
{
    static const char hdr[] = "-----BEGIN";
    size_t n = (len < 2048) ? len : 2048;

    if (ptr == NULL || n < (sizeof(hdr) - 1))
    {
        return false;
    }

    for (size_t i = 0; (i + (sizeof(hdr) - 1)) <= n; i++)
    {
        if (memcmp(ptr + i, hdr, sizeof(hdr) - 1) == 0)
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add client certificates to the secure socket context.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_FORMAT_ERROR  Invalid certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnCertificate
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const uint8_t*    certificatePtr,   ///< [IN] Certificate pointer
    size_t            certificateLen    ///< [IN] Certificate length
)
{
    const uint8_t*  p           = NULL;
    X509*           cert        = NULL;
    le_result_t     status      = LE_FAULT;
    bool            isFirst     = true;
    long            remaining   = 0;
    le_clk_Time_t   currentTime;

    // Check input parameters
    if ((!ctxPtr) || (!certificatePtr) || (!certificateLen))
    {
        LE_ERROR("Invalid parameter: ctxPtr %p, certificatePtr %p, certificateLen %"PRIuS,
                 ctxPtr, certificatePtr, certificateLen);
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_FAULT;
    }

    // Clear any existing extra chain certificates to avoid accumulation
    if (contextPtr->sslCtxPtr != NULL)
    {
        SSL_CTX_clear_extra_chain_certs(contextPtr->sslCtxPtr);
    }

    LE_INFO("Own certificate: %p Len:%"PRIuS, certificatePtr, certificateLen);

    // Clear any stale errors before processing
    ERR_clear_error();

    p           = certificatePtr;
    remaining   = (long)certificateLen;
    currentTime = le_clk_GetAbsoluteTime();

    // DER or PEM
    bool looksPem = HasPemHeader(certificatePtr, certificateLen);

    if (!looksPem)  // DER
    {
        // The buffer may contain a chain: one leaf certificate followed by zero or more
        // intermediate CA certificates, all DER-encoded and concatenated.
        // Parse them one by one until the buffer is exhausted.
        while (remaining > 0)
        {
            const uint8_t* before = p;

            cert = d2i_X509(NULL, &p, remaining);
            if (!cert)
            {
                unsigned long code = ERR_peek_last_error();
                contextPtr->openssl_errcode = ERR_GET_REASON(code);
                LE_ERROR("Unable to decode certificate from buffer (offset %ld)",
                        (long)(p - certificatePtr));
                status = LE_FORMAT_ERROR;
                goto end;
            }

            // Advance the remaining byte count by how many bytes d2i_X509 consumed
            remaining -= (long)(p - before);

            // Check certificate validity period
            if ((X509_cmp_time(X509_get_notBefore(cert), &currentTime.sec) >= 0) ||
                (X509_cmp_time(X509_get_notAfter(cert),  &currentTime.sec) <= 0))
            {
                contextPtr->openssl_errcode = X509_V_ERR_CERT_HAS_EXPIRED;
                LE_ERROR("Certificate is expired or not yet valid, please provide a valid certificate");
                X509_free(cert);
                cert   = NULL;
                status = LE_FORMAT_ERROR;
                goto end;
            }

            if (isFirst)
            {
                // The first certificate in the buffer is the server's own (leaf) certificate.
                // SSL_CTX_use_certificate() installs it as the certificate to present to clients
                // during the TLS handshake. It does NOT take ownership, so free it afterwards.
                if (SSL_CTX_use_certificate(contextPtr->sslCtxPtr, cert) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_use_certificate failed (reason: %lu)",
                            (unsigned long)ERR_GET_REASON(code));
                    X509_free(cert);
                    cert   = NULL;
                    status = LE_FORMAT_ERROR;
                    goto end;
                }

                X509_free(cert);
                cert    = NULL;
                isFirst = false;
            }
            else
            {
                // Subsequent certificates are intermediate CA certificates that complete the
                // chain sent to the peer. SSL_CTX_add_extra_chain_cert() TAKES ownership of
                // the X509 object on success, so do NOT free it in that case.
                if (SSL_CTX_add_extra_chain_cert(contextPtr->sslCtxPtr, cert) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_add_extra_chain_cert failed (reason: %lu)",
                            (unsigned long)ERR_GET_REASON(code));
                    X509_free(cert);
                    cert = NULL;
                    status = LE_FORMAT_ERROR;
                    goto end;
                }

                // Ownership transferred to SSL_CTX on success; do not free.
                cert = NULL;
            }
        }

        if (isFirst)
        {
            // Buffer was non-empty but contained no decodable DER certificate
            LE_ERROR("Certificate buffer contained no valid DER certificate");
            status = LE_FORMAT_ERROR;
            goto end;
        }
    }
    else  // PEM
    {
        // Parse one or more PEM cert blocks from memory BIO.

        BIO *bio = BIO_new_mem_buf((void*)certificatePtr, (int)certificateLen);
        if (!bio)
        {
            contextPtr->openssl_errcode = ERR_R_MALLOC_FAILURE;
            LE_ERROR("BIO_new_mem_buf failed");
            status = LE_FAULT;
            goto end;
        }

        while (1)
        {
            ERR_clear_error();
            cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
            if (!cert)
            {
                unsigned long code = ERR_peek_last_error();
                if ((ERR_GET_LIB(code) == ERR_LIB_PEM && ERR_GET_REASON(code) == PEM_R_NO_START_LINE))
                {
                    // No start line
                    ERR_clear_error();
                    break;
                }

                // At EOF it's normal that PEM_read_bio_X509 returns NULL.
                // But it may also set an error like "no start line".
                if (BIO_eof(bio))
                {
                    break;
                }

                contextPtr->openssl_errcode = ERR_GET_REASON(code);
                LE_ERROR("Unable to decode PEM certificate (reason: %lu)",
                         (unsigned long)ERR_GET_REASON(code));
                status = LE_FORMAT_ERROR;
                BIO_free(bio);
                goto end;
            }

            // Validity check
            if ((X509_cmp_time(X509_get_notBefore(cert), &currentTime.sec) >= 0) ||
                (X509_cmp_time(X509_get_notAfter(cert),  &currentTime.sec) <= 0))
            {
                contextPtr->openssl_errcode = X509_V_ERR_CERT_HAS_EXPIRED;
                X509_free(cert);
                cert = NULL;
                status = LE_FORMAT_ERROR;
                BIO_free(bio);
                goto end;
            }

            if (isFirst)
            {
                if (SSL_CTX_use_certificate(contextPtr->sslCtxPtr, cert) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_use_certificate failed (reason: %lu)",
                             (unsigned long)ERR_GET_REASON(code));
                    X509_free(cert);
                    cert = NULL;
                    status = LE_FORMAT_ERROR;
                    BIO_free(bio);
                    goto end;
                }

                X509_free(cert);
                cert = NULL;
                isFirst = false;
            }
            else
            {
                if (SSL_CTX_add_extra_chain_cert(contextPtr->sslCtxPtr, cert) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_add_extra_chain_cert failed (reason: %lu)",
                             (unsigned long)ERR_GET_REASON(code));
                    X509_free(cert);
                    cert = NULL;
                    status = LE_FORMAT_ERROR;
                    BIO_free(bio);
                    goto end;
                }

                // ownership transferred
                cert = NULL;
            }
        }

        BIO_free(bio);

        if (isFirst)
        {
            LE_ERROR("Certificate buffer contained no valid PEM certificate");
            status = LE_FORMAT_ERROR;
            goto end;
        }

    }

    status = LE_OK;

end:
    if (cert)
    {
        X509_free(cert);
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add the client private key to the secure socket context.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnPrivateKey
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const uint8_t*    pkeyPtr,          ///< [IN] Private key pointer
    size_t            pkeyLen           ///< [IN] Private key length
)
{
    EVP_PKEY*       pkey      = NULL;
    const uint8_t*  p         = NULL;
    le_result_t     status    = LE_FAULT;
    BIO*            bio       = NULL;

    // Check input parameters
    if ((!ctxPtr) || (!pkeyPtr) || (!pkeyLen))
    {
        LE_ERROR("Invalid parameter: ctxPtr %p, pkeyPtr %p, pkeyLen %"PRIuS,
                 ctxPtr, pkeyPtr, pkeyLen);
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    LE_INFO("Own private key: %p Len:%"PRIuS, pkeyPtr, pkeyLen);

    // Clear any stale errors before processing
    ERR_clear_error();

    bool looksPem = HasPemHeader(pkeyPtr, pkeyLen);
    if (!looksPem)
    {
        // Decode the DER-encoded private key into an EVP_PKEY structure.
        // d2i_PrivateKey supports RSA, EC and DSA keys in PKCS#8 / traditional DER format.
        p    = pkeyPtr;
        pkey = d2i_PrivateKey(EVP_PKEY_NONE, NULL, &p, (long)pkeyLen);
        if (!pkey)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to decode private key (reason: %lu)",
                    (unsigned long)ERR_GET_REASON(code));
            status = LE_FORMAT_ERROR;
            goto end;
        }
    }
    else
    {
        // PEM: parse private key from memory BIO
        bio = BIO_new_mem_buf((void*)pkeyPtr, (int)pkeyLen);
        if (!bio)
        {
            contextPtr->openssl_errcode = ERR_R_MALLOC_FAILURE;
            LE_ERROR("BIO_new_mem_buf failed");
            status = LE_FAULT;
            goto end;
        }

        ERR_clear_error();
        pkey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
        if (!pkey)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to decode PEM private key (reason: %lu)",
                     (unsigned long)ERR_GET_REASON(code));
            status = LE_FORMAT_ERROR;
            goto end;
        }
    }

    // Load the private key into the SSL context so it is used during the TLS handshake.
    // SSL_CTX_use_PrivateKey() does NOT take ownership of pkey, so free it afterwards.
    if (SSL_CTX_use_PrivateKey(contextPtr->sslCtxPtr, pkey) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_CTX_use_PrivateKey failed (reason: %lu)",
                 (unsigned long)ERR_GET_REASON(code));
        goto end;
    }

    // Verify that the private key is consistent with the certificate already loaded
    // via secSocket_AddOwnCertificate(). This catches mismatched key/cert pairs early.
    if (SSL_CTX_check_private_key(contextPtr->sslCtxPtr) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("Private key does not match the certificate (reason: %lu)",
                 (unsigned long)ERR_GET_REASON(code));
        goto end;
    }

    status = LE_OK;

end:
    if (bio)
    {
        BIO_free(bio);
    }

    if (pkey)
    {
        EVP_PKEY_free(pkey);
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Cipher suite table indexed by cipherIdx.
 *
 * Each entry holds:
 *  - tls12Ciphers : colon-separated OpenSSL cipher string for TLS 1.0-1.2
 *                   (passed to SSL_CTX_set_cipher_list)
 *  - tls13Ciphers : colon-separated OpenSSL cipher string for TLS 1.3
 *                   (passed to SSL_CTX_set_ciphersuites, NULL = keep default)
 *
 * Index 0 is reserved as "no restriction" — OpenSSL built-in defaults are kept.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char* tls12Ciphers;   ///< TLS 1.0-1.2 cipher list string
    const char* tls13Ciphers;   ///< TLS 1.3 cipher list string (NULL = default)
}
CipherEntry_t;

static const CipherEntry_t CipherTable[] =
{
    /* 0 - default: no restriction, let OpenSSL choose */
    { NULL,                                                          NULL },
    /* 1 - RSA with AES-128-CBC and SHA-1 (broad compatibility) */
    { "AES128-SHA",                                                  NULL },
    /* 2 - RSA with AES-128-CBC and SHA-256 */
    { "AES128-SHA256",                                               NULL },
    /* 3 - RSA with AES-256-CBC and SHA-256 */
    { "AES256-SHA256",                                               NULL },
    /* 4 - ECDHE-RSA with AES-128-CBC and SHA-256 (PFS) */
    { "ECDHE-RSA-AES128-SHA256",                                     NULL },
    /* 5 - ECDHE-RSA with AES-256-CBC and SHA-384 (PFS) */
    { "ECDHE-RSA-AES256-SHA384",                                     NULL },
    /* 6 - ECDHE-RSA with AES-128-GCM and SHA-256 (PFS + AEAD) */
    { "ECDHE-RSA-AES128-GCM-SHA256",                                 NULL },
    /* 7 - ECDHE-RSA with AES-256-GCM and SHA-384 (PFS + AEAD) */
    { "ECDHE-RSA-AES256-GCM-SHA384",                                 NULL },
    /* 8 - ECDHE-ECDSA with AES-128-GCM and SHA-256 (PFS + AEAD) */
    { "ECDHE-ECDSA-AES128-GCM-SHA256",                               NULL },
    /* 9 - ECDHE-ECDSA with AES-256-GCM and SHA-384 (PFS + AEAD) */
    { "ECDHE-ECDSA-AES256-GCM-SHA384",                               NULL },
    /* 10 - TLS 1.3 only: AES-128-GCM-SHA256 */
    { "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256",  "TLS_AES_128_GCM_SHA256" },
    /* 11 - TLS 1.3 only: AES-256-GCM-SHA384 */
    { "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384",  "TLS_AES_256_GCM_SHA384" },
    /* 12 - TLS 1.3 only: CHACHA20-POLY1305-SHA256 */
    { "ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-CHACHA20-POLY1305",  "TLS_CHACHA20_POLY1305_SHA256" },
};

#define CIPHER_TABLE_SIZE   (sizeof(CipherTable) / sizeof(CipherTable[0]))

//--------------------------------------------------------------------------------------------------
/**
 * Set cipher suites to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetCipherSuites
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           cipherIdx         ///< [IN] Cipher suites index
)
{
    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return;
    }

    const CipherEntry_t* entryPtr  = NULL;
    char                  tls12Buf[512] = {0};
    char                  tls13Buf[256] = {0};

    if (cipherIdx >= CIPHER_TABLE_SIZE)
    {
        LE_ERROR("Cipher suite index %u is out of range (max: %u), ignoring",
                 cipherIdx, (unsigned)(CIPHER_TABLE_SIZE - 1));
        return;
    }

    if (cipherIdx == 0)
    {
        // Index 0: build a combined cipher string from ALL entries in CipherTable[]
        // (indices 1 through CIPHER_TABLE_SIZE-1), so every listed suite is accepted.
        LE_INFO("Cipher suite index 0: enabling all cipher suites in CipherTable");

        for (uint8_t i = 1; i < CIPHER_TABLE_SIZE; i++)
        {
            // Accumulate TLS 1.0-1.2 ciphers
            if (CipherTable[i].tls12Ciphers)
            {
                if (tls12Buf[0] != '\0')
                {
                    le_utf8_Append(tls12Buf, ":", sizeof(tls12Buf), NULL);
                }
                le_utf8_Append(tls12Buf, CipherTable[i].tls12Ciphers, sizeof(tls12Buf), NULL);
            }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
            // Accumulate TLS 1.3 ciphers
            if (CipherTable[i].tls13Ciphers)
            {
                if (tls13Buf[0] != '\0')
                {
                    le_utf8_Append(tls13Buf, ":", sizeof(tls13Buf), NULL);
                }
                le_utf8_Append(tls13Buf, CipherTable[i].tls13Ciphers, sizeof(tls13Buf), NULL);
            }
#endif
        }
    }
    else
    {
        // Specific index: use only the single entry from the table
        entryPtr = &CipherTable[cipherIdx];

        if (entryPtr->tls12Ciphers)
        {
            le_utf8_Copy(tls12Buf, entryPtr->tls12Ciphers, sizeof(tls12Buf), NULL);
        }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
        if (entryPtr->tls13Ciphers)
        {
            le_utf8_Copy(tls13Buf, entryPtr->tls13Ciphers, sizeof(tls13Buf), NULL);
        }
#endif
    }

    LE_INFO("Setting cipher suite index %u: TLS1.2='%s' TLS1.3='%s'",
            cipherIdx,
            tls12Buf[0] ? tls12Buf : "(default)",
            tls13Buf[0] ? tls13Buf : "(default)");

    // Apply TLS 1.0-1.2 cipher list
    if (tls12Buf[0])
    {
        if (SSL_CTX_set_cipher_list(contextPtr->sslCtxPtr, tls12Buf) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("SSL_CTX_set_cipher_list failed for '%s' (reason: %lu)",
                     tls12Buf, (unsigned long)ERR_GET_REASON(code));
        }
    }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    // Apply TLS 1.3 cipher suites (API available from OpenSSL 1.1.1 onwards)
    if (tls13Buf[0])
    {
        if (SSL_CTX_set_ciphersuites(contextPtr->sslCtxPtr, tls13Buf) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("SSL_CTX_set_ciphersuites failed for '%s' (reason: %lu)",
                     tls13Buf, (unsigned long)ERR_GET_REASON(code));
        }
    }
#endif
}

//--------------------------------------------------------------------------------------------------
/**
 * Set authentication type to the secure socket context.
 *
 * Behaviour differs by role:
 *
 * SERVER role:
 *   AUTH_SERVER (1) - One-way TLS: the server presents its own certificate to the client
 *                     but does NOT request a certificate from the client (SSL_VERIFY_NONE).
 *                     This is the standard HTTPS-style setup.
 *
 *   AUTH_MUTUAL (3) - Mutual TLS (mTLS): the server requests AND verifies a certificate
 *                     from the client (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT).
 *                     The handshake is aborted if the client provides no valid certificate.
 *
 * CLIENT role:
 *   AUTH_SERVER (1) - The client verifies the server's certificate (SSL_VERIFY_PEER).
 *                     This is the standard behaviour for a TLS client.
 *
 *   AUTH_MUTUAL (3) - Same server-certificate verification as AUTH_SERVER. The client
 *                     certificate is presented when the server requests it; this is
 *                     configured by loading the client cert/key via
 *                     secSocket_AddOwnCertificate() / secSocket_AddOwnPrivateKey().
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAuthType
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           auth              ///< [IN] Authentication type (AuthType_t)
)
{
    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return;
    }

    if (contextPtr->role == PROTO_ROLE_SERVER)
    {
        switch ((AuthType_t)auth)
        {
            case AUTH_SERVER:
                // One-way TLS: present our certificate to the client but do not
                // request one from the client. SSL_VERIFY_NONE on the server side
                // suppresses the CertificateRequest message entirely.
                LE_INFO("Server: setting one-way authentication (AUTH_SERVER)");
                SSL_CTX_set_verify(contextPtr->sslCtxPtr, SSL_VERIFY_NONE, NULL);
                break;

            case AUTH_MUTUAL:
                // Mutual TLS: request and verify a certificate from the client.
                //   SSL_VERIFY_PEER                 - send CertificateRequest to the client
                //   SSL_VERIFY_FAIL_IF_NO_PEER_CERT - abort handshake if client sends nothing
                LE_INFO("Server: setting mutual authentication (AUTH_MUTUAL)");
                SSL_CTX_set_verify(contextPtr->sslCtxPtr,
                                   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                                   NULL);
                break;

            default:
                LE_ERROR("Server: unknown authentication type %u, ignoring", auth);
                break;
        }
    }
    else
    {
        // CLIENT role
        switch ((AuthType_t)auth)
        {
            case AUTH_SERVER:
                // Standard client behaviour: verify the server's certificate.
                // SSL_VERIFY_PEER causes the handshake to fail if the server
                // certificate cannot be verified against the loaded CA store.
                LE_INFO("Client: setting server-certificate verification (AUTH_SERVER)");
                SSL_CTX_set_verify(contextPtr->sslCtxPtr, SSL_VERIFY_PEER, NULL);
                break;

            case AUTH_MUTUAL:
                // Mutual TLS from the client side: still verify the server certificate.
                // The client certificate is presented automatically when the server
                // sends a CertificateRequest, provided it has been loaded via
                // secSocket_AddOwnCertificate() and secSocket_AddOwnPrivateKey().
                LE_INFO("Client: setting mutual authentication (AUTH_MUTUAL)");
                SSL_CTX_set_verify(contextPtr->sslCtxPtr, SSL_VERIFY_PEER, NULL);
                break;

            default:
                LE_ERROR("Client: unknown authentication type %u, ignoring", auth);
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Server-side ALPN selection callback.
 *
 * OpenSSL calls this during the TLS handshake after receiving the client's list of supported
 * protocols. SSL_select_next_proto() walks the server's preference list and picks the first
 * protocol that also appears in the client's list (server-preference order).
 */
//--------------------------------------------------------------------------------------------------
static int AlpnSelectCallback
(
    SSL*           sslPtr,      ///< [IN]  SSL connection object (unused here)
    const uint8_t** out,        ///< [OUT] Selected protocol name
    uint8_t*        outLen,     ///< [OUT] Selected protocol name length
    const uint8_t*  in,         ///< [IN]  Client's protocol list (length-prefixed wire format)
    unsigned int    inLen,      ///< [IN]  Byte length of client's protocol list
    void*           argPtr      ///< [IN]  Pointer to OpensslCtx_t
)
{
    LE_UNUSED(sslPtr);

    OpensslCtx_t* contextPtr = (OpensslCtx_t*)argPtr;

    // SSL_select_next_proto implements the server-preference ALPN matching algorithm
    // defined in RFC 7301. It returns OPENSSL_NPN_NEGOTIATED on success.
    if (SSL_select_next_proto((uint8_t**)out, outLen,
                              contextPtr->alpnProtosBuf, (unsigned int)contextPtr->alpnProtosBufLen,
                              in, inLen) != OPENSSL_NPN_NEGOTIATED)
    {
        LE_WARN("ALPN: no protocol match found");
        return SSL_TLSEXT_ERR_NOACK;
    }

    LE_INFO("ALPN: selected protocol '%.*s'", (int)*outLen, *out);
    return SSL_TLSEXT_ERR_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the ALPN protocol list in the secure socket context.
 *
 * alpnList is a NULL-terminated array of protocol name strings, e.g.:
 *   const char* list[] = { "h2", "http/1.1", NULL };
 *
 * The strings are encoded into the length-prefixed wire format required by OpenSSL:
 *   <1-byte length> <protocol bytes> <1-byte length> <protocol bytes> ...
 *
 * Role behaviour:
 *   CLIENT - SSL_CTX_set_alpn_protos(): the encoded list is sent in the ClientHello
 *            ALPN extension so the server can select a protocol.
 *
 *   SERVER - SSL_CTX_set_alpn_select_cb(): a callback is registered that uses the
 *            encoded list as the server's preference order when choosing from the
 *            protocols advertised by the client.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAlpnProtocolList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char**      alpnList          ///< [IN] NULL-terminated ALPN protocol name list
)
{
    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return;
    }

    if (!alpnList || !alpnList[0])
    {
        LE_ERROR("ALPN protocol list is NULL or empty");
        return;
    }

    // -------------------------------------------------------------------------
    // Step 1: calculate the total byte length of the length-prefixed wire buffer.
    //
    // Wire format per RFC 7301:
    //   for each protocol name:  1 byte (length) + N bytes (name)
    // -------------------------------------------------------------------------
    size_t bufLen = 0;

    for (int i = 0; alpnList[i] != NULL; i++)
    {
        size_t nameLen = strlen(alpnList[i]);

        if (nameLen == 0 || nameLen > ALPN_PROTO_NAME_MAX_LEN)
        {
            LE_ERROR("ALPN protocol name '%s' has invalid length %"PRIuS", skipping",
                     alpnList[i], nameLen);
            return;
        }

        bufLen += 1 + nameLen;  // 1-byte length prefix + name bytes
    }

    // -------------------------------------------------------------------------
    // Step 2: allocate and fill the wire-format buffer.
    // Free any previously stored buffer first.
    // -------------------------------------------------------------------------
    if (contextPtr->alpnProtosBuf)
    {
        free(contextPtr->alpnProtosBuf);
        contextPtr->alpnProtosBuf    = NULL;
        contextPtr->alpnProtosBufLen = 0;
    }

    contextPtr->alpnProtosBuf = (uint8_t*)malloc(bufLen);
    if (!contextPtr->alpnProtosBuf)
    {
        LE_ERROR("ALPN: failed to allocate %"PRIuS" bytes for protocol buffer", bufLen);
        contextPtr->openssl_errcode = ERR_R_MALLOC_FAILURE;
        return;
    }

    uint8_t* p = contextPtr->alpnProtosBuf;

    for (int i = 0; alpnList[i] != NULL; i++)
    {
        size_t nameLen = strlen(alpnList[i]);
        *p++ = (uint8_t)nameLen;
        memcpy(p, alpnList[i], nameLen);
        p += nameLen;

        LE_INFO("ALPN: adding protocol '%s'", alpnList[i]);
    }

    contextPtr->alpnProtosBufLen = bufLen;

    // -------------------------------------------------------------------------
    // Step 3: register the ALPN list with OpenSSL, using the role-appropriate API.
    // -------------------------------------------------------------------------
    if (contextPtr->role == PROTO_ROLE_CLIENT)
    {
        // Client: advertise the protocol list in the ClientHello ALPN extension.
        // SSL_CTX_set_alpn_protos() returns 0 on success, non-zero on failure
        // (inverted convention compared to most OpenSSL APIs).
        if (SSL_CTX_set_alpn_protos(contextPtr->sslCtxPtr,
                                    contextPtr->alpnProtosBuf,
                                    (unsigned int)contextPtr->alpnProtosBufLen) != 0)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Client: SSL_CTX_set_alpn_protos failed (reason: %lu)",
                     (unsigned long)ERR_GET_REASON(code));
        }
        else
        {
            LE_INFO("Client: ALPN protocol list set successfully");
        }
    }
    else
    {
        // Server: register the selection callback. OpenSSL will invoke it during the
        // handshake to pick one protocol from the client's advertised list, using
        // the server's preference order stored in contextPtr->alpnProtosBuf.
        SSL_CTX_set_alpn_select_cb(contextPtr->sslCtxPtr, AlpnSelectCallback, contextPtr);
        LE_INFO("Server: ALPN selection callback registered");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Map an SSL_get_error() return code to a le_result_t.
 *
 * @return  Mapped le_result_t value
 */
//--------------------------------------------------------------------------------------------------
static le_result_t MapSslError
(
    OpensslCtx_t*  contextPtr,  ///< [IN] OpenSSL context (for storing openssl_errcode)
    SSL*           sslPtr,      ///< [IN] SSL object that produced the error
    int            sslErr       ///< [IN] Return value of SSL_get_error()
)
{
    unsigned long code = ERR_peek_last_error();
    contextPtr->openssl_errcode = (int)ERR_GET_REASON(code);

    switch (sslErr)
    {
        case SSL_ERROR_ZERO_RETURN:
            // Peer closed the TLS session cleanly
            return LE_CLOSED;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return LE_WOULD_BLOCK;

        case SSL_ERROR_SYSCALL:
            if (errno == ETIMEDOUT)
            {
                return LE_TIMEOUT;
            }
            return LE_FAULT;

        case SSL_ERROR_SSL:
            return LE_FAULT;

        default:
            return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Performs TLS Handshake on an already-connected TCP file descriptor.
 *
 * This function is used to upgrade an existing plain TCP connection to TLS without
 * creating a new network connection (see le_socket_SecureConnection()).
 *
 * Role behaviour:
 *   CLIENT - Creates an SSL object, optionally sets the SNI hostname from hostPtr,
 *            wraps the fd in a BIO, then calls SSL_connect() to initiate the handshake.
 *
 *   SERVER - Creates an SSL object, wraps the fd in a BIO, then calls SSL_accept()
 *            to wait for and complete the client-initiated handshake.
 *            hostPtr is unused for the server role.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       Timeout during execution
 *  - LE_FAULT         Internal error
 *  - LE_NO_MEMORY     Memory allocation issue
 *  - LE_CLOSED        In case of end of file error
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_PerformHandshake
(
    secSocket_Ctx_t*    ctxPtr,    ///< [INOUT] Secure socket context pointer
    char*               hostPtr,   ///< [IN] Host name for SNI (client only, may be NULL)
    int                 fd         ///< [IN] Already-connected TCP file descriptor
)
{
    SSL*        sslPtr  = NULL;
    BIO*        bioPtr  = NULL;
    BIO*        sslBio  = NULL;
    le_result_t status  = LE_FAULT;
    int         ret;
    int         sslErr;

    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return LE_BAD_PARAMETER;
    }

    if (fd < 0)
    {
        LE_ERROR("Invalid file descriptor %d", fd);
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    ERR_clear_error();

    // -------------------------------------------------------------------------
    // Step 1: create a new SSL object from the pre-configured SSL_CTX.
    // The SSL_CTX already holds the certificate, private key, CA store,
    // verify mode, cipher suites and ALPN settings configured earlier.
    // -------------------------------------------------------------------------
    sslPtr = SSL_new(contextPtr->sslCtxPtr);
    if (!sslPtr)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_new failed (reason: %lu)", (unsigned long)ERR_GET_REASON(code));
        return LE_NO_MEMORY;
    }

    // -------------------------------------------------------------------------
    // Step 2: wrap the existing TCP fd in a socket BIO and hand it to the SSL
    // object. BIO_NOCLOSE means OpenSSL will NOT close the fd when the BIO is
    // freed — the caller owns the fd lifecycle.
    // -------------------------------------------------------------------------
    bioPtr = BIO_new_socket(fd, BIO_NOCLOSE);
    if (!bioPtr)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("BIO_new_socket failed (reason: %lu)", (unsigned long)ERR_GET_REASON(code));
        status = LE_NO_MEMORY;
        goto err;
    }

    SSL_set_bio(sslPtr, bioPtr, bioPtr);  // SSL takes ownership of bioPtr
    bioPtr = NULL;                        // Do not double-free

    // -------------------------------------------------------------------------
    // Step 3: role-specific pre-handshake configuration.
    // -------------------------------------------------------------------------
    if (contextPtr->role == PROTO_ROLE_CLIENT)
    {
        // Set SNI hostname so the server can select the correct virtual host
        // certificate. Only set when a non-empty hostname is provided.
        if (hostPtr && hostPtr[0] != '\0')
        {
            if (SSL_set_tlsext_host_name(sslPtr, hostPtr) != 1)
            {
                LE_WARN("SSL_set_tlsext_host_name failed for '%s', continuing without SNI",
                        hostPtr);
            }
            else
            {
                LE_INFO("Client: SNI hostname set to '%s'", hostPtr);
            }
        }

        // -------------------------------------------------------------------------
        // Step 4a (CLIENT): initiate the TLS handshake.
        // SSL_connect() sends ClientHello and drives the full handshake to completion.
        // -------------------------------------------------------------------------
        LE_INFO("Client: starting TLS handshake on fd %d", fd);
        ret = SSL_connect(sslPtr);
    }
    else
    {
        // -------------------------------------------------------------------------
        // Step 4b (SERVER): wait for and complete the client-initiated handshake.
        // SSL_accept() processes the ClientHello and drives the full handshake.
        // -------------------------------------------------------------------------
        LE_INFO("Server: waiting for TLS handshake on fd %d", fd);
        ret = SSL_accept(sslPtr);
    }

    if (ret != 1)
    {
        sslErr = SSL_get_error(sslPtr, ret);
        LE_ERROR("%s: TLS handshake failed, SSL_get_error=%d",
                 (contextPtr->role == PROTO_ROLE_CLIENT) ? "Client" : "Server", sslErr);
        status = MapSslError(contextPtr, sslPtr, sslErr);
        goto err;
    }

    LE_INFO("%s: TLS handshake completed successfully on fd %d, cipher='%s'",
            (contextPtr->role == PROTO_ROLE_CLIENT) ? "Client" : "Server",
            fd, SSL_get_cipher(sslPtr));

    // -------------------------------------------------------------------------
    // Step 5: wrap the SSL object in a BIO chain so that secSocket_Read/Write
    // can continue to use contextPtr->bioPtr transparently.
    // -------------------------------------------------------------------------
    sslBio = BIO_new(BIO_f_ssl());
    if (!sslBio)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("BIO_new(BIO_f_ssl()) failed (reason: %lu)",
                 (unsigned long)ERR_GET_REASON(code));
        status = LE_NO_MEMORY;
        goto err;
    }

    BIO_set_ssl(sslBio, sslPtr, BIO_CLOSE);  // sslBio takes ownership of sslPtr
    sslPtr = NULL;                            // Do not double-free

    contextPtr->bioPtr = sslBio;
    return LE_OK;

err:
    if (sslPtr)
    {
        SSL_free(sslPtr);   // Also frees the socket BIO if SSL_set_bio was called
    }
    else if (bioPtr)
    {
        BIO_free(bioPtr);
    }

    if (sslBio)
    {
        BIO_free(sslBio);
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initiate a connection with host:port and the given protocol
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       Timeout during execution
 *  - LE_UNAVAILABLE   Unable to reach the server or DNS issue
 *  - LE_FAULT         Internal error
 *  - LE_NO_MEMORY     Memory allocation issue
 *  - LE_CLOSED        In case of end of file error
 *  - LE_COMM_ERROR    Connection failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Connect
(
    secSocket_Ctx_t* ctxPtr,     ///< [INOUT] Secure socket context pointer
    char*            hostPtr,    ///< [IN] Host to connect on
    uint16_t         port,       ///< [IN] Port to connect on
    char*            srcAddrPtr, ///< [IN] Source address pointer (not used)
    SocketType_t     type,       ///< [IN] Socket type (TCP, UDP)
    int*             fdPtr       ///< [OUT] Socket file descriptor
)
{
    SSL* sslPtr = NULL;
    BIO* bioPtr = NULL;
    char hostAndPort[HOST_ADDR_LEN + PORT_STR_LEN + 1];
    le_result_t status = LE_FAULT;
    unsigned long code;

    if ((!ctxPtr) || (!hostPtr) || (!fdPtr))
    {
        LE_ERROR("Invalid argument: ctxPtr %p, hostPtr %p fdPtr %p", ctxPtr, hostPtr, fdPtr);
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    // Start the connection
    snprintf(hostAndPort, sizeof(hostAndPort), "%s:%d", hostPtr, port);
    LE_INFO("Connecting to %d/%s:%d - %s...", type, hostPtr, port, hostAndPort);

    // Clear the current thread's OpenSSL error queue
    ERR_clear_error();

    // Setting up the BIO abstraction layer
    bioPtr = BIO_new_ssl_connect(contextPtr->sslCtxPtr);
    if (!bioPtr)
    {
        LE_ERROR("Unable to allocate and connect BIO");
        goto err;
    }

    BIO_get_ssl(bioPtr, &sslPtr);
    if (!sslPtr)
    {
        LE_ERROR("Unable to locate SSL pointer");
        goto err;
    }

    // Set the SSL_MODE_AUTO_RETRY flag: it will cause read/write operations to only return after
    // the handshake and successful completion
    SSL_set_mode(sslPtr, SSL_MODE_AUTO_RETRY);

    BIO_set_conn_hostname(bioPtr, hostAndPort);

    // Attempt to connect the supplied BIO and perform the handshake.
    // This function returns 1 if the connection was successfully established and 0 or -1 if the
    // connection failed.
    if (BIO_do_connect(bioPtr) != 1)
    {
        LE_ERROR("Unable to connect BIO to %s", hostAndPort);
        goto err;
    }

    // Get the FD linked to the BIO
    BIO_get_fd(bioPtr, fdPtr);
    BIO_socket_nbio(*fdPtr, 1);

    contextPtr->bioPtr = bioPtr;
    return LE_OK;

err:
    code = ERR_peek_last_error();

    if ((ERR_GET_LIB(code) == ERR_LIB_BIO) || (ERR_GET_LIB(code) == ERR_LIB_SSL))
    {
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        switch (ERR_GET_REASON(code))
        {
            case ERR_R_MALLOC_FAILURE:
                status = LE_NO_MEMORY;
                break;

            case BIO_R_NULL_PARAMETER:
                status = LE_BAD_PARAMETER;
                break;

#if defined(BIO_R_BAD_HOSTNAME_LOOKUP)
            case BIO_R_BAD_HOSTNAME_LOOKUP:
                status = LE_UNAVAILABLE;
                break;
#endif

            case BIO_R_CONNECT_ERROR:
                status = LE_COMM_ERROR;
                break;

#if defined(BIO_R_EOF_ON_MEMORY_BIO)
            case BIO_R_EOF_ON_MEMORY_BIO:
                status = LE_CLOSED;
                break;
#endif

            default:
                status = LE_FAULT;
                break;
        }
     }

    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gracefully close the socket connection while keeping the SSL configuration.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Disconnect
(
    secSocket_Ctx_t* ctxPtr      ///< [INOUT] Secure socket context pointer
)
{
    if (!ctxPtr)
    {
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    BIO_ssl_shutdown(contextPtr->bioPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Free the last connection resources including the certificate and SSL configuration.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Delete
(
    secSocket_Ctx_t* ctxPtr      ///< [INOUT] Secure socket context pointer
)
{
    if (!ctxPtr)
    {
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    BIO_free_all(contextPtr->bioPtr);
    contextPtr->bioPtr = NULL;
    SSL_CTX_free(contextPtr->sslCtxPtr);
    contextPtr->sslCtxPtr = NULL;

#if OPENSSL_API_COMPAT < 0x10100000L
#if OPENSSL_API_COMPAT > 0x10002000L
    // In versions of OpenSSL prior to 1.1.0 SSL_COMP_free_compression_methods() freed the
    // internal table of compression methods that were built internally, and possibly augmented
    // by adding SSL_COMP_add_compression_method().
    // However this is now unnecessary from version 1.1.0. No explicit initialisation or
    // de-initialisation is necessary.
    SSL_COMP_free_compression_methods();
#endif
#endif
    EVP_cleanup();
    ERR_free_strings();

    CRYPTO_cleanup_all_ex_data();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_state(0);
#endif
    contextPtr->isInit = false;
    le_mem_Release(contextPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Write an amount of data to the secure socket.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Write
(
    secSocket_Ctx_t* ctxPtr,      ///< [INOUT] Secure socket context pointer
    const char*      dataPtr,     ///< [IN] Data pointer
    size_t           dataLen      ///< [IN] Data length
)
{
    if ((!ctxPtr) || (!dataPtr))
    {
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    int r = BIO_write(contextPtr->bioPtr, dataPtr, dataLen);
    if (0 >= r)
    {
        LE_ERROR("Write failed. Error code: %d", r);
        contextPtr->openssl_errcode = r;
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read data from the socket file descriptor in a blocking way. If the timeout is zero, then the
 * API returns immediatly.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 *  - LE_TIMEOUT       Timeout during execution
 *  - LE_WOULD_BLOCK   Would have blocked if non-blocking behaviour was not requested
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Read
(
    secSocket_Ctx_t* ctxPtr,       ///< [INOUT] Secure socket context pointer
    char*            dataPtr,      ///< [INOUT] Data pointer
    size_t*          dataLenPtr,   ///< [IN] Data length pointer
    uint32_t         timeout       ///< [IN] Read timeout in milliseconds.
)
{
    fd_set set;
    int rv, fd;

    if ((!ctxPtr) || (!dataPtr) || (!dataLenPtr))
    {
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return LE_BAD_PARAMETER;
    }

    if (!contextPtr->bioPtr)
    {
        LE_ERROR("Socket not connected");
        return LE_FAULT;
    }

    if (!BIO_pending(contextPtr->bioPtr))
    {
        struct timeval time = {.tv_sec = timeout / 1000, .tv_usec = (timeout % 1000) * 1000};

        // Get file descriptor of the current BIO
        BIO_get_fd(contextPtr->bioPtr, &fd);

        do
        {
           FD_ZERO(&set);
           FD_SET(fd, &set);
           rv = select(fd + 1, &set, NULL, NULL, &time);
        }
        while (rv == -1 && errno == EINTR);

        if (rv > 0)
        {
            if (!FD_ISSET(fd, &set))
            {
                LE_ERROR("Nothing to read");
                return LE_FAULT;
            }
        }
        else if (rv == 0)
        {
            return LE_TIMEOUT;
        }
        else
        {
            return LE_FAULT;
        }
    }

    // At this point, there is something available for reading from BIO
    rv = BIO_read(contextPtr->bioPtr, dataPtr, *dataLenPtr);
    if (rv <= 0)
    {
        if (BIO_should_retry(contextPtr->bioPtr))
        {
             return LE_WOULD_BLOCK;
        }
        else
        {
            LE_ERROR("Read failed. Error code: %d", rv);
            contextPtr->openssl_errcode = rv;
            return LE_FAULT;
        }
    }

    *dataLenPtr = rv;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if data is available to be read
 *
 * @return
 *  - True if data is available to be read, false otherwise
 */
//--------------------------------------------------------------------------------------------------
bool secSocket_IsDataAvailable
(
    secSocket_Ctx_t* ctxPtr       ///< [INOUT] Secure socket context pointer
)
{
    if (!ctxPtr)
    {
        return false;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return false;
    }

    return (BIO_pending(contextPtr->bioPtr) ? true: false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the tls version to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetTlsVersion
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           tlsVersion        ///< [IN] Supported TLS version (Minor version number)
)
{
    if (!ctxPtr)
    {
        return;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        return;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get tls error code
 *
 * @note Get tls error code
 *
 * @return
 *  - INT tls error code
 */
//--------------------------------------------------------------------------------------------------
int secSocket_GetTlsErrorCode
(
    secSocket_Ctx_t *ctxPtr     ///< [IN] Secure socket context pointer
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);

    if (!contextPtr)
    {
        return 0;
    }
    return contextPtr->openssl_errcode;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set tls error code
 *
 * @note Set tls error code
 *
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetTlsErrorCode
(
    secSocket_Ctx_t *ctxPtr,         ///< [IN] Secure socket context pointer
    int              err_code        ///< [IN] INT error code
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);

    if (!contextPtr)
    {
        return;
    }
    contextPtr->openssl_errcode = err_code;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy the TLS configuration from a source secure socket context into a destination context.
 *
 * This is used by le_socket_Accept() to give each accepted client connection its own
 * OpensslCtx_t that shares the same SSL_CTX as the server listener. Sharing the SSL_CTX
 * is safe because SSL_CTX is reference-counted: SSL_CTX_up_ref() increments the count so
 * that both the server context and the client context can call SSL_CTX_free() independently
 * without a double-free.
 *
 * The destination context receives:
 *  - A shared reference to the source SSL_CTX (via SSL_CTX_up_ref)
 *  - The same role (always PROTO_ROLE_SERVER on the accept path)
 *  - hasCert / isInit flags set to match the source
 *
 * The BIO pointer is intentionally NOT copied — the caller (secSocket_PerformHandshake)
 * will create a fresh per-connection BIO wrapping the accepted fd.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_CopyConfig
(
    secSocket_Ctx_t*  srcCtxPtr,    ///< [IN]  Source secure socket context (server listener)
    secSocket_Ctx_t*  dstCtxPtr     ///< [INOUT] Destination secure socket context (accepted client)
)
{
    if (!srcCtxPtr || !dstCtxPtr)
    {
        LE_ERROR("Invalid parameter: srcCtxPtr %p, dstCtxPtr %p", srcCtxPtr, dstCtxPtr);
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* srcPtr = GetContext(srcCtxPtr);
    OpensslCtx_t* dstPtr = GetContext(dstCtxPtr);

    if (!srcPtr || !dstPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    if (!srcPtr->sslCtxPtr)
    {
        LE_ERROR("Source SSL_CTX is NULL");
        return LE_FAULT;
    }

    // If the destination already holds an SSL_CTX, release it before replacing it
    if (dstPtr->sslCtxPtr)
    {
        SSL_CTX_free(dstPtr->sslCtxPtr);
        dstPtr->sslCtxPtr = NULL;
    }

    // Atomically increment the SSL_CTX reference count so both the server listener
    // context and this per-connection client context share the same SSL_CTX.
    // Each context will call SSL_CTX_free() independently; the underlying object is
    // only destroyed when the reference count reaches zero.
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    if (SSL_CTX_up_ref(srcPtr->sslCtxPtr) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        dstPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_CTX_up_ref failed (reason: %lu)", (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }
    dstPtr->sslCtxPtr = srcPtr->sslCtxPtr;
#else
    // OpenSSL < 1.1.0 does not have SSL_CTX_up_ref. Fall back to creating a new
    // SSL_CTX with the same method and re-applying the verify mode from the source.
    dstPtr->sslCtxPtr = SSL_CTX_new(TLSv1_server_method());
    if (!dstPtr->sslCtxPtr)
    {
        unsigned long code = ERR_peek_last_error();
        dstPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_CTX_new failed (reason: %lu)", (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }
    // Copy the verify mode so AUTH_SERVER / AUTH_MUTUAL is preserved
    SSL_CTX_set_verify(dstPtr->sslCtxPtr,
                       SSL_CTX_get_verify_mode(srcPtr->sslCtxPtr),
                       NULL);
#endif

    dstPtr->role   = srcPtr->role;
    dstPtr->isInit = srcPtr->isInit;

    LE_INFO("TLS config copied from server listener context to accepted client context");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * One-time init for Secure Socket component
 *
 * This pre-initializes the secSocket memory pools.
 *
 */
//--------------------------------------------------------------------------------------------------
void secSocket_InitializeOnce
(
    void
)
{
    // Initialize the socket context pool
    SocketCtxPoolRef = le_mem_InitStaticPool(SocketCtxPool,
                                             MAX_SOCKET_NB,
                                             sizeof(OpensslCtx_t));
}
