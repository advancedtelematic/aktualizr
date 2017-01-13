// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#include <CommonAPI/Deployable.hpp>
#include <CommonAPI/Deployment.hpp>

#ifndef COMMONAPI_VARIANT_HPP_
#define COMMONAPI_VARIANT_HPP_

namespace CommonAPI {

template<class Derived_>
class InputStream;

template<class Derived_>
class OutputStream;

template<typename... Types_>
struct MaxSize;

template<>
struct MaxSize<> {
    static const unsigned int value = 0;
};

template<typename Type_, typename... Types_>
struct MaxSize<Type_, Types_...> {
    static const unsigned int current_type_size = sizeof(Type_);
    static const unsigned int next_type_size = MaxSize<Types_...>::value;
    static const unsigned int value =
                    current_type_size > next_type_size ?
                                    current_type_size : next_type_size;
};

template<typename SearchType_, typename... RestTypes_>
struct VariantTypeSelector;

template<typename SearchType_, typename... RestTypes_>
struct VariantTypeSelector<SearchType_, SearchType_, RestTypes_...> {
    typedef SearchType_ type;
};

/**
 * \brief A templated generic variant class which provides type safe access and operators
 *
 * A templated generic variant class which provides type safe access and operators
 */
template<typename... Types_>
class Variant {
private:
    typedef std::tuple_size<std::tuple<Types_...>> TypesTupleSize;

public:

    static const unsigned int maxSize = MaxSize<Types_...>::value;

    /**
     * \brief Construct an empty variant
     *
     * Construct an empty variant
     */
    Variant();


    /**
     * \brief Copy constructor. Must have identical templates.
     *
     * Copy constructor. Must have identical templates.
     *
     * @param _source Variant to copy
     */
    Variant(const Variant &_source);

    /**
     * \brief Copy constructor. Must have identical templates.
     *
     * Copy constructor. Must have identical templates.
     *
     * @param _source Variant to copy
     */
    Variant(Variant &&_source);

    ~Variant();

    /**
      * \brief Assignment of another variant. Must have identical templates.
      *
      * Assignment of another variant. Must have identical templates.
      *
      * @param _source Variant to assign
      */
    Variant &operator=(const Variant &_source);
    /**
     * \brief Assignment of another variant. Must have identical templates.
     *
     * Assignment of another variant. Must have identical templates.
     *
     * @param _source Variant to assign
     */
    Variant &operator=(Variant &&_source);

    /**
     * \brief Assignment of a contained type. Must be one of the valid templated types.
     *
     * Assignment of a contained type. Must be one of the valid templated types.
     *
     * @param _value Value to assign
     */
    template<typename Type_>
    typename std::enable_if<!std::is_same<Type_, Variant<Types_...>>::value, Variant<Types_...>&>::type
    operator=(const Type_ &_value);

    /**
     * \brief Equality of another variant. Must have identical template list and content.
     *
     * Equality of another variant. Must have identical template list and content.
     *
     * @param _other Variant to compare
     */
    bool operator==(const Variant<Types_...> &_other) const;

    /**
      * \brief Not-Equality of another variant. Must have identical template list and content.
      *
      * Not-Equality of another variant. Must have identical template list and content.
      *
      * @param _other Variant to compare
      */
    bool operator!=(const Variant<Types_...> &_other) const;

    /**
      * \brief Testif the contained type is the same as the template on this method.
      *
      * Testif the contained type is the same as the template on this method.
      *
      * @return Is same type
      */
    template<typename Type_>
    bool isType() const;

    /**
     * \brief Construct variant with content type set to value.
     *
     * Construct variant with content type set to value.
     *
     * @param _value Value to place
     */
    template<typename Type_>
    Variant(const Type_ &_value,
            typename std::enable_if<!std::is_const<Type_>::value>::type* = 0,
            typename std::enable_if<!std::is_reference<Type_>::value>::type* = 0,
            typename std::enable_if<!std::is_same<Type_, Variant>::value>::type* = 0);

    /**
     * \brief Construct variant with content type set to value.
     *
     * Construct variant with content type set to value.
     *
     * @param _value Value to place
     */
    template<typename Type_>
    Variant(Type_ &&_value,
            typename std::enable_if<!std::is_const<Type_>::value>::type* = 0,
            typename std::enable_if<!std::is_reference<Type_>::value>::type* = 0,
            typename std::enable_if<!std::is_same<Type_, Variant>::value>::type* = 0);

