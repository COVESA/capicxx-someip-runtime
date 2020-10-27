// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
//#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <bitset>

#include <iostream>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Version.hpp>
#include <CommonAPI/SomeIP/OutputStream.hpp>
#include <CommonAPI/SomeIP/StringEncoder.hpp>

namespace CommonAPI {
namespace SomeIP {

OutputStream::OutputStream(Message _message, bool _isLittleEndian)
    : message_(_message),
      errorOccurred_(false),
      currentByte_(0),
      currentBit_(0),
      isLittleEndian_(_isLittleEndian) {
}

OutputStream::~OutputStream() {
}

// Internal
size_t OutputStream::getPosition() {
    return payload_.size();
}

void OutputStream::pushPosition() {
    positions_.push_back(payload_.size());
}

size_t OutputStream::popPosition() {
    size_t itsPosition = positions_.back();
    positions_.pop_back();
    return itsPosition;
}

OutputStream& OutputStream::writeValue(const bool &_value, const EmptyDeployment *) {
    return _writeBitValue(_value, 8, false);
}

OutputStream& OutputStream::writeValue(const int8_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<int8_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const int8_t &_value, const IntegerDeployment<int8_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 8), true);
}

OutputStream& OutputStream::writeValue(const int16_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<int16_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const int16_t &_value, const IntegerDeployment<int16_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 16), true);
}

OutputStream& OutputStream::writeValue(const int32_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<int32_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const int32_t &_value, const IntegerDeployment<int32_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 32), true);
}

OutputStream& OutputStream::writeValue(const int64_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<int64_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const int64_t &_value, const IntegerDeployment<int64_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 64), true);
}

OutputStream& OutputStream::writeValue(const uint8_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<uint8_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const uint8_t &_value, const IntegerDeployment<uint8_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 8), false);
}

OutputStream& OutputStream::writeValue(const uint16_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<uint16_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const uint16_t &_value, const IntegerDeployment<uint16_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 16), false);
}

OutputStream& OutputStream::writeValue(const uint32_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<uint32_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const uint32_t &_value, const IntegerDeployment<uint32_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 32), false);
}

OutputStream& OutputStream::writeValue(const uint64_t &_value, const EmptyDeployment *) {
    return writeValue(_value, static_cast<const IntegerDeployment<uint64_t> *>(nullptr));
}

OutputStream& OutputStream::writeValue(const uint64_t &_value, const IntegerDeployment<uint64_t> *_depl) {
    return _writeBitValue(_value, (_depl ? _depl->bits_ : 64), false);
}

OutputStream& OutputStream::writeValue(const float &_value, const EmptyDeployment *) {
    return _writeBitValue(_value, static_cast<uint8_t>((sizeof(float) << 3)), false);
}

OutputStream& OutputStream::writeValue(const double &_value, const EmptyDeployment *) {
    return _writeBitValue(_value, static_cast<uint8_t>((sizeof(double) << 3)), false);
}

OutputStream& OutputStream::_writeValue(const uint32_t &_value, const uint8_t &_width) {
    switch (_width) {
    case 1:
        if (_value > std::numeric_limits<uint8_t>::max()) {
            errorOccurred_ = true;
        }
        _writeBitValue(static_cast<uint8_t>(_value), 8, false);
        break;
    case 2:
        if (_value > std::numeric_limits<uint16_t>::max()) {
            errorOccurred_ = true;
        }
        _writeBitValue(static_cast<uint16_t>(_value), 16, false);
        break;
    case 4:
        if (_value > std::numeric_limits<uint32_t>::max()) {
            errorOccurred_ = true;
        }
        _writeBitValue(static_cast<uint32_t>(_value), 32, false);
        break;
    default:
        break;
    }

    return (*this);
}

