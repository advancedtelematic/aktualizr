// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DEPLOYABLE_HPP_
#define COMMONAPI_DEPLOYABLE_HPP_

namespace CommonAPI {

struct DeployableBase {
};

template<typename Type_, typename TypeDepl_>
struct Deployable : DeployableBase
{
    Deployable(const TypeDepl_ *_depl = nullptr)
        : depl_(const_cast<TypeDepl_ *>(_depl)) {
    }

    Deployable(const Type_ &_value, const TypeDepl_ *_depl)
        : value_(_value),
          depl_(const_cast<TypeDepl_ *>(_depl)) {
    };

    Deployable<Type_, TypeDepl_>& operator=(const Deployable<Type_, TypeDepl_> &_source) {
        value_ = _source.value_;
        depl_ = _source.depl_;
        return (*this);
    }

    operator Type_() const {
        return value_;
    }

    const Type_ &getValue() const {
        return value_;
    }

    Type_ &getValue() {
        return value_;
    }

    const TypeDepl_ *getDepl() const {
        return depl_;
    }

 protected:
    Type_ value_;
    TypeDepl_ *depl_;
};

} // namespace CommonAPI

#endif // COMMONAPI_DEPLOYABLE_HPP_
