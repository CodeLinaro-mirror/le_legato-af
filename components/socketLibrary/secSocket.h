/**
 * @file secSocket.h
 *
 * Networking functions definition for secure TCP/UDP sockets using secure library.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef LE_SEC_SOCKET_LIB_H
#define LE_SEC_SOCKET_LIB_H

#include "legato.h"
#include "interfaces.h"
#include "common.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Secure socket handler context definition
 */
//--------------------------------------------------------------------------------------------------
typedef void secSocket_Ctx_t;

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Add root CA certificates to the secure socket context.
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
);

//--------------------------------------------------------------------------------------------------
/**
 * Add root CA certificates to the secure socket context from a file.
 *
 * The file may be PEM or DER encoded, and either form may hold several concatenated
 * certificates. The certificate content is read by the underlying TLS library and never copied
 * into the caller's memory.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty path, or path too long)
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Invalid or expired certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] CA certificate file path (PEM or DER)
);

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own certificates to the secure socket context for mutual authentication.
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
);

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own certificate chain to the secure socket context from a file, for mutual
 * authentication.
 *
 * The file may be PEM or DER encoded, and may hold a full chain: the leaf certificate first,
 * optionally followed by the intermediate CA certificates completing the chain.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty path, or path too long)
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Invalid or expired certificate
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own certificate chain file path (PEM or DER)
);

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own private key to the secure socket context for mutual authentication.
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
);

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own private key to the secure socket context from a file, for mutual
 * authentication.
 *
 * The file may be PEM or DER encoded. The key is loaded by the underlying TLS library, so the
 * key material never transits through the caller's memory. Because the load goes through the
 * library's decoder chain, a key held by a crypto provider - i.e. a file holding a key reference
 * or wrapped key blob rather than the raw key material - is also supported, provided that
 * provider is loaded and registers a matching decoder.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty path, or path too long)
 *  - LE_NOT_FOUND     File does not exist or is not readable
 *  - LE_FORMAT_ERROR  Unable to decode the private key
 *  - LE_FAULT         Failure, including a key that does not match the loaded certificate
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnPrivateKeyFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own private key file path
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the cryptographic provider of the secure socket context.
 *
 * Backs the provider block of the caller's configuration:
 *   "type", "provider_name" and "provider_module_path".
 *
 * With SOCKET_PROVIDER_TYPE_DEFAULT nothing is loaded and the TLS library keeps its own built-in
 * implementations, so a configuration that asks for no external provider goes through the same
 * call. With SOCKET_PROVIDER_TYPE_OPENSSL the named module is loaded and activated.
 *
 * The strings belong to the caller and are only read for the duration of the call.
 *
 * @note A key held by the provider is loaded separately, with
 *       secSocket_AddOwnPrivateKeyFile(), which already routes a key reference file through the
 *       TLS library's decoder chain. Call this function first, so the provider is active by the
 *       time the key reference has to be decoded.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter, unrecognized type, or no module named
 *  - LE_NOT_FOUND     Provider module file does not exist or is not readable
 *  - LE_UNSUPPORTED   Provider type not supported by the underlying TLS library
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetProvider
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       typePtr,          ///< [IN] "type" value (required)
    const char*       namePtr,          ///< [IN] "provider_name" value, NULL if absent
    const char*       modulePathPtr     ///< [IN] "provider_module_path" value, NULL if absent
);

//--------------------------------------------------------------------------------------------------
/**
 * Set cipher suites to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetCipherSuites
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           cipherIdx         ///< [IN] Cipher suites index
);

//--------------------------------------------------------------------------------------------------
/**
 * Set authentication type to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAuthType
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           auth              ///< [IN] Authentication type
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the ALPN protocol list in the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAlpnProtocolList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char**      alpnList          ///< [IN] ALPN protocol list pointer
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum acceptable TLS protocol version in the secure socket context.
 *
 * Backs the "tls_min_version" configuration entry. Accepted values are "TLSv1", "TLSv1.0",
 * "TLSv1.1", "TLSv1.2" and "TLSv1.3", matched case-insensitively.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter or unrecognized version string
 *  - LE_UNSUPPORTED   Version understood but not supported by the underlying TLS library
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetTlsMinVersion
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       versionPtr        ///< [IN] Minimum TLS version string
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the cipher suite list in the secure socket context from a colon-separated string.
 *
 * Backs the "tls_cipher_suites" configuration entry, e.g.
 * "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384".
 *
 * Names are dispatched by TLS version: entries prefixed with "TLS_" are TLS 1.3 suite names,
 * every other entry is a TLS 1.0-1.2 cipher name. A version whose list ends up empty keeps the
 * TLS library defaults, so restricting the TLS 1.3 suites does not implicitly disable TLS 1.2.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty string, or string too long)
 *  - LE_FAULT         Failure, including a name the TLS library does not recognize
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetCipherSuiteList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       cipherSuitesPtr   ///< [IN] Colon-separated cipher suite list
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the accepted signature algorithms in the secure socket context from a colon-separated
 * string.
 *
 * Backs the "signature_algorithms" configuration entry, e.g. "ecdsa_secp256r1_sha256".
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter (null/empty string, or string too long)
 *  - LE_FAULT         Failure, including a name the TLS library does not recognize
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetSignatureAlgorithms
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       sigAlgsPtr        ///< [IN] Colon-separated signature algorithm list
);

//--------------------------------------------------------------------------------------------------
/**
 * Performs TLS Handshake
 *
 * Warning: Deprecated function. Use secSocket_Connect() to connect remote host and doing handshake.
 *
 * @return
 *  - LE_OK                 The function succeeded
 *  - LE_NOT_IMPLEMENTED    Not implemented for device
 *  - LE_TIMEOUT            Timeout during execution
 *  - LE_FAULT              Internal error
 *  - LE_NO_MEMORY          Memory allocation issue
 *  - LE_CLOSED             In case of end of file error
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_PerformHandshake
(
    secSocket_Ctx_t*    ctxPtr,    ///< [INOUT] Secure socket context pointer
    char*               hostPtr,   ///< [IN] Host to connect on
    int                 fd         ///< [IN] File descriptor
);

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
    char*            srcAddrPtr, ///< [IN] Source address pointer
    SocketType_t     type,       ///< [IN] Socket type (TCP, UDP)
    int*             fdPtr       ///< [OUT] Socket file descriptor
);

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
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Write an amount of data to the secure socket.
 *
 * Backends may retry internally while the underlying transport reports a retryable condition, so
 * this call can block for a bounded time even on a non-blocking descriptor.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_TIMEOUT       The transport did not accept the data before the timeout elapsed
 *  - LE_CLOSED        In case of end of file error
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Write
(
    secSocket_Ctx_t* ctxPtr,      ///< [INOUT] Secure socket context pointer
    const char*      dataPtr,     ///< [IN] Data pointer
    size_t           dataLen      ///< [IN] Data length
);

//--------------------------------------------------------------------------------------------------
/**
 * Read data from the socket file descriptor in a blocking way. If the timeout is zero, then the
 * API returns immediatly.
 *
 * Transport records that carry no application data are consumed transparently and do not end the
 * call: the wait resumes with the time left in the timeout budget.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_IN_PROGRESS   Secure HandShake still in progress
 *  - LE_BAD_PARAMETER Invalid parameter
 *  - LE_FAULT         Internal error
 *  - LE_TIMEOUT       Timeout during execution
 *  - LE_CLOSED        In case of end of file error
 *  - LE_WOULD_BLOCK   Zero timeout requested and no application data was available
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Read
(
    secSocket_Ctx_t* ctxPtr,       ///< [INOUT] Secure socket context pointer
    char*            dataPtr,      ///< [INOUT] Data pointer
    size_t*          dataLenPtr,   ///< [IN] Data length pointer
    uint32_t         timeout       ///< [IN] Read timeout in milliseconds.
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the tls version to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetTlsVersion
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           tlsVersion        ///< [IN] Supported TLS version (Minor version number)
);

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
);

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
    secSocket_Ctx_t *ctxPtr,     ///< [IN] Secure socket context pointer
    int             err_code     ///< [IN] INT error code
);

//--------------------------------------------------------------------------------------------------
/**
 * Copy the TLS configuration from a source secure socket context into a destination context.
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
);

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
);

#endif /* LE_SEC_SOCKET_LIB_H */
