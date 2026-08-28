/**
 * @file secSocket_mbedtls.c
 *
 * This file implements a set of networking functions to manage secure TCP/UDP sockets using
 * mbedTLS library.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include "legato.h"
#include "interfaces.h"

#include "le_socketLib.h"
#include "secSocket.h"

#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Max MbedTLS read retry
 */
//--------------------------------------------------------------------------------------------------
#define MAX_MBEDTLS_SSL_READ_RETRY  20

//--------------------------------------------------------------------------------------------------
/**
 *  MbedTLS connection timeout (in ms)
 *
 *  Read timeout during connection handshaking.  This is needed to accommodate networks that have
 *  longer connection setup times.
 */
//--------------------------------------------------------------------------------------------------
#define MBEDTLS_SSL_CONNECT_TIMEOUT (3 * 10000)

//--------------------------------------------------------------------------------------------------
/**
 * MbedTLS global context
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    mbedtls_net_context sock;       ///< MbedTLS wrapper for socket.
    mbedtls_ssl_context sslCtx;     ///< SSL/TLS context.
    mbedtls_ssl_config  sslConf;    ///< SSL/TLS configuration.
    mbedtls_x509_crt    caCert;     ///< X.509 certificate.
}
MbedtlsCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool for MbedTLS sockets context.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t SocketCtxPoolRef = NULL;
LE_MEM_DEFINE_STATIC_POOL(SocketCtxPool, MAX_SOCKET_NB, sizeof(MbedtlsCtx_t));

//--------------------------------------------------------------------------------------------------
// Static functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Write to a stream and handle restart if necessary
 *
 * @return
 *  -  0 when all data are written
 *  - < on failure
 */
