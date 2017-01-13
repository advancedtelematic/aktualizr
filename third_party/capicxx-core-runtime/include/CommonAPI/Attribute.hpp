// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_ATTRIBUTE_HPP_
#define COMMONAPI_ATTRIBUTE_HPP_

#include <cstdint>
#include <functional>
#include <future>
#include <memory>

#include <CommonAPI/CallInfo.hpp>
#include <CommonAPI/Event.hpp>
#include <CommonAPI/Types.hpp>

namespace CommonAPI {

/**
 * \brief Class representing a read only attribute
 *
 * Class representing a read only attribute
 */
template <typename ValueType_>
class ReadonlyAttribute {
 public:
    typedef ValueType_ ValueType;

    typedef std::function<void(const CallStatus &, ValueType_)> AttributeAsyncCallback;

    virtual ~ReadonlyAttribute() { }

    /**
     * \brief Get value of attribute, usually from remote. Synchronous call.
     *
     * Get value of attribute, usually from remote. Synchronous call.
     *
     * @param value Reference to be filled with value.
     * @param callStatus call status reference will be filled with status of the operation
     */
    virtual void getValue(CallStatus &_status,
                          ValueType_ &_value,
                          const CallInfo *_info = nullptr) const = 0;

    /**
     * \brief Get value of attribute, usually from remote. Asynchronous call.
     *
     * Get value of attribute, usually from remote. Asynchronous call.
     *
     * @param attributeAsyncCallback std::function object for the callback to be invoked.
     * @return std::future containing the call status of the operation.
     */
    virtual std::future<CallStatus> getValueAsync(AttributeAsyncCallback attributeAsyncCallback,
                                                  const CallInfo *_info = nullptr) = 0;
};

/**
 * \brief Class representing a read and writable attribute
 *
 * Class representing a read and writable attribute
 */
template <typename ValueType_>
class Attribute: public ReadonlyAttribute<ValueType_> {
 public:
    typedef typename ReadonlyAttribute<ValueType_>::ValueType ValueType;
    typedef typename ReadonlyAttribute<ValueType_>::AttributeAsyncCallback AttributeAsyncCallback;

    virtual ~Attribute() { }

    /**
     * \brief Set value of attribute, usually to remote. Synchronous call.
     *
     * Set value of attribute, usually to remote. Synchronous call.
     *
     * @param requestValue Value to be set
     * @param callStatus call status reference will be filled with status of the operation
     * @param responseValue Reference which will contain the actuall value set by the remote.
     */
    virtual void setValue(const ValueType_& requestValue,
                          CallStatus& callStatus,
                          ValueType_& responseValue,
                          const CallInfo *_info = nullptr) = 0;

    /**
     * \brief Set value of attribute, usually to remote. Asynchronous call.
     *
     * Set value of attribute, usually to remote. Asynchronous call.
     *
     * @param requestValue Value to be set
     * @param attributeAsyncCallback std::function object for the callback to be invoked.
     * @return std::future containing the call status of the operation.
     */
    virtual std::future<CallStatus> setValueAsync(const ValueType_& requestValue,
                                                  AttributeAsyncCallback attributeAsyncCallback,
                                                  const CallInfo *_info = nullptr) = 0;
};

/**
 * \brief Class representing an observable attribute
 *
 * Class representing an observable attribute
 */
template <typename AttributeBaseClass_>
class ObservableAttributeImpl: public AttributeBaseClass_ {
 public:
    typedef typename AttributeBaseClass_::ValueType ValueType;
    typedef typename AttributeBaseClass_::AttributeAsyncCallback AttributeAsyncCallback;
    typedef Event<ValueType> ChangedEvent;

    virtual ~ObservableAttributeImpl() { }

    /**
     * \brief Returns the event handler for the remote change notification event
     *
     * Returns the event handler for the remote change notification event
     *
     * @return The event handler object
     */
    virtual ChangedEvent& getChangedEvent() = 0;
};

template <typename ValueType_>
struct ObservableReadonlyAttribute: ObservableAttributeImpl< ReadonlyAttribute<ValueType_> > {
};

template <typename ValueType_>
struct ObservableAttribute: ObservableAttributeImpl< Attribute<ValueType_> > {
};

#ifdef WIN32
struct WINDummyAttribute {
    WINDummyAttribute() {}
};
#endif

} // namespace CommonAPI

#endif // COMMONAPI_ATTRIBUTE_HPP_
