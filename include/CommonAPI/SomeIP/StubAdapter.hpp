// Copyright (C) 2013-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_STUBADAPTER_HPP_
#define COMMONAPI_SOMEIP_STUBADAPTER_HPP_

#include <memory>
#include <set>
#include <string>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Stub.hpp>
#include <CommonAPI/SomeIP/Address.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/InterfaceHandler.hpp>
#include <CommonAPI/SomeIP/Message.hpp>

namespace CommonAPI {
namespace SomeIP {

class ObjectManagerStub;
class Factory;

class COMMONAPI_EXPORT_CLASS_EXPLICIT StubAdapter: virtual public CommonAPI::StubAdapter, public InterfaceHandler {
 public:
     COMMONAPI_EXPORT StubAdapter(const Address &_address,
                const std::shared_ptr<ProxyConnection> &_connection);

     COMMONAPI_EXPORT virtual ~StubAdapter();

     COMMONAPI_EXPORT virtual void init(std::shared_ptr<StubAdapter> instance);
     COMMONAPI_EXPORT virtual void deinit();

     COMMONAPI_EXPORT const Address &getSomeIpAddress() const;
     COMMONAPI_EXPORT const std::shared_ptr< ProxyConnection > & getConnection() const;

     COMMONAPI_EXPORT bool isManagingInterface();

     COMMONAPI_EXPORT void registerEvent(event_id_t _event,
             const std::set<eventgroup_id_t> &_eventGroups, event_type_e _type,
             reliability_type_e _reliability);
     COMMONAPI_EXPORT void unregisterEvent(event_id_t _event);

     COMMONAPI_EXPORT virtual bool onInterfaceMessage(const Message &message) = 0;

     COMMONAPI_EXPORT virtual void registerSelectiveEventHandlers() = 0;
     COMMONAPI_EXPORT virtual void unregisterSelectiveEventHandlers() = 0;

protected:
    Address someipAddress_;
    const std::shared_ptr<ProxyConnection> connection_;
};


} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_STUBADAPTER_HPP_