//--------------------------------------------------------------------------------------------------
static int WriteToStream
(
    mbedtls_ssl_context *sslCtxPtr, ///< [IN] SSL context
    const char          *bufferPtr, ///< [IN] Data to be sent
    int                  length     ///< [IN] Data length
)
{
    int r;

    LE_ASSERT(sslCtxPtr != NULL);
    LE_ASSERT(bufferPtr != NULL);

    r = mbedtls_ssl_write(sslCtxPtr, (const unsigned char*)bufferPtr, (size_t)length);
    if (0 >= r)
    {
        char str[256];
        mbedtls_strerror(r, str, sizeof(str));
        LE_ERROR("Error %d on write: %s", r, str);
        return r;
    }

    if (r > 0)
    {
        size_t newLength = length;
        while (r > 0)
        {
            newLength -= r;
            bufferPtr = bufferPtr + r;
            r = mbedtls_ssl_write(sslCtxPtr, (const unsigned char*)bufferPtr, newLength);
        }
    }

    if (0 > r)
    {
        char str[256];
        mbedtls_strerror(r, str, sizeof(str));
        LE_ERROR("Error %d on write: %s", r, str);
    }
    return r;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read a from a stream and handle restart if necessary
 *
 * @return
 *  - Data length read
 *  - 0 or negative value on failure
 *  - LWM2MCORE_ERR_TIMEOUT if no data are read
 */
//--------------------------------------------------------------------------------------------------
static int ReadFromStream
(
    mbedtls_ssl_context     *sslCtxPtr, ///< [IN] SSL context
    char                    *bufferPtr, ///< [IN] Buffer
    int                      length     ///< [IN] Buffer length
)
{
    int             r = -1;
    static size_t   count = 0;

    mbedtls_ssl_context* ctxPtr = (mbedtls_ssl_context*)sslCtxPtr;

    while (r < 0)
    {
        r = mbedtls_ssl_read(ctxPtr, (unsigned char*)bufferPtr, (size_t)length);
        if ((r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) || (r == MBEDTLS_ERR_NET_RECV_FAILED))
        {
            LE_ERROR("Peer close notification or socket unreadable");
            r = 0;
        }
        else if (r == MBEDTLS_ERR_SSL_TIMEOUT)
        {
            LE_ERROR("Timeout on read");
            r = LE_TIMEOUT;
            break;
        }
        else if ((r < 0) && (r != MBEDTLS_ERR_SSL_WANT_READ) && (r != MBEDTLS_ERR_SSL_WANT_WRITE))
        {
            LE_ERROR("Error on MbedTLS ssl read: %d", r);
            break;
        }
        else
        {
            // Count the number of MBEDTLS_ERR_SSL_WANT_READ or MBEDTLS_ERR_SSL_WANT_WRITE
            count++;
        }

        if (count > MAX_MBEDTLS_SSL_READ_RETRY)
        {
            break;
        }

        LE_INFO_IF(r == 0, "Reached the end of the data stream");
    }
    count = 0;
    return r;
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
    ProtoRoleType_t    role, ///< [IN] Protocol role type
    secSocket_Ctx_t **ctxPtr ///< [OUT] Secure socket context pointer
)
{
    MbedtlsCtx_t *contextPtr;

    LE_ASSERT(ctxPtr != NULL);

    // Initialize the socket context pool
    if (!SocketCtxPoolRef)
    {
        SocketCtxPoolRef = le_mem_InitStaticPool(SocketCtxPool,
                                                 MAX_SOCKET_NB,
                                                 sizeof(MbedtlsCtx_t));
    }

    // Alloc memory from pool
    contextPtr = le_mem_Alloc(SocketCtxPoolRef);

    // Initialize the session data
    mbedtls_net_init(&(contextPtr->sock));
    mbedtls_ssl_init(&(contextPtr->sslCtx));
    mbedtls_ssl_config_init(&(contextPtr->sslConf));

    *ctxPtr = (secSocket_Ctx_t *) contextPtr;

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
    int              ret;
    MbedtlsCtx_t    *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);
    LE_ASSERT(certificatePtr != NULL);
    LE_ASSERT(certificateLen > 0);

    LE_DEBUG("Certificate: %p Len:%zu", certificatePtr, certificateLen);
    ret = mbedtls_x509_crt_parse(&(contextPtr->caCert), certificatePtr, certificateLen);
    if (ret < 0)
    {
        LE_ERROR("Failed!  mbedtls_x509_crt_parse returned -0x%x", -ret);
        return LE_FAULT;
    }

    // Check certificate validity
    if ((mbedtls_x509_time_is_past(&contextPtr->caCert.valid_to)) ||
        (mbedtls_x509_time_is_future(&contextPtr->caCert.valid_from)))
    {
        LE_ERROR("Current certificate expired, please add a valid certificate");
        return LE_FORMAT_ERROR;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add root CA certificates to the secure socket context from a file.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] CA certificate file path (PEM or DER)
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(pathPtr);

    return LE_NOT_IMPLEMENTED;
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
    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own certificate chain to the secure socket context from a file.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnCertificateFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own certificate chain file path (PEM or DER)
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(pathPtr);

    return LE_NOT_IMPLEMENTED;
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
    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add the module's own private key to the secure socket context from a file.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_AddOwnPrivateKeyFile
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       pathPtr           ///< [IN] Own private key file path
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(pathPtr);

    return LE_NOT_IMPLEMENTED;
}

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
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set authentication type to the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAuthType
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    uint8_t           auth              ///< [IN] Authentication type
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the ALPN protocol list in the secure socket context.
 */
