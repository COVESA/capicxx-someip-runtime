// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <bitset>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/InputStream.hpp>
#include <CommonAPI/SomeIP/StringEncoder.hpp>

namespace CommonAPI {
namespace SomeIP {

InputStream::InputStream(const CommonAPI::SomeIP::Message &_message,
        bool _isLittleEndian)
    : dataBegin_(_message.getBodyData()),
      current_(_message.getBodyData()),
      currentBit_(0),
      remaining_(_message.getBodyLength()),
      message_(_message),
      errorOccurred_(false) {
    buffer_.push_back(static_cast<byte_t>(_isLittleEndian));
}

InputStream::~InputStream() {}

bool InputStream::hasError() const {
    return errorOccurred_;
}

void InputStream::align(const size_t) {
}

byte_t *InputStream::_readRaw(const size_t _size) {
    if( _size > remaining_ )
        errorOccurred_ = true;

    if ( _size >= remaining_ )
        remaining_ = 0;
    else
        remaining_ -= _size;

    byte_t *data = current_;
    current_ += _size;
    return data;
}

InputStream& InputStream::readValue(bool &_value, const EmptyDeployment *) {
    errorOccurred_ = _readBitValue(_value, 8, false);
    return (*this);
}

InputStream& InputStream::readValue(int8_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<int8_t> *>(nullptr));
}

InputStream& InputStream::readValue(int8_t &_value, const IntegerDeployment<int8_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 8), true);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(int16_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<int16_t> *>(nullptr));
}

InputStream& InputStream::readValue(int16_t &_value, const IntegerDeployment<int16_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 16), true);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(int32_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<int32_t> *>(nullptr));
}

InputStream& InputStream::readValue(int32_t &_value, const IntegerDeployment<int32_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 32), true);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(int64_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<int64_t> *>(nullptr));
}

InputStream& InputStream::readValue(int64_t &_value, const IntegerDeployment<int64_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 64), true);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(uint8_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<uint8_t> *>(nullptr));
}

InputStream& InputStream::readValue(uint8_t &_value, const IntegerDeployment<uint8_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 8), false);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(uint16_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<uint16_t> *>(nullptr));
}

InputStream& InputStream::readValue(uint16_t &_value, const IntegerDeployment<uint16_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 16), false);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(uint32_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<uint32_t> *>(nullptr));
}

InputStream& InputStream::readValue(uint32_t &_value, const IntegerDeployment<uint32_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 32), false);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(uint64_t &_value, const EmptyDeployment *) {
    return readValue(_value, static_cast<const IntegerDeployment<uint64_t> *>(nullptr));
}

InputStream& InputStream::readValue(uint64_t &_value, const IntegerDeployment<uint64_t> *_depl) {
    errorOccurred_ = _readBitValue(_value, (_depl ? _depl->bits_ : 64), false);
    if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
        _value = _depl->invalid_;
        errorOccurred_ = false;
    }
    return (*this);
}

InputStream& InputStream::readValue(float &_value, const EmptyDeployment *) {
    bitAlign();
    errorOccurred_ = _readBitValue(_value, 32, false);
    return (*this);
}

InputStream& InputStream::readValue(double &_value, const EmptyDeployment *) {
    bitAlign();
    errorOccurred_ = _readBitValue(_value, 64, false);
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
            _readBitValue(temp, 8, false);
            _value = temp;
        }
        break;
    case 2:
        {
            uint16_t temp;
            _readBitValue(temp, 16, false);
            _value = temp;
        }
        break;
    case 4:
        _readBitValue(_value, 32, false);
        break;
    default:
        errorOccurred_ = true;
    }

    return *this;
}

InputStream& InputStream::readValue(std::string &_value, const EmptyDeployment *) {
    bitAlign();
    return readValue(_value, static_cast<const StringDeployment*>(nullptr));
}

InputStream& InputStream::readValue(std::string &_value, const StringDeployment *_depl) {
    bitAlign();

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

    if (itsSize > remaining_) {
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
                        while (itsSize > 1 && (data[itsSize - 1] != 0x00 || data[itsSize - 2] != 0x00))
                            itsSize--;

                        if (itsSize % 2 != 0) {
                            errorOccurred_ = true;
                        }

                        if(!hasError()) {
                            encoder->utf16To8((byte_t *) data, BIG_ENDIAN, itsSize - 2, status, &bytes, length);
                            itsSize = static_cast<uint32_t>(length);
                        }
                        break;

                    case StringEncoding::UTF16LE:
                        while (itsSize > 1 && (data[itsSize - 1] != 0x00 || data[itsSize - 2] != 0x00))
                            itsSize--;

                        if (itsSize % 2 != 0) {
                            errorOccurred_ = true;
                        }

                        if(!hasError()) {
                            encoder->utf16To8((byte_t *) data, LITTLE_ENDIAN, itsSize - 2, status, &bytes, length);
                            itsSize = static_cast<uint32_t>(length);
                        }
                        break;

                    default:
                        if (data[itsSize - 1] != 0x00) {
                            errorOccurred_ = true;
                        }

                        bytes = (byte_t *) data;
                        break;
                }

                status = EncodingStatus::SUCCESS;
            } else {
                status = EncodingStatus::INVALID_BOM;
            }

            if (status != EncodingStatus::SUCCESS)
            {
                errorOccurred_ = true;
            }
        } else {
            if (encoder->checkBom(data, itsSize, StringEncoding::UTF8)) {
                if (data[itsSize - 1] != 0x00) {
                    errorOccurred_ = true;
                }

                bytes = new byte_t[itsSize];
                memcpy(bytes, (byte_t *) data, itsSize);
            }
            else
                errorOccurred_ = true;
        }
        if (bytes == NULL) {
            _value = "";
        } else {
            // explicitly assign to support NUL (U+0000 code point) in UTF-8 strings
            _value.assign(std::string((const char*)bytes, itsSize - 1u));
            //only delete bytes if not allocated in this function (this is the case for deployed fixed length UTF8 strings)
            if( bytes != (byte_t *) data)
                delete[] bytes;
            bytes = NULL;
        }
    }

    return *this;
}

InputStream& InputStream::readValue(ByteBuffer &_value, const ByteBufferDeployment *_depl) {
    bitAlign();

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
            readValue(itsElement, static_cast<const IntegerDeployment<uint8_t> *>(nullptr));
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
    bitAlign();

    _readBitValue(_value.Major, 32, false);
    _readBitValue(_value.Minor, 32, false);
    return *this;
}

} // namespace SomeIP
} // namespace CommonAPI
