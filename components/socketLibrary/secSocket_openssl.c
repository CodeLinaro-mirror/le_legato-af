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
#include <strings.h>
#include "secSocket.h"
#include "le_socketLib.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

// The provider API only exists from OpenSSL 3.0 onwards. Everything guarded by this symbol degrades
// to LE_UNSUPPORTED on an older library, so a "default" provider configuration still works there.
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

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
 * Maximum number of protocol names that can be stored in the ALPN list
 */
//--------------------------------------------------------------------------------------------------
#define ALPN_PROTO_LIST_MAX_COUNT   8

//--------------------------------------------------------------------------------------------------
/**
 * Maximum byte length of the encoded (length-prefixed) ALPN protocol list buffer
 */
//--------------------------------------------------------------------------------------------------
#define ALPN_PROTOS_BUF_MAX_LEN     (ALPN_PROTO_LIST_MAX_COUNT * (1 + ALPN_PROTO_NAME_MAX_LEN))

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of a credential (certificate or private key) file path, including the
 * terminating null byte
 */
//--------------------------------------------------------------------------------------------------
#define CRED_FILE_PATH_MAX_LEN      256

//--------------------------------------------------------------------------------------------------
/**
 * Number of bytes read from the head of a credential file to tell PEM from DER.
 *
 * Matches the window inspected by HasPemHeader().
 */
//--------------------------------------------------------------------------------------------------
#define CRED_FILE_SNIFF_LEN         2048

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length, including the terminating null byte, of a TLS configuration string such as
 * "tls_cipher_suites" or "signature_algorithms".
 *
 * Sized to hold a full colon-separated list rather than a single name.
 */
//--------------------------------------------------------------------------------------------------
#define TLS_CFG_STR_MAX_LEN         512

//--------------------------------------------------------------------------------------------------
/**
 * Upper bound, in milliseconds, on how long secSocket_Write() waits for a non-blocking descriptor
 * to accept the data before giving up with LE_TIMEOUT.
 *
 * Unlike secSocket_Read(), the write path takes no timeout argument, so it cannot honour the
 * per-socket value set through le_socket_SetTimeout(). The library default is used instead, which
 * keeps a stalled peer from blocking the caller forever.
 */
//--------------------------------------------------------------------------------------------------
#define SEC_SOCKET_WRITE_TIMEOUT_MS COMM_TIMEOUT_DEFAULT_MS

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
    uint8_t                  alpnProtosBuf[ALPN_PROTOS_BUF_MAX_LEN]; ///< Encoded ALPN protocol list
    size_t                   alpnProtosBufLen;  ///< Byte length of valid data in alpnProtosBuf
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    OSSL_PROVIDER*           providerPtr;       ///< External provider handle, NULL if none loaded
#endif
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
/**
 * Search for a PEM header in the first up-to-2KB bytes of a buffer, e.g.
 * BEGIN CERTIFICATE / BEGIN PRIVATE KEY / BEGIN RSA PRIVATE KEY.
 *
 * @return
 *  - true if a PEM header was found, false otherwise (implies DER format)
 */