    /**
     * \brief Get value of variant, template to content type. Throws exception if type is not contained.
     *
     * Get value of variant, template to content type. Throws exception if type is not contained.
     */
    template<typename Type_>
    const Type_ &get() const;

    /**
     * \brief Get index in template list of type actually contained, starting at 1 at the end of the template list
     *
     * Get index in template list of type actually contained, starting at 1 at the end of the template list
     *
     * @return Index of contained type
     */
    uint8_t getValueType() const {
        return valueType_;
    }

    uint8_t getMaxValueType() const {
        return uint8_t(TypesTupleSize::value);
    }

private:
    template<typename Type_>
    void set(const Type_ &_value, const bool clear);

    template<typename Type_>
    void set(Type_ &&_value, const bool clear);

    template<typename FriendType_>
    friend struct TypeWriter;
    template<typename ... FriendTypes_>
    friend struct AssignmentVisitor;
    template<typename FriendType_>
    friend struct TypeEqualsVisitor;
    template<typename ... FriendTypes_>
    friend struct PartialEqualsVisitor;
    template<class Derived_, typename ... FriendTypes_>
    friend struct InputStreamReadVisitor;
    template<class Variant_, typename ... FTypes_>
    friend struct ApplyVoidIndexVisitor;

public:
    inline bool hasValue() const {
        return (valueType_ <= TypesTupleSize::value);
    }
    typename std::aligned_storage<maxSize>::type valueStorage_;

    uint8_t valueType_;
};

template<class Variant_, typename... Types_>
struct ApplyVoidIndexVisitor;

template<class Variant_>
struct ApplyVoidIndexVisitor<Variant_> {
    static const uint8_t index = 0;

    static
    void visit(Variant_ &, uint8_t &) {
        assert(false);
    }
};

template<class Variant_, typename Type_, typename... Types_>
struct ApplyVoidIndexVisitor<Variant_, Type_, Types_...> {
    static const uint8_t index
        = ApplyVoidIndexVisitor<Variant_, Types_...>::index + 1;

    static void visit(Variant_ &_variant, uint8_t &_index) {
        if (index == _index) {
            new (&_variant.valueStorage_) Type_();
            _variant.valueType_ = index;
        } else {
            ApplyVoidIndexVisitor<
                Variant_, Types_...
            >::visit(_variant, _index);
        }
    }
};

template<class Visitor_, class Variant_, typename... Types_>
struct ApplyVoidVisitor;

template<class Visitor_, class Variant_>
struct ApplyVoidVisitor<Visitor_, Variant_> {
    static const uint8_t index = 0;

    static
    void visit(Visitor_ &, Variant_ &) {
        assert(false);
    }

    static
    void visit(Visitor_ &, const Variant_ &) {
        assert(false);
    }
};

template<class Visitor_, class Variant_, typename Type_, typename ... Types_>
struct ApplyVoidVisitor<Visitor_, Variant_, Type_, Types_...> {
    static const uint8_t index
        = ApplyVoidVisitor<Visitor_, Variant_, Types_...>::index + 1;

    static void visit(Visitor_ &_visitor, Variant_ &_variant) {
        if (_variant.getValueType() == index) {
            _visitor(_variant.template get<Type_>());
        } else {
            ApplyVoidVisitor<
                Visitor_, Variant_, Types_...
            >::visit(_visitor, _variant);
        }
    }

    static void visit(Visitor_ &_visitor, const Variant_ &_variant) {
        if (_variant.getValueType() == index) {
            _visitor(_variant.template get<Type_>());
        } else {
            ApplyVoidVisitor<
                Visitor_, Variant_, Types_...
            >::visit(_visitor, _variant);
        }
    }
};

template<class Visitor_, class Variant_, typename ... Types_>
struct ApplyBoolVisitor;

template<class Visitor_, class Variant_>
struct ApplyBoolVisitor<Visitor_, Variant_> {
    static const uint8_t index = 0;

    static bool visit(Visitor_ &, Variant_ &) {
        assert(false);
        return false;
    }
};

template<class Visitor_, class Variant_, typename Type_, typename ... Types_>
struct ApplyBoolVisitor<Visitor_, Variant_, Type_, Types_...> {
    static const uint8_t index
        = ApplyBoolVisitor<Visitor_, Variant_, Types_...>::index + 1;

