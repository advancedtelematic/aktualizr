// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_TYPES_HPP_
#define COMMONAPI_TYPES_HPP_

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <memory>
#include <tuple>

// define CallStatus before including Event.hpp
namespace CommonAPI {
    enum class CallStatus {
        SUCCESS,
        OUT_OF_MEMORY,
        NOT_AVAILABLE,
        CONNECTION_FAILED,
        REMOTE_ERROR,
        UNKNOWN,
        INVALID_VALUE,
        SUBSCRIPTION_REFUSED
    };
} // namespace CommonAPI


#include <CommonAPI/ByteBuffer.hpp>
#include <CommonAPI/ContainerUtils.hpp>
#include <CommonAPI/Event.hpp>
#include <CommonAPI/Export.hpp>
#include <CommonAPI/Version.hpp>

#if  __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#  define COMMONAPI_DEPRECATED __attribute__ ((__deprecated__))
#elif defined(_MSC_VER) && (_MSC_VER >= 1300)
#  define COMMONAPI_DEPRECATED __declspec(deprecated)
#else
#  define COMMONAPI_DEPRECATED
#endif


#ifdef WIN32
#define CCALL __cdecl
#pragma section(".CRT$XCU",read)
#define INITIALIZER(f) \
    static void __cdecl f(void); \
    __declspec(allocate(".CRT$XCU")) void(__cdecl*f##_)(void) = f; \
    static void __cdecl f(void)
#define DEINITIALIZER(f) \
    static void __cdecl f(void); \
    static void _##f##_wrapper(void) { \
        atexit(f); \
    } \
    __declspec(allocate(".CRT$XCU")) void(__cdecl * f##_)(void) = _##f##_wrapper; \
    static void __cdecl f(void)
#else
#define CCALL
#define INITIALIZER(f) \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#define DEINITIALIZER(f) \
    static void f(void) __attribute__((destructor)); \
    static void f(void)
#endif

namespace CommonAPI {

enum class AvailabilityStatus {
    UNKNOWN,
    AVAILABLE,
    NOT_AVAILABLE
};

typedef uint32_t CallId_t;
typedef std::string ConnectionId_t;
typedef int Timeout_t; // in ms, -1 means "forever"
typedef uint32_t Sender_t;

/**
 * \brief Identifies a client sending a call to a stub.
 *
 * The ClientId is used to identify the caller within a stub.
 * The ClientId is supposed to be added by the middleware and can be compared using the == operator.
 */
class ClientId {
public:
    virtual ~ClientId() { }
    virtual bool operator==(ClientId& clientIdToCompare) = 0;
    virtual std::size_t hashCode() = 0;
};

template <typename ... Args_>
struct SelectiveBroadcastFunctorHelper {
    typedef std::function<void(Args_...)> SelectiveBroadcastFunctor;
};

typedef std::unordered_set<
    std::shared_ptr<CommonAPI::ClientId>,
    SharedPointerClientIdContentHash,
    SharedPointerClientIdContentEqual
> ClientIdList;

template<typename EnumType_>
class EnumHasher {
public:
    size_t operator()(const EnumType_& testEnum) const {
        return static_cast<int32_t>(testEnum);
    }
};

// Type identifier for polymorphic structs
typedef uint32_t Serial;

} // namespace CommonAPI

#endif // COMMONAPI_TYPES_HPP_

#if defined(COMMONAPI_TYPES_LOCAL_INCLUDE)
#include COMMONAPI_TYPES_LOCAL_INCLUDE
#endif
