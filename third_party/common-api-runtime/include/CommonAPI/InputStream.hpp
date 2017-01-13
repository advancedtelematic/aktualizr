// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_INPUT_STREAM_HPP_
#define COMMONAPI_INPUT_STREAM_HPP_

#include <unordered_map>

#include <CommonAPI/ByteBuffer.hpp>
#include <CommonAPI/Deployable.hpp>
#include <CommonAPI/Deployment.hpp>
#include <CommonAPI/Enumeration.hpp>
#include <CommonAPI/Struct.hpp>
#include <CommonAPI/Variant.hpp>
#include <CommonAPI/Version.hpp>

namespace CommonAPI {

template<class Derived_>
class InputStream {
public:
    template<class Deployment_>
    InputStream &readValue(bool &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(int8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(int16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(int32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(int64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(uint8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(uint16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(uint32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(uint64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(float &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(double &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(std::string &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, typename Base_>
    InputStream &readValue(Enumeration<Base_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, typename... Types_>
    InputStream &readValue(Struct<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, class PolymorphicStruct_>
    InputStream &readValue(std::shared_ptr<PolymorphicStruct_> &_value,
                           const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, typename... Types_>
    InputStream &readValue(Variant<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, typename ElementType_>
    InputStream &readValue(std::vector<ElementType_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    InputStream &readValue(std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                           const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    template<class Deployment_>
    InputStream &readValue(Version &_value, const Deployment_ *_depl = nullptr) {
        return get()->readValue(_value, _depl);
    }

    bool hasError() const {
        return get()->hasError();
    }

private:
    inline Derived_ *get() {
        return static_cast<Derived_ *>(this);
    }

    inline const Derived_ *get() const {
        return static_cast<const Derived_ *>(this);
    }
};

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, bool &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, int8_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, int16_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, int32_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, int64_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, uint8_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, uint16_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, uint32_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, uint64_t &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, float &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, double &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, std::string &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, Version &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename Base_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, Enumeration<Base_> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, Struct<Types_...> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, class PolymorphicStruct_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, std::shared_ptr<PolymorphicStruct_> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
InputStream<Derived_> & operator>>(InputStream<Derived_> &_input, Variant<Types_...> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename ElementType_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, std::vector<ElementType_> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename KeyType_, typename ValueType_, typename HasherType_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, std::unordered_map<KeyType_, ValueType_, HasherType_> &_value) {
    return _input.template readValue<EmptyDeployment>(_value);
}

template<class Derived_, typename Type_, typename TypeDeployment_>
InputStream<Derived_> &operator>>(InputStream<Derived_> &_input, Deployable<Type_, TypeDeployment_> &_value) {
    return _input.template readValue<TypeDeployment_>(_value.getValue(), _value.getDepl());
}

} // namespace CommonAPI

#endif // COMMONAPI_INPUT_STREAM_HPP_
