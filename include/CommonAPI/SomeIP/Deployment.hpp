// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

template<typename Type_>
struct IntegerDeployment : CommonAPI::Deployment<>  {
    IntegerDeployment(uint8_t _bits)
            : bits_(_bits), hasInvalid_(false) {
    }
    IntegerDeployment(uint8_t _bits, const Type_ &_invalid)
            : bits_(_bits), invalid_(_invalid), hasInvalid_(true) {
    }

    uint8_t bits_;

    Type_ invalid_;
    bool hasInvalid_;
};

template<typename Type_>
struct EnumerationDeployment : CommonAPI::Deployment<> {
    EnumerationDeployment(uint8_t _bits, bool _isSigned)
            : bits_(_bits), isSigned_(_isSigned), hasInvalid_(false) {
    }
    EnumerationDeployment(uint8_t _bits, bool _isSigned, const Type_ &_invalid)
            : bits_(_bits), isSigned_(_isSigned), invalid_(_invalid), hasInvalid_(true) {
    }

    uint8_t bits_;  // represents width (in bits), if no explicit bit deployment is provided
    bool isSigned_;

    Type_ invalid_;
    bool hasInvalid_;
};

enum class StringEncoding { UTF8, UTF16LE, UTF16BE };

struct StringDeployment : CommonAPI::Deployment<> {
    COMMONAPI_EXPORT StringDeployment(uint32_t _stringLength,
            uint8_t _stringLengthWidth, StringEncoding _stringEncoding)
        : stringLength_(_stringLength),
          stringLengthWidth_(_stringLengthWidth),
          stringEncoding_(_stringEncoding) {};

    uint32_t stringLength_;
    // If stringLengthWidth_ == 0, the length of the string has StringLength bytes.
    // If stringLengthWidth_ == 1, 2 or 4 bytes, stringLength_ is ignored.
    uint8_t stringLengthWidth_;
    StringEncoding stringEncoding_;
};

struct ByteBufferDeployment : CommonAPI::Deployment<> {
    ByteBufferDeployment(uint32_t _byteBufferMinLength, uint32_t _byteBufferMaxLength,
              uint8_t _byteBufferLengthWidth = 4)
        : byteBufferMinLength_(_byteBufferMinLength),
          byteBufferMaxLength_(_byteBufferMaxLength),
          byteBufferLengthWidth_(_byteBufferLengthWidth) {}

    uint32_t byteBufferMinLength_; // == 0 means unlimited
    uint32_t byteBufferMaxLength_;
    // If byteBufferLengthWidth_ == 0, the array has byteBufferMaxLength_ elements.
    // If byteBufferLengthWidth_ == 1, 2 or 4 bytes, byteBufferMinLength_ and byteBufferMaxLength_ are taken into account if > 0.
    uint8_t byteBufferLengthWidth_;
};

template<typename... Types_>
struct StructDeployment : CommonAPI::Deployment<Types_...> {
    StructDeployment(uint8_t _structLengthWidth, Types_*... t)
        : CommonAPI::Deployment<Types_...>(t...),
          structLengthWidth_(_structLengthWidth) {};

    // The length field of the struct contains the size of the struct in bytes;
    // The structLengthWidth_ determines the size of the length field; allowed values are 0, 1, 2, 4.
    // 0 means that there is no length field.
    uint8_t structLengthWidth_;
};

template<typename... Types_>
struct VariantDeployment : CommonAPI::Deployment<Types_...> {
    VariantDeployment(uint8_t _unionLengthWidth, uint8_t _unionTypeWidth,
            bool _unionDefaultOrder, uint32_t _unionMaxLength, Types_*... t)
        : CommonAPI::Deployment<Types_...>(t...),
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

template<typename ElementDepl_>
struct ArrayDeployment : CommonAPI::ArrayDeployment<ElementDepl_> {
    ArrayDeployment(ElementDepl_ *_element, uint32_t _arrayMinLength,
            uint32_t _arrayMaxLength, uint8_t _arrayLengthWidth)
        : CommonAPI::ArrayDeployment<ElementDepl_>(_element),
          arrayMinLength_(_arrayMinLength),
          arrayMaxLength_(_arrayMaxLength),
          arrayLengthWidth_(_arrayLengthWidth) {}

    uint32_t arrayMinLength_;
    uint32_t arrayMaxLength_;
    // If arrayLengthWidth_ == 0, the array has arrayMaxLength_ elements.
    // If arrayLengthWidth_ == 1, 2 or 4 bytes, arrayMinLength_ and arrayMaxLength_ are taken into account if > 0.
    uint8_t arrayLengthWidth_;
};

template<typename ElementDepl_, typename ValueDepl_>
struct MapDeployment : CommonAPI::MapDeployment<ElementDepl_, ValueDepl_> {
    MapDeployment(ElementDepl_ *_element, ValueDepl_ *_value,
            uint32_t _mapMinLength, uint32_t _mapMaxLength,
            uint8_t _mapLengthWidth)
        : CommonAPI::MapDeployment<ElementDepl_, ValueDepl_>(_element, _value),
          mapMinLength_(_mapMinLength),
          mapMaxLength_(_mapMaxLength),
          mapLengthWidth_(_mapLengthWidth) {}

    uint32_t mapMinLength_;
    uint32_t mapMaxLength_;
    uint8_t mapLengthWidth_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_DEPLOYMENT_HPP_
