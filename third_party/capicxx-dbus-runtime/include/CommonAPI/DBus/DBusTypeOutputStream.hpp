// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_DBUS_DBUSTYPEOUTPUTSTREAM_H_
#define COMMONAPI_DBUS_DBUSTYPEOUTPUTSTREAM_H_

#include <CommonAPI/TypeOutputStream.hpp>

namespace CommonAPI {
namespace DBus {

class DBusTypeOutputStream: public TypeOutputStream<DBusTypeOutputStream> {
public:
    DBusTypeOutputStream() : signature_("") {}

    TypeOutputStream &writeType(const bool &, const EmptyDeployment *) {
        signature_.append("b");
        return (*this);
    }

    TypeOutputStream &writeType(const int8_t &, const EmptyDeployment *) {
        signature_.append("y");
        return (*this);
    }

    TypeOutputStream &writeType(const int16_t &, const EmptyDeployment *) {
        signature_.append("n");
        return (*this);
    }

    TypeOutputStream &writeType(const int32_t &, const EmptyDeployment *) {
        signature_.append("i");
        return (*this);
    }

    TypeOutputStream &writeType(const  int64_t &, const EmptyDeployment *) {
        signature_.append("x");
        return (*this);
    }

    TypeOutputStream &writeType(const uint8_t &, const EmptyDeployment *) {
        signature_.append("y");
        return (*this);
    }

    TypeOutputStream &writeType(const uint16_t &, const EmptyDeployment *) {
        signature_.append("q");
        return (*this);
    }

    TypeOutputStream &writeType(const uint32_t &, const EmptyDeployment *) {
        signature_.append("u");
        return (*this);
    }

    TypeOutputStream &writeType(const uint64_t &, const EmptyDeployment *) {
        signature_.append("t");
        return (*this);
    }

    TypeOutputStream &writeType(const float &, const EmptyDeployment *) {
        signature_.append("d");
        return (*this);
    }

    TypeOutputStream &writeType(const double &, const EmptyDeployment *) {
        signature_.append("d");
        return (*this);
    }

    TypeOutputStream &writeType(const std::string &, const StringDeployment *_depl) {
        if ((_depl != nullptr) && _depl->isObjectPath_) {
            signature_.append("o");
        } else {
            signature_.append("s");
        }
        return (*this);
    }

    TypeOutputStream &writeType(const std::string &, const CommonAPI::EmptyDeployment * = nullptr) {
        signature_.append("s");
        return (*this);
    }

    TypeOutputStream &writeType() {
        signature_.append("ay");
        return (*this);
    }

    TypeOutputStream &writeType(const Version &, const EmptyDeployment *) {
        signature_.append("(uu)");
        return (*this);
    }

    TypeOutputStream &writeVersionType() {
        signature_.append("(uu)");
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    TypeOutputStream &writeType(const Struct<Types_...> &_value, const Deployment_ * _depl = nullptr) {
        (void)_value;
        signature_.append("(");
        const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
        StructTypeWriter<Deployment_, itsSize-1, DBusTypeOutputStream, Struct<Types_...>>{}
            (*this, _value, _depl);

        signature_.append(")");
        return (*this);
    }

    template<typename Deployment_, class PolymorphicStruct_>
    TypeOutputStream &writeType(const std::shared_ptr<PolymorphicStruct_> &_value, const Deployment_ * _depl = nullptr) {
        (void)_value;
        signature_.append("(");
        _value->template writeType<>((*this), _depl);
        signature_.append(")");
        return (*this);
    }

    template<typename... Types_>
    TypeOutputStream &writeType(const Variant<Types_...> &_value, const EmptyDeployment *_depl) {
        (void)_value;
        (void)_depl;
        signature_.append("(yv)");
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    TypeOutputStream &writeType(const Variant<Types_...> &_value, const Deployment_ *_depl) {
        (void)_value;
        if (_depl != nullptr && _depl->isDBus_) {
            signature_.append("v");
        } else {
            signature_.append("(yv)");
        }

        return (*this);
    }

    template<typename ElementType_>
    TypeOutputStream &writeType(const std::vector<ElementType_> &_value, const EmptyDeployment * _depl = nullptr) {
        (void)_value;
        signature_.append("a");
        ElementType_ dummyElement;
        writeType(dummyElement, _depl);
        return (*this);
    }

    template<typename Deployment_, typename ElementType_>
    TypeOutputStream &writeType(const std::vector<ElementType_> &_value, const Deployment_ * _depl = nullptr) {
        (void)_value;
        signature_.append("a");
        ElementType_ dummyElement;
        writeType(dummyElement, (_depl ? _depl->elementDepl_ : nullptr)); 
        return (*this);
    }

    template<typename KeyType_, typename ValueType_, typename HasherType_>
    TypeOutputStream &writeType(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value, const EmptyDeployment *_depl = nullptr) {
        (void)_value;
        signature_.append("a{");
        KeyType_ dummyKey;
        writeType(dummyKey, _depl);
        ValueType_ dummyValue;
        writeType(dummyValue, _depl);
        signature_.append("}");
        return (*this);
    }

    template<typename Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    TypeOutputStream &writeType(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value, const Deployment_ *_depl = nullptr) {
        (void)_value;
        signature_.append("a{");
        KeyType_ dummyKey;
        writeType(dummyKey, (_depl ? _depl->key_ : nullptr)); 
        ValueType_ dummyValue;
        writeType(dummyValue, (_depl ? _depl->value_ : nullptr));
        signature_.append("}");
        return (*this);
    }

    inline std::string getSignature() {
        return std::move(signature_);
    }

private:
    std::string signature_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUSTYPEOUTPUTSTREAM_HPP_

