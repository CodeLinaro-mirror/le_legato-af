/**
 * @file common.h
 *

 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef LE_SOCKET_COMMON_H
#define LE_SOCKET_COMMON_H

//--------------------------------------------------------------------------------------------------
/**
 * Enumeration of supported socket types
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TCP_TYPE = 0,
    UDP_TYPE,
    UNKNOWN_TYPE
}
SocketType_t;

//--------------------------------------------------------------------------------------------------
/**
 * Enumeration of Address Family types
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    SOCK_AF_IPV4,   ///< IPV4
    SOCK_AF_IPV6,   ///< IPV6
    SOCK_AF_ANY     ///< Unspecified
}
SocketAfType_t;

//--------------------------------------------------------------------------------------------------
/**
 * Authentication types, which is the 7th parameter in the ksslcrypto write command.
 */
//--------------------------------------------------------------------------------------------------
typedef enum AuthType
{
    AUTH_SERVER  = 1,
    AUTH_MUTUAL  = 3,
    AUTH_UNKNOWN = 0xFF
}
AuthType_t;

//--------------------------------------------------------------------------------------------------
/**
 * Protocol role types.
 */
//--------------------------------------------------------------------------------------------------
typedef enum ProtoRoleType
{
    PROTO_ROLE_SERVER,
    PROTO_ROLE_CLIENT
}
ProtoRoleType_t;

//--------------------------------------------------------------------------------------------------
/**
 * Max MbedTLS ALPN Protocol List size
 */
//--------------------------------------------------------------------------------------------------
#define ALPN_LIST_SIZE       1

//--------------------------------------------------------------------------------------------------
/**
 * Accepted values of the provider configuration "type" entry, passed to le_socket_SetProvider().
 *
 * SOCKET_PROVIDER_TYPE_DEFAULT keeps the TLS library's own built-in implementations, which is also
 * what happens when no provider is configured at all. SOCKET_PROVIDER_TYPE_OPENSSL activates an
 * external OpenSSL provider module, so that key operations can be carried out by a separate
 * cryptographic backend.
 */
//--------------------------------------------------------------------------------------------------
#define SOCKET_PROVIDER_TYPE_DEFAULT  "default"
#define SOCKET_PROVIDER_TYPE_OPENSSL  "openssl_provider"

#endif /* LE_SOCKET_COMMON_H */
