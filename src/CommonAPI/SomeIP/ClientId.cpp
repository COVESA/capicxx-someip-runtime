// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <typeinfo>

#include <CommonAPI/Logger.hpp>
#include <CommonAPI/SomeIP/ClientId.hpp>
#include <CommonAPI/SomeIP/Helper.hpp>
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

ClientId::ClientId()
        : client_id_ {0xffff},
          uid_ {0xffffffff},
          gid_ {0xffffffff} {
}

ClientId::ClientId(client_id_t _client,
        const vsomeip_sec_client_t *_sec_client, const std::string &_env)
    : client_id_ {_client},
      uid_ {_sec_client ? _sec_client->user : 0xffffffff},
      gid_ {_sec_client ? _sec_client->group : 0xffffffff},
      env_(_env) {

    if (_sec_client)
        hostAddress_ = addressToString(_sec_client->host);
}

ClientId::ClientId(client_id_t _client,
        const vsomeip_sec_client_t &_sec_client, const std::string &_env)
    : ClientId(_client, &_sec_client, _env) {
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

std::string ClientId::getHostAddress() const {
    return hostAddress_;
}

std::string ClientId::getEnv() const {
    return env_;
}

std::shared_ptr<ClientId>
ClientId::getSomeIPClient(const std::shared_ptr<CommonAPI::ClientId> _client) {

    std::shared_ptr<ClientId> client = std::dynamic_pointer_cast<ClientId, CommonAPI::ClientId>(_client);

    if (client == nullptr)
    {
        COMMONAPI_ERROR("CommonAPI::SomeIP::getSomeIPClient dynamic_pointer_cast returning nullptr.");
    }

    return client;
}

} // namespace SomeIP
} // namespace CommonAPI
