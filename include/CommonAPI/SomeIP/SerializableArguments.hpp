// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_SERIALIZABLE_ARGUMENTS_HPP_
#define COMMONAPI_SOMEIP_SERIALIZABLE_ARGUMENTS_HPP_

#include <CommonAPI/SomeIP/InputStream.hpp>
#include <CommonAPI/SomeIP/OutputStream.hpp>

namespace CommonAPI {
namespace SomeIP {

template < typename... _Arguments >
struct SerializableArguments;

template <>
struct SerializableArguments<> {
    static inline bool serialize(OutputStream& outputStream) {
        return true;
    }

    static inline bool deserialize(InputStream& inputStream) {
        return true;
    }
};

template < typename _ArgumentType >
struct SerializableArguments<_ArgumentType > {
    static inline bool serialize(OutputStream& outputStream, const _ArgumentType& argument) {
        outputStream << argument;
        return !outputStream.hasError();
    }

    static inline bool deserialize(InputStream& inputStream, _ArgumentType& argument) {
        inputStream >> argument;
        return !inputStream.hasError();
    }
};

template < typename _ArgumentType, typename ... _Rest >
struct SerializableArguments< _ArgumentType, _Rest... > {
    static inline bool serialize(OutputStream& outputStream, const _ArgumentType& argument, const _Rest&... rest) {
        outputStream << argument;
        const bool success = !outputStream.hasError();
        return success ? SerializableArguments<_Rest...>::serialize(outputStream, rest...) : false;
    }

    static inline bool deserialize(InputStream& inputStream, _ArgumentType& argument, _Rest&... rest) {
        inputStream >> argument;
        const bool success = !inputStream.hasError();
        return success ? SerializableArguments<_Rest...>::deserialize(inputStream, rest...) : false;
    }
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_SERIALIZABLE_ARGUMENTS_HPP_
