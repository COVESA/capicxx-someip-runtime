// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/InputStream.hpp>
#include <CommonAPI/SomeIP/StringEncoder.hpp>
#include <bitset>

namespace CommonAPI {
namespace SomeIP {

InputStream::InputStream(const CommonAPI::SomeIP::Message& message)
    : dataBegin_(message.getBodyData()),
      current_(message.getBodyData()),
      remaining_(message.getBodyLength()),
      message_(message),
      errorOccurred_(false) {
}

InputStream::~InputStream() {}

bool InputStream::hasError() const {
    return errorOccurred_;
}

void InputStream::align(const size_t _boundary) {
//    const unsigned int mask = _boundary - 1;
//    current_ = (current_ + mask) & (~mask);
}

byte_t *InputStream::_readRaw(const size_t _size) {
    assert(remaining_ >= _size);

    byte_t *data = current_;
    current_ += _size;
    remaining_ -= _size;
    return data;
}

InputStream& InputStream::readValue(bool &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int8_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int16_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int32_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int64_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint8_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint16_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint32_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint64_t &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(float &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(double &_value, const EmptyDeployment *_depl) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint32_t &_value, const uint8_t &_width, const bool &_permitZeroWidth) {
    switch (_width) {
    case 0:
        if (_permitZeroWidth) {
            _value = 0;
        } else {
            errorOccurred_ = true;
        }
        break;
    case 1:
        {
            uint8_t temp;
            _readValue(temp);
            _value = temp;
        }
        break;
    case 2:
        {
            uint16_t temp;
            _readValue(temp);
            _value = temp;
        }
        break;
    case 4:
        _readValue(_value);
        break;
    default:
        errorOccurred_ = true;
    }

    return *this;
}

InputStream& InputStream::readValue(std::string &_value, const EmptyDeployment *_depl) {
    uint32_t itsSize(0);
    readValue(itsSize, 4, false);

    if(itsSize > remaining_) {
        errorOccurred_ = true;
    }

    // Read string, if reading size has been successful
    if(!hasError()) {
        char *data = reinterpret_cast<char*>(_readRaw(itsSize));

        if(data[itsSize - 1] == '\0') {
            // The string contained in a message is required to be 0-terminated, therefore the following line works
            _value = data; // TODO encoding, byte order
        } else {
            errorOccurred_ = true;
        }
    }

    return *this;
}

InputStream& InputStream::readValue(std::string &_value, const StringDeployment *_depl) {
    uint32_t itsSize(0);

    // Read string size
    if (_depl != nullptr) {
        if (_depl->stringLengthWidth_ == 0) {
            itsSize = _depl->stringLength_;
        } else {
            readValue(itsSize, _depl->stringLengthWidth_, false);
        }
    } else {
        readValue(itsSize, 4, false);
    }

    if(itsSize > remaining_) {
        errorOccurred_ = true;
    }

    // Read string, if reading size has been successful
    if(!hasError()) {
        char *data = reinterpret_cast<char*>(_readRaw(itsSize));

        byte_t *bytes;
        if(_depl != nullptr)
        {
            EncodingStatus status = EncodingStatus::SUCCESS;
            size_t length = 0;
            std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();

            switch (_depl->stringEncoding_)
            {
                case StringEncoding::UTF16BE:
                    encoder->utf16To8((byte_t *) data, BIG_ENDIAN, itsSize, status, &bytes, length);
                    break;

                case StringEncoding::UTF16LE:
                    encoder->utf16To8((byte_t *) data, LITTLE_ENDIAN, itsSize, status, &bytes, length);
                    break;

                default:
                    bytes = (byte_t *) data;
                    break;
            }

            if(status != EncodingStatus::SUCCESS)
            {
                //TODO error handling
            }
        } else
        {
            bytes = (byte_t *) data;
        }

        _value = (char*)bytes;
    }

    return *this;
}

InputStream& InputStream::readValue(ByteBuffer &_value, const EmptyDeployment *_depl) {
//    *this >> byteBufferValue;
    return *this;
}

InputStream& InputStream::readValue(Version &_value, const EmptyDeployment *_depl) {
    //_readValue(versionValue.Major);
    //_readValue(versionValue.Minor);
    return *this;
}

} // namespace SomeIP
} // namespace CommonAPI
