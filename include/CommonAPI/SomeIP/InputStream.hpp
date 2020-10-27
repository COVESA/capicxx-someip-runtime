// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_INPUTSTREAM_HPP_
#define COMMONAPI_SOMEIP_INPUTSTREAM_HPP_

#include <CommonAPI/InputStream.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

#if defined(LINUX)
#include <endian.h>
#elif defined(FREEBSD)
#include <sys/endian.h>
#else
//#error "Undefined OS (only Linux/FreeBSD are currently supported)"
#endif

#include <stdint.h>
#include <cstring> // memset
#include <string>
#include <vector>
#include <stack>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

namespace CommonAPI {
namespace SomeIP {

/**
 * @class InputMessageStream
 *
 * Used to deserialize and read data from a #Message. For all data types that can be read from a #Message, a ">>"-operator should be defined to handle the reading
 * (this operator is predefined for all basic data types and for vectors).
 */
class InputStream: public CommonAPI::InputStream<InputStream> {
public:
    COMMONAPI_EXPORT virtual bool hasError() const;

    COMMONAPI_EXPORT virtual InputStream &readValue(bool &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(int8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int8_t &_value, const IntegerDeployment<int8_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(int16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int16_t &_value, const IntegerDeployment<int16_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(int32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int32_t &_value, const IntegerDeployment<int32_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(int64_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int64_t &_value, const IntegerDeployment<int64_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint8_t &_value, const IntegerDeployment<uint8_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint16_t &_value, const IntegerDeployment<uint16_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const IntegerDeployment<uint32_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint64_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint64_t &_value, const IntegerDeployment<uint64_t> *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(float &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(double &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const StringDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(ByteBuffer &_value, const ByteBufferDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(Version &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const uint8_t &_width, const bool &_permitZeroWidth);

    template<int minimum, int maximum>
    COMMONAPI_EXPORT InputStream &readValue(RangedInteger<minimum, maximum> &_value, const EmptyDeployment *) {
        int32_t tempValue;
        readValue(tempValue, static_cast<EmptyDeployment *>(nullptr));
        if (tempValue < minimum || tempValue > maximum) {
            errorOccurred_ = true;
        }
        if (!errorOccurred_) {
            _value = tempValue;
        }
        return (*this);
    }
    template<int minimum, int maximum>
    COMMONAPI_EXPORT InputStream &readValue(RangedInteger<minimum, maximum> &_value, const IntegerDeployment<int> *_depl) {

        int32_t tempValue;
        errorOccurred_ = _readBitValue(tempValue, (_depl ? _depl->bits_ : 32), true);
        if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
            tempValue = _depl->invalid_;
            errorOccurred_ = false;
        }
        if (tempValue < minimum || tempValue > maximum) {
            errorOccurred_ = true;
        }
        if (!errorOccurred_) {
            _value = tempValue;
        }
        return (*this);
    }
    template<typename Base_>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<Base_> &_value, const EmptyDeployment *) {
        Base_ tmpValue;
        readValue(tmpValue, static_cast<EmptyDeployment *>(nullptr));
        _value = tmpValue;

        return (*this);
    }

    template<class Deployment_, typename Base_>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<Base_> &_value, const Deployment_ *_depl) {
        if (_depl != nullptr) {
            uint8_t width = static_cast<uint8_t>(_depl ? (_depl->bits_ >> 3) : 0);
            switch (width) {
                case 1:
                {
                    uint8_t value;
                    errorOccurred_ = _readBitValue(value, _depl->bits_, _depl->isSigned_);
                    _value.value_ = static_cast<Base_>(value);
                    break;
                }
                case 2:
                {
                    uint16_t value;
                    errorOccurred_ = _readBitValue(value, _depl->bits_, _depl->isSigned_);
                    _value.value_ = static_cast<Base_>(value);
                    break;
                }
                default:
                {
                    Base_ value;
                    errorOccurred_ = _readBitValue(value, _depl->bits_, _depl->isSigned_);
                    _value.value_ = value;
                    break;
                }
            }

            if (errorOccurred_ && _depl != nullptr && _depl->hasInvalid_) {
                _value.value_ = _depl->invalid_;
                errorOccurred_ = false;
            }
        } else {
            readValue(_value.value_, static_cast<EmptyDeployment *>(nullptr));
        }

        return (*this);
    }

    template<typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Struct<Types_...> &_value,
                           const EmptyDeployment *_depl) {
        bitAlign();

        if (!hasError()) {
            const auto itsFieldSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<Types_...>, EmptyDeployment>{}(
                (*this), _value, _depl);
        }
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Struct<Types_...> &_value,
                           const Deployment_ *_depl) {
        bitAlign();

        uint32_t itsSize;
        uint8_t structLengthWidth = (_depl ? _depl->structLengthWidth_ : 0);

        // Read struct size
        readValue(itsSize, structLengthWidth, true);

        // Read struct fields, if reading size has been successful
        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            const auto itsFieldSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<Types_...>, Deployment_>{}(
                (*this), _value, _depl);

            if (structLengthWidth != 0) {
                size_t deserialized = remainingBeforeRead - remaining_;
                if (itsSize > deserialized) {
                    (void)_readRaw(itsSize - deserialized);
                }
            }
        }
        return (*this);
    }

    template<typename Deployment_, class Polymorphic_Struct>
    COMMONAPI_EXPORT InputStream &readValue(std::shared_ptr<Polymorphic_Struct> &_value,
                           const Deployment_ *_depl) {
        bitAlign();

        uint32_t serial;
        errorOccurred_ = _readBitValue(serial, 32, false);
        if (!hasError()) {
            _value = Polymorphic_Struct::create(serial);
            _value->template readValue<InputStream>(*this, _depl);
        }

        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Variant<Types_...> &_value,
                           const Deployment_ *_depl) {
        bitAlign();

        if(_value.hasValue()) {
#if _MSC_VER < 1900
            const auto maxSize = Variant<Types_...>::maxSize;
#else
            constexpr auto maxSize = Variant<Types_...>::maxSize;
#endif
            DeleteVisitor<maxSize> visitor(_value.valueStorage_);
            ApplyVoidVisitor<DeleteVisitor<maxSize>,
                Variant<Types_...>, Types_... >::visit(visitor, _value);
        }
        _value.valueType_ = 0;

        uint32_t itsSize;
        uint32_t itsType;

        bool unionDefaultOrder = (_depl ? _depl->unionDefaultOrder_ : true);
        uint8_t unionLengthWidth = (_depl ? _depl->unionLengthWidth_ : 4);
        uint8_t unionTypeWidth = (_depl ? _depl->unionTypeWidth_ : 4);

        if (unionDefaultOrder) {
            readValue(itsSize, unionLengthWidth, true);
            readValue(itsType, unionTypeWidth, false);
        } else {
            readValue(itsType, unionTypeWidth, false);
            readValue(itsSize, unionLengthWidth, true);
        }

        if (!itsType)
            errorOccurred_ = true;

        if (!hasError()) {
            // CommonAPI variant supports only 255 different union types!
            _value.valueType_ = uint8_t((itsType > 255) ? 255 : uint8_t(_value.getMaxValueType()) - itsType + 1);

            size_t remainingBeforeRead = remaining_;

            InputStreamReadVisitor<InputStream, Types_...> visitor((*this), _value);
            ApplyStreamVisitor<InputStreamReadVisitor<InputStream, Types_... >,
                Variant<Types_...>, Deployment_, Types_...>::visit(visitor, _value, _depl);

            size_t paddingCount = 0;
            size_t remainingCount = remainingBeforeRead - remaining_;
            if (unionLengthWidth != 0) {
                if (itsSize < remainingCount) {
                    errorOccurred_ = true;
                } else {
                    paddingCount = itsSize - remainingCount;
                }
            } else {
                if (_depl->unionMaxLength_ < remainingCount) {
                    errorOccurred_ = true;
                } else {
                    paddingCount = _depl->unionMaxLength_ - remainingCount;
                }
            }

            if (!errorOccurred_) {
                (void)_readRaw(paddingCount);
            }
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_,
        typename std::enable_if<(std::is_same<int8_t, ElementType_>::value ||
                                 std::is_same<uint8_t, ElementType_>::value), int>::type = 0>
    COMMONAPI_EXPORT InputStream &readValue(std::vector<ElementType_> &_value,
            const ArrayDeployment<ElementDepl_> *_depl) {
        bitAlign();

        uint32_t itsSize;

        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        // Read array size
        if (arrayLengthWidth > 0) {
            readValue(itsSize, arrayLengthWidth, true);
            if (itsSize < arrayMinLength ||
                    (arrayMaxLength != 0 && itsSize > arrayMaxLength) ||
                    itsSize > remaining_) {
                errorOccurred_ = true;
            }
        } else {
            itsSize = arrayMaxLength;
        }

        // Reset target
        _value.clear();

        // Read elements, if reading size has been successful
        if (!hasError()) {
            ElementType_ *base = reinterpret_cast<ElementType_ *>(_readRaw(itsSize));
            _value.assign(base, base+itsSize);
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_,
        typename std::enable_if<(!std::is_same<int8_t, ElementType_>::value &&
                                 !std::is_same<uint8_t, ElementType_>::value), int>::type = 0>
    COMMONAPI_EXPORT InputStream &readValue(std::vector<ElementType_> &_value,
                           const ArrayDeployment<ElementDepl_> *_depl) {
        bitAlign();

        uint32_t itsSize;

        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        // Read array size
        readValue(itsSize, arrayLengthWidth, true);

        // Reset target
        _value.clear();

        // Read elements, if reading size has been successful
        if (!hasError()) {
            while (itsSize > 0 || (arrayLengthWidth == 0 && _value.size() < arrayMaxLength)) {

                size_t remainingBeforeRead = remaining_;

                ElementType_ itsElement;
                readValue(itsElement, (_depl ? _depl->elementDepl_ : nullptr));
                if (hasError()) {
                    break;
                }

                _value.push_back(std::move(itsElement));

                if (arrayLengthWidth != 0) {
                    itsSize -= uint32_t(remainingBeforeRead - remaining_);
                }
            }

            if (arrayLengthWidth != 0) {
                if (itsSize != 0) {
                    errorOccurred_ = true;
                }
                if (arrayMinLength != 0 && _value.size() < arrayMinLength) {
                    errorOccurred_ = true;
                }
                if (arrayMaxLength != 0 && _value.size() > arrayMaxLength) {
                    errorOccurred_ = true;
                }
            } else {
                if (arrayMaxLength != _value.size()) {
                    errorOccurred_ = true;
                }
            }


        }

        return (*this);
    }

    template<typename Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_,
                           ValueType_, HasherType_> &_value,
                           const Deployment_ *_depl) {
        bitAlign();

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;

        uint8_t mapLengthWidth = (_depl ? _depl->mapLengthWidth_ : 4);
        uint32_t mapMinLength = (_depl ? _depl->mapMinLength_ : 0);
        uint32_t mapMaxLength = (_depl ? _depl->mapMaxLength_ : 0xFFFFFFFF);

        _value.clear();

        readValue(itsSize, mapLengthWidth, true);

        if (!hasError()) {
            while (itsSize > 0 || (mapLengthWidth == 0 && _value.size() < mapMaxLength)) {
                size_t remainingBeforeRead = remaining_;

                KeyType_ itsKey;
                readValue(itsKey, (_depl ? _depl->key_ : nullptr));
                if (hasError()) {
                    break;
                }

                ValueType_ itsValue;
                readValue(itsValue, (_depl ? _depl->value_ : nullptr));
                if (hasError()) {
                    break;
                }

                _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

                if (mapLengthWidth != 0) {
                    itsSize -= uint32_t(remainingBeforeRead - remaining_);
                }
            }
        }

        if (mapLengthWidth != 0) {
            if (itsSize != 0) {
                errorOccurred_ = true;
            }
            if (mapMinLength != 0 && _value.size() < mapMinLength) {
                errorOccurred_ = true;
            }
            if (mapMaxLength != 0 && _value.size() > mapMaxLength) {
                errorOccurred_ = true;
            }
        } else {
            if (mapMaxLength != _value.size()) {
                errorOccurred_ = true;
            }
        }

        return (*this);
    }

    template<typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_,
                           ValueType_, HasherType_> &_value,
                           const EmptyDeployment *_depl) {
        bitAlign();

        (void)_depl;

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;

        _value.clear();

        errorOccurred_ = _readBitValue(itsSize, 32, false);

        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            KeyType_ itsKey;
            readValue(itsKey, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            ValueType_ itsValue;
            readValue(itsValue, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

            itsSize -= uint32_t(remainingBeforeRead - remaining_);
        }

        if (itsSize != 0) {
            errorOccurred_ = true;
        }

        return (*this);
    }
    /**
     * Creates a #InputMessageStream which can be used to deserialize and read data from the given #Message.
     * As no message-signature is checked, the user is responsible to ensure that the correct data types are read in the correct order.
     *
     * @param message the #Message from which data should be read.
     */
    COMMONAPI_EXPORT InputStream(const Message &_message, bool _isLittleEndian);
    COMMONAPI_EXPORT InputStream(const InputStream &_stream) = delete;

    /**
     * Destructor; does not call the destructor of the referred #Message. Make sure to maintain a reference to the
     * #Message outside of the stream if you intend to make further use of the message.
     */
    COMMONAPI_EXPORT virtual ~InputStream();

    /**
     * Aligns the stream to the given byte boundary, i.e. the stream skips as many bytes as are necessary to execute the next read
     * starting from the given boundary.
     *
     * @param _boundary the byte boundary to which the stream needs to be aligned.
     */
    COMMONAPI_EXPORT void align(const size_t _boundary);

    /**
     * Reads the given number of bytes and returns them as an array of characters.
     *
     * Actually, for performance reasons this command only returns a pointer to the current position in the stream,
     * and then increases the position of this pointer by the number of bytes indicated by the given parameter.
     * It is the user's responsibility to actually use only the number of bytes he indicated he would use.
     * It is assumed the user knows what kind of value is stored next in the #Message the data is streamed from.
     * Using a reinterpret_cast on the returned pointer should then restore the original value.
     *
     * Example use case:
     * @code
     * ...
     * inputMessageStream.alignForBasicType(sizeof(int32_t));
     * byte_t* const dataPtr = inputMessageStream.readRaw(sizeof(int32_t));
     * int32_t val = *(reinterpret_cast<int32_t*>(dataPtr));
     * ...
     * @endcode
     */
    COMMONAPI_EXPORT byte_t *_readRaw(const size_t _size);

    /**
     * Handles all reading of basic types from a given #InputStream.
     * Basic types in this context are: uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, float, double.
     * Any types not listed here (especially all complex types, e.g. structs, unions etc.) need to provide a
     * specialized implementation of this operator.
     *
     * @tparam Type_ The type of the value that is to be read from the given stream.
     * @param _value The variable in which the retrieved value is to be stored
     * @return The given inputMessageStream to allow for successive reading
     */
    template<typename Type_>
    COMMONAPI_EXPORT bool _readBitValue(Type_ &_value, uint8_t _bits, bool _isSigned) {
        bool isError(false);
        bool isLittleEndian = static_cast<bool>(buffer_[0]);

        union {
            Type_ typed_;
            char raw_[sizeof(Type_)];
        } value;
        std::memset(value.raw_, 0, sizeof(Type_));

	// sanity check for _bits
        if (_bits > (sizeof(Type_) << 3))
            _bits = (sizeof(Type_) << 3);

        if (_bits == 0 || remaining_ < size_t(((_bits - 1) >> 3) + 1)) {
            isError = true;
        } else {
            if (currentBit_ == 0 && _bits == (sizeof(Type_) << 3) && current_ != NULL) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                if (isLittleEndian) {
                    std::memcpy(value.raw_, current_, sizeof(Type_));
                    current_ += sizeof(Type_);
                } else {
                    byte_t *target = reinterpret_cast<byte_t *>(&value.raw_[sizeof(Type_)-1]);
                    for (size_t i = 0; i < sizeof(Type_); ++i) {
                        *target-- = *current_++;
                    }
                }
#else
                if (isLittleEndian) {
                    byte_t *target = reinterpret_cast<byte_t *>(&value.raw_[sizeof(Type_)-1]);
                    for (size_t i = 0; i < sizeof(Type_); ++i) {
                        *target-- = *current_++;
                    }
                } else {
                    std::memcpy(value.raw_, current_, sizeof(Type_));
                    current_ += sizeof(Type_);
                }
#endif
                remaining_ -= sizeof(Type_);
            } else {
                bool isNegative(false);

                byte_t itsMask(0x00);
                byte_t itsValue;

                byte_t itsCurrentByte(0x0);
                std::size_t itsCurrentCount(1);

                byte_t itsSignedInitializer(0x0);

                // For signed values, the highest bit must be checked to determine
                // whether the value is positive or negative
                if (_isSigned) {
                    std::size_t itsHighestByte(0);

                    // Calculate the remaining bits after the current byte
                    int8_t numberOfBits = int8_t(_bits - (8 - currentBit_));

                    // Calculate the mask for the sign bit
                    itsMask = byte_t(0x1 << (numberOfBits > 0 ?
                            ((numberOfBits - 1) % 8) :
                            ((currentBit_ + _bits - 1) % 8)));

                    // Build the initializer for the highest byte of the target value
                    itsSignedInitializer = byte_t(0xFF << ((_bits-1) % 8));

                    // Find the highest byte
                    while (numberOfBits > 0) {
                        itsHighestByte = std::size_t(itsHighestByte + 1);
                        numberOfBits = int8_t(numberOfBits - 8);
                    }

                    // Check the sign bit
                    byte_t *itsSignedByte = current_ + itsHighestByte;
                    if ((*itsSignedByte) & itsMask) {
                        isNegative = true;
                        // Toggle the bits for negative values
                        std::memset(value.raw_, 0xFF, sizeof(Type_));

                        // If the first byte also is the last,
                        // initialize it with the calculated initializer
                        if (itsCurrentCount == sizeof(Type_))
                            itsCurrentByte = itsSignedInitializer;
                    }
                }

                // Set the target (TODO: Add isLittleEndian_ member and use it here)
                byte_t *target(nullptr);
#if __BYTE_ORDER == __LITTLE_ENDIAN
                if (isLittleEndian)
                    target = reinterpret_cast<byte_t *>(&value.raw_[0]);
                else
                    target = reinterpret_cast<byte_t *>(&value.raw_[sizeof(Type_) - 1]);
#else
                if (isLittleEndian)
                    target = reinterpret_cast<byte_t *>(&value.raw_[sizeof(Type_) - 1]);
                else
                    target = reinterpret_cast<byte_t *>(&value.raw_[0]);
#endif

                uint8_t writePosition = 0;
                while (_bits > 0) {
                    // Determine the number of bits to copy
                    uint8_t maxRead = uint8_t(8 - currentBit_);
                    uint8_t maxWrite = uint8_t(8 - writePosition);
                    if (maxWrite > _bits) maxWrite = _bits;
                    uint8_t numCopy = (maxRead < maxWrite ? maxRead : maxWrite);

                    // Calculate the mask to access the bits
                    itsMask = byte_t(0xFF >> (8 - numCopy));
                    itsMask = byte_t(itsMask << currentBit_);

                    // Get the value
                    itsValue = ((*current_) & itsMask);
                    if (writePosition < currentBit_) {
                        itsValue = byte_t(itsValue >> (currentBit_ - writePosition));
                    } else {
                        itsValue = byte_t(itsValue << (writePosition - currentBit_));
                    }

                    // Update the current byte
                    itsCurrentByte |= itsValue;

                    // Wrap if all bits of the current source byte are consumed
                    _bits = uint8_t(_bits - numCopy);
                    currentBit_ = uint8_t(currentBit_ + numCopy);
                    if (currentBit_ == uint8_t(8)) {
                        current_++;
                        currentBit_ = 0;
                        remaining_--;
                        itsCurrentCount++;
                    }

                    // Update the write position and the target pointer
                    writePosition = uint8_t(writePosition + numCopy);
                    if (writePosition == uint8_t(8) || _bits == 0) {
                        if (isNegative) {
                            (*target) &= itsCurrentByte;
                        } else {
                            (*target) |= itsCurrentByte;
                        }
#if __BYTE_ORDER == __LITTLE_ENDIAN
                        if (isLittleEndian)
                            target++;
                        else
                            target--;
#else
                        if (isLittleEndian)
                            target--;
                        else
                            target++;
#endif
                        writePosition = 0;

                        // Reset current byte
                        if (isNegative && itsCurrentCount == sizeof(Type_)) {
                            itsCurrentByte = itsSignedInitializer;
                        } else {
                            itsCurrentByte = 0;
                        }
                    }
                }
            }
        }

        // Return the target value
         _value = value.typed_;

         return (isError);
    }

private:
    inline void bitAlign() {
        if (currentBit_ > 0) {
            current_++;
            remaining_--;
            currentBit_ = 0;
        }
    }

private:
    byte_t* dataBegin_;
    byte_t* current_;
    uint8_t currentBit_;
    size_t remaining_;
    Message message_;
    bool errorOccurred_;

    std::vector<byte_t> buffer_; // used for handling "Little Endian" messages
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_INPUTSTREAM_HPP_