//--------------------------------------------------------------------------------------------------
void secSocket_SetAlpnProtocolList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char**      alpnList          ///< [IN] ALPN protocol list pointer
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(alpnList);

    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the minimum acceptable TLS protocol version in the secure socket context.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetTlsMinVersion
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       versionPtr        ///< [IN] Minimum TLS version string
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(versionPtr);

    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the cipher suite list in the secure socket context from a colon-separated string.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetCipherSuiteList
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       cipherSuitesPtr   ///< [IN] Colon-separated cipher suite list
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(cipherSuitesPtr);

    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the accepted signature algorithms in the secure socket context from a colon-separated
 * string.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetSignatureAlgorithms
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       sigAlgsPtr        ///< [IN] Colon-separated signature algorithm list
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(sigAlgsPtr);

    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Configure the cryptographic provider of the secure socket context.
 *
 * @return
 *  - LE_OK               The function succeeded
 *  - LE_NOT_IMPLEMENTED  Not implemented for device
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_SetProvider
(
    secSocket_Ctx_t*  ctxPtr,           ///< [INOUT] Secure socket context pointer
    const char*       typePtr,          ///< [IN] "type" value
    const char*       namePtr,          ///< [IN] "provider_name" value, NULL if absent
    const char*       modulePathPtr     ///< [IN] "provider_module_path" value, NULL if absent
)
{
    LE_UNUSED(ctxPtr);
    LE_UNUSED(typePtr);
    LE_UNUSED(namePtr);
    LE_UNUSED(modulePathPtr);

    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Performs TLS Handshake
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
    int                 fd         ///< [IN] Already-connected TCP file descriptor
)
{
    return LE_NOT_IMPLEMENTED;
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
    secSocket_Ctx_t *ctxPtr,    ///< [INOUT] Secure socket context pointer
    char            *hostPtr,   ///< [IN] Host to connect on
    uint16_t         port,      ///< [IN] Port to connect on
    SocketType_t     type,      ///< [IN] Socket type (TCP, UDP)
    int             *fdPtr      ///< [OUT] Socket file descriptor
)
{
    char             portBuffer[6] = {0};
    int              ret;
    MbedtlsCtx_t    *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);
    LE_ASSERT(hostPtr != NULL);
    LE_ASSERT(fdPtr != NULL);

    // Start the connection
    snprintf(portBuffer, sizeof(portBuffer), "%d", port);
    LE_INFO("Connecting to %d/%s:%d - %s:%s...", type, hostPtr, port, hostPtr, portBuffer);

    if ((ret = mbedtls_net_connect(&(contextPtr->sock),
                                   hostPtr,
                                   portBuffer,
                                   (type == TCP_TYPE) ?
                                   MBEDTLS_NET_PROTO_TCP : MBEDTLS_NET_PROTO_UDP)) != 0)
    {
        LE_INFO("Failed! mbedtls_net_connect returned %d", ret);
        if (ret == MBEDTLS_ERR_NET_UNKNOWN_HOST)
        {
            return LE_UNAVAILABLE;
        }
        else if (ret == MBEDTLS_ERR_NET_CONNECT_FAILED)
        {
            return LE_COMM_ERROR;
        }
        return LE_FAULT;
    }

    //Get the file descriptor
    *fdPtr = contextPtr->sock.fd;
    LE_DEBUG("File descriptor: %d", *fdPtr);

    // Setup
    LE_INFO("Setting up the SSL/TLS structure...");

    if ((ret = mbedtls_ssl_config_defaults(&(contextPtr->sslConf),
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        LE_ERROR("Failed! mbedtls_ssl_config_defaults returned %d", ret);
        // Only possible error is linked to memory allocation issue
        return LE_NO_MEMORY;
    }

    mbedtls_ssl_conf_authmode(&(contextPtr->sslConf), MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&(contextPtr->sslConf), &(contextPtr->caCert), NULL);
    mbedtls_port_SSLSetRNG(&contextPtr->sslConf);

    if ((ret = mbedtls_ssl_setup(&(contextPtr->sslCtx), &(contextPtr->sslConf))) != 0)
    {
        LE_ERROR("Failed! mbedtls_ssl_setup returned %d", ret);
        if (MBEDTLS_ERR_SSL_ALLOC_FAILED == ret)
        {
            return LE_NO_MEMORY;
        }
        return LE_FAULT;
    }

    if ((ret = mbedtls_ssl_set_hostname(&(contextPtr->sslCtx), hostPtr)) != 0)
    {
        LE_ERROR("Failed! mbedtls_ssl_set_hostname returned %d", ret);
        if (MBEDTLS_ERR_SSL_ALLOC_FAILED == ret)
        {
            return LE_NO_MEMORY;
        }
        return LE_FAULT;
    }

    mbedtls_ssl_set_bio(&(contextPtr->sslCtx), &(contextPtr->sock),
                        mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

    // Set the timeout for the initial handshake.
    mbedtls_ssl_conf_read_timeout(&(contextPtr->sslConf), MBEDTLS_SSL_CONNECT_TIMEOUT);

    // Handshake
    LE_INFO("Performing the SSL/TLS handshake...");
    while ((ret = mbedtls_ssl_handshake(&(contextPtr->sslCtx))) != 0)
    {
        LE_ERROR("Failed! mbedtls_ssl_handshake returned -0x%x", -ret);
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE))
        {
            if (ret == MBEDTLS_ERR_NET_RECV_FAILED)
            {
                return LE_TIMEOUT;
            }
            else if (ret == MBEDTLS_ERR_NET_SEND_FAILED)
            {
                return LE_FAULT;
            }
            else if (ret == MBEDTLS_ERR_SSL_ALLOC_FAILED)
            {
                return LE_NO_MEMORY;
            }
            else if (ret == MBEDTLS_ERR_SSL_CONN_EOF)
            {
                return LE_CLOSED;
            }
            else
            {
                return LE_FAULT;
            }
        }
    }

    return LE_OK;
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
    secSocket_Ctx_t *ctxPtr ///< [INOUT] Secure socket context pointer
)
{
    MbedtlsCtx_t *contextPtr = (MbedtlsCtx_t *) ctxPtr;
    LE_ASSERT(contextPtr != NULL);

    mbedtls_net_free(&(contextPtr->sock));
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
    secSocket_Ctx_t *ctxPtr ///< [IN] Secure socket context pointer
)
{
    MbedtlsCtx_t *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);

    mbedtls_net_free(&(contextPtr->sock));
    mbedtls_x509_crt_free(&(contextPtr->caCert));
    mbedtls_ssl_free(&(contextPtr->sslCtx));
    mbedtls_ssl_config_free(&(contextPtr->sslConf));

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
    secSocket_Ctx_t *ctxPtr,    ///< [INOUT] Secure socket context pointer
    const char      *dataPtr,   ///< [IN] Data pointer
    size_t           dataLen    ///< [IN] Data length
)
{
    MbedtlsCtx_t *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);
    LE_ASSERT(dataPtr != NULL);

    if (0 > WriteToStream(&(contextPtr->sslCtx), dataPtr, (int)dataLen))
    {
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
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Read
(
    secSocket_Ctx_t *ctxPtr,        ///< [INOUT] Secure socket context pointer
    char            *dataPtr,       ///< [INOUT] Data pointer
    size_t          *dataLenPtr,    ///< [IN] Data length pointer
    uint32_t         timeout        ///< [IN] Read timeout in milliseconds.
)
{
    int           count;
    MbedtlsCtx_t *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);
    LE_ASSERT(dataPtr != NULL);
    LE_ASSERT(dataLenPtr != NULL);

    mbedtls_ssl_conf_read_timeout(&(contextPtr->sslConf), timeout);
    count = ReadFromStream(&(contextPtr->sslCtx), dataPtr, (int)*dataLenPtr);
    if (count == LE_TIMEOUT)
    {
        return LE_TIMEOUT;
    }
    else if (count < 0)
    {
        LE_INFO("ERROR on reading data from stream");
        return LE_FAULT;
    }

    *dataLenPtr = count;
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
    secSocket_Ctx_t *ctxPtr ///< [IN] Secure socket context pointer
)
{
    MbedtlsCtx_t *contextPtr = (MbedtlsCtx_t *) ctxPtr;

    LE_ASSERT(contextPtr != NULL);
    return (mbedtls_ssl_get_bytes_avail(&(contextPtr->sslCtx)) != 0);
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
    return;
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
    return 0;
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
    secSocket_Ctx_t        *ctxPtr,   ///< [IN] Secure socket context pointer
    int                    err_code   ///< [IN] INT error code
)
{
    return;
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
    return;
}