// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_OUTPUTSTREAM_HPP_
#define COMMONAPI_OUTPUTSTREAM_HPP_

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
class OutputStream {
public:
    template<class Deployment_>
    OutputStream &writeValue(const bool &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const int8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const int16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const int32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const int64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const uint8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const uint16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const uint32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const uint64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const float &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const double &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const std::string &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_>
    OutputStream &writeValue(const Version &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, typename Base_>
    OutputStream &writeValue(const Enumeration<Base_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, typename... Types_>
    OutputStream &writeValue(const Struct<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, class PolymorphicStruct_>
    OutputStream &writeValue(const std::shared_ptr<PolymorphicStruct_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, typename... Types_>
    OutputStream &writeValue(const Variant<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, typename ElementType_>
    OutputStream &writeValue(const std::vector<ElementType_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
    }

    template<class Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    OutputStream &writeValue(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                             const Deployment_ *_depl = nullptr) {
        return get()->writeValue(_value, _depl);
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
inline OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const bool &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const int8_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const int16_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const int32_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const int64_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const uint8_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const uint16_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const uint32_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const uint64_t &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const float &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const double &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const std::string &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename Type_, typename TypeDepl_>
inline OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const Deployable<Type_, TypeDepl_> &_value) {
    return _output.template writeValue<TypeDepl_>(_value.getValue(), _value.getDepl());
}

template<class Derived_>
inline OutputStream<Derived_>& operator<<(OutputStream<Derived_> &_output, const Version &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename Base_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const Enumeration<Base_> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const Struct<Types_...> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, class PolymorphicStruct_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output,    const std::shared_ptr<PolymorphicStruct_> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const Variant<Types_...> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename ElementType_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output, const std::vector<ElementType_> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

template<class Derived_, typename KeyType_, typename ValueType_, typename HasherType_>
OutputStream<Derived_> &operator<<(OutputStream<Derived_> &_output,
                         const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value) {
    return _output.template writeValue<EmptyDeployment>(_value);
}

} // namespace CommonAPI

#endif // COMMONAPI_OUTPUTSTREAM_HPP_
