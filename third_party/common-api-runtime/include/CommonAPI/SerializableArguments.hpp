// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SERIALIZABLEARGUMENTS_HPP_
#define COMMONAPI_SERIALIZABLEARGUMENTS_HPP_

#include <CommonAPI/InputStream.hpp>
#include <CommonAPI/OutputStream.hpp>

namespace CommonAPI {

template<class In_, class Out_, typename... Arguments_>
struct SerializableArguments;

template<class In_, class Out_>
struct SerializableArguments<In_, Out_> {
    static bool serialize(OutputStream<Out_> &_output) {
        (void)_output;
        return true;
    }

    static bool deserialize(InputStream<In_> &_input) {
        (void)_input;
        return true;
    }
};

template<class In_, class Out_, typename ArgumentType_>
struct SerializableArguments<In_, Out_, ArgumentType_> {
    static bool serialize(OutputStream<Out_> &_output, const ArgumentType_ &_argument) {
        _output << _argument;
        return !_output.hasError();
    }

    static bool deserialize(InputStream<In_> &_input, ArgumentType_ &_argument) {
        _input >> _argument;
        return !_input.hasError();
    }
};

template <class In_, class Out_, typename ArgumentType_, typename ... Rest_>
struct SerializableArguments<In_, Out_, ArgumentType_, Rest_...> {
    static bool serialize(OutputStream<Out_> &_output, const ArgumentType_ &_argument, const Rest_&... _rest) {
        _output << _argument;
        return !_output.hasError() ?
                    SerializableArguments<In_, Out_, Rest_...>::serialize(_output, _rest...) : false;
    }

    static bool deserialize(InputStream<In_> &_input, ArgumentType_ &_argument, Rest_&... _rest) {
        _input >> _argument;
        return !_input.hasError() ?
                    SerializableArguments<In_, Out_, Rest_...>::deserialize(_input, _rest...) : false;
    }
};

} // namespace CommonAPI

#endif // COMMONAPI_SERIALIZABLE_ARGUMENTS_HPP_
