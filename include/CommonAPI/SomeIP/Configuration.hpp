// Copyright (C) 2014-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_CONFIGURATION_HPP_
#define COMMONAPI_SOMEIP_CONFIGURATION_HPP_

#include <CommonAPI/Export.hpp>
#include <CommonAPI/Address.hpp>
#include <CommonAPI/SomeIP/Address.hpp>

namespace CommonAPI {
namespace SomeIP {

class Configuration {
public:
	static std::shared_ptr<Configuration> get();
	static void load();

	Configuration();
	~Configuration() = default;

	COMMONAPI_EXPORT int64_t getMaxProcessingTime(const std::string &_name) const;
	COMMONAPI_EXPORT std::size_t getMaxQueueSize(const std::string &_name) const;

    COMMONAPI_EXPORT const Address & getAddressAlias(const Address &_address) const;
    COMMONAPI_EXPORT method_id_t getMethodAlias(const Address &_address,
            const method_id_t _method) const;
    COMMONAPI_EXPORT eventgroup_id_t getEventgroupAlias(const Address &_address,
            const eventgroup_id_t _eventgroup) const;

private:
    COMMONAPI_EXPORT void readConfiguration();
    COMMONAPI_EXPORT void readServiceAlias(const std::string &_source,
            const std::string &_target);
    COMMONAPI_EXPORT void readMethodAlias(const std::string &_source,
            const std::string &_target);
    COMMONAPI_EXPORT void readEventgroupAlias(const std::string &_source,
            const std::string &_target);
    COMMONAPI_EXPORT bool readValue(const std::string &_data,
            Address &_sourceAddress, uint16_t &_id, bool _readId);

    COMMONAPI_EXPORT bool isValidMethod(const method_id_t) const;
    COMMONAPI_EXPORT bool isValidEventgroup(const eventgroup_id_t) const;

    std::string defaultConfig_;

    typedef std::map<method_id_t, method_id_t> MethodAlias_t;
    typedef std::map<eventgroup_id_t, eventgroup_id_t> EventgroupAlias_t;
    typedef std::tuple<Address, MethodAlias_t, EventgroupAlias_t> Alias_t;
    std::map<Address, Alias_t > aliases_;

    std::map<std::string, std::pair<int64_t, std::size_t> > watchLimits_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_CONFIGURATION_HPP_
