// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_OUTPUTSTREAM_HPP_
#define COMMONAPI_SOMEIP_OUTPUTSTREAM_HPP_

#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Export.hpp>
#include <CommonAPI/OutputStream.hpp>

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

namespace CommonAPI {
namespace SomeIP {

/**
 * Used to mark the position of a pointer within an array of bytes.
 */
typedef uint32_t position_t;

/**
 * @class OutputMessageStream
 *
 * Used to serialize and write data into a #Message. For all data types that may be written to a #Message, a "<<"-operator should be defined to handle the writing
 * (this operator is predefined for all basic data types and for vectors). The signature that has to be written to the #Message separately is assumed
 * to match the actual data that is inserted via the #OutputMessageStream.
 */
class OutputStream: public CommonAPI::OutputStream<OutputStream> {
public:

    /**
     * Creates a #OutputMessageStream which can be used to serialize and write data into the given #Message. Any data written is buffered within the stream.
     * Remember to call flush() when you are done with writing: Only then the data actually is written to the #Message.
     *
     * @param Message The #Message any data pushed into this stream should be written to.
     */
    COMMONAPI_EXPORT OutputStream(Message message, bool _isLittleEndian = false);

    /**
     * Destructor; does not call the destructor of the referred #Message. Make sure to maintain a reference to the
     * #Message outside of the stream if you intend to make further use of the message, e.g. in order to send it,
     * now that you have written some payload into it.
     */
    COMMONAPI_EXPORT virtual ~OutputStream();

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const bool &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int8_t &_value, const IntegerDeployment<int8_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int16_t &_value, const IntegerDeployment<int16_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int32_t &_value, const IntegerDeployment<int32_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int64_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int64_t &_value, const IntegerDeployment<int64_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint8_t &_value, const IntegerDeployment<uint8_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint16_t &_value, const IntegerDeployment<uint16_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint32_t &_value, const IntegerDeployment<uint32_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint64_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint64_t &_value, const IntegerDeployment<uint64_t> *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const float &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const double &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const std::string &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const std::string &_value, const StringDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const ByteBuffer &_value, const ByteBufferDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const Version &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &_writeValue(const uint32_t &_value, const uint8_t &_width);
    COMMONAPI_EXPORT virtual OutputStream &_writeValueAt(const uint32_t &_value, const uint8_t &_width, const uint32_t &_position);

    template<int minimum, int maximum>
    COMMONAPI_EXPORT OutputStream &writeValue(const RangedInteger<minimum, maximum> &_value, const EmptyDeployment *) {
        if (_value.validate()) {
            writeValue(_value.value_, static_cast<EmptyDeployment *>(nullptr));
        }
        else
            errorOccurred_ = true;
        return (*this);
    }

    template<int minimum, int maximum>
    COMMONAPI_EXPORT OutputStream &writeValue(const RangedInteger<minimum, maximum> &_value, const IntegerDeployment<int> *_depl) {
        if (_value.validate()) {
            _writeBitValue(_value.value_,(_depl ? _depl->bits_ : 32), true);
        }
        else
            errorOccurred_ = true;
        return (*this);
    }

     template<typename Base_>
     COMMONAPI_EXPORT OutputStream &writeValue(const Enumeration<Base_> &_value, const EmptyDeployment *) {
         writeValue(static_cast<Base_>(_value), static_cast<EmptyDeployment *>(nullptr));
         return (*this);
     }

     template<class Deployment_, typename Base_>
     COMMONAPI_EXPORT OutputStream &writeValue(const Enumeration<Base_> &_value, const Deployment_ *_depl) {
         if (_depl != nullptr) {
             uint8_t width = static_cast<uint8_t>(_depl ? (_depl->bits_ >> 3) : 0);
             switch (width) {
                 case 1:
                 {
                     uint8_t value = static_cast<uint8_t>(_value);
                     _writeBitValue(value, _depl->bits_, _depl->isSigned_);
                 }
                 break;

                 case 2:
                 {
                     uint16_t value = static_cast<uint16_t>(_value);
                     _writeBitValue(value, _depl->bits_, _depl->isSigned_);
                 }
                 break;

                 default:
                 {
                     _writeBitValue(_value.value_, _depl->bits_, _depl->isSigned_);
                 }
                 break;
             }
         } else {
             writeValue(_value.value_, static_cast<EmptyDeployment *>(nullptr));
         }
         return (*this);
    }