    static bool visit(Visitor_ &_visitor, Variant_ &_variant) {
        if (_variant.getValueType() == index) {
            return _visitor(_variant.template get<Type_>());
        } else {
            return ApplyBoolVisitor<
                        Visitor_, Variant_, Types_...
                   >::visit(_visitor, _variant);
        }
    }
};

template<class Visitor_, class Variant_, class Deployment_, typename ... Types_>
struct ApplyStreamVisitor;

template<class Visitor_, class Variant_, class Deployment_>
struct ApplyStreamVisitor<Visitor_, Variant_, Deployment_> {
    static const uint8_t index = 0;

    static
    void visit(Visitor_ &, Variant_ &, const Deployment_ *) {
        assert(false);
    }

    static
    void visit(Visitor_ &, const Variant_ &, const Deployment_ *) {
        assert(false);
    }
};

template<class Visitor_, class Variant_, class Deployment_, typename Type_, typename ... Types_>
struct ApplyStreamVisitor<Visitor_, Variant_, Deployment_, Type_, Types_...> {
    static const uint8_t index
        = ApplyStreamVisitor<Visitor_, Variant_, Deployment_, Types_...>::index + 1;

    static void visit(Visitor_ &_visitor, Variant_ &_variant, const Deployment_ *_depl) {
        if (_variant.getValueType() == index) {
            _visitor(_variant.template get<Type_>(),
                     (_depl ? std::get<std::tuple_size<decltype(_depl->values_)>::value-index>(_depl->values_)
                            : nullptr));
        } else {
            ApplyStreamVisitor<
                Visitor_, Variant_, Deployment_, Types_...
            >::visit(_visitor, _variant, _depl);
        }
    }

    static void visit(Visitor_ &_visitor, const Variant_ &_variant, const Deployment_ *_depl) {
        if (_variant.getValueType() == index) {
            _visitor(_variant.template get<Type_>(),
                     (_depl ? std::get<std::tuple_size<decltype(_depl->values_)>::value-index>(_depl->values_)
                            : nullptr));
        } else {
            ApplyStreamVisitor<
                Visitor_, Variant_, Deployment_, Types_...
            >::visit(_visitor, _variant, _depl);
        }
    }
};

template<uint32_t Size_>
struct DeleteVisitor {
public:
    DeleteVisitor(typename std::aligned_storage<Size_>::type &_storage)
        : storage_(_storage) {
    }

    template<typename Type_>
    void operator()(const Type_ &) const {
        (reinterpret_cast<const Type_ *>(&storage_))->~Type_();
    }

private:
    typename std::aligned_storage<Size_>::type &storage_;
};

template<class Derived_>
struct OutputStreamWriteVisitor {
public:
    OutputStreamWriteVisitor(OutputStream<Derived_> &_output)
        : output_(_output) {
    }

    template<typename Type_, typename Deployment_ = EmptyDeployment>
    void operator()(const Type_ &_value, const Deployment_ *_depl = nullptr) const {
        Deployable<Type_, Deployment_> itsValue(_value, _depl);
        output_ << itsValue;
    }

private:
    OutputStream<Derived_> &output_;
};

template<class Derived_, typename ... Types_>
struct InputStreamReadVisitor {
public:
    InputStreamReadVisitor(InputStream<Derived_> &_input, Variant<Types_...> &_target)
        : input_(_input), target_(_target) {
    }

    template<typename Type_, typename Deployment_ = EmptyDeployment>
    void operator()(const Type_ &_value, const Deployment_ *_depl = nullptr) {
        (void)_value;
        Deployable<Type_, Deployment_> itsValue(_depl);
        input_ >> itsValue;
        target_.Variant<Types_...>::template set<Type_>(std::move(itsValue.getValue()), false);
    }

private:
    InputStream<Derived_> &input_;
    Variant<Types_...> &target_;
};

template<class Derived_>
struct TypeOutputStreamWriteVisitor {
public:
    TypeOutputStreamWriteVisitor(Derived_ &_output)
        : output_(_output) {
    }

    template<typename Type_, typename Deployment_ = EmptyDeployment>
    void operator()(const Type_ &_value, const Deployment_ *_depl = nullptr) const {
        Deployable<Type_, Deployment_> _itsValue(_value, _depl);
        output_ << _itsValue;
    }

private:
    Derived_ &output_;
};

template<typename Type_>
struct TypeEqualsVisitor
{
public:
    TypeEqualsVisitor(const Type_ &_me)
        : me_(_me) {
    }

    bool operator()(const Type_ &_other) const {
        return (me_ == _other);
    }

