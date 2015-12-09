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

void InputStream::align(const size_t) {
}

byte_t *InputStream::_readRaw(const size_t _size) {
    assert(remaining_ >= _size);

    byte_t *data = current_;
    current_ += _size;
    remaining_ -= _size;
    return data;
}

InputStream& InputStream::readValue(bool &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int8_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int16_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int32_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(int64_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint8_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint16_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint32_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(uint64_t &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(float &_value, const EmptyDeployment *) {
    errorOccurred_ = _readValue(_value);
    return (*this);
}
InputStream& InputStream::readValue(double &_value, const EmptyDeployment *) {
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

InputStream& InputStream::readValue(std::string &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const StringDeployment*>(nullptr));
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

        byte_t *data = _readRaw(itsSize);

        std::shared_ptr<StringEncoder> encoder = std::make_shared<StringEncoder>();
        byte_t *bytes = NULL;

        if(_depl != nullptr)
        {
            EncodingStatus status = EncodingStatus::UNKNOWN;
            size_t length = 0;

            if (encoder->checkBom(data, itsSize, _depl->stringEncoding_)) {
                switch (_depl->stringEncoding_)
                {
                    case StringEncoding::UTF16BE:
                        //TODO do not return error if itsSize is odd and itsSize is too short
                        if(itsSize % 2 != 0 && data[itsSize - 1] == 0x00 && data[itsSize - 2] == 0x00 )
                            errorOccurred_ = true;
                        if(!hasError())
                            encoder->utf16To8((byte_t *) data, BIG_ENDIAN, itsSize - 2, status, &bytes, length);
                        break;

                    case StringEncoding::UTF16LE:
                        if(itsSize % 2 != 0 && data[itsSize - 1] == 0x00 && data[itsSize - 2] == 0x00 )
                            errorOccurred_ = true;
                        if(!hasError())
                            encoder->utf16To8((byte_t *) data, LITTLE_ENDIAN, itsSize - 2, status, &bytes, length);
                        break;

                    default:
                        bytes = (byte_t *) data;
                        break;
                }

                status = EncodingStatus::SUCCESS;
            } else {
                status = EncodingStatus::INVALID_BOM;
            }

            if(status != EncodingStatus::SUCCESS)
            {
                errorOccurred_ = true;
            }
        } else
        {
            if (encoder->checkBom(data, itsSize, StringEncoding::UTF8)) {
                bytes = new byte_t[itsSize];
                memcpy(bytes, (byte_t *) data, itsSize);
            }
            else
                errorOccurred_ = true;
        }
        if (bytes == NULL) {
	        _value = "";
	    } else {
	        _value = (char*)bytes;
	        //only delete bytes if not allocated in this function (this is the case for deployed fixed length UTF8 strings)
	        if( bytes != (byte_t *) data)
	            delete[] bytes;
	        bytes = NULL;
	    }
    }

    return *this;
}

InputStream& InputStream::readValue(ByteBuffer &_value, const ByteBufferDeployment *_depl) {
    uint32_t byteBufferMinLength = (_depl ? _depl->byteBufferMinLength_ : 0);
    uint32_t byteBufferMaxLength = (_depl ? _depl->byteBufferMaxLength_ : 0xFFFFFFFF);

    uint32_t itsSize;

    // Read array size
    readValue(itsSize, 4, true);

    // Reset target
    _value.clear();

    // Read elements, if reading size has been successful
    if (!hasError()) {
        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            uint8_t itsElement;
            readValue(itsElement, nullptr);
            if (hasError()) {
                break;
            }

            _value.push_back(std::move(itsElement));

            itsSize -= uint32_t(remainingBeforeRead - remaining_);
        }

        if (byteBufferMinLength != 0 && _value.size() < byteBufferMinLength) {
            errorOccurred_ = true;
        }
        if (byteBufferMaxLength != 0 && _value.size() > byteBufferMaxLength) {
            errorOccurred_ = true;
        }
    }

    return (*this);
}

InputStream& InputStream::readValue(Version &_value, const EmptyDeployment *) {
    _readValue(_value.Major);
    _readValue(_value.Minor);
    return *this;
}

} // namespace SomeIP
} // namespace CommonAPI
