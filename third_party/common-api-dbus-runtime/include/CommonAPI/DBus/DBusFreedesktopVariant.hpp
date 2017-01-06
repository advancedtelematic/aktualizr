// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#include <CommonAPI/DBus/DBusTypeOutputStream.hpp>

#ifndef COMMONAPI_DBUS_FREEDESKTOPVARIANT_HPP_
#define COMMONAPI_DBUS_FREEDESKTOPVARIANT_HPP_

namespace CommonAPI {
namespace DBus {
template<class Visitor_, class Variant_, class Deployment_, typename ... Types_>
struct ApplyTypeCompareVisitor;

template<class Visitor_, class Variant_, class Deployment_>
struct ApplyTypeCompareVisitor<Visitor_, Variant_, Deployment_> {
    static const uint8_t index = 0;

    static bool visit(Visitor_&, const Variant_&, const Deployment_ *_depl, uint8_t &typeindex) {
        (void)_depl;
        // will be called only if the variant does not contain the requested type
        typeindex = index;
        return false;
    }
};

template<class Visitor_, class Variant_, class Deployment_, typename Type_, typename ... Types_>
struct ApplyTypeCompareVisitor<Visitor_, Variant_, Deployment_, Type_, Types_...> {
    static const uint8_t index
        = ApplyTypeCompareVisitor<Visitor_, Variant_, Deployment_, Types_...>::index + 1;

    static bool visit(Visitor_ &_visitor, const Variant_ &_variant, const Deployment_ *_depl, uint8_t &typeindex) {
        DBusTypeOutputStream output;
        Type_ current;

        if ((0 < Deployment_::size_) && (_depl)) {
            output.writeType(current, std::get<Deployment_::size_-index>(_depl->values_));
           }
           else {
            output.writeType(current, static_cast<CommonAPI::EmptyDeployment*>(nullptr));
        }

#ifdef WIN32
        if (_visitor.operator()<Type_>(output.getSignature())) {
#else
        if (_visitor.template operator()<Type_>(output.getSignature())) {
#endif
            typeindex = index;
            return true;
        } else {
            return ApplyTypeCompareVisitor<
                        Visitor_, Variant_, Deployment_, Types_...
                   >::visit(_visitor, _variant, _depl, typeindex);
        }
    }
};
template<typename ... Types_>
struct TypeCompareVisitor {
public:
    TypeCompareVisitor(const std::string &_type)
        : type_(_type) {
    }

    template<typename Type_>
    bool operator()(const std::string &_type) const {
        return (_type == type_);
    }

private:
    const std::string type_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_FREEDESKTOPVARIANT_HPP_
