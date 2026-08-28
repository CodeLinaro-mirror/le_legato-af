/**
 * @file secSocket_default.c
 *
 * This file provides a set of stub networking functions to manage secure TCP/UDP sockets.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "secSocket.h"

//--------------------------------------------------------------------------------------------------
// Public functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Initialize a secure socket using the input certificate.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Init
(
    ProtoRoleType_t    role,        ///< [IN] Protocol role type
    secSocket_Ctx_t**  ctxPtr       ///< [INOUT] Secure socket context pointer
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add one or more certificates to the secure socket context.
 *
 * @return
 *  - LE_OK            The function succeeded
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
    return LE_FAULT;
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
 *  - LE_FAULT         Internal error
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
)
{
    return LE_FAULT;
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
    return LE_BAD_PARAMETER;
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
    return LE_BAD_PARAMETER;
}

//--------------------------------------------------------------------------------------------------
/**
 * Write an amount of data to the secure socket.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_FAULT         Internal error
 */
//--------------------------------------------------------------------------------------------------
le_result_t secSocket_Write
(
    secSocket_Ctx_t* ctxPtr,      ///< [INOUT] Secure socket context pointer
    char*            dataPtr,     ///< [IN] Data pointer
    size_t           dataLen      ///< [IN] Data length
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read data from the socket file descriptor in a blocking way. If the timeout is zero, then the
 * API returns immediatly.
 *
 * @return
 *  - LE_OK            The function succeeded
 *  - LE_BAD_PARAMETER Invalid parameter
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
    return LE_BAD_PARAMETER;
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
    return false;
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
