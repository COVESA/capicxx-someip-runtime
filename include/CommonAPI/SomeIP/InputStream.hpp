// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_INPUT_STREAM_HPP_
#define COMMONAPI_SOMEIP_INPUT_STREAM_HPP_

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
#include <cassert>
#include <cstring> // memset
#include <string>
#include <vector>
#include <stack>

#include <CommonAPI/Export.hpp>

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
	COMMONAPI_EXPORT virtual InputStream &readValue(int16_t &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(int32_t &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(int64_t &_value, const EmptyDeployment *_depl);

	COMMONAPI_EXPORT virtual InputStream &readValue(uint8_t &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(uint16_t &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(uint64_t &_value, const EmptyDeployment *_depl);

	COMMONAPI_EXPORT virtual InputStream &readValue(float &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(double &_value, const EmptyDeployment *_depl);

	COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const EmptyDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const StringDeployment *_depl);
	COMMONAPI_EXPORT virtual InputStream &readValue(ByteBuffer &_value, const EmptyDeployment *_depl);

	COMMONAPI_EXPORT virtual InputStream &readValue(Version &_value, const EmptyDeployment *_depl);

	COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const uint8_t &_width, const bool &_permitZeroWidth);

	template<typename _Base>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<_Base> &_value, const EmptyDeployment *_depl) {
	    _Base tmpValue;
	    readValue(tmpValue, nullptr);
	    _value = tmpValue;
	    return (*this);
	}

    template<class _Deployment, typename _Base>
	COMMONAPI_EXPORT InputStream &readValue(Enumeration<_Base> &_value, const _Deployment *_depl) {
    	uint8_t width = (_depl ? _depl->width_ : 0);

    	switch (width) {
    	case 1:
    		uint8_t tmpValue1;
    		readValue(tmpValue1, nullptr);
    		_value = static_cast<_Base>(tmpValue1);
    		break;

    	case 2:
    		uint16_t tmpValue2;
    		readValue(tmpValue2, nullptr);
    		_value = static_cast<_Base>(tmpValue2);
    		break;

    	default:
    		_Base tmpValue;
    		readValue(tmpValue, nullptr);
    		_value = tmpValue;
    		break;
    	}

    	return (*this);
    }

    template<typename... _Types>
    COMMONAPI_EXPORT InputStream &readValue(Struct<_Types...> &_value,
                           const EmptyDeployment *_depl) {
        uint32_t itsSize;

        // Read struct size
        readValue(itsSize, 4, true);

        // Read struct fields, if reading size has been successful
        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            const auto itsFieldSize(std::tuple_size<std::tuple<_Types...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<_Types...>, EmptyDeployment>{}(
                (*this), _value, _depl);

            if (itsSize != (remainingBeforeRead - remaining_)) {
                errorOccurred_ = true;
            }
        }

        return (*this);
    }

    template<typename _Deployment, typename... _Types>
	COMMONAPI_EXPORT InputStream &readValue(Struct<_Types...> &_value,
                           const _Deployment *_depl) {
        uint32_t itsSize;

        uint8_t structLengthWidth = (_depl ? _depl->structLengthWidth_ : 4);

        // Read struct size
        readValue(itsSize, structLengthWidth, true);

        // Read struct fields, if reading size has been successful
        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            const auto itsFieldSize(std::tuple_size<std::tuple<_Types...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<_Types...>, _Deployment>{}(
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

    template<typename _Deployment, class _PolymorphicStruct>
	COMMONAPI_EXPORT InputStream &readValue(std::shared_ptr<_PolymorphicStruct> &_value,
                           const _Deployment *_depl) {
        uint32_t serial;
        _readValue(serial);
        if (!hasError()) {
            _value = _PolymorphicStruct::create(serial);
            _value->template readValue<InputStream>(*this, _depl);
        }

        return (*this);
    }

    template<typename _Deployment, typename... _Types>
	COMMONAPI_EXPORT InputStream &readValue(Variant<_Types...> &_value,
                           const _Deployment *_depl) {
        if(_value.hasValue()) {
            DeleteVisitor<_value.maxSize> visitor(_value.valueStorage_);
            ApplyVoidVisitor<DeleteVisitor<_value.maxSize>,
                Variant<_Types...>, _Types... >::visit(visitor, _value);
        }

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

        // CommonAPI variant supports only 255 different union types!
        _value.valueType_ = (itsType > 255) ? 255 : (uint8_t) itsType;

        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            InputStreamReadVisitor<InputStream, _Types...> visitor((*this), _value);
            ApplyStreamVisitor<InputStreamReadVisitor<InputStream, _Types... >,
                Variant<_Types...>, _Deployment, _Types...>::visit(visitor, _value, _depl);

            if (unionLengthWidth != 0) {
                if (itsSize != (remainingBeforeRead - remaining_)) {
                    errorOccurred_ = true;
                }
            } else {
                size_t paddingCount = _depl->unionMaxLength_ - (remainingBeforeRead - remaining_);
                if (paddingCount < 0) {
                    errorOccurred_ = true;
                } else {
                    (void)_readRaw(paddingCount);
                }
            }
        }

        return (*this);
    }

    template<typename _ElementType, typename _ElementDepl>
	COMMONAPI_EXPORT InputStream &readValue(std::vector<_ElementType> &_value,
                           const ArrayDeployment<_ElementDepl> *_depl) {
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
            while (itsSize > 0 || arrayLengthWidth == 0) {
                size_t remainingBeforeRead = remaining_;

                _ElementType itsElement;
                readValue(itsElement, (_depl ? _depl->elementDepl_ : nullptr));
                if (hasError()) {
                    break;
                }

                _value.push_back(std::move(itsElement));

                if (arrayLengthWidth != 0) {
                    itsSize -= (remainingBeforeRead - remaining_);
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

    template<typename _KeyType, typename _ValueType, typename _HasherType>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<_KeyType,
                           _ValueType, _HasherType> &_value,
                           const EmptyDeployment *_depl) {

        typedef typename std::unordered_map<_KeyType, _ValueType, _HasherType>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);

        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            _KeyType itsKey;
            readValue(itsKey, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            _ValueType itsValue;
            readValue(itsValue, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

            itsSize -= (remainingBeforeRead - remaining_);
        }

        if (itsSize != 0) {
            errorOccurred_ = true;
        }

        return (*this);
    }

    template<typename _Deployment, typename _KeyType, typename _ValueType, typename _HasherType>
	COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<_KeyType,
                           _ValueType, _HasherType> &_value,
                           const _Deployment *_depl) {

        typedef typename std::unordered_map<_KeyType, _ValueType, _HasherType>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);

        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            _KeyType itsKey;
            readValue(itsKey, (_depl ? _depl->key_ : nullptr));
            if (hasError()) {
                break;
            }

            _ValueType itsValue;
            readValue(itsValue, (_depl ? _depl->value_ : nullptr));
            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

            itsSize -= (remainingBeforeRead - remaining_);
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
	COMMONAPI_EXPORT InputStream(const Message &_message);
	COMMONAPI_EXPORT InputStream(const InputStream &_stream) = delete;

    /**
     * Destructor; does not call the destructor of the referred #Message. Make sure to maintain a reference to the
     * #Message outside of the stream if you intend to make further use of the message.
     */
	COMMONAPI_EXPORT ~InputStream();

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
     * Handles all reading of basic types from a given #DBusInputMessageStream.
     * Basic types in this context are: uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, float, double.
     * Any types not listed here (especially all complex types, e.g. structs, unions etc.) need to provide a
     * specialized implementation of this operator.
     *
     * @tparam _Type The type of the value that is to be read from the given stream.
     * @param _value The variable in which the retrieved value is to be stored
     * @return The given inputMessageStream to allow for successive reading
     */
    template<typename T>
	COMMONAPI_EXPORT bool _readValue(T &_value) {
        bool isError(false);
        union {
            T typed;
            char raw[sizeof(T)];
        } value;

        if (remaining_ < sizeof(T)) {
            std::memset(value.raw, 0, sizeof(T));
            isError = true;
        } else {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
            char *target = &value.raw[sizeof(T)-1];
            for (size_t i = 0; i < sizeof(T); ++i) {
                *target-- = *current_++;
            }
    #else
            std::memcpy(value.raw, current_, sizeof(T));
            current_ += sizeof(T);
    #endif
            remaining_ -= sizeof(T);
        }

         _value = value.typed;
         return (isError);
    }

private:
    byte_t* dataBegin_;
    byte_t* current_;
    size_t remaining_;
    Message message_;
    bool errorOccurred_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_INPUT_STREAM_HPP_
