// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STRINGENCODER_HPP_
#define COMMONAPI_SOMEIP_STRINGENCODER_HPP_

#include <CommonAPI/SomeIP/Deployment.hpp>
#include <CommonAPI/SomeIP/Types.hpp>
#include <vector>

#if defined(_WIN32) || defined(ANDROID)
    #if !defined(LITTLE_ENDIAN)
    #define LITTLE_ENDIAN 1234
    #endif

    #if !defined(BIG_ENDIAN)
    #define BIG_ENDIAN 4321
    #endif
#endif

namespace CommonAPI {
namespace SomeIP {

typedef std::vector<byte_t> bytes_t;

enum class EncodingStatus
{
    UNKNOWN,
    SUCCESS,
    NOT_ENOUGH_ROOM,
    INVALID_BOM,
    INVALID_LEAD,
    INVALID_CODE_POINT,
    INCOMPLETE_SEQUENCE,
    SEQUENCE_TOO_LONG,
    INVALID_UTF16
};

class StringEncoder {
public:
    bool checkBom(byte_t *&_data, uint32_t &_size, StringEncoding _encoding);

    void utf16To8(const byte_t *_utf16Str, int _endianess, size_t _size, EncodingStatus &_status, byte_t **_result, size_t &_length);
    void utf8To16(const byte_t *_utf8Str, int _endianess, EncodingStatus &_status, byte_t **_result, size_t &_length);

    bool isUtf8Valid(const byte_t *_utf8Str);

private:
    bool isNewSequence(byte_t _byte);
    bool isSurrogate(uint16_t _codePoint);
    bool isCodePointValid(uint32_t _codePoint);
    bool isSequenceTooLong(uint32_t _codePoint, int _size);
    bool nextUtf8(const byte_t **_bytes, EncodingStatus &_status);
    bool nextUtf16(const byte_t **_bytes, int &_index, size_t _length, EncodingStatus &_status);

    uint32_t getByteSequence1(const byte_t _byte, EncodingStatus &_status);
    uint32_t getByteSequence2(const byte_t **_bytes, EncodingStatus &_status);
    uint32_t getByteSequence3(const byte_t **_bytes, EncodingStatus &_status);
    uint32_t getByteSequence4(const byte_t **_bytes, EncodingStatus &_status);

    int getSequenceLength(byte_t _byte);

    uint32_t getNextBytes(const byte_t **_bytes, EncodingStatus &_status);

    bytes_t push(uint32_t _codePoint, EncodingStatus &_status);
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STRINGENCODER_HPP_