//--------------------------------------------------------------------------------------------------
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
 * Validate a credential file path before handing it to OpenSSL.
 *
 * Checking readability up front keeps "file missing" distinguishable from "file malformed":
 * the OpenSSL file APIs report both as a generic failure.
 *
 * @return
 *  - LE_OK            The path is well-formed and the file is readable
 *  - LE_BAD_PARAMETER Null, empty or over-long path
 *  - LE_NOT_FOUND     File does not exist or is not readable
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ValidateCredFilePath
(
    const char*  pathPtr    ///< [IN] Credential file path
)
{
    if ((!pathPtr) || (pathPtr[0] == '\0'))
    {
        LE_ERROR("Null or empty credential file path");
        return LE_BAD_PARAMETER;
    }

    if (strnlen(pathPtr, CRED_FILE_PATH_MAX_LEN) >= CRED_FILE_PATH_MAX_LEN)
    {
        LE_ERROR("Credential file path exceeds maximum length of %d", CRED_FILE_PATH_MAX_LEN);
        return LE_BAD_PARAMETER;
    }

    if (access(pathPtr, R_OK) != 0)
    {
        LE_ERROR("Credential file '%s' does not exist or is not readable (errno %d)",
                 pathPtr, errno);
        return LE_NOT_FOUND;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Determine whether a credential file holds PEM or DER content.
 *
 * Reads the head of the file and reuses HasPemHeader(), so that the file-based entry points
 * classify content exactly the same way as the buffer-based ones.
 *
 * @return
 *  - LE_OK        Detection succeeded; *isPemPtr is set
 *  - LE_NOT_FOUND Unable to open the file
 *  - LE_FAULT     Unable to read the file
 */
//--------------------------------------------------------------------------------------------------
static le_result_t IsCredFilePem
(
    const char*  pathPtr,      ///< [IN] Credential file path
    bool*        isPemPtr      ///< [OUT] True if the file looks like PEM, false if DER
)
{
    uint8_t buf[CRED_FILE_SNIFF_LEN];
    int     fd;
    ssize_t rd;

    fd = open(pathPtr, O_RDONLY);
    if (fd < 0)
    {
        LE_ERROR("Unable to open credential file '%s' (errno %d)", pathPtr, errno);
        return LE_NOT_FOUND;
    }

    do
    {
        rd = read(fd, buf, sizeof(buf));
    }
    while ((rd < 0) && (errno == EINTR));

    close(fd);

    if (rd < 0)
    {
        LE_ERROR("Unable to read credential file '%s' (errno %d)", pathPtr, errno);
        return LE_FAULT;
    }

    *isPemPtr = HasPemHeader(buf, (size_t)rd);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check that a certificate is within its validity period.
 *
 * @return
 *  - LE_OK            The certificate is currently valid
 *  - LE_FORMAT_ERROR  The certificate is expired or not yet valid
 */
//--------------------------------------------------------------------------------------------------
static le_result_t CheckCertValidity
(
    X509*          certPtr,       ///< [IN] Certificate to check
    OpensslCtx_t*  contextPtr     ///< [INOUT] OpenSSL socket context pointer
)
{
    le_clk_Time_t currentTime = le_clk_GetAbsoluteTime();

    if ((X509_cmp_time(X509_get_notBefore(certPtr), &currentTime.sec) >= 0) ||
        (X509_cmp_time(X509_get_notAfter(certPtr),  &currentTime.sec) <= 0))
    {
        contextPtr->openssl_errcode = X509_V_ERR_CERT_HAS_EXPIRED;
        LE_ERROR("Certificate is expired or not yet valid, please provide a valid certificate");
        return LE_FORMAT_ERROR;
    }

    return LE_OK;
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
    contextPtr = le_mem_TryAlloc(SocketCtxPoolRef);
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
    contextPtr->alpnProtosBufLen = 0;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    contextPtr->providerPtr     = NULL;
#endif
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

    ERR_clear_error();

    // Detect whether the buffer holds a PEM ("-----BEGIN...") or a raw DER certificate
    // and decode it into an X509 structure accordingly.
    if (HasPemHeader(certificatePtr, certificateLen))
    {
        BIO* bio = BIO_new_mem_buf((void*)certificatePtr, (int)certificateLen);
        if (!bio)
        {
            LE_ERROR("Unable to allocate BIO pointer");
            contextPtr->openssl_errcode = ERR_R_MALLOC_FAILURE;
            return LE_FAULT;
        }

        cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
        BIO_free(bio);

        if (!cert)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to decode PEM certificate (reason: %lu)",
                     (unsigned long)ERR_GET_REASON(code));
            return LE_FORMAT_ERROR;
        }
    }
    else
    {
        const uint8_t* p = certificatePtr;

        // Read the DER formatted certificate from memory into an X509 structure
        cert = d2i_X509(NULL, &p, (long)certificateLen);
        if (!cert)
        {
            contextPtr->openssl_errcode = ERR_R_X509_LIB;
            LE_ERROR("Unable to read certificate");
            return LE_FORMAT_ERROR;
        }
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

    return status;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add root CA certificates to the secure socket context from a file.
 *
 * Both PEM and DER files are supported, and either may hold several concatenated certificates:
 *
 *   PEM - SSL_CTX_load_verify_locations() loads every certificate in the file.
 *   DER - SSL_CTX_load_verify_locations() does not understand DER, so the certificates are
 *         decoded one by one from a BIO and added to the verification store individually.
 *
 * The certificate content is read by OpenSSL itself, so it never transits through the caller's
 * memory.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Invalid or expired certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] CA certificate file path (PEM or DER)
)
{
    le_result_t status;
    bool        isPem = false;

    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    status = ValidateCredFilePath(pathPtr);
    if (status != LE_OK)
    {
        return status;
    }

    status = IsCredFilePem(pathPtr, &isPem);
    if (status != LE_OK)
    {
        return status;
    }

    LE_INFO("Adding CA certificate(s) from %s file '%s'", isPem ? "PEM" : "DER", pathPtr);

    ERR_clear_error();

    X509_STORE* store = SSL_CTX_get_cert_store(contextPtr->sslCtxPtr);
    if (!store)
    {
        contextPtr->openssl_errcode = X509_V_ERR_UNABLE_TO_GET_CRL;
        LE_ERROR("Unable to get a pointer to the X509 certificate store");
        return LE_FAULT;
    }

    if (isPem)
    {
        if (SSL_CTX_load_verify_locations(contextPtr->sslCtxPtr, pathPtr, NULL) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to load PEM CA certificate(s) from '%s' (reason: %lu)",
                     pathPtr, (unsigned long)ERR_GET_REASON(code));
            return LE_FORMAT_ERROR;
        }

        // OpenSSL does not check validity periods at load time, so walk the verification store
        // and reject expired certificates, matching the behaviour of secSocket_AddCertificate().
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        STACK_OF(X509_OBJECT)* objsPtr = X509_STORE_get0_objects(store);
        int                    objNb   = sk_X509_OBJECT_num(objsPtr);

        for (int i = 0; i < objNb; i++)
        {
            // Returns NULL for entries that are not certificates, e.g. CRLs
            X509* certPtr = X509_OBJECT_get0_X509(sk_X509_OBJECT_value(objsPtr, i));

            if ((certPtr) && (CheckCertValidity(certPtr, contextPtr) != LE_OK))
            {
                return LE_FORMAT_ERROR;
            }
        }
#else
        LE_WARN("OpenSSL < 1.1.0: skipping CA certificate validity pre-check");
#endif
        return LE_OK;
    }

    // DER: the file may hold one or more concatenated DER-encoded certificates. Decode them
    // one by one until the file is exhausted.
    BIO* bioPtr = BIO_new_file(pathPtr, "rb");
    if (!bioPtr)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("Unable to open certificate file '%s' (reason: %lu)",
                 pathPtr, (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }

    int certNb = 0;

    while (1)
    {
        ERR_clear_error();

        X509* certPtr = d2i_X509_bio(bioPtr, NULL);
        if (!certPtr)
        {
            // Normal end of file once at least one certificate has been read
            if (certNb > 0)
            {
                ERR_clear_error();
                break;
            }

            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to decode DER certificate from '%s' (reason: %lu)",
                     pathPtr, (unsigned long)ERR_GET_REASON(code));
            BIO_free(bioPtr);
            return LE_FORMAT_ERROR;
        }

        status = CheckCertValidity(certPtr, contextPtr);
        if (status != LE_OK)
        {
            X509_free(certPtr);
            BIO_free(bioPtr);
            return status;
        }

        // X509_STORE_add_cert() takes its own reference, so release ours afterwards
        if (!X509_STORE_add_cert(store, certPtr))
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to add certificate to pool (reason: %lu)",
                     (unsigned long)ERR_GET_REASON(code));
            X509_free(certPtr);
            BIO_free(bioPtr);
            return LE_FAULT;
        }

        X509_free(certPtr);
        certNb++;
    }

    BIO_free(bioPtr);

    LE_INFO("Added %d DER CA certificate(s) from '%s'", certNb, pathPtr);

    return LE_OK;
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
 * Add the module's own certificate chain to the secure socket context from a file.
 *
 * Both PEM and DER files are supported, and either may hold a full chain - the leaf certificate
 * first, followed by the intermediate CA certificates:
 *
 *   PEM - SSL_CTX_use_certificate_chain_file() loads the leaf plus the intermediates in one call.
 *   DER - SSL_CTX_use_certificate_file() would only ever read the first certificate, so the
 *         chain is decoded one certificate at a time, mirroring secSocket_AddOwnCertificate():
 *         the first goes in via SSL_CTX_use_certificate() and the rest via
 *         SSL_CTX_add_extra_chain_cert().
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Invalid or expired certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own certificate chain file path (PEM or DER)
)
{
    le_result_t status;
    bool        isPem = false;

    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    status = ValidateCredFilePath(pathPtr);
    if (status != LE_OK)
    {
        return status;
    }

    status = IsCredFilePem(pathPtr, &isPem);
    if (status != LE_OK)
    {
        return status;
    }

    LE_INFO("Adding own certificate chain from %s file '%s'", isPem ? "PEM" : "DER", pathPtr);

    // Clear any existing extra chain certificates to avoid accumulation across calls
    SSL_CTX_clear_extra_chain_certs(contextPtr->sslCtxPtr);

    ERR_clear_error();

    if (isPem)
    {
        if (SSL_CTX_use_certificate_chain_file(contextPtr->sslCtxPtr, pathPtr) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to load PEM own certificate chain from '%s' (reason: %lu)",
                     pathPtr, (unsigned long)ERR_GET_REASON(code));
            return LE_FORMAT_ERROR;
        }
    }
    else
    {
        BIO* bioPtr = BIO_new_file(pathPtr, "rb");
        if (!bioPtr)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to open certificate file '%s' (reason: %lu)",
                     pathPtr, (unsigned long)ERR_GET_REASON(code));
            return LE_FAULT;
        }

        int certNb = 0;

        while (1)
        {
            ERR_clear_error();

            X509* certPtr = d2i_X509_bio(bioPtr, NULL);
            if (!certPtr)
            {
                // Normal end of file once the leaf has been read
                if (certNb > 0)
                {
                    ERR_clear_error();
                    break;
                }

                unsigned long code = ERR_peek_last_error();
                contextPtr->openssl_errcode = ERR_GET_REASON(code);
                LE_ERROR("Unable to decode DER certificate from '%s' (reason: %lu)",
                         pathPtr, (unsigned long)ERR_GET_REASON(code));
                BIO_free(bioPtr);
                return LE_FORMAT_ERROR;
            }

            status = CheckCertValidity(certPtr, contextPtr);
            if (status != LE_OK)
            {
                X509_free(certPtr);
                BIO_free(bioPtr);
                return status;
            }

            if (certNb == 0)
            {
                // The first certificate is our own (leaf) certificate. SSL_CTX_use_certificate()
                // does NOT take ownership, so free it afterwards.
                if (SSL_CTX_use_certificate(contextPtr->sslCtxPtr, certPtr) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_use_certificate failed (reason: %lu)",
                             (unsigned long)ERR_GET_REASON(code));
                    X509_free(certPtr);
                    BIO_free(bioPtr);
                    return LE_FORMAT_ERROR;
                }

                X509_free(certPtr);
            }
            else
            {
                // Subsequent certificates complete the chain sent to the peer.
                // SSL_CTX_add_extra_chain_cert() TAKES ownership on success, so do not free it.
                if (SSL_CTX_add_extra_chain_cert(contextPtr->sslCtxPtr, certPtr) != 1)
                {
                    unsigned long code = ERR_peek_last_error();
                    contextPtr->openssl_errcode = ERR_GET_REASON(code);
                    LE_ERROR("SSL_CTX_add_extra_chain_cert failed (reason: %lu)",
                             (unsigned long)ERR_GET_REASON(code));
                    X509_free(certPtr);
                    BIO_free(bioPtr);
                    return LE_FORMAT_ERROR;
                }
            }

            certNb++;
        }

        BIO_free(bioPtr);

        LE_INFO("Added DER own certificate chain from '%s' (%d certificate(s))", pathPtr, certNb);

        // Validity of every certificate in the chain was checked in the loop above
        return LE_OK;
    }

    // PEM path: check the validity period of the leaf certificate, matching the behaviour of
    // secSocket_AddOwnCertificate(). SSL_CTX_get0_certificate() returns an internal pointer
    // that must not be freed.
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    X509* leafPtr = SSL_CTX_get0_certificate(contextPtr->sslCtxPtr);

    if (!leafPtr)
    {
        LE_ERROR("Unable to retrieve the loaded own certificate for validity check");
        return LE_FAULT;
    }

    status = CheckCertValidity(leafPtr, contextPtr);
    if (status != LE_OK)
    {
        return status;
    }
