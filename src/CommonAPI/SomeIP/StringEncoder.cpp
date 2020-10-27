// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/StringEncoder.hpp>

namespace CommonAPI {
namespace SomeIP {

const uint16_t HIGH_SURROGATE_LEAD  = 0xd800;  // 1101 1000 0000 0000
const uint16_t LOW_SURROGATE_LEAD   = 0xdc00;  // 1101 1100 0000 0000
const uint16_t SURROGATE_MIN        = 0xd800;
const uint16_t SURROGATE_MAX        = 0xdfff;
const uint32_t CODE_POINT_MAX       = 0x0010ffff;
const uint16_t UNICODE_MAX          = 0xffff;

bool StringEncoder::checkBom(byte_t *&_data, uint32_t &_size, StringEncoding _encoding) {
    bool result(false);
    if (_size > 3) { // BOM + Termination byte(s)
        switch (_encoding) {
        case StringEncoding::UTF8:
            if (_data[0] == 0xEF && _data[1] == 0xBB && _data[2] == 0xBF) {
                _data = _data + 3;
                _size = _size - 3;
                result = true;
            };
            break;
        case StringEncoding::UTF16LE:
            if (_data[0] == 0xFF && _data[1] == 0xFE) {
                _data = _data + 2;
                _size = _size - 2;
                result = true;
            };
            break;
        case StringEncoding::UTF16BE:
            if (_data[0] == 0xFE && _data[1] == 0xFF) {
                _data = _data + 2;
                _size = _size - 2;
                result = true;
            };
            break;
        }
    }

    return result;
}

void StringEncoder::utf16To8(const byte_t *_utf16Str, int _endianess, size_t _size, EncodingStatus &_status, byte_t **_result, size_t &_length)
{
    _status = EncodingStatus::SUCCESS;
    bytes_t bytes;
    int i = 0;
    while (i < int(_size))
    {
        uint32_t firstByte = *_utf16Str & 0xff;

        if (!nextUtf16(&_utf16Str, i, _size, _status))
        {
            *_result = new byte_t[0];
            _length = 0;
            return;
        }

        uint32_t secondByte = *_utf16Str & 0xff;

        uint32_t codePoint = 0;
        if (_endianess == BIG_ENDIAN)
            codePoint = firstByte << 8 | secondByte;
        else if (_endianess == LITTLE_ENDIAN)
            codePoint = secondByte << 8 | firstByte;

        if (isSurrogate(uint16_t(codePoint)))
        {
            uint32_t firstSurrogate = codePoint;
            uint32_t firstSurrogateNoLead = firstSurrogate & 0x3ff; // mask leading surrogate sequence

            if (!nextUtf16(&_utf16Str, i, _size, _status))
            {
                *_result = new byte_t[0];
                _length = 0;
                return;
            }

            uint32_t thirdByte = *_utf16Str & 0xff;

            if (!nextUtf16(&_utf16Str, i, _size, _status))
            {
                *_result = new byte_t[0];
                _length = 0;
                return;
            }

            uint32_t fourthByte = *_utf16Str & 0xff;

            uint32_t secondSurrogate = 0;
            if (_endianess == BIG_ENDIAN)
                secondSurrogate = thirdByte << 8 | fourthByte;
            else if (_endianess == LITTLE_ENDIAN)
                secondSurrogate = fourthByte << 8 | thirdByte;

            if (isSurrogate(uint16_t(secondSurrogate)))
            {
                uint32_t secondSurrogateNoLead = secondSurrogate & 0x3ff;// mask leading surrogate sequence

                codePoint = (firstSurrogateNoLead << 10
                        | secondSurrogateNoLead) + 0x10000;

                bytes_t b = push(codePoint, _status);
                bytes.insert(bytes.end(), b.begin(), b.end());
                nextUtf16(&_utf16Str, i, _size, _status);

            } else
            {
                _status = EncodingStatus::INVALID_UTF16;
                *_result = new byte_t[0];
                _length = 0;
                return;
            }
        } else
        {
            bytes_t b = push(codePoint, _status);
            bytes.insert(bytes.end(), b.begin(), b.end());
            nextUtf16(&_utf16Str, i, _size, _status);
        }
    }

    _length = bytes.size() + 1;
    byte_t *tmp = new byte_t[_length];
    int j=0;
    for(std::vector<byte_t>::iterator it = bytes.begin(); it != bytes.end(); ++it)
    {
        tmp[j] = *it;
        j++;
    }
    tmp[_length-1] = '\0';
    *_result = tmp;
}

void StringEncoder::utf8To16(const byte_t *_utf8Str, int _endianess, EncodingStatus &_status, byte_t **_result, size_t &_length)
{
    _status = EncodingStatus::SUCCESS;

    std::vector<byte_t> bytes;
    while (*_utf8Str != '\0')
    {
        uint32_t codePoint = getNextBytes(&_utf8Str, _status);
        if (codePoint > UNICODE_MAX)
        {
            //code point > Unicode max value --> create high and low surrogate

            //subtract with 65536 --> results in 20 Bits value
            uint32_t base = codePoint - 0x10000;

            //split 20 Bits value. 1-10 bits = part of low surrogate. 11-20 bits = part of high surrogate

            //high surrogate: shift with 10 and add high surrogate lead 110111
            uint16_t highSurrogate = static_cast<uint16_t>((base >> 10)
                    + HIGH_SURROGATE_LEAD);

            //low surrogate: mask last 10 bits and add low surrogate lead 110110
            uint16_t lowSurrogate = static_cast<uint16_t>((base & 0x3ff)
                    + LOW_SURROGATE_LEAD);

            if (_endianess == BIG_ENDIAN)
            {
                bytes.push_back(static_cast<uint8_t>(highSurrogate >> 8));
                bytes.push_back(static_cast<uint8_t>(highSurrogate));
                bytes.push_back(static_cast<uint8_t>(lowSurrogate >> 8));
                bytes.push_back(static_cast<uint8_t>(lowSurrogate));
            } else
            {
                bytes.push_back(static_cast<uint8_t>(highSurrogate ));
                bytes.push_back(static_cast<uint8_t>(highSurrogate >> 8));
                bytes.push_back(static_cast<uint8_t>(lowSurrogate));
                bytes.push_back(static_cast<uint8_t>(lowSurrogate >> 8));
            }
        } else
        {
            if (_endianess == BIG_ENDIAN)
            {
                bytes.push_back(static_cast<uint8_t>(codePoint >> 8));
                bytes.push_back(static_cast<uint8_t>(codePoint));
            } else
            {
                bytes.push_back(static_cast<uint8_t>(codePoint));
                bytes.push_back(static_cast<uint8_t>(codePoint >> 8));
            }
        }
        _utf8Str++;
    }

    _length = bytes.size();
    byte_t *tmp = new byte_t[_length];
    int j=0;
    for(std::vector<byte_t>::iterator it = bytes.begin(); it != bytes.end(); ++it)
    {
        tmp[j] = *it;
        j++;
    }
    *_result = tmp;
}

bool StringEncoder::isUtf8Valid(const byte_t *_utf8Str)
{
    while (*_utf8Str != '\0')
    {
        EncodingStatus status = EncodingStatus::UNKNOWN;
        getNextBytes(&_utf8Str, status);
        if (status != EncodingStatus::SUCCESS)
            return false;
        _utf8Str++;
    }
    return true;
}

bool StringEncoder::isNewSequence(byte_t _byte)
{
    return ((_byte >> 6) == 0x2);
}

bool StringEncoder::isSurrogate(uint16_t _codePoint)
{
    return (_codePoint >= SURROGATE_MIN && _codePoint <= SURROGATE_MAX);
}

bool StringEncoder::isCodePointValid(uint32_t _codePoint)
{
    return (_codePoint <= CODE_POINT_MAX && !isSurrogate(uint16_t(_codePoint)));
}

bool StringEncoder::isSequenceTooLong(uint32_t _codePoint, int _size)
{
    if (_codePoint < 0x80)
    {
        if (_size != 1)
            return true;
    } else if (_codePoint < 0x800)
    {
        if (_size != 2)
            return true;
    } else if (_codePoint < 0x10000)
    {
        if (_size != 3)
            return true;
    }
    return false;
}

bool StringEncoder::nextUtf8(const byte_t **_bytes, EncodingStatus &_status)
{
    (*_bytes)++;
    if (**_bytes == '\0')
    {
        _status = EncodingStatus::NOT_ENOUGH_ROOM;
        return false;
    }

    if (!isNewSequence(**_bytes))
    {
        _status = EncodingStatus::INCOMPLETE_SEQUENCE;
        return false;
    }
    return true;
}

bool StringEncoder::nextUtf16(const byte_t **_bytes, int &_index, size_t _length, EncodingStatus &_status)
{
    (*_bytes)++;
    _index++;
    if (_index > int(_length))
    {
        _status = EncodingStatus::INVALID_UTF16;
        return false;
    }
    return true;
}

uint32_t StringEncoder::getByteSequence1(byte_t _byte, EncodingStatus &_status)
{
    if (_byte == '\0')
    {
        _status = EncodingStatus::NOT_ENOUGH_ROOM;
        return 0;
    }
    return _byte;
}

uint32_t StringEncoder::getByteSequence2(const byte_t **_bytes, EncodingStatus &_status)
{
    // 2 bytes utf8 format: 110x xxxx 10xx xxxx

    if (**_bytes == '\0') {
        _status = EncodingStatus::NOT_ENOUGH_ROOM;
        return 0;
    }

    uint32_t codePoint = **_bytes;

    if (!nextUtf8(_bytes, _status))
        return 0;

    // shift + mask the leading sequence bits and sum up the unicode values
    codePoint = ((codePoint << 6) & 0x7ff) + (**_bytes & 0x3f);

    return codePoint;
}

uint32_t StringEncoder::getByteSequence3(const byte_t **_bytes, EncodingStatus &_status)
{
    // 3 bytes utf8 format: 1110 xxxx 10xx xxxx 10xx xxxx

    if (**_bytes == '\0')
    {
        _status = EncodingStatus::NOT_ENOUGH_ROOM;
        return 0;
    }

    uint32_t codePoint = **_bytes;

    if (!nextUtf8(_bytes, _status))
        return 0;

    // shift + mask the leading sequence bits and sum up the unicode values
    codePoint = ((codePoint << 12) & 0xffff) + (**_bytes << 6 & 0xfff);

    if (!nextUtf8(_bytes, _status))
        return 0;

    // mask the leading sequence bits and sum up the unicode values
    codePoint += **_bytes & 0x3f;

    return codePoint;
}

uint32_t StringEncoder::getByteSequence4(const byte_t **_bytes, EncodingStatus &_status)
{
    // 4 bytes utf8 format: 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx

    if (**_bytes == '\0')
    {
        _status = EncodingStatus::NOT_ENOUGH_ROOM;
        return 0;
    }

    uint32_t codePoint = **_bytes;

    if (!nextUtf8(_bytes, _status))
        return 0;

    // shift + mask the leading sequence bits and sum up the unicode values
    codePoint = ((codePoint << 18) & 0x1fffff) + (**_bytes << 12 & 0x3ffff);

    if (!nextUtf8(_bytes, _status))
        return 0;

    // shift + mask the leading sequence bits and sum up the unicode values
    codePoint += (**_bytes << 6) & 0xfff;

    if (!nextUtf8(_bytes, _status))
        return 0;

    // mask the leading sequence bits and sum up the unicode values
    codePoint += **_bytes & 0x3f;

    return codePoint;
}

int StringEncoder::getSequenceLength(byte_t _byte)
{
    if (_byte < 0x80)
        return 1;
    else if ((_byte >> 5) == 0x6)    // lead = 0000 01100 = 6 -> 2 bytes sequence
        return 2;
    else if ((_byte >> 4) == 0xe)    // lead = 0000 11100 = 14 -> 3 bytes sequence
        return 3;
    else if ((_byte >> 3) == 0x1e)   // lead = 0001 11100 = 30 -> 4 bytes sequence
        return 4;
    else
        return 0;
}

uint32_t StringEncoder::getNextBytes(const byte_t **_bytes, EncodingStatus &_status)
{
    uint32_t codePoint = 0;
    int sequenceLength = getSequenceLength(**_bytes);

    switch (sequenceLength)
    {
        case 0:
            _status = EncodingStatus::INVALID_LEAD;
            break;
        case 1:
            codePoint = getByteSequence1(**_bytes, _status);
            break;
        case 2:
            codePoint = getByteSequence2(_bytes, _status);
            break;
        case 3:
            codePoint = getByteSequence3(_bytes, _status);
            break;
        case 4:
            codePoint = getByteSequence4(_bytes, _status);
            break;
    }

    if (_status == EncodingStatus::SUCCESS)
    {
        if (isSequenceTooLong(codePoint, sequenceLength))
        {
            _status = EncodingStatus::SEQUENCE_TOO_LONG;
            return 0;
        }
        if (!isCodePointValid(codePoint))
        {
            _status = EncodingStatus::INVALID_CODE_POINT;
            return 0;
        }
    } else
    {
        return 0;
    }

    return codePoint;
}

bytes_t StringEncoder::push(uint32_t _codePoint, EncodingStatus &_status)
{
    bytes_t result;
    if (!isCodePointValid(_codePoint))
    {
        _status = EncodingStatus::INVALID_CODE_POINT;
        result.push_back(0);
        return result;
    }

    if (_codePoint < 0x80)
    {
        result.push_back(static_cast<uint8_t>(_codePoint));
    } else if (_codePoint < 0x800)
    {
        result.push_back(static_cast<uint8_t>((_codePoint >> 6) | 0xc0));               // 0xc0 = 1100 0000 add leading sequence (2 bytes)
        result.push_back(static_cast<uint8_t>((_codePoint & 0x3f) | 0x80));             // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
    } else if (_codePoint < 0x10000)
    {
        result.push_back(static_cast<uint8_t>((_codePoint >> 12) | 0xe0));              // 0xe0 = 1110 0000 add leading sequence (3 bytes)
        result.push_back(static_cast<uint8_t>(((_codePoint >> 6) & 0x3f) | 0x80));      // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
        result.push_back(static_cast<uint8_t>((_codePoint & 0x3f) | 0x80));             // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
    } else
    {
        result.push_back(static_cast<uint8_t>((_codePoint >> 18) | 0xf0));              // 0xf0 = 1111 0000 add leading sequence (4 bytes)
        result.push_back(static_cast<uint8_t>(((_codePoint >> 12) & 0x3f) | 0x80));     // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
        result.push_back(static_cast<uint8_t>(((_codePoint >> 6) & 0x3f) | 0x80));      // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
        result.push_back(static_cast<uint8_t>((_codePoint & 0x3f) | 0x80));             // 0x80 = 1000 0000 add leading sequence 10 to the following bytes
    }

    return result;
}

} // namespace SomeIP
} // namespace CommonAPI
