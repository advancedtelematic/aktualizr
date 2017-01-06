// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_STRUCT_HPP_
#define COMMONAPI_STRUCT_HPP_

#include <iostream>
#include <tuple>

namespace CommonAPI {

typedef uint32_t Serial;

template<class Derived_>
class InputStream;

template<class Derived_>
class OutputStream;

template<class Derived_>
class TypeOutputStream;

template<int, class, class, class>
struct StructReader;

template<
    int Index_, class Input_,
    template<class...> class V_, class... Values_,
    template<class...> class D_, class... Depls_>
struct StructReader<Index_, Input_, V_<Values_...>, D_<Depls_...>> {
    void operator()(InputStream<Input_> &_input,
                    V_<Values_...> &_values,
                    const D_<Depls_...> *_depls) {
        StructReader<Index_-1, Input_, V_<Values_...>, D_<Depls_...>>{}(_input, _values, _depls);
        _input.template readValue<>(std::get<Index_>(_values.values_),
                                    (_depls ? std::get<Index_>(_depls->values_) : nullptr));
    }
};

template<
    int Index_, class Input_,
    template<class...> class V_, class... Values_,
    class D_>
struct StructReader<Index_, Input_, V_<Values_...>, D_> {
    void operator()(InputStream<Input_> &_input,
                    V_<Values_...> &_values,
                    const D_ *_depls) {
        StructReader<Index_-1, Input_, V_<Values_...>, D_>{}(_input, _values, _depls);
        _input.template readValue<D_>(std::get<Index_>(_values.values_));
    }
};

template<class Input_,
    template<class...> class V_, class... Values_,
    template<class...> class D_, class... Depls_>
struct StructReader<0, Input_, V_<Values_...>, D_<Depls_...>> {
    void operator()(InputStream<Input_> &_input,
                    V_<Values_...> &_values,
                    const D_<Depls_...> *_depls) {
        _input.template readValue<>(std::get<0>(_values.values_),
                                    (_depls ? std::get<0>(_depls->values_) : nullptr));
    }
};

template<class Input_,
    template<class...> class V_, class... Values_,
    class D_>
struct StructReader<0, Input_, V_<Values_...>, D_> {
    void operator()(InputStream<Input_> &_input,
                    V_<Values_...> &_values,
                    const D_ *_depls) {
        (void)_depls;
        _input.template readValue<D_>(std::get<0>(_values.values_));
    }
};


template< int, class, class, class >
struct StructWriter;

template<
    int Index_, class Output_,
    template<class ...> class V_, class... Values_,
    template <class...> class D_, class... Depls_>
struct StructWriter<Index_, Output_, V_<Values_...>, D_<Depls_...>> {
    void operator()(OutputStream<Output_> &_output,
                    const V_<Values_...> &_values,
                    const D_<Depls_...> *_depls) {
        StructWriter<Index_-1, Output_, V_<Values_...>, D_<Depls_...>>{}(_output, _values, _depls);
        _output.template writeValue<>(std::get<Index_>(_values.values_),
                                      (_depls ? std::get<Index_>(_depls->values_) : nullptr));
    }
};

template<
    int Index_, class Output_,
    template<class...> class V_, class... Values_,
    class D_>
struct StructWriter<Index_, Output_, V_<Values_...>, D_> {
    void operator()(OutputStream<Output_> &_output,
                    const V_<Values_...> &_values,
                    const D_ *_depls) {
        StructWriter<Index_-1, Output_, V_<Values_...>, D_>{}(_output, _values, _depls);
        _output.template writeValue<D_>(std::get<Index_>(_values.values_));
    }
};

template<class Output_,
    template<class...> class V_, class... Values_,
    template<class...> class D_, class... Depls_>
struct StructWriter<0, Output_, V_<Values_...>, D_<Depls_...>> {
    void operator()(OutputStream<Output_> &_output,
                    const V_<Values_...> &_values,
                    const D_<Depls_...> *_depls) {
        _output.template writeValue<>(std::get<0>(_values.values_),
                                      (_depls ? std::get<0>(_depls->values_) : nullptr));
    }
};

template<class Output_,
    template<class...> class V_, class... Values_,
    class D_>
struct StructWriter<0, Output_, V_<Values_...>, D_> {
    void operator()(OutputStream<Output_> &_output,
                    const V_<Values_...> &_values,
                    const D_ *_depls) {
        (void)_depls;
        _output.template writeValue<D_>(std::get<0>(_values.values_));
    }
};

template<class, int,  class, class>
struct StructTypeWriter;

template<int Index_, class  TypeOutput_,
        template<class...> class V_, class... Values_>
struct StructTypeWriter<EmptyDeployment, Index_, TypeOutput_, V_<Values_...>> {
        void operator()(TypeOutputStream<TypeOutput_> &_output,
                        const V_<Values_...> &_values,
                        const EmptyDeployment *_depl = nullptr) {
                StructTypeWriter<EmptyDeployment, Index_-1, TypeOutput_, V_<Values_...>>{}(_output, _values, _depl);
#ifdef WIN32
                _output.writeType(std::get<Index_>(_values.values_), _depl);
#else
                _output.template writeType(std::get<Index_>(_values.values_), _depl);
#endif
        }
};

template<class TypeOutput_,
        template<class...> class V_, class... Values_>
struct StructTypeWriter<EmptyDeployment, 0, TypeOutput_, V_<Values_...>> {
        void operator()(TypeOutputStream<TypeOutput_> &_output,
                        const V_<Values_...> &_values,
                        const EmptyDeployment *_depl = nullptr) {
#ifdef WIN32
                _output.writeType(std::get<0>(_values.values_), _depl);
#else
                _output.template writeType(std::get<0>(_values.values_), _depl);
#endif
        }
};
template<typename Deployment_, int Index_, class TypeOutput_,
        template<class...> class V_, class... Values_>
struct StructTypeWriter<Deployment_, Index_, TypeOutput_, V_<Values_...>> {
        void operator()(TypeOutputStream<TypeOutput_> &_output,
                        const V_<Values_...> &_values,
                        const Deployment_ *_depl = nullptr) {
                StructTypeWriter<Deployment_, Index_-1, TypeOutput_, V_<Values_...>>{}(_output, _values, _depl);
#ifdef WIN32
                _output.writeType(std::get<Index_>(_values.values_),
                                  (_depl ? std::get<Index_>(_depl->values_)
                                         : nullptr));
#else
                _output.template writeType(std::get<Index_>(_values.values_),
                                           (_depl ? std::get<Index_>(_depl->values_)
                                                  : nullptr));
#endif
        }
};

template<typename Deployment_, class TypeOutput_,
        template<class...> class V_, class... Values_>
struct StructTypeWriter<Deployment_, 0, TypeOutput_, V_<Values_...>> {
        void operator()(TypeOutputStream<TypeOutput_> &_output,
                        const V_<Values_...> &_values,
                        const Deployment_ *_depl = nullptr) {
#ifdef WIN32
                _output.writeType(std::get<0>(_values.values_),
                                    (_depl ? std::get<0>(_depl->values_)
                                                  : nullptr));
#else
                _output.template writeType(std::get<0>(_values.values_),
                                    (_depl ? std::get<0>(_depl->values_)
                                                  : nullptr));
#endif
        }
};

// Structures are mapped to a (generated) struct which inherits from CommonAPI::Struct.
// CommonAPI::Struct holds the structured data in a tuple. The generated class provides
// getter- and setter-methods for the structure members.
template<typename... Types_>
struct Struct {
    std::tuple<Types_...> values_;
};

// Polymorphic structs are mapped to an interface that is derived from the base class
// PolymorphicStruct and contain their parameter in a Struct.
struct PolymorphicStruct {
    virtual ~PolymorphicStruct() {};
    virtual Serial getSerial() const = 0;
};

} // namespace CommonAPI

#endif // COMMONAPI_STRUCT_HPP_
