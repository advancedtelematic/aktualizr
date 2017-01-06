// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_TYPEOUTPUTSTREAM_HPP_
#define COMMONAPI_TYPEOUTPUTSTREAM_HPP_

#include <unordered_map>

#include <CommonAPI/Struct.hpp>
#include <CommonAPI/Variant.hpp>
#include <CommonAPI/Types.hpp>

namespace CommonAPI {

template<class Derived_>
class TypeOutputStream {
public:
    template<class Deployment_>
    inline TypeOutputStream &writeType(const bool &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const int8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const int16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const int32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const int64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const uint8_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const uint16_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const uint32_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const uint64_t &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const float &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const double &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const std::string &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_>
    inline TypeOutputStream &writeType(const Version &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_, typename Type_>
    TypeOutputStream &writeType(const Enumeration<Type_> &_value, const Deployment_ *_depl = nullptr) {
        (void)_value;
        Type_ tmpValue;
        return get()->writeType(tmpValue, _depl);
    }

    template<class Deployment_, typename... Types_>
    TypeOutputStream &writeType(const Struct<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_, class PolymorphicStruct_>
    TypeOutputStream &writeType(const std::shared_ptr<PolymorphicStruct_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_, typename... Types_>
    TypeOutputStream &writeType(const Variant<Types_...> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_, typename ElementType_>
    TypeOutputStream &writeType(const std::vector<ElementType_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

    template<class Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    TypeOutputStream &writeType(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value, const Deployment_ *_depl = nullptr) {
        return get()->writeType(_value, _depl);
    }

private:
    inline Derived_ *get() {
        return static_cast<Derived_ *>(this);
    }
};


template<class Derived_>
inline TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const bool &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const int8_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const int16_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const int32_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const int64_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const uint8_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const uint16_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const uint32_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const uint64_t &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const float &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const double &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const std::string &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, typename Type_, typename TypeDepl_>
inline TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const Deployable<Type_, TypeDepl_> &_value) {
    return _output.template writeType<TypeDepl_>(_value.getValue(), _value.getDepl());
}

template<class Derived_>
inline TypeOutputStream<Derived_>& operator<<(TypeOutputStream<Derived_> &_output, const Version &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const Struct<Types_...> &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, class PolymorphicStruct_>
TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const std::shared_ptr<PolymorphicStruct_> &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, typename... Types_>
TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const Variant<Types_...> &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, typename ElementType_>
TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output, const std::vector<ElementType_> &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

template<class Derived_, typename KeyType_, typename ValueType_, typename HasherType_>
TypeOutputStream<Derived_> &operator<<(TypeOutputStream<Derived_> &_output,
                                          const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value) {
    return _output.template writeType<EmptyDeployment>(_value);
}

} // namespace CommonAPI

#endif // COMMONAPI_TYPEOUTPUTSTREAM_HPP_
