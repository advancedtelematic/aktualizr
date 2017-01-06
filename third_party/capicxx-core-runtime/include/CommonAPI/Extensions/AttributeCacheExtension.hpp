// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_EXTENSIONS_ATTRIBUTE_CACHE_EXTENSION_HPP_
#define COMMONAPI_EXTENSIONS_ATTRIBUTE_CACHE_EXTENSION_HPP_

#include <CommonAPI/CommonAPI.hpp>

#include <memory>
#include <type_traits>

namespace CommonAPI {
namespace Extensions {

namespace AttributeCache {
template<typename AttributeType_>
struct AttributeTraits;

template<typename AttributeType_, bool>
class AttributeCacheExtensionImpl;
}

template <typename AttributeType_>
using AttributeCacheExtension = AttributeCache::AttributeCacheExtensionImpl<AttributeType_,
                                                                            AttributeCache::AttributeTraits<AttributeType_>::observable>;

namespace AttributeCache {

template<typename AttributeType_>
struct AttributeTraits {
    static const bool observable = (std::is_base_of<
                                            CommonAPI::ObservableAttribute<
                                                    typename AttributeType_::ValueType>,
                                            AttributeType_>::value
                                            || std::is_base_of<
                                                    CommonAPI::ObservableReadonlyAttribute<
                                                            typename AttributeType_::ValueType>,
                                                    AttributeType_>::value);
};

template<typename AttributeType_>
class AttributeCacheExtensionImpl<AttributeType_, false> : public CommonAPI::AttributeExtension<
        AttributeType_> {
protected:
    typedef CommonAPI::AttributeExtension<AttributeType_> __baseClass_t;

    typedef typename AttributeType_::ValueType value_t;

public:
    AttributeCacheExtensionImpl(AttributeType_& baseAttribute)
            : CommonAPI::AttributeExtension<AttributeType_>(baseAttribute) {
    }

};

template<typename AttributeType_>
class AttributeCacheExtensionImpl<AttributeType_, true> : public CommonAPI::AttributeExtension<
        AttributeType_> {
    typedef CommonAPI::AttributeExtension<AttributeType_> __baseClass_t;

protected:
    typedef typename AttributeType_::ValueType value_t;
    typedef std::shared_ptr<const value_t> valueptr_t;

public:
    AttributeCacheExtensionImpl(AttributeType_& baseAttribute)
            : CommonAPI::AttributeExtension<AttributeType_>(baseAttribute) {
        auto &event = __baseClass_t::getBaseAttribute().getChangedEvent();
        event.subscribe(
                std::bind(
                        &AttributeCacheExtensionImpl<AttributeType_, true>::onValueUpdate,
                        this, std::placeholders::_1));
    }

    /**
     * @brief getCachedValue Retrieve attribute value from the cache
     * @return The value of the attribute or a null pointer if the value is not
     *         yet available. Retrieving a non-cached value will trigger
     *         retrieval of the value. Changes to the cached value are emitted
     *         via the getChangedEvent.
     */
    valueptr_t getCachedValue() {
        if (cachedValue_) {
            return cachedValue_;
        }

        return nullptr;
    }

    /**
     * @brief getCachedValue Retrieve attribute value from the cache returning a
     *                       default value if the cache was empty.
     * @param errorValue The value to return if the value could not be found in
     *                   the cache.
     * @return The value of the attribute or errorValue.
     */
    valueptr_t getCachedValue(const value_t &errorValue) {
        valueptr_t result = getCachedValue();

        if (!result)
            result = std::make_shared<const value_t>(errorValue);

        return result;
    }

private:

    void valueRetrieved(const CommonAPI::CallStatus &callStatus, value_t t) {
        if (callStatus == CommonAPI::CallStatus::SUCCESS) {
            onValueUpdate(t);
        }
    }

    void onValueUpdate(const value_t& t) {
        if (cachedValue_ && *cachedValue_ == t) {
            return;
        }

        cachedValue_ = std::make_shared<const value_t>(t);
    }

    valueptr_t cachedValue_;
};

} // namespace AttributeCache
} // namespace Extensions
} // namespace CommonAPI

#endif // COMMONAPI_EXTENSIONS_ATTRIBUTE_CACHE_EXTENSION_HPP_
