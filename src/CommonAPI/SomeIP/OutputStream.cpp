// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
//#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Version.hpp>
#include <CommonAPI/SomeIP/OutputStream.hpp>
#include <CommonAPI/SomeIP/StringEncoder.hpp>
#include <bitset>

namespace CommonAPI {
namespace SomeIP {

OutputStream::OutputStream(Message message)
    : message_(message),
      errorOccurred_(false) {
}

OutputStream::~OutputStream() {
}

// Internal
size_t OutputStream::getPosition() {
    return payload_.size();
}

void OutputStream::pushPosition() {
    positions_.push(payload_.size());
}

size_t OutputStream::popPosition() {
    size_t itsPosition = positions_.top();
    positions_.pop();
    return itsPosition;
}

OutputStream& OutputStream::writeValue(const bool &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const int8_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const int16_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const int32_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const int64_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const uint8_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const uint16_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const uint32_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const uint64_t &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const float &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::writeValue(const double &_value, const EmptyDeployment *) {
    return _writeValue(_value);
}

OutputStream& OutputStream::_writeValue(const uint32_t &_value, const uint8_t &_width) {
    switch (_width) {
    case 1:
        if (_value > std::numeric_limits<uint8_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValue(static_cast<uint8_t>(_value));
        break;
    case 2:
        if (_value > std::numeric_limits<uint16_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValue(static_cast<uint16_t>(_value));
        break;
    case 4:
        if (_value > std::numeric_limits<uint32_t>::max()) {
            errorOccurred_ = true;
        }
        _writeValue(static_cast<uint32_t>(_value));
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
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const std::string &_value, const EmptyDeployment *) {
    bool errorOccurred = false;
    size_t stringLength = _value.size() + 1; //adding null termination

    _writeValue(uint32_t(stringLength), 4);

    if(!errorOccurred) {
        // Write string content
        _writeString(_value, nullptr);
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const std::string &_value, const StringDeployment *_depl) {

    bool errorOccurred = false;
    size_t stringLength;
    byte_t *bytes;

    //Determine string length
    if(_depl != nullptr)
    {
        EncodingStatus status = EncodingStatus::SUCCESS;
        std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();

        switch (_depl->stringEncoding_)
        {
            case StringEncoding::UTF16BE:
                encoder->utf8To16((byte_t *)_value.c_str(), BIG_ENDIAN, status, &bytes, stringLength);
                delete[] bytes;
                bytes = NULL;
                break;

            case StringEncoding::UTF16LE:
                encoder->utf8To16((byte_t *)_value.c_str(), LITTLE_ENDIAN, status, &bytes, stringLength);
                delete[] bytes;
                bytes = NULL;
                break;

            default:
                stringLength = _value.size();
                break;
        }

        if(status != EncodingStatus::SUCCESS)
        {
            //TODO error handling
        }

    } else
    {
        stringLength = _value.size() + 1;   //adding null termination
    }

    //write string length
    if (_depl != nullptr) {
        if (_depl->stringLengthWidth_ == 0) {
            if (_depl->stringLength_ != stringLength) {
                errorOccurred = true;
            }
        } else {
            _writeValue(uint32_t(stringLength), _depl->stringLengthWidth_);
        }
    } else {
        _writeValue(uint32_t(stringLength), 4);
    }

    if(!errorOccurred) {
        // Write string content
        _writeString(_value, _depl);
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const ByteBuffer &_value, const EmptyDeployment *) {
    pushPosition();     // Start of length field
    _writeValue(0, 4);  // Length field placeholder
    pushPosition();     // Start of vector data

    if (!hasError()) {
        // Write array/vector content
        for (auto i : _value) {
            writeValue(i, nullptr);
            if (hasError()) {
                break;
            }
        }
    }

    // Write actual value of length field
    size_t length = getPosition() - popPosition();
    size_t position2Write = popPosition();
    _writeValueAt(uint32_t(length), 4, uint32_t(position2Write));

    return (*this);
}

OutputStream& OutputStream::writeValue(const Version &_value, const EmptyDeployment *) {
    _writeValue(_value.Major);
    _writeValue(_value.Minor);
    return (*this);
}

bool OutputStream::hasError() const {
    return errorOccurred_;
}

//Additional 0-termination, so this is 8 byte of \0
static const byte_t eightByteZeroString[] = { 0 };

void OutputStream::align(const size_t _boundary) {
    assert(_boundary > 0 && _boundary <= 8 &&
        (_boundary % 2 == 0 || _boundary == 1));

    size_t mask = _boundary - 1;
    size_t necessary = ((mask - (payload_.size() & mask)) + 1) & mask;

    _writeRaw(eightByteZeroString, necessary);
}

void OutputStream::_writeRaw(const byte_t &_data) {
    payload_.push_back(_data);
}

void OutputStream::_writeRaw(const byte_t *_data, const size_t _size) {
    payload_.insert(payload_.end(), _data, _data + _size);
}

void OutputStream::_writeRawAt(const byte_t *_data, const size_t _size, const size_t _position) {
    std::memcpy(&payload_[_position], _data, _size);
}

OutputStream& OutputStream::_writeString(const std::string &_value, const StringDeployment *_depl) {
    assert(_value.c_str()[_value.size()] == '\0');

    bool converted = false;
    size_t stringLength;
    byte_t *bytes;

    // Encoding
    if(_depl != nullptr)
    {
        EncodingStatus status = EncodingStatus::SUCCESS;
        std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();

        switch (_depl->stringEncoding_)
        {
            case StringEncoding::UTF16BE:
                encoder->utf8To16((byte_t *)_value.c_str(), BIG_ENDIAN, status, &bytes, stringLength);
                converted = true;
                break;

            case StringEncoding::UTF16LE:
                encoder->utf8To16((byte_t *)_value.c_str(), LITTLE_ENDIAN, status, &bytes, stringLength);
                converted = true;
                break;

            default:
                bytes = (byte_t *)_value.c_str();
                stringLength = _value.size();
                break;
        }

        if(status != EncodingStatus::SUCCESS)
        {
            //TODO error handling
        }

    } else
    {
        bytes = (byte_t *)_value.c_str();
        stringLength = _value.size() + 1;
    }

    _writeRaw(bytes, stringLength);

    if(converted)
    {
        delete[] bytes;
        bytes = NULL;
    }

    return (*this);
}

void OutputStream::flush() {
    message_.setPayloadData((byte_t *)payload_.data(), uint32_t(payload_.size()));
}

void OutputStream::reserveMemory(size_t) {
}

} // namespace SomeIP
} // namespace CommonAPI
