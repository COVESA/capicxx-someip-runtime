// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef MANAGERSTUBIMPL_H_
#define MANAGERSTUBIMPL_H_

#include <map>

#include <CommonAPI/CommonAPI.hpp>
#include <v1_0/managed/ManagerStubDefault.hpp>

#include "DeviceStubImpl.hpp"
#include "SpecialDeviceStubImpl.hpp"

using namespace v1_0::managed;

class ManagerStubImpl: public ManagerStubDefault {

public:
    ManagerStubImpl();
    ManagerStubImpl(const std::string);
    virtual ~ManagerStubImpl();

    void deviceDetected(unsigned int);
    void specialDeviceDetected(unsigned int);

    void deviceRemoved(unsigned int);
    void specialDeviceRemoved(unsigned int);
private:
    std::string managerInstanceName;

    typedef std::shared_ptr<DeviceStubImpl> DevicePtr;
    typedef std::shared_ptr<SpecialDeviceStubImpl> SpecialDevicePtr;

    std::map<std::string, DevicePtr> myDevices;
    std::map<std::string, SpecialDevicePtr> mySpecialDevices;

    std::string getDeviceName(unsigned int);
    std::string getSpecialDeviceName(unsigned int);
};

#endif /* MANAGERSTUBIMPL_H_ */
