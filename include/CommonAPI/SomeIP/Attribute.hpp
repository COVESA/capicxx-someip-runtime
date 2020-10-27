// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_ATTRIBUTE_HPP_
#define COMMONAPI_SOMEIP_ATTRIBUTE_HPP_

#include <cstdint>
#include <tuple>

#include <CommonAPI/SomeIP/Constants.hpp>
#include <CommonAPI/SomeIP/Event.hpp>
#include <CommonAPI/SomeIP/ProxyHelper.hpp>
#include <CommonAPI/Logger.hpp>

namespace CommonAPI {
namespace SomeIP {

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class ReadonlyAttribute: public AttributeType_ {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef AttributeDepl_ ValueTypeDepl;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    ReadonlyAttribute(Proxy &_proxy,
                      const method_id_t _getMethodId,
                      const bool _getReliable,
                      const bool _isLittleEndian,
                      AttributeDepl_ *_depl = nullptr)
        : proxy_(_proxy),
          getMethodId_(_getMethodId),
          getReliable_(_getReliable),
          isLittleEndian_(_isLittleEndian),
          depl_(_depl) {
    }

    void getValue(CallStatus &_status, ValueType &_value, const CommonAPI::CallInfo *_info) const {

        if (getMethodId_ != 0) {
            CommonAPI::Deployable<ValueType, AttributeDepl_> deployedValue(depl_);
            ProxyHelper<
                SerializableArguments<>,
                SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
            >::callMethodWithReply(
                    proxy_,
                    getMethodId_,
                    getReliable_,
                    isLittleEndian_,
                    (_info ? _info : &defaultCallInfo),
                    _status,
                    deployedValue);
            _value = deployedValue.getValue();
        } else {
            _status = CommonAPI::CallStatus::NOT_AVAILABLE;
            COMMONAPI_ERROR("Wrong deployment configuration: SomeIpGetterID is set to zero.");
        }
    }

    std::future<CallStatus> getValueAsync(AttributeAsyncCallback _callback, const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedValue(depl_);

        if (getMethodId_ != 0) {
            return ProxyHelper<
                        SerializableArguments<>,
                        SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
                >::callMethodAsync(
                    proxy_,
                    getMethodId_,
                    getReliable_,
                    isLittleEndian_,
                    (_info ? _info : &defaultCallInfo),
                    [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, AttributeDepl_> _response) {
                        _callback(_status, _response.getValue());
                    },
                    std::make_tuple(deployedValue));
        } else {
            CommonAPI::CallStatus callStatus = CommonAPI::CallStatus::NOT_AVAILABLE;
            COMMONAPI_ERROR("Wrong deployment configuration: SomeIpGetterID is set to zero.");
            ProxyHelper<
                SerializableArguments<>,
                SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
                >::callCallbackForCallStatus(
                    callStatus,
                    [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, AttributeDepl_> _response) {
                        _callback(_status, _response.getValue());
                    },
                    std::make_tuple(deployedValue));

            std::promise<CommonAPI::CallStatus> promise;
            promise.set_value(callStatus);
            return promise.get_future();
        }
    }

protected:
    Proxy &proxy_;
    const method_id_t getMethodId_;
    const bool getReliable_;
    const bool isLittleEndian_;
    AttributeDepl_ *depl_;
};

template <typename AttributeType_, typename AttributeDepl_ = EmptyDeployment>
class Attribute: public ReadonlyAttribute<AttributeType_, AttributeDepl_> {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;

    Attribute(Proxy &_proxy,
              const method_id_t _getMethodId,
              const bool _getReliable,
              const bool _isLittleEndian,
              const method_id_t _setMethodId,
              const bool _setReliable,
              AttributeDepl_ *_depl = nullptr)
        : ReadonlyAttribute< AttributeType_, AttributeDepl_>(_proxy, _getMethodId, _getReliable, _isLittleEndian, _depl),
          setMethodId_(_setMethodId),
          setReliable_(_setReliable) {
    }

    void setValue(const ValueType &_request,
                  CallStatus &_status,
                  ValueType &_response,
                  const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedResponse(this->depl_);
        ProxyHelper<
            SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>,
            SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
        >::callMethodWithReply(
                this->proxy_,
                setMethodId_,
                setReliable_,
                this->isLittleEndian_,
                (_info ? _info : &defaultCallInfo),
                deployedRequest,
                _status,
                deployedResponse);
        _response = deployedResponse.getValue();
    }

    std::future<CommonAPI::CallStatus> setValueAsync(const ValueType &_request,
                                                     AttributeAsyncCallback _callback,
                                                     const CommonAPI::CallInfo *_info) {
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedRequest(_request, this->depl_);
        CommonAPI::Deployable<ValueType, AttributeDepl_> deployedResponse(this->depl_);
        return ProxyHelper<
                    SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>,
                    SerializableArguments<CommonAPI::Deployable<ValueType, AttributeDepl_>>
               >::callMethodAsync(this->proxy_,
                                  setMethodId_,
                                  setReliable_,
                                  this->isLittleEndian_,
                                  (_info ? _info : &defaultCallInfo),
                                  deployedRequest,
                                  [_callback](CommonAPI::CallStatus _status, CommonAPI::Deployable<ValueType, AttributeDepl_> _response) {
                                      _callback(_status, _response.getValue());
                                  },
                                  std::make_tuple(deployedResponse));
    }

protected:
    const method_id_t setMethodId_;
    const bool setReliable_;
};

template <typename AttributeType_>
class ObservableAttribute: public AttributeType_ {
public:
    typedef typename AttributeType_::ValueType ValueType;
    typedef typename AttributeType_::ValueTypeDepl ValueTypeDepl;
    typedef typename AttributeType_::AttributeAsyncCallback AttributeAsyncCallback;
    typedef typename AttributeType_::ChangedEvent ChangedEvent;

    template <typename... AttributeType_Arguments>
    ObservableAttribute(Proxy &_proxy,
                        const eventgroup_id_t _eventgroupId,
                        const event_id_t _eventId,
                        const method_id_t _getMethodId,
                        const bool _getReliable,
                        const reliability_type_e _reliabilityType,
                        const bool _isLittleEndian,
                        AttributeType_Arguments... _arguments)
        : AttributeType_(_proxy, _getMethodId, _getReliable, _isLittleEndian, _arguments...),
          changedEvent_(_proxy,
                        _eventgroupId,
                        _eventId,
                        CommonAPI::SomeIP::event_type_e::ET_FIELD,
                        _reliabilityType,
                        _isLittleEndian,
                        std::make_tuple(CommonAPI::Deployable<ValueType, ValueTypeDepl>(this->depl_))) {
    }

    ChangedEvent& getChangedEvent() {
        return changedEvent_;
    }

protected:
    Event<ChangedEvent, CommonAPI::Deployable<ValueType, ValueTypeDepl>> changedEvent_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_ATTRIBUTE_HPP_