    template<typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Struct<Types_...> &_value,
                             const EmptyDeployment *_depl) {
        bitAlign();

        // don't write length field as default length width is 0
        if(!hasError()) {
            // Write struct content
            const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructWriter<itsSize-1, OutputStream, Struct<Types_...>, EmptyDeployment>{}((*this), _value, _depl);
        }
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Struct<Types_...> &_value,
                             const Deployment_ *_depl) {
        bitAlign();

        uint8_t structLengthWidth = (_depl ? _depl->structLengthWidth_ : 0);

        if (structLengthWidth != 0) {
            pushPosition();
            // Length field placeholder
            _writeValue(0, structLengthWidth);
            pushPosition(); // Start of struct data
        }

        if(!hasError()) {
            // Write struct content
            const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructWriter<itsSize-1, OutputStream, Struct<Types_...>, Deployment_>{}((*this), _value, _depl);
        }

        // Write actual value of length field
        if (structLengthWidth != 0) {
            size_t length = getPosition() - popPosition();
            size_t position = popPosition();
            _writeValueAt(uint32_t(length), structLengthWidth, uint32_t(position));
        }

        return (*this);
    }

    template<class PolymorphicStruct_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::shared_ptr<PolymorphicStruct_> &_value,
                             const EmptyDeployment *_depl = nullptr) {
        bitAlign();

        if (_value) {
            _writeBitValue(_value->getSerial(), 32, false);
            if (!hasError()) {
                _value->template writeValue<OutputStream>((*this), _depl);
            }
        }
        return (*this);
    }

