// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_SERIALIZABLEARGUMENTS_HPP_
#define COMMONAPI_SOMEIP_SERIALIZABLEARGUMENTS_HPP_

#include <CommonAPI/SomeIP/InputStream.hpp>
#include <CommonAPI/SomeIP/OutputStream.hpp>

namespace CommonAPI {
namespace SomeIP {

template < typename... Arguments_ >
struct SerializableArguments;

template <>
struct SerializableArguments<> {
    static inline bool serialize(OutputStream &) {
        return true;
    }

    static inline bool deserialize(InputStream &) {
        return true;
    }
};

template < typename ArgumentType_ >
struct SerializableArguments< ArgumentType_ > {
    static inline bool serialize(OutputStream &_output, const ArgumentType_ &_argument) {
        _output << _argument;
        return !_output.hasError();
    }

    static inline bool deserialize(InputStream &_input, ArgumentType_ &_argument) {
        _input >> _argument;
        return !_input.hasError();
    }
};

template < typename ArgumentType_, typename ... Rest_ >
struct SerializableArguments< ArgumentType_, Rest_... > {
    static inline bool serialize(OutputStream &_output, const ArgumentType_ &_argument, const Rest_ &... _rest) {
        _output << _argument;
        const bool success = !_output.hasError();
        return success ? SerializableArguments<Rest_...>::serialize(_output, _rest...) : false;
    }

    static inline bool deserialize(InputStream &_input, ArgumentType_ &_argument, Rest_ &... _rest) {
        _input >> _argument;
        const bool success = !_input.hasError();
        return success ? SerializableArguments<Rest_...>::deserialize(_input, _rest...) : false;
    }
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_SERIALIZABLEARGUMENTS_HPP_
