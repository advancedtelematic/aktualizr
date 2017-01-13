// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_ENUMERATION_HPP_
#define COMMONAPI_ENUMERATION_HPP_

namespace CommonAPI {

template<typename Base_>
struct Enumeration {
    Enumeration() = default;
    Enumeration(const Base_ &_value) :
            value_(_value) {
    }

    virtual ~Enumeration() {}

    inline Enumeration &operator=(const Base_ &_value) {
        value_ = _value;
        return (*this);
    }

    inline operator const Base_() const {
        return value_;
    }

    inline bool operator==(const Enumeration<Base_> &_other) const {
        return (value_ == _other.value_);
    }

    inline bool operator!=(const Enumeration<Base_> &_other) const {
        return (value_ != _other.value_);
    }

    inline bool operator<(const Enumeration<Base_> &_other) const {
        return (value_ < _other.value_);
    }

    inline bool operator<=(const Enumeration<Base_> &_other) const {
        return (value_ <= _other.value_);
    }

    inline bool operator>(const Enumeration<Base_> &_other) const {
        return (value_ > _other.value_);
    }

    inline bool operator>=(const Enumeration<Base_> &_other) const {
        return (value_ >= _other.value_);
    }

    virtual bool validate() const = 0;

    Base_ value_;
};

} // namespace CommonAPI

#endif // COMMONAPI_ENUMERATION_HPP_
