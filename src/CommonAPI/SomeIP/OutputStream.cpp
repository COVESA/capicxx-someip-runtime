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
    return writeValue(_value, static_cast<const StringDeployment *>(nullptr));
}

OutputStream& OutputStream::writeValue(const std::string &_value, const StringDeployment *_depl) {

    bool errorOccurred = false;
    size_t size, terminationSize(2);
    size_t bomSize(2);
    byte_t *bytes;

    //Determine string length
    if(_depl != nullptr)
    {
        EncodingStatus status = EncodingStatus::SUCCESS;
        std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();

        switch (_depl->stringEncoding_)
        {
            case StringEncoding::UTF16BE:
                encoder->utf8To16((byte_t *)_value.c_str(), BIG_ENDIAN, status, &bytes, size);
                break;

            case StringEncoding::UTF16LE:
                encoder->utf8To16((byte_t *)_value.c_str(), LITTLE_ENDIAN, status, &bytes, size);
                break;

            default:
                bytes = (byte_t *)(_value.c_str());
                size = _value.size();
                bomSize = 3;
                terminationSize = 1;
                break;
        }

        if(status != EncodingStatus::SUCCESS)
        {
            //TODO error handling
        }

    } else
    {
        bytes = (byte_t *)(_value.c_str());
        size = _value.size();
        bomSize = 3;
        terminationSize = 1;
    }

    //write string length
    if (_depl != nullptr) {
        if (_depl->stringLengthWidth_ == 0
                && _depl->stringLength_  != size + terminationSize + bomSize ) {
                errorOccurred = true;
        } else {
            _writeValue(uint32_t(size + terminationSize + bomSize),
                    _depl->stringLengthWidth_);
        }
    } else {
        _writeValue(uint32_t(size + terminationSize + bomSize), 4);
    }


    if(!errorOccurred) {
        // Write BOM
        _writeBom(_depl);

        // Write sting content
        _writeRaw(bytes, size);

        // Write termination
        const byte_t termination[] = { 0x00, 0x00 };
        _writeRaw(termination, terminationSize);
    }

    if (bytes != (byte_t*)_value.c_str()) {
        delete [] bytes;
    }

    return (*this);
}

OutputStream& OutputStream::writeValue(const ByteBuffer &_value, const ByteBufferDeployment *_depl) {
    uint32_t byteBufferMinLength = (_depl ? _depl->byteBufferMinLength_ : 0);
    uint32_t byteBufferMaxLength = (_depl ? _depl->byteBufferMaxLength_ : 0xFFFFFFFF);

    pushPosition();     // Start of length field
    _writeValue(0, 4);  // Length field placeholder
    pushPosition();     // Start of vector data

    if (byteBufferMinLength != 0 && _value.size() < byteBufferMinLength) {
        errorOccurred_ = true;
    }
    if (byteBufferMaxLength != 0 && _value.size() > byteBufferMaxLength) {
        errorOccurred_ = true;
    }

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
    message_.setPayloadData((byte_t *)payload_.data(), uint32_t(payload_.size()));
}

void OutputStream::reserveMemory(size_t) {
}

} // namespace SomeIP
} // namespace CommonAPI