#else
    LE_WARN("OpenSSL < 1.0.2: skipping own certificate validity pre-check");
#endif

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own private key to the secure socket context from a file.
 *
 * The key is loaded by OpenSSL, so the key material never transits through the caller's memory.
 * Because SSL_CTX_use_PrivateKey_file() goes through OpenSSL's decoder chain, a key held by a
 * crypto provider - a file holding a key reference or wrapped key blob rather than the raw key
 * material - is also supported, provided that provider is loaded and registers a matching
 * decoder.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Unable to decode the private key
 *  - LE_FAULT         Failure, including a key that does not match the loaded certificate
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnPrivateKeyFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own private key file path
)
{
    le_result_t status;
    bool        isPem = false;

    if (!ctxPtr)
    {
        LE_ERROR("Null context pointer");
        return LE_BAD_PARAMETER;
    }

    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    status = ValidateCredFilePath(pathPtr);
    if (status != LE_OK)
    {
        return status;
    }

    status = IsCredFilePem(pathPtr, &isPem);
    if (status != LE_OK)
    {
        return status;
    }

    LE_INFO("Adding own private key from %s file '%s'", isPem ? "PEM" : "DER", pathPtr);

    ERR_clear_error();

    // The detected format is tried first. A provider-held key is normally PEM-wrapped, so the
    // PEM path covers it; the other format is still attempted as a fallback, because a provider
    // decoder may claim a blob whose framing does not match either classic encoding.
    int firstType  = isPem ? SSL_FILETYPE_PEM  : SSL_FILETYPE_ASN1;
    int secondType = isPem ? SSL_FILETYPE_ASN1 : SSL_FILETYPE_PEM;

    if (SSL_CTX_use_PrivateKey_file(contextPtr->sslCtxPtr, pathPtr, firstType) != 1)
    {
        ERR_clear_error();

        if (SSL_CTX_use_PrivateKey_file(contextPtr->sslCtxPtr, pathPtr, secondType) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to load private key from '%s' as PEM or DER (reason: %lu). "
                     "If this is a provider-held key, check that the provider is loaded and "
                     "registers a decoder for this format.",
                     pathPtr, (unsigned long)ERR_GET_REASON(code));
            return LE_FORMAT_ERROR;
        }
    }

    // Verify that the private key is consistent with the certificate already loaded via
    // secSocket_AddOwnCertificate() or secSocket_AddOwnCertificateFile(). This catches
    // mismatched key/cert pairs early.
    //
    // Note: this check requires the public key to be available. A provider that does not
    // expose the public key of a hardware-held private key will fail here; the reason code is
    // logged so that case can be told apart from a genuine key/certificate mismatch.
    if (SSL_CTX_check_private_key(contextPtr->sslCtxPtr) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("Private key does not match the certificate (reason: %lu)",
                 (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }

    return LE_OK;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length, including the terminating null byte, of an OpenSSL provider name
 */
//--------------------------------------------------------------------------------------------------
#define PROVIDER_NAME_MAX_LEN       64

//--------------------------------------------------------------------------------------------------
/**
 * Suffix of a provider module file on this platform.
 *
 * OpenSSL appends it itself when it turns a provider name into a file name, so it has to be taken
 * off a configured module path before that path's file name is handed over as a provider name.
 */
//--------------------------------------------------------------------------------------------------
#define PROVIDER_MODULE_SUFFIX      ".so"

//--------------------------------------------------------------------------------------------------
/**
 * Split a configured provider module path into the directory OpenSSL should search and the provider
 * name it should load.
 *
 * OpenSSL exposes no entry point that loads one exact file: OSSL_PROVIDER_load() takes a name, turns
 * it into that name plus the platform DSO suffix, and looks the result up in the module search path.
 * A configured path such as
 *
 *     /usr/lib/ossl-modules/libotp_telsdk_provider.so
 *
 * therefore has to be taken apart into the search directory "/usr/lib/ossl-modules" and the name
 * "libotp_telsdk_provider", which OpenSSL then reassembles into the original file name.
 *
 * @return
 *  - LE_OK            dirBuf and nameBuf are filled in
 *  - LE_BAD_PARAMETER The path holds no file name, or a part of it does not fit
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SplitProviderModulePath
(
    const char*  modulePathPtr,   ///< [IN]  Configured "provider_module_path"
    char*        dirBuf,          ///< [OUT] Search directory, empty if the path holds none
    size_t       dirBufSize,      ///< [IN]  Size of dirBuf
    char*        nameBuf,         ///< [OUT] Provider name to load
    size_t       nameBufSize      ///< [IN]  Size of nameBuf
)
{
    const char* slashPtr = strrchr(modulePathPtr, '/');
    const char* basePtr;

    if (slashPtr)
    {
        size_t dirLen = (size_t)(slashPtr - modulePathPtr);

        if (dirLen == 0)
        {
            // "/libfoo.so": an empty directory part still means the root directory
            le_utf8_Copy(dirBuf, "/", dirBufSize, NULL);
        }
        else if (dirLen >= dirBufSize)
        {
            LE_ERROR("Provider module directory exceeds %"PRIuS" bytes", dirBufSize - 1);
            return LE_BAD_PARAMETER;
        }
        else
        {
            memcpy(dirBuf, modulePathPtr, dirLen);
            dirBuf[dirLen] = '\0';
        }

        basePtr = slashPtr + 1;
    }
    else
    {
        // A bare file name leaves the search path alone, so OpenSSL keeps looking in its own module
        // directory
        dirBuf[0] = '\0';
        basePtr   = modulePathPtr;
    }

    if (basePtr[0] == '\0')
    {
        LE_ERROR("Provider module path '%s' holds no file name", modulePathPtr);
        return LE_BAD_PARAMETER;
    }

    if (le_utf8_Copy(nameBuf, basePtr, nameBufSize, NULL) != LE_OK)
    {
        LE_ERROR("Provider module name exceeds %"PRIuS" bytes", nameBufSize - 1);
        return LE_BAD_PARAMETER;
    }

    // Take the DSO suffix off, since OpenSSL adds it back when it builds the file name. Only an
    // exact trailing match is removed, so a name that merely contains dots survives intact.
    size_t nameLen   = strlen(nameBuf);
    size_t suffixLen = strlen(PROVIDER_MODULE_SUFFIX);

    if ((nameLen > suffixLen) &&
        (strcmp(nameBuf + (nameLen - suffixLen), PROVIDER_MODULE_SUFFIX) == 0))
    {
        nameBuf[nameLen - suffixLen] = '\0';
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Activate one provider by name, keeping OpenSSL's own providers usable.
 *
 * OSSL_PROVIDER_try_load() is used rather than OSSL_PROVIDER_load() for its retain_fallbacks
 * argument. Activating any provider explicitly otherwise switches off the implicitly activated
 * default provider, which would take the standard algorithms away from the rest of the handshake.
 * Retaining the fallbacks keeps an external provider additive: it supplies the key operations, and
 * OpenSSL goes on supplying everything else.
 *
 * @return The provider handle, or NULL on failure.
 */
//--------------------------------------------------------------------------------------------------
static OSSL_PROVIDER* TryLoadProvider
(
    const char*  namePtr   ///< [IN] Provider name to load
)
{
    OSSL_PROVIDER* provPtr = OSSL_PROVIDER_try_load(NULL, namePtr, 1);

    if (!provPtr)
    {
        LE_WARN("Unable to load provider '%s' (reason: %lu)",
                namePtr, (unsigned long)ERR_GET_REASON(ERR_peek_last_error()));
    }

    return provPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Load and activate the external OpenSSL provider named by the configuration.
 *
 * Two names can identify the module, and they need not agree: the file name inside
 * "provider_module_path" and the "provider_name" entry. The path names the exact file the caller
 * pointed at, so the name derived from it is tried first; "provider_name" is the fallback, which
 * covers a module whose file name differs from the name its provider is registered under. Either
 * entry on its own is enough.
 *
 * @return
 *  - LE_OK            The provider is activated
 *  - LE_BAD_PARAMETER Neither entry names a module, or a path does not fit
 *  - LE_NOT_FOUND     The module file does not exist or is not readable
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
static le_result_t LoadOpensslProvider
(
    OpensslCtx_t*  contextPtr,      ///< [INOUT] OpenSSL socket context pointer
    const char*    namePtr,         ///< [IN] "provider_name" value, NULL if absent
    const char*    modulePathPtr    ///< [IN] "provider_module_path" value, NULL if absent
)
{
    char           moduleDir[CRED_FILE_PATH_MAX_LEN] = {0};
    char           moduleName[PROVIDER_NAME_MAX_LEN] = {0};
    const char*    candidates[2]   = { NULL, NULL };
    int            candidateCount  = 0;
    OSSL_PROVIDER* provPtr         = NULL;
    const char*    loadedNamePtr   = NULL;

    // -------------------------------------------------------------------------
    // Step 1: turn a configured module path into a search directory and a name.
    //
    // Readability is checked up front so that a missing module reports
    // LE_NOT_FOUND, instead of surfacing as a generic load failure later.
    // -------------------------------------------------------------------------
    if (modulePathPtr && (modulePathPtr[0] != '\0'))
    {
        if (strnlen(modulePathPtr, CRED_FILE_PATH_MAX_LEN) >= CRED_FILE_PATH_MAX_LEN)
        {
            LE_ERROR("Provider module path exceeds maximum length of %d", CRED_FILE_PATH_MAX_LEN);
            return LE_BAD_PARAMETER;
        }

        if (access(modulePathPtr, R_OK) != 0)
        {
            LE_ERROR("Provider module '%s' does not exist or is not readable (errno %d)",
                     modulePathPtr, errno);
            return LE_NOT_FOUND;
        }

        le_result_t status = SplitProviderModulePath(modulePathPtr,
                                                    moduleDir,  sizeof(moduleDir),
                                                    moduleName, sizeof(moduleName));
        if (status != LE_OK)
        {
            return status;
        }
    }

    // -------------------------------------------------------------------------
    // Step 2: collect the names worth trying, most specific first.
    // -------------------------------------------------------------------------
    if (moduleName[0] != '\0')
    {
        candidates[candidateCount++] = moduleName;
    }

    if (namePtr && (namePtr[0] != '\0')
        && ((moduleName[0] == '\0') || (strcmp(namePtr, moduleName) != 0)))
    {
        candidates[candidateCount++] = namePtr;
    }

    if (candidateCount == 0)
    {
        LE_ERROR("Provider type '%s' needs 'provider_name' or 'provider_module_path'",
                 SOCKET_PROVIDER_TYPE_OPENSSL);
        return LE_BAD_PARAMETER;
    }

    // -------------------------------------------------------------------------
    // Step 3: point OpenSSL at the configured directory.
    //
    // This is library-wide state, so it is only touched when the configuration
    // actually named a directory; otherwise OpenSSL keeps using its own.
    // -------------------------------------------------------------------------
    if (moduleDir[0] != '\0')
    {
        if (OSSL_PROVIDER_set_default_search_path(NULL, moduleDir) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("Unable to set the provider search path to '%s' (reason: %lu)",
                     moduleDir, (unsigned long)ERR_GET_REASON(code));
            return LE_FAULT;
        }
    }

    // -------------------------------------------------------------------------
    // Step 4: activate the module.
    // -------------------------------------------------------------------------
    for (int i = 0; (i < candidateCount) && (provPtr == NULL); i++)
    {
        provPtr = TryLoadProvider(candidates[i]);
        if (provPtr)
        {
            loadedNamePtr = candidates[i];
        }
    }

    if (!provPtr)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("Unable to activate a provider from name '%s' / module '%s'",
                 (namePtr && namePtr[0]) ? namePtr : "(unset)",
                 (modulePathPtr && modulePathPtr[0]) ? modulePathPtr : "(unset)");
        return LE_FAULT;
    }

    // Release the provider this context activated earlier, so that calling this twice does not
    // leak a reference.
    if (contextPtr->providerPtr)
    {
        OSSL_PROVIDER_unload(contextPtr->providerPtr);
    }
    contextPtr->providerPtr = provPtr;

    LE_INFO("Provider '%s' activated from %s", loadedNamePtr,
            moduleDir[0] ? moduleDir : "the OpenSSL module directory");

    return LE_OK;
}

#endif /* OPENSSL_VERSION_NUMBER >= 0x30000000L */

//--------------------------------------------------------------------------------------------------
/**
 * Configure the cryptographic provider of the secure socket context.
 *
 * Backs the provider block of the caller's configuration. A "default" type keeps OpenSSL's own
 * implementations and ignores the other two arguments, so a caller can hand over a whole
 * configuration block unchanged whether or not it asks for an external provider.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter, unrecognized type, or no module named
 *  - LE_NOT_FOUND     Provider module file does not exist or is not readable
 *  - LE_UNSUPPORTED   Provider type not supported by this OpenSSL build
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetProvider
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       typePtr,          ///< [IN] "type" value (required)
    const char*       namePtr,          ///< [IN] "provider_name" value, NULL if absent
    const char*       modulePathPtr     ///< [IN] "provider_module_path" value, NULL if absent
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    if ((!typePtr) || (typePtr[0] == '\0'))
    {
        LE_ERROR("Provider type is NULL or empty");
        return LE_BAD_PARAMETER;
    }

    if (strcasecmp(typePtr, SOCKET_PROVIDER_TYPE_DEFAULT) == 0)
    {
        LE_INFO("Provider type '%s': keeping the OpenSSL built-in implementations",
                SOCKET_PROVIDER_TYPE_DEFAULT);
        return LE_OK;
    }

    if (strcasecmp(typePtr, SOCKET_PROVIDER_TYPE_OPENSSL) == 0)
    {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        return LoadOpensslProvider(contextPtr, namePtr, modulePathPtr);
#else
        LE_UNUSED(namePtr);
        LE_UNUSED(modulePathPtr);
        LE_ERROR("Provider type '%s' requires OpenSSL 3.0 or later", typePtr);
        return LE_UNSUPPORTED;
#endif
    }

    LE_ERROR("Unrecognized provider type '%s'", typePtr);
    return LE_BAD_PARAMETER;
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
    // Step 1: validate the list and calculate the total byte length of the
    // length-prefixed wire buffer.
    //
    // Wire format per RFC 7301:
    //   for each protocol name:  1 byte (length) + N bytes (name)
    // -------------------------------------------------------------------------
    size_t bufLen = 0;
    int    count  = 0;

    for (; alpnList[count] != NULL; count++)
    {
        size_t nameLen = strlen(alpnList[count]);

        if (nameLen == 0 || nameLen > ALPN_PROTO_NAME_MAX_LEN)
        {
            LE_ERROR("ALPN protocol name '%s' has invalid length %"PRIuS", skipping",
                     alpnList[count], nameLen);
            return;
        }

        if (count >= ALPN_PROTO_LIST_MAX_COUNT)
        {
            LE_ERROR("ALPN protocol list exceeds maximum count of %d", ALPN_PROTO_LIST_MAX_COUNT);
            return;
        }

        bufLen += 1 + nameLen;  // 1-byte length prefix + name bytes
    }

    // -------------------------------------------------------------------------
    // Step 2: fill the fixed-size wire-format buffer, replacing any previously
    // stored list.
    // -------------------------------------------------------------------------
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
 * Mapping between a "tls_min_version" configuration string and the matching OpenSSL protocol
 * version constant.
 *
 * The version constants come from <openssl/tls1.h>, reached transitively through
 * <openssl/ssl.h>. TLS1_3_VERSION only exists from OpenSSL 1.1.1 onwards, and even there it is
 * defined unconditionally - so a library configured with no-tls1_3 still compiles it. Both
 * conditions are therefore tested: too old a library, or TLS 1.3 compiled out, leaves a zero
 * version.
 *
 * A zero version marks a name this OpenSSL build understands but cannot honour, which is reported
 * as LE_UNSUPPORTED so that it stays distinguishable from a typo in the configuration.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char* namePtr;   ///< Version string accepted in the configuration
    int         version;   ///< OpenSSL protocol version constant (0 = not supported by this build)
}
TlsVersionEntry_t;

static const TlsVersionEntry_t TlsVersionTable[] =
{
    { "TLSv1",   TLS1_VERSION   },
    { "TLSv1.0", TLS1_VERSION   },
    { "TLSv1.1", TLS1_1_VERSION },
    { "TLSv1.2", TLS1_2_VERSION },
#if (OPENSSL_VERSION_NUMBER >= 0x10101000L) && !defined(OPENSSL_NO_TLS1_3)
    { "TLSv1.3", TLS1_3_VERSION },
#else
    { "TLSv1.3", 0 },
#endif
};

#define TLS_VERSION_TABLE_SIZE  (sizeof(TlsVersionTable) / sizeof(TlsVersionTable[0]))

//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum acceptable TLS protocol version in the secure socket context.
 *
 * Backs the "tls_min_version" configuration entry. The value replaces the TLS 1.2 floor installed
 * by secSocket_Init(), so it can both raise and lower the minimum version.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter or unrecognized version string
 *  - LE_UNSUPPORTED   Version understood but not supported by this OpenSSL build
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetTlsMinVersion
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       versionPtr        ///< [IN] Minimum TLS version string
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    if ((!versionPtr) || (versionPtr[0] == '\0'))
    {
        LE_ERROR("TLS minimum version string is NULL or empty");
        return LE_BAD_PARAMETER;
    }

    if (!contextPtr->sslCtxPtr)
    {
        LE_ERROR("SSL_CTX is NULL");
        return LE_FAULT;
    }

    const TlsVersionEntry_t* entryPtr = NULL;

    for (size_t i = 0; i < TLS_VERSION_TABLE_SIZE; i++)
    {
        if (strcasecmp(versionPtr, TlsVersionTable[i].namePtr) == 0)
        {
            entryPtr = &TlsVersionTable[i];
            break;
        }
    }

    if (!entryPtr)
    {
        LE_ERROR("Unrecognized TLS minimum version '%s'", versionPtr);
        return LE_BAD_PARAMETER;
    }

    if (entryPtr->version == 0)
    {
        LE_ERROR("TLS minimum version '%s' is not supported by this OpenSSL build", versionPtr);
        return LE_UNSUPPORTED;
    }

    if (SSL_CTX_set_min_proto_version(contextPtr->sslCtxPtr, entryPtr->version) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_CTX_set_min_proto_version failed for '%s' (reason: %lu)",
                 versionPtr, (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }

    LE_INFO("TLS minimum version set to '%s'", versionPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the cipher suite list in the secure socket context from a colon-separated string.
 *
 * Backs the "tls_cipher_suites" configuration entry, e.g.
 * "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384".
 *
 * OpenSSL keeps the TLS 1.3 suites and the TLS 1.0-1.2 ciphers in two independent lists, set
 * through two different calls, so the incoming names are split by prefix: "TLS_" marks a TLS 1.3
 * suite name, anything else is a TLS 1.0-1.2 cipher name. A list that ends up empty is left
 * untouched, so restricting the TLS 1.3 suites does not implicitly narrow TLS 1.2.
 *
 * @note When names for both versions are given and only the second call fails, the first list
 *       stays applied. The failure is reported, but the context is not rolled back.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty string, or string too long)
 *  - LE_UNSUPPORTED   TLS 1.3 suites requested but unsupported by this OpenSSL build
 *  - LE_FAULT         Failure, including a name OpenSSL does not recognize
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetCipherSuiteList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       cipherSuitesPtr   ///< [IN] Colon-separated cipher suite list
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    if ((!cipherSuitesPtr) || (cipherSuitesPtr[0] == '\0'))
    {
        LE_ERROR("Cipher suite list is NULL or empty");
        return LE_BAD_PARAMETER;
    }

    if (!contextPtr->sslCtxPtr)
    {
        LE_ERROR("SSL_CTX is NULL");
        return LE_FAULT;
    }

    char workBuf[TLS_CFG_STR_MAX_LEN];
    char tls12Buf[TLS_CFG_STR_MAX_LEN] = {0};
    char tls13Buf[TLS_CFG_STR_MAX_LEN] = {0};

    // strtok_r() writes into its input, so work on a private copy of the caller's string
    if (le_utf8_Copy(workBuf, cipherSuitesPtr, sizeof(workBuf), NULL) != LE_OK)
    {
        LE_ERROR("Cipher suite list exceeds %d bytes", TLS_CFG_STR_MAX_LEN - 1);
        return LE_BAD_PARAMETER;
    }

    // -------------------------------------------------------------------------
    // Step 1: split the list into a TLS 1.3 part and a TLS 1.0-1.2 part.
    //
    // Consecutive, leading and trailing ':' separators yield no token, so a
    // value such as "A::B" is accepted and read as two names.
    // -------------------------------------------------------------------------
    char* savePtr = NULL;

    for (char* tokenPtr = strtok_r(workBuf, ":", &savePtr);
         tokenPtr != NULL;
         tokenPtr = strtok_r(NULL, ":", &savePtr))
    {
        // TLS 1.3 suite names are the IANA ones, all prefixed with "TLS_"
        char* dstPtr = (strncmp(tokenPtr, "TLS_", 4) == 0) ? tls13Buf : tls12Buf;

        if (dstPtr[0] != '\0')
        {
            le_utf8_Append(dstPtr, ":", TLS_CFG_STR_MAX_LEN, NULL);
        }
        le_utf8_Append(dstPtr, tokenPtr, TLS_CFG_STR_MAX_LEN, NULL);
    }

    if ((tls12Buf[0] == '\0') && (tls13Buf[0] == '\0'))
    {
        LE_ERROR("Cipher suite list '%s' holds no name", cipherSuitesPtr);
        return LE_BAD_PARAMETER;
    }

    LE_INFO("Setting cipher suites: TLS1.2='%s' TLS1.3='%s'",
            tls12Buf[0] ? tls12Buf : "(default)",
            tls13Buf[0] ? tls13Buf : "(default)");

    // -------------------------------------------------------------------------
    // Step 2: hand each list to the matching OpenSSL setter.
    // -------------------------------------------------------------------------
    if (tls12Buf[0])
    {
        if (SSL_CTX_set_cipher_list(contextPtr->sslCtxPtr, tls12Buf) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("SSL_CTX_set_cipher_list failed for '%s' (reason: %lu)",
                     tls12Buf, (unsigned long)ERR_GET_REASON(code));
            return LE_FAULT;
        }
    }

    if (tls13Buf[0])
    {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
        if (SSL_CTX_set_ciphersuites(contextPtr->sslCtxPtr, tls13Buf) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("SSL_CTX_set_ciphersuites failed for '%s' (reason: %lu)",
                     tls13Buf, (unsigned long)ERR_GET_REASON(code));
            return LE_FAULT;
        }
#else
        LE_ERROR("TLS 1.3 cipher suites '%s' require OpenSSL 1.1.1 or later", tls13Buf);
        return LE_UNSUPPORTED;
#endif
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the accepted signature algorithms in the secure socket context from a colon-separated
 * string.
 *
 * Backs the "signature_algorithms" configuration entry, e.g. "ecdsa_secp256r1_sha256". OpenSSL
 * accepts both the RFC 8446 scheme names and the legacy "algorithm+hash" form, so the string is
 * passed through as-is.
 *
 * Role behaviour:
 *   CLIENT - SSL_CTX_set1_sigalgs_list(): the list is advertised in the ClientHello
 *            signature_algorithms extension and bounds the algorithms the client will sign with.
 *
 *   SERVER - the same call bounds what the server signs with and accepts, plus
 *            SSL_CTX_set1_client_sigalgs_list() so the list also applies to the
 *            signature_algorithms carried in the CertificateRequest. Without the second call the
 *            client certificate would stay unconstrained under mutual authentication.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty string, or string too long)
 *  - LE_UNSUPPORTED   Not supported by this OpenSSL build
 *  - LE_FAULT         Failure, including a name OpenSSL does not recognize
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetSignatureAlgorithms
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       sigAlgsPtr        ///< [IN] Colon-separated signature algorithm list
)
{
    OpensslCtx_t* contextPtr = GetContext(ctxPtr);
    if (!contextPtr)
    {
        LE_ERROR("Invalid SSL context");
        return LE_BAD_PARAMETER;
    }

    if ((!sigAlgsPtr) || (sigAlgsPtr[0] == '\0'))
    {
        LE_ERROR("Signature algorithm list is NULL or empty");
        return LE_BAD_PARAMETER;
    }

    if (strlen(sigAlgsPtr) >= TLS_CFG_STR_MAX_LEN)
    {
        LE_ERROR("Signature algorithm list exceeds %d bytes", TLS_CFG_STR_MAX_LEN - 1);
        return LE_BAD_PARAMETER;
    }

    if (!contextPtr->sslCtxPtr)
    {
        LE_ERROR("SSL_CTX is NULL");
        return LE_FAULT;
    }

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    if (SSL_CTX_set1_sigalgs_list(contextPtr->sslCtxPtr, sigAlgsPtr) != 1)
    {
        unsigned long code = ERR_peek_last_error();
        contextPtr->openssl_errcode = ERR_GET_REASON(code);
        LE_ERROR("SSL_CTX_set1_sigalgs_list failed for '%s' (reason: %lu)",
                 sigAlgsPtr, (unsigned long)ERR_GET_REASON(code));
        return LE_FAULT;
    }

    if (contextPtr->role == PROTO_ROLE_SERVER)
    {
        // Also constrain the CertificateRequest, so the list covers client certificates
        // when mutual authentication is in use.
        if (SSL_CTX_set1_client_sigalgs_list(contextPtr->sslCtxPtr, sigAlgsPtr) != 1)
        {
            unsigned long code = ERR_peek_last_error();
            contextPtr->openssl_errcode = ERR_GET_REASON(code);
            LE_ERROR("SSL_CTX_set1_client_sigalgs_list failed for '%s' (reason: %lu)",
                     sigAlgsPtr, (unsigned long)ERR_GET_REASON(code));
            return LE_FAULT;
        }
    }

    LE_INFO("Signature algorithms set to '%s'", sigAlgsPtr);
    return LE_OK;
#else
    LE_ERROR("Setting signature algorithms requires OpenSSL 1.0.2 or later");
    return LE_UNSUPPORTED;
#endif
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
 * Map a failed BIO_read()/BIO_write() on the SSL BIO chain to a le_result_t, and record the
 * OpenSSL reason code so that le_socket_GetTlsErrorCode() reports something usable.
 *
 * @return  Mapped le_result_t value
 */
//--------------------------------------------------------------------------------------------------
static le_result_t MapBioError
(
    OpensslCtx_t*  contextPtr,  ///< [IN] OpenSSL context (for storing openssl_errcode)
    int            ret         ///< [IN] Return value of the failed BIO operation
)
{
    SSL* sslPtr = NULL;

    BIO_get_ssl(contextPtr->bioPtr, &sslPtr);
    if (!sslPtr)
    {
        // No SSL object in the chain, so SSL_get_error() cannot be consulted. Record the last
        // queued reason rather than the BIO return value, which carries no diagnostic value.
        contextPtr->openssl_errcode = (int)ERR_GET_REASON(ERR_peek_last_error());
        return LE_FAULT;
    }

    return MapSslError(contextPtr, sslPtr, SSL_get_error(sslPtr, ret));
}

//--------------------------------------------------------------------------------------------------
/**
 * Time left until 'deadline', clamped to zero once the deadline has passed.
 *
 * @return  Remaining time, suitable for passing to select()
 */
//--------------------------------------------------------------------------------------------------
static struct timeval RemainingTime
(
    le_clk_Time_t  deadline    ///< [IN] Relative-clock instant to count down to
)
{
    struct timeval tv  = { .tv_sec = 0, .tv_usec = 0 };
    le_clk_Time_t  now = le_clk_GetRelativeTime();

    if (le_clk_GreaterThan(deadline, now))
    {
        le_clk_Time_t left = le_clk_Sub(deadline, now);

        // le_clk_Time_t is wider than struct timeval on 32-bit targets, so narrow explicitly.
        tv.tv_sec  = (time_t)left.sec;
        tv.tv_usec = (suseconds_t)left.usec;
    }

    return tv;
}

//--------------------------------------------------------------------------------------------------
/**
 * Relative-clock instant 'timeout' milliseconds from now.
 *
 * @return  The computed deadline
 */
//--------------------------------------------------------------------------------------------------
static le_clk_Time_t DeadlineFromNow
(
    uint32_t  timeout    ///< [IN] Timeout in milliseconds
)
{
    le_clk_Time_t span = {
        .sec  = timeout / 1000,
        .usec = (timeout % 1000) * 1000
    };

    return le_clk_Add(le_clk_GetRelativeTime(), span);
}

//--------------------------------------------------------------------------------------------------
/**
 * Wait until the BIO's underlying file descriptor is ready for I/O, or 'deadline' passes.
 *
 * A zero remaining budget performs a single non-blocking poll, which preserves the documented
 * "return immediately when the timeout is zero" behaviour of secSocket_Read().
 *
 * @return
 *  - LE_OK        The descriptor became ready
 *  - LE_TIMEOUT   The budget was exhausted first
 *  - LE_FAULT     The descriptor could not be retrieved, or select() failed
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WaitForBioReady
(
    BIO*           bioPtr,     ///< [IN] BIO whose underlying descriptor is polled
    bool           forRead,    ///< [IN] Poll for readability, otherwise for writability
    le_clk_Time_t  deadline    ///< [IN] Relative-clock instant at which to give up
)
{
    fd_set set;
    int    fd, rv;

    // Get file descriptor of the current BIO. An unchecked failure here would leave 'fd'
    // uninitialised and hand a garbage descriptor to select().
    if (BIO_get_fd(bioPtr, &fd) < 0)
    {
        LE_ERROR("Unable to retrieve the file descriptor of the BIO");
        return LE_FAULT;
    }

    do
    {
        // Recomputed on every pass so that an interrupted select() resumes with the time that is
        // actually left instead of restarting the whole budget.
        struct timeval time = RemainingTime(deadline);

        FD_ZERO(&set);
        FD_SET(fd, &set);
        rv = select(fd + 1, forRead ? &set : NULL, forRead ? NULL : &set, NULL, &time);
    }
    while ((rv == -1) && (errno == EINTR));

    if (rv < 0)
    {
        LE_ERROR("select() failed: %s", strerror(errno));
        return LE_FAULT;
    }

    if ((rv == 0) || (!FD_ISSET(fd, &set)))
    {
        return LE_TIMEOUT;
    }

    return LE_OK;
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

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // Drop this context's reference on the external provider, if one was activated. The provider is
    // reference counted inside OpenSSL, so a module shared with another socket stays loaded.
    if (contextPtr->providerPtr)
    {
        if (OSSL_PROVIDER_unload(contextPtr->providerPtr) != 1)
        {
            LE_WARN("OSSL_PROVIDER_unload failed (reason: %lu)",
                    (unsigned long)ERR_GET_REASON(ERR_peek_last_error()));
        }
        contextPtr->providerPtr = NULL;
    }
#endif

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
 * Retries internally while OpenSSL reports a retryable condition, for at most
 * SEC_SOCKET_WRITE_TIMEOUT_MS milliseconds.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       The descriptor did not accept the data before the timeout elapsed
 *  - LE_CLOSED        The peer closed the TLS session
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

    if (!contextPtr->bioPtr)
    {
        LE_ERROR("Socket not connected");
        return LE_FAULT;
    }

    le_clk_Time_t deadline = DeadlineFromNow(SEC_SOCKET_WRITE_TIMEOUT_MS);

    for (;;)
    {
        int r = BIO_write(contextPtr->bioPtr, dataPtr, dataLen);

        if (r > 0)
        {
            return LE_OK;
        }

        if (!BIO_should_retry(contextPtr->bioPtr))
        {
            le_result_t status = MapBioError(contextPtr, r);

            LE_ERROR("Write failed: %s (OpenSSL reason: %d)",
                     LE_RESULT_TXT(status), contextPtr->openssl_errcode);
            return status;
        }

        // Retryable failure on a non-blocking descriptor: either the send buffer is full, or the
        // SSL layer needs to read before it can write (renegotiation, TLS 1.3 key update). Wait
        // for the direction OpenSSL asked for and present the very same buffer again, which is
        // what SSL_write() requires when partial writes are not enabled.
        bool forRead = BIO_should_read(contextPtr->bioPtr);

        le_result_t waitStatus = WaitForBioReady(contextPtr->bioPtr, forRead, deadline);
        if (waitStatus != LE_OK)
        {
            if (waitStatus == LE_TIMEOUT)
            {
                LE_ERROR("Write timed out after %d ms", SEC_SOCKET_WRITE_TIMEOUT_MS);
            }
            return waitStatus;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Read data from the socket file descriptor in a blocking way. If the timeout is zero, then the
 * API returns immediatly.
 *
 * Records that carry no application data (TLS 1.3 NewSessionTicket, key update, partial records)
 * are consumed transparently and do not end the call: the wait resumes with the time left in the
 * timeout budget.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 *  - LE_TIMEOUT       Timeout during execution
 *  - LE_CLOSED        The peer closed the TLS session
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
    int rv;

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

    // Absolute budget for the whole call. A wake-up that yields no application data must not
    // consume it in one shot, so every retry below counts down against this deadline instead of
    // restarting the timeout.
    le_clk_Time_t deadline = DeadlineFromNow(timeout);

    for (;;)
    {
        if (!BIO_pending(contextPtr->bioPtr))
        {
            le_result_t waitStatus = WaitForBioReady(contextPtr->bioPtr, true, deadline);

            if (waitStatus != LE_OK)
            {
                return waitStatus;
            }
        }

        // At this point, there is something available for reading from BIO
        rv = BIO_read(contextPtr->bioPtr, dataPtr, *dataLenPtr);
        if (rv > 0)
        {
            *dataLenPtr = rv;
            return LE_OK;
        }

        if (!BIO_should_retry(contextPtr->bioPtr))
        {
            le_result_t status = MapBioError(contextPtr, rv);

            if (status == LE_CLOSED)
            {
                LE_INFO("Read: peer closed the TLS session");
            }
            else
            {
                LE_ERROR("Read failed: %s (OpenSSL reason: %d)",
                         LE_RESULT_TXT(status), contextPtr->openssl_errcode);
            }

            return status;
        }

        // The readable bytes carried no application data. On TLS 1.3 this is most often a
        // NewSessionTicket, which an OpenSSL server emits from inside SSL_accept() and therefore
        // reaches the peer ahead of any payload, but a key update or the first half of a split
        // record land here too. Reporting LE_WOULD_BLOCK on the first such record would abandon
        // the caller's timeout with the whole budget unspent, so wait for more instead.
        struct timeval left = RemainingTime(deadline);

        if ((left.tv_sec == 0) && (left.tv_usec == 0))
        {
            // A zero timeout was a request to poll once and report back, so distinguish it from
            // a budget that was genuinely waited out.
            return (timeout == 0) ? LE_WOULD_BLOCK : LE_TIMEOUT;
        }
    }
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