OutputStream& OutputStream::_writeValueAt(const uint32_t &_value, const uint8_t &_width, const uint32_t &_position) {
    switch (_width) {
    case 1:
        if (_value > std::numeric_limits<uint8_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValueAt(static_cast<uint8_t>(_value), _position);
        break;
    case 2:
        if (_value > std::numeric_limits<uint16_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValueAt(static_cast<uint16_t>(_value), _position);
        break;
    case 4:
        if (_value > std::numeric_limits<uint32_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValueAt(static_cast<uint32_t>(_value), _position);
        break;
    default:
        break;
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const std::string &_value, const EmptyDeployment *) {
    bitAlign();
    return writeValue(_value, static_cast<const StringDeployment *>(nullptr));
}

OutputStream& OutputStream::writeValue(const std::string &_value, const StringDeployment *_depl) {
    bitAlign();

    bool errorOccurred = false;
    size_t size, terminationSize(2), bomSize(2), stringSize(0);
    byte_t *bytes;

    //Determine string length
    if(_depl != nullptr)
    {
        EncodingStatus status = EncodingStatus::SUCCESS;
        std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();

        switch (_depl->stringEncoding_)
        {
            case StringEncoding::UTF16BE:
                encoder->utf8To16(reinterpret_cast<const byte_t *>(_value.c_str()), BIG_ENDIAN, status, &bytes, size);
                break;

            case StringEncoding::UTF16LE:
                encoder->utf8To16(reinterpret_cast<const byte_t *>(_value.c_str()), LITTLE_ENDIAN, status, &bytes, size);
                break;

            default:
                bytes = reinterpret_cast<byte_t *>(const_cast<char *>(_value.c_str()));
                size = _value.size();
                bomSize = 3;
                terminationSize = 1;
                break;
        }

        if (status != EncodingStatus::SUCCESS)
        {
            COMMONAPI_ERROR("OutputStream::writeValue(string):error, encoding status is: ",
                    static_cast<std::uint32_t>(status));
        }

    } else
    {
        bytes = reinterpret_cast<byte_t *>(const_cast<char *>(_value.c_str()));
        size = _value.size();
        bomSize = 3;
        terminationSize = 1;
    }
    stringSize = size + terminationSize + bomSize;
    //write string length
    if (_depl != nullptr) {
        if (_depl->stringLengthWidth_ == 0) {

            if (_depl->stringLength_  <  stringSize ) {
                errorOccurred = true;
            } else {
                terminationSize += (_depl->stringLength_ - stringSize);
            }

        } else {
            _writeValue(uint32_t(stringSize),
                    _depl->stringLengthWidth_);
        }
    } else {
        _writeValue(uint32_t(stringSize), 4);
    }

    if(!errorOccurred) {
        // Write BOM
        _writeBom(_depl);
        // Write sting content
        _writeRaw(bytes, size);
        // Write termination
        _writeRawFill(0x00, terminationSize);
    } else {
        COMMONAPI_ERROR("OutputStream::writeValue(string): error occurred");
    }

    if (bytes != reinterpret_cast<byte_t *>(const_cast<char *>(_value.c_str()))) {
        delete [] bytes;
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const ByteBuffer &_value, const ByteBufferDeployment *_depl) {
    bitAlign();

    uint32_t byteBufferMinLength = (_depl ? _depl->byteBufferMinLength_ : 0);
    uint32_t byteBufferMaxLength = (_depl ? _depl->byteBufferMaxLength_ : 0xFFFFFFFF);
    uint32_t byteBufferLengthWidth = (_depl ? _depl->byteBufferLengthWidth_ : 4);

    uint32_t maxSize = 0;
    if (byteBufferLengthWidth != 0 && byteBufferMaxLength > 0 && byteBufferMaxLength < _value.size() ) {
        maxSize = byteBufferMaxLength;
    }
    else {
        maxSize = static_cast<std::uint32_t>(_value.size());
    }

    if ((byteBufferLengthWidth == 0 && byteBufferMaxLength != maxSize) ||
        (byteBufferLengthWidth != 0 && maxSize < byteBufferMinLength)) {
        errorOccurred_ = true;
    }

    if (!hasError()) {
        switch (byteBufferLengthWidth) {
        case 0:
            break;
        case 1:
            if (maxSize > 255)
                errorOccurred_ = true;
            else
                _writeValue(uint8_t(maxSize), 1);
            break;
        case 2:
            if (maxSize > (1U<<16) - 1)
                errorOccurred_ = true;
            else
                _writeValue(uint16_t(maxSize), 2);
            break;
        case 4:
            _writeValue(uint32_t(maxSize), 4);
            break;
        default:
            errorOccurred_ = true;
        }
    }
    if (!hasError()) {
        if (_value.size()) {
            _writeRaw(static_cast<const byte_t *>(&_value[0]), maxSize);
        }
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const Version &_value, const EmptyDeployment *) {
    bitAlign();

    _writeBitValue(_value.Major, static_cast<uint8_t>((sizeof(_value.Major) << 3)), false);
    _writeBitValue(_value.Minor, static_cast<uint8_t>((sizeof(_value.Minor) << 3)), false);
    return (*this);
}

bool OutputStream::hasError() const {
    return errorOccurred_;
}

//Additional 0-termination, so this is 8 byte of \0
static const byte_t eightByteZeroString[] = { 0 };

void OutputStream::align(const size_t _boundary) {
    if ((_boundary > 0 && _boundary <= 8 &&
        (_boundary % 2 == 0 || _boundary == 1))) {

        size_t mask = _boundary - 1;
        size_t necessary = ((mask - (payload_.size() & mask)) + 1) & mask;

        _writeRaw(eightByteZeroString, necessary);
    } else {
        COMMONAPI_ERROR("OutputStream::align invalid boundary: ", _boundary);
    }
}

void OutputStream::_writeRaw(const byte_t &_data) {
    payload_.push_back(_data);
}

void OutputStream::_writeRaw(const byte_t *_data, const size_t _size) {
    payload_.insert(payload_.end(), _data, _data + _size);
}

void OutputStream::_writeRawFill(const byte_t _data, const size_t _size) {
    payload_.insert(payload_.end(), _size, _data);
}

void OutputStream::_writeRawAt(const byte_t *_data, const size_t _size, const size_t _position) {
    std::memcpy(&payload_[_position], _data, _size);
}

void OutputStream::_writeBom(const StringDeployment *_depl) {
    const byte_t utf8Bom[] = { 0xEF, 0xBB, 0xBF };
    const byte_t utf16LeBom[] = { 0xFF, 0xFE };
    const byte_t utf16BeBom[] = { 0xFE, 0xFF };

    if (_depl == NULL ||
            (_depl != NULL && _depl->stringEncoding_ == StringEncoding::UTF8)) {
        _writeRaw(utf8Bom, sizeof(utf8Bom));
    } else if (_depl->stringEncoding_ == StringEncoding::UTF16LE) {
        _writeRaw(utf16LeBom, sizeof(utf16LeBom));
    } else if (_depl->stringEncoding_ == StringEncoding::UTF16BE) {
        _writeRaw(utf16BeBom, sizeof(utf16BeBom));
    } else {
        errorOccurred_ = true;
    }
}

void OutputStream::flush() {
    // Check whether last byte was already added
    if (currentBit_ > 0) {
        _writeRaw(currentByte_);
        currentByte_ = 0x0;
        currentBit_ = 0;
    }

    message_.setPayloadData((byte_t *)payload_.data(), uint32_t(payload_.size()));

    // clear
    payload_.clear();
}

} // namespace SomeIP
} // namespace CommonAPI
