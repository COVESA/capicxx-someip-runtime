// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_CLIENT_ID_HPP_
#define COMMONAPI_SOMEIP_CLIENT_ID_HPP_

#include <string>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Types.hpp>
#include <CommonAPI/SomeIP/Types.hpp>

namespace CommonAPI {
namespace SomeIP {

class Message;

/**
 * \brief Implementation of CommonAPI::ClientId for SomeIp
 *
 * This class represents the SomeIp specific implementation of CommonAPI::ClientId.
 */
class COMMONAPI_EXPORT_CLASS_EXPLICIT ClientId : public CommonAPI::ClientId{
    friend struct std::hash< ClientId >;

public:
    COMMONAPI_EXPORT ClientId(client_id_t client_id, uid_t _uid, gid_t _gid);
    COMMONAPI_EXPORT virtual ~ClientId();

    COMMONAPI_EXPORT bool operator==(CommonAPI::ClientId& clientIdToCompare);
    COMMONAPI_EXPORT bool operator==(ClientId& clientIdToCompare);
    COMMONAPI_EXPORT size_t hashCode();

    COMMONAPI_EXPORT client_id_t getClientId();
    COMMONAPI_EXPORT uid_t getUid() const;
    COMMONAPI_EXPORT gid_t getGid() const;

protected:
    client_id_t client_id_;
    uid_t uid_;
    gid_t gid_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CLIENT_ID_HPP_