    template<typename OtherType_>
    bool operator()(const OtherType_ &) const {
        return false;
    }

private:
    const Type_& me_;
};

template<typename ... Types_>
struct PartialEqualsVisitor
{
public:
    PartialEqualsVisitor(const Variant<Types_...> &_me)
        : me_(_me) {
    }

    template<typename Type_>
    bool operator()(const Type_ &_other) const {
        TypeEqualsVisitor<Type_> visitor(_other);
        return ApplyBoolVisitor<
                    TypeEqualsVisitor<Type_>, const Variant<Types_...>, Types_...
               >::visit(visitor, me_);
    }

private:
    const Variant<Types_...> &me_;
};

template<typename ... Types_>
struct AssignmentVisitor {
public:
    AssignmentVisitor(Variant<Types_...> &_me, const bool _clear = true)
        : me_(_me), clear_(_clear) {
    }

    template<typename Type_>
    void operator()(const Type_ &_value) const {
        me_.Variant<Types_...>::template set<Type_>(_value, clear_);
    }

    template<typename Type_>
    void operator()(Type_ &_value) const {
        me_.Variant<Types_...>::template set<Type_>(_value, clear_);
    }

private:
    Variant<Types_...> &me_;
    const bool clear_;
};

template<typename... Types_>
struct TypeSelector;

template<typename Type_>
struct TypeSelector<Type_> {
};

template<typename Type_, typename... Types_>
struct TypeSelector<typename Type_::Literal, Type_, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<typename Type_::Literal &, Type_, Types_...> {
    typedef Type_ type;
};


template<typename Type_, typename... Types_>
struct TypeSelector<typename Type_::Literal, const Type_&, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<const typename Type_::Literal &, Type_, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_, Type_, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_, Type_ &, Types_...> {
    typedef Type_& type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_ &, Type_, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_, const Type_ &, Types_...> {
    typedef const Type_ &type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<const Type_&, Type_, Types_...> {
    typedef Type_ type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_*, const Type_*, Types_...> {
    typedef const Type_ *type;
};

template<typename Type_, typename... Types_>
struct TypeSelector<Type_ &, const Type_ &, Types_...> {
    typedef const Type_ &type;
};

template<typename U_, typename Type_, typename... Types_>
struct TypeSelector<U_, Type_, Types_...> {
    typedef typename TypeSelector<U_, Types_...>::type type;
};

template<typename... Types_>
struct TypeIndex;

template<>
struct TypeIndex<> {
    static const uint8_t index = 0;

    template<typename U_>
    static uint8_t get() {
        return 0;
    }
};

template<typename Type_, typename ... Types_>
struct TypeIndex<Type_, Types_...> {
    static const uint8_t index = TypeIndex<Types_...>::index + 1;

    template<typename U_>
    static uint8_t get(typename std::enable_if<std::is_same<Type_, U_>::value>::type* = 0) {
        return index;
    }

    template<typename U_>
    static uint8_t get(typename std::enable_if<!std::is_same<Type_, U_>::value>::type* = 0) {
        return TypeIndex<Types_...>::template get<U_>();
    }
};

template<typename... Types_>
Variant<Types_...>::Variant()
    : valueType_(TypesTupleSize::value) {
    ApplyVoidIndexVisitor<Variant<Types_...>, Types_...>::visit(*this, valueType_);
}

template<typename... Types_>
Variant<Types_...>::Variant(const Variant &_source) {
    AssignmentVisitor<Types_...> visitor(*this, false);
    ApplyVoidVisitor<
        AssignmentVisitor<Types_...> , Variant<Types_...>, Types_...
    >::visit(visitor, _source);
}

template<typename... Types_>
Variant<Types_...>::Variant(Variant &&_source)
{
    AssignmentVisitor<Types_...> visitor(*this, false);
    ApplyVoidVisitor<
        AssignmentVisitor<Types_...> , Variant<Types_...>, Types_...
    >::visit(visitor, _source);
}

template<typename... Types_>
Variant<Types_...>::~Variant() {
    if (hasValue()) {
        DeleteVisitor<maxSize> visitor(valueStorage_);
        ApplyVoidVisitor<
            DeleteVisitor<maxSize>, Variant<Types_...>, Types_...
        >::visit(visitor, *this);
    }
}

template<typename... Types_>
Variant<Types_...>& Variant<Types_...>::operator=(const Variant<Types_...> &_source) {
    AssignmentVisitor<Types_...> visitor(*this, hasValue());
    ApplyVoidVisitor<
        AssignmentVisitor<Types_...>, Variant<Types_...>, Types_...
    >::visit(visitor, _source);
    return *this;
}

template<typename... Types_>
Variant<Types_...>& Variant<Types_...>::operator=(Variant<Types_...> &&_source) {
    AssignmentVisitor<Types_...> visitor(*this, hasValue());
    ApplyVoidVisitor<
        AssignmentVisitor<Types_...>, Variant<Types_...>, Types_...
    >::visit(visitor, _source);
    return *this;
}

template<typename ... Types_>
template<typename Type_>
typename std::enable_if<!std::is_same<Type_, Variant<Types_...>>::value, Variant<Types_...>&>::type
Variant<Types_...>::operator=(const Type_ &_value) {
    set<typename TypeSelector<Type_, Types_...>::type>(_value, hasValue());
    return *this;
}

template<typename ... Types_>
template<typename Type_>
bool Variant<Types_...>::isType() const {
    typedef typename TypeSelector<Type_, Types_...>::type selected_type_t;
    uint8_t itsType = TypeIndex<Types_...>::template get<selected_type_t>();
    if (itsType == valueType_) {
        return true;
    } else {
        return false;
    }
}

template<typename ... Types_>
template<typename Type_>
Variant<Types_...>::Variant(const Type_ &_value,
                            typename std::enable_if<!std::is_const<Type_>::value>::type*,
                            typename std::enable_if<!std::is_reference<Type_>::value>::type*,
                            typename std::enable_if<!std::is_same<Type_, Variant<Types_...>>::value>::type*) {
    set<typename TypeSelector<Type_, Types_...>::type>(_value, false);
}

template<typename ... Types_>
template<typename Type_>
Variant<Types_...>::Variant(Type_ &&_value,
typename std::enable_if<!std::is_const<Type_>::value>::type*,
typename std::enable_if<!std::is_reference<Type_>::value>::type*,
typename std::enable_if<!std::is_same<Type_, Variant<Types_...>>::value>::type*) {
    set<typename TypeSelector<Type_, Types_...>::type>(std::move(_value), false);
}


template<typename ... Types_>
template<typename Type_>
const Type_ & Variant<Types_...>::get() const {
    typedef typename TypeSelector<Type_, Types_...>::type selected_type_t;
    uint8_t itsType = TypeIndex<Types_...>::template get<selected_type_t>();
    if (itsType == valueType_) {
        return *(reinterpret_cast<const Type_ *>(&valueStorage_));
    } else {
#ifdef __EXCEPTIONS
        std::bad_cast toThrow;
        throw toThrow;
#else
        printf("SerializableVariant.hpp:%i %s: Incorrect access to variant; attempting to get type not currently contained", __LINE__, __FUNCTION__);
        abort();
#endif
    }
}


template<typename ... Types_>
template<typename U_>
void Variant<Types_...>::set(const U_ &_value, const bool _clear) {
    typedef typename TypeSelector<U_, Types_...>::type selected_type_t;

    if (_clear) {
        DeleteVisitor<maxSize> visitor(valueStorage_);
        ApplyVoidVisitor<
            DeleteVisitor<maxSize>, Variant<Types_...>, Types_...
        >::visit(visitor, *this);
    }
    new (&valueStorage_) selected_type_t(std::move(_value));
    valueType_ = TypeIndex<Types_...>::template get<selected_type_t>();
}

template<typename ... Types_>
template<typename U_>
void Variant<Types_...>::set(U_ &&_value, const bool _clear) {
    typedef typename TypeSelector<U_, Types_...>::type selected_type_t;

    selected_type_t&& any_container_value = std::move(_value);
    if(_clear)
    {
        DeleteVisitor<maxSize> visitor(valueStorage_);
        ApplyVoidVisitor<
            DeleteVisitor<maxSize>, Variant<Types_...>, Types_...
        >::visit(visitor, *this);
    } else {
        new (&valueStorage_) selected_type_t(std::move(any_container_value));
    }

    valueType_ = TypeIndex<Types_...>::template get<selected_type_t>();
}

template<typename ... Types_>
bool Variant<Types_...>::operator==(const Variant<Types_...> &_other) const
{
    PartialEqualsVisitor<Types_...> visitor(*this);
    return ApplyBoolVisitor<
                PartialEqualsVisitor<Types_...>, const Variant<Types_...>, Types_...
           >::visit(visitor, _other);
}

template<typename ... Types_>
bool Variant<Types_...>::operator!=(const Variant<Types_...> &_other) const
{
    return !(*this == _other);
}

} // namespace CommonAPI

#endif // COMMONAPI_VARIANT_HPP_
