// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <typeinfo>

#include <CommonAPI/SomeIP/ClientId.hpp>
#include <CommonAPI/SomeIP/Message.hpp>

namespace std {

template<>
struct hash<CommonAPI::SomeIP::ClientId> {
public:
    size_t operator()(CommonAPI::SomeIP::ClientId* clientIdToHash) const {
        return std::hash<CommonAPI::SomeIP::client_id_t>()(clientIdToHash->client_id_);
    }
};

} // namespace std

namespace CommonAPI {
namespace SomeIP {

ClientId::ClientId(client_id_t client_id, uid_t _uid, gid_t _gid)
    : client_id_(client_id), uid_(_uid), gid_(_gid) {
}

ClientId::~ClientId() {
}

bool ClientId::operator==(CommonAPI::ClientId& clientIdToCompare) {
    try {
        ClientId clientIdToCompareSomeIp = ClientId(dynamic_cast<ClientId&>(clientIdToCompare));
        return (clientIdToCompareSomeIp == *this);
    }
    catch (...) {
        return false;
    }
}

bool ClientId::operator==(ClientId& clientIdToCompare) {
    return clientIdToCompare.client_id_ == client_id_;
}

size_t ClientId::hashCode() {
    return std::hash<ClientId>()(this);
}

client_id_t ClientId::getClientId() {
    return client_id_;
}

uid_t ClientId::getUid() const {
    return uid_;
}

gid_t ClientId::getGid() const {
    return gid_;
}

} // namespace SomeIP
} // namespace CommonAPI
