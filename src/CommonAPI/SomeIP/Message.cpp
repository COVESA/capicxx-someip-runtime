// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Connection.hpp>

namespace CommonAPI {
namespace SomeIP {

Message::Message()
    : message_(nullptr) {
}

Message::Message(const std::shared_ptr<vsomeip::message> &_source)
    : message_(_source) {
}

Message::Message(const Message &_source)
    : message_(_source.message_) {
}

Message::Message(Message &&_source)
    : message_(_source.message_) {
    _source.message_ = nullptr;
}

Message::~Message() {
}

Message &
Message::operator=(const Message &_source) {
    if (this != &_source) {
        message_ = _source.message_;
    }

    return (*this);
}

Message &
Message::operator=(Message &&_source) {
    if (this != &_source) {
        message_ = _source.message_;
        _source.message_ = nullptr;
    }

    return (*this);
}

Message::operator bool() const {
    return (message_ != NULL);
}

Message
Message::createMethodCall(const Address &_address, const method_id_t _method, bool _reliable) {
    std::shared_ptr<vsomeip::message> message(
        vsomeip::runtime::get()->create_request(_reliable)
    );
    message->set_service(_address.getService());
    message->set_instance(_address.getInstance());
    message->set_method(_method);
    message->set_interface_version(_address.getMajorVersion());
    return Message(message);
}

Message
Message::createResponseMessage() const {
    std::shared_ptr<vsomeip::message> message(
        vsomeip::runtime::get()->create_response(message_)
    );
    return Message(message);
}

Message
Message::createErrorResponseMessage(return_code_e _return_code) const {
    std::shared_ptr<vsomeip::message> message(
        vsomeip::runtime::get()->create_response(message_)
    );
    message->set_message_type(message_type_e::MT_ERROR);
    message->set_return_code(_return_code);
    return Message(message);
}

Message
Message::createNotificationMessage(
        const Address &_address, const event_id_t _event, bool _reliable) {
    std::shared_ptr<vsomeip::message> message(
        vsomeip::runtime::get()->create_notification(_reliable)
    );
    message->set_service(_address.getService());
    message->set_instance(_address.getInstance());
    message->set_method(_event);
    message->set_interface_version(_address.getMajorVersion());
    return Message(message);
}

bool
Message::isResponseType() const {
    return (message_ && (message_->get_message_type() == message_type_e::MT_RESPONSE));
}

bool
Message::isErrorType() const {
    return (!message_ || (message_->get_message_type() == message_type_e::MT_ERROR));
}

bool Message::isRequestType() const {
    return (!message_ || (message_->get_message_type() == message_type_e::MT_REQUEST));
}

bool Message::isRequestNoResponseType() const {
    return (!message_ || (message_->get_message_type() == message_type_e::MT_REQUEST_NO_RETURN));
}

bool Message::isInitialValue() const {
    return message_->is_initial();
}

byte_t *
Message::getBodyData() const {
    std::shared_ptr<vsomeip::payload> payload = message_->get_payload();

    if(!payload) {
        return NULL;
    }

    return payload->get_data();
}

message_length_t
Message::getBodyLength() const {
    std::shared_ptr<vsomeip::payload> payload = message_->get_payload();

    if(!payload) {
        return 0;
    }

    return payload->get_length();
}

return_code_e
Message::getReturnCode() const {
    return message_->get_return_code();
}

service_id_t
Message::getServiceId() const {
    return message_->get_service();
}

instance_id_t
Message::getInstanceId() const {
    return message_->get_instance();
}

method_id_t
Message::getMethodId() const {
    return message_->get_method();
}

client_id_t
Message::getClientId() const {
    return message_->get_client();
}

session_id_t
Message::getSessionId() const {
    return message_->get_session();
}

void
Message::setPayloadData(const byte_t* data, message_length_t length) {
    std::shared_ptr<vsomeip::payload> payload = message_->get_payload();
    if(!payload) {
        payload = vsomeip::runtime::get()->create_payload();
        message_->set_payload(payload);
    }
    payload->set_data(data, length);
}


bool Message::isValidCRC() const {
    return message_->is_valid_crc();
}

uid_t Message::getUid() const {
    return message_->get_uid();
}

gid_t Message::getGid() const {
    return message_->get_gid();
}

} // namespace SomeIP
} // namespace CommonAPI