    template<class PolymorphicStruct_, typename Deployment_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::shared_ptr<PolymorphicStruct_> &_value,
                             const Deployment_ *_depl = nullptr) {
        bitAlign();

        if (_value) {
            _writeBitValue(_value->getSerial(), 32, false);
            if (!hasError()) {
                _value->template writeValue<OutputStream>((*this), _depl);
            }
        }
        return (*this);
    }
    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Variant<Types_...> &_value,
                             const Deployment_ *_depl) {
        bitAlign();

        bool unionDefaultOrder = (_depl ? _depl->unionDefaultOrder_ : true);
        uint8_t unionLengthWidth = (_depl ? _depl->unionLengthWidth_ : 4);
        uint8_t unionTypeWidth = (_depl ? _depl->unionTypeWidth_ : 4);

        if (unionDefaultOrder) {
            pushPosition();
            _writeValue(0, unionLengthWidth);
            _writeValue(uint8_t(_value.getMaxValueType()) - _value.getValueType() + 1, unionTypeWidth);
            pushPosition();
        } else {
            _writeValue(uint8_t(_value.getMaxValueType()) - _value.getValueType() + 1, unionTypeWidth);
            pushPosition();
            _writeValue(0, unionLengthWidth);
            pushPosition();
        }

        if (!hasError()) {
            OutputStreamWriteVisitor<OutputStream> valueVisitor(*this);
            ApplyStreamVisitor<OutputStreamWriteVisitor<OutputStream>,
                 Variant<Types_...>, Deployment_, Types_...>::visit(valueVisitor, _value, _depl);
        }

        size_t length = getPosition() - popPosition();
        size_t position = popPosition();

        // Write actual value of length field
        if (unionLengthWidth != 0) {
            _writeValueAt(uint32_t(length), unionLengthWidth, uint32_t(position));
        } else {
            size_t paddingCount = _depl->unionMaxLength_ - length;
            if (_depl->unionMaxLength_ < length) {
                errorOccurred_ = true;
            } else {
                for(size_t i = 0; i < paddingCount; i++) {
                    _writeValue('\0', 8);
                }
            }
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_,
        typename std::enable_if<(std::is_same<int8_t, ElementType_>::value ||
                                 std::is_same<uint8_t, ElementType_>::value), int>::type = 0>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::vector<ElementType_> &_value,
                             const ArrayDeployment<ElementDepl_> *_depl) {
        bitAlign();

        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        if (arrayLengthWidth != 0) {
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

        if (!hasError()) {
            _writeValue(uint32_t(_value.size()), arrayLengthWidth);
            if (_value.size()) {
                _writeRaw(reinterpret_cast<const byte_t *>(&_value[0]), _value.size());
            }
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_,
        typename std::enable_if<(!std::is_same<int8_t, ElementType_>::value &&
                                 !std::is_same<uint8_t, ElementType_>::value), int>::type = 0>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::vector<ElementType_> &_value,
                             const ArrayDeployment<ElementDepl_> *_depl) {
        bitAlign();

        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        if (arrayLengthWidth != 0) {
            pushPosition();
            // Length field placeholder
            _writeValue(0, arrayLengthWidth);
            pushPosition(); // Start of vector data

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

        if (!hasError()) {
            // Write array/vector content
            for (auto i : _value) {
                writeValue(i, (_depl ? _depl->elementDepl_ : nullptr));
                if (hasError()) {
                    break;
                }
            }
        }

        // Write actual value of length field
        if (arrayLengthWidth != 0) {
            size_t length = getPosition() - popPosition();
            size_t position2Write = popPosition();
            _writeValueAt(uint32_t(length), arrayLengthWidth, uint32_t(position2Write));
        }

        return (*this);
    }

    template<typename Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                             const Deployment_ *_depl) {
        bitAlign();

        uint8_t mapLengthWidth = (_depl ? _depl->mapLengthWidth_ : 4);
        uint32_t mapMinLength = (_depl ? _depl->mapMinLength_ : 0);
        uint32_t mapMaxLength = (_depl ? _depl->mapMaxLength_ : 0xFFFFFFFF);

        if (mapLengthWidth != 0) {
            pushPosition();
            // Length field placeholder
            _writeValue(0, mapLengthWidth);
            pushPosition(); // Start of map data

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

        for (auto v : _value) {
            writeValue(v.first, (_depl ? _depl->key_ : nullptr));
            if (hasError()) {
                return (*this);
            }

            writeValue(v.second, (_depl ? _depl->value_ : nullptr));
            if (hasError()) {
                return (*this);
            }
        }

        // Write actual value of length field
        if (mapLengthWidth != 0) {
            size_t length = getPosition() - popPosition();
            size_t position2Write = popPosition();
            _writeValueAt(uint32_t(length), mapLengthWidth, uint32_t(position2Write));
        }

        return (*this);
    }


    template<typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                             const EmptyDeployment *_depl) {
        bitAlign();

        (void)_depl;

        pushPosition();
        _writeValue(static_cast<uint32_t>(0), 4); // Placeholder
        pushPosition(); // Start of map data

        for (auto v : _value) {
            writeValue(v.first, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                return (*this);
            }

            writeValue(v.second, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                return (*this);
            }
        }

        // Write number of written bytes to placeholder position
        uint32_t length = uint32_t(getPosition() - popPosition());
        _writeValueAt(length, popPosition());

        return (*this);
    }

    COMMONAPI_EXPORT virtual bool hasError() const;

    /**
     * Writes the data that was buffered within this #OutputMessageStream to the #Message that was given to the constructor. Each call to flush()
     * will completely override the data that currently is contained in the #Message. The data that is buffered in this #OutputMessageStream is
     * not deleted by calling flush().
     */
    COMMONAPI_EXPORT void flush();

    template<typename Type_>
    COMMONAPI_EXPORT OutputStream &_writeBitValue(const Type_ &_value, uint8_t _bits, bool _is_signed) {
        union {
            Type_ typed_;
            byte_t raw_[sizeof(Type_)];
        } value;

        // Initialize the source value
        value.typed_ = _value;

	// sanity check bit count
	if (_bits > (sizeof(Type_) << 3))
            _bits = (sizeof(Type_) << 3);

        if (currentBit_ == 0 && _bits == (sizeof(Type_) << 3)) {
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            if (isLittleEndian_) {
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    _writeRaw(value.raw_[i]);
                }
            } else {
                byte_t *source = &value.raw_[sizeof(Type_)-1];
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    _writeRaw(*source--);
                }
            }
        #else
            if (isLittleEndian_) {
                byte_t *source = &value.raw_[sizeof(Type_)-1];
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    _writeRaw(*source--);
                }
            } else {
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    _writeRaw(value.raw_[i]);
                }
            }
        #endif
        } else {
            // For signed types we need at least two bits to make a value
            if (_bits < 1 || (_is_signed && _bits < 2 )) {
                errorOccurred_ = true;
                return (*this);
            }

            // Set the source pointer dependend on the byte orders
            byte_t * source(nullptr);
            std::size_t firstUsedByte(((sizeof(Type_) << 3) - _bits) >> 3);
            #if __BYTE_ORDER == __LITTLE_ENDIAN
            if (isLittleEndian_)
                source = &value.raw_[firstUsedByte];
            else
                source = &value.raw_[sizeof(Type_) - 1 - firstUsedByte];
            #else
            if (isLittleEndian_)
                source = &value.raw_[sizeof(Type_) - 1 - firstUsedByte];
            else
                source = &value.raw_[firstUsedByte];
            #endif

            uint8_t readPosition = 0;
            while (_bits > 0) {
                // Determine number of bits to copy
                uint8_t maxRead = uint8_t(_bits < (8 - readPosition) ? _bits : (8 - readPosition));
                uint8_t maxWrite = uint8_t(8 - currentBit_);
                uint8_t numCopy = uint8_t(maxRead < maxWrite ? maxRead : maxWrite);

                // Calculate mask to access the bits
                byte_t itsMask = byte_t(0xFF >> (8 - numCopy));
                itsMask = byte_t(itsMask << readPosition);

                // Get the value
                byte_t itsValue = uint8_t((*source) & itsMask);
                if (currentBit_ > readPosition)
                    itsValue = byte_t(itsValue << (currentBit_ - readPosition));
                else
                    itsValue = byte_t(itsValue >> (readPosition - currentBit_));

                // Add value to current byte
                currentByte_ |= itsValue;

                // Update number of remaining bits
                _bits = uint8_t(_bits - numCopy);

                // Wrap if necessary
                currentBit_ = uint8_t(currentBit_ + numCopy);
                if (currentBit_ == 8) {
                    _writeRaw(currentByte_);
                    currentBit_ = 0;
                    currentByte_ = 0;
                }

                // Update source pointer if the current byte was consumed
                readPosition = uint8_t(readPosition + numCopy);
                if (readPosition == 8) {
                    readPosition = 0;
                    #if __BYTE_ORDER == __LITTLE_ENDIAN
                    if (isLittleEndian_)
                        source++;
                    else
                        source--;
                    #else
                    if (isLittleEndian_)
                        source--;
                    else
                        source++;
                    #endif
                }
            }
        }

        return (*this);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void _writeValueAt(const Type_ &_value, size_t _position) {
        if (_position + sizeof(Type_) <= payload_.size()) {
            union {
                Type_ typed;
                byte_t raw[sizeof(Type_)];
            } value;
            value.typed = _value;
    #if __BYTE_ORDER == __LITTLE_ENDIAN
            if (isLittleEndian_) {
                _writeRawAt(value.raw, sizeof(Type_), _position);
            } else {
                byte_t reordered[sizeof(Type_)];
                byte_t *source = &value.raw[sizeof(Type_) - 1];
                byte_t *target = reordered;
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    *target++ = *source--;
                }
                _writeRawAt(reordered, sizeof(Type_), _position);
            }
    #else
            if (isLittleEndian_) {
                byte_t reordered[sizeof(Type_)];
                byte_t *source = &value.raw[sizeof(Type_) - 1];
                byte_t *target = reordered;
                for (size_t i = 0; i < sizeof(Type_); ++i) {
                    *target++ = *source--;
                }
                _writeRawAt(reordered, sizeof(Type_), _position);
            } else {
                _writeRawAt(value.raw, sizeof(Type_), _position);
            }
    #endif
        } else {
            COMMONAPI_ERROR("SomeIP::OutputStream::_writeValueAt payload too small ",
                            payload_.size(), " pos: ", _position, " value size", sizeof(Type_));
        }
    }

    /**
     * Fills the stream with 0-bytes to make the next value be aligned to the boundary given.
     * This means that as many 0-bytes are written to the buffer as are necessary
     * to make the next value start with the given alignment.
     *
     * @param _boundary The byte-boundary to which the next value should be aligned.
     */
    COMMONAPI_EXPORT void align(const size_t _boundary);

    /**
     * Takes sizeInByte characters, starting from the character which val points to, and stores them for later writing.
     * When calling flush(), all values that were written to this stream are copied into the payload of the #Message.
     *
     * The array of characters might be created from a pointer to a given value by using a reinterpret_cast. Example:
     * @code
     * ...
     * int32_t val = 15;
     * outputMessageStream.alignForBasicType(sizeof(int32_t));
     * const char* const reinterpreted = reinterpret_cast<const char*>(&val);
     * outputMessageStream.writeValue(reinterpreted, sizeof(int32_t));
     * ...
     * @endcode
     *
     * @param _data The array of chars that should serve as input
     * @param _size The number of bytes that should be written
     * @return true if writing was successful, false otherwise.
     *
     * @see OutputMessageStream()
     * @see flush()
     */
    COMMONAPI_EXPORT void _writeRaw(const byte_t &_data);
    COMMONAPI_EXPORT void _writeRaw(const byte_t *_data, const size_t _size);
    COMMONAPI_EXPORT void _writeRawFill(const byte_t _data, const size_t _size);
    COMMONAPI_EXPORT void _writeRawAt(const byte_t *_data, const size_t _size, const size_t _position);

    COMMONAPI_EXPORT void _writeBom(const StringDeployment *_depl);

protected:
    std::vector<byte_t> payload_;

private:
    COMMONAPI_EXPORT size_t getPosition();
    COMMONAPI_EXPORT void pushPosition();
    COMMONAPI_EXPORT size_t popPosition();

    inline void bitAlign() {
        if (currentBit_ != 0) {
            _writeRaw(currentByte_);
            currentByte_ = 0x0;
            currentBit_ = 0;
        }
    }

    Message message_;
    bool errorOccurred_;

    std::vector<size_t> positions_;

    byte_t currentByte_;
    uint8_t currentBit_;

    bool isLittleEndian_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_OUTPUTSTREAM_HPP_
