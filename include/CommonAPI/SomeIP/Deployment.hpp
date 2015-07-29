// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_DEPLOYMENT_HPP_
#define COMMONAPI_SOMEIP_DEPLOYMENT_HPP_

#include <cstdint>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Deployment.hpp>

namespace CommonAPI {
namespace SomeIP {

struct EnumerationDeployment : CommonAPI::Deployment<> {
	EnumerationDeployment(uint8_t _width) : width_(_width) {}

	uint8_t width_;
};

enum class StringEncoding {UTF8, UTF16LE, UTF16BE};

struct StringDeployment : CommonAPI::Deployment<> {
	COMMONAPI_EXPORT StringDeployment(uint32_t _stringLength, uint8_t _stringLengthWidth, StringEncoding _stringEncoding)
        : stringLength_(_stringLength),
          stringLengthWidth_(_stringLengthWidth),
          stringEncoding_(_stringEncoding) {};

    uint32_t stringLength_;
    // If stringLengthWidth_ == 0, the length of the string has StringLength bytes.
    // If stringLengthWidth_ == 1, 2 or 4 bytes, stringLength_ is ignored.
    uint8_t stringLengthWidth_;
    StringEncoding stringEncoding_;
};

template<typename... _Types>
struct StructDeployment : CommonAPI::Deployment<_Types...> {
    StructDeployment(uint8_t _structLengthWidth, _Types*... t)
        : CommonAPI::Deployment<_Types...>(t...),
          structLengthWidth_(_structLengthWidth) {};

    // The length field of the struct contains the size of the struct in bytes;
    // The structLengthWidth_ determines the size of the length field; allowed values are 0, 1, 2, 4.
    // 0 means that there is no length field.
    uint8_t structLengthWidth_;
};

template<typename... _Types>
struct VariantDeployment : CommonAPI::Deployment<_Types...> {
    VariantDeployment(uint8_t _unionLengthWidth, uint8_t _unionTypeWidth, bool _unionDefaultOrder, uint32_t _unionMaxLength, _Types*... t)
        : CommonAPI::Deployment<_Types...>(t...),
          unionLengthWidth_(_unionLengthWidth),
          unionTypeWidth_(_unionTypeWidth),
          unionDefaultOrder_(_unionDefaultOrder),
          unionMaxLength_(_unionMaxLength) {};

    // The length field of the union contains the size of the biggest element in the union in bytes;
    // The unionLengthWidth_ determines the size of the length field; allowed values are 0, 1, 2, 4.
    // 0 means that all types in the union have the same size.
    uint8_t unionLengthWidth_;
    // 2^unionTypeWidth_*8 different types in the union.
    uint8_t unionTypeWidth_;
    // True means length field before type field, false means length field after type field.
    bool unionDefaultOrder_;
    // If unionLengthWidth_ == 0, unionMaxLength_ must be set to the size of the biggest contained type.
    uint32_t unionMaxLength_;
};

template<typename _ElementDepl>
struct ArrayDeployment : CommonAPI::ArrayDeployment<_ElementDepl> {
    ArrayDeployment(_ElementDepl *_element, uint32_t _arrayMinLength, uint32_t _arrayMaxLength, uint8_t _arrayLengthWidth)
        : CommonAPI::ArrayDeployment<_ElementDepl>(_element),
          arrayMinLength_(_arrayMinLength),
          arrayMaxLength_(_arrayMaxLength),
          arrayLengthWidth_(_arrayLengthWidth) {}

    uint32_t arrayMinLength_;
    uint32_t arrayMaxLength_;
    // If arrayLengthWidth_ == 0, the array has arrayMaxLength_ elements.
    // If arrayLengthWidth_ == 1, 2 or 4 bytes, arrayMinLength_ and arrayMaxLength_ are taken into account if > 0.
    // If LengthWidth == 0, the array has arrayMaxLength_ elements.
    // If LengthWidth == 1, 2 or 4 bytes, arrayMinLength_ and arrayMaxLength_ are taken into account if > 0.
    uint8_t arrayLengthWidth_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_DEPLOYMENT_HPP_
