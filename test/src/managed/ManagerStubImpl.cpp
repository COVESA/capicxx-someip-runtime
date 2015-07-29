// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sstream>
#include <iomanip>

#include "ManagerStubImpl.hpp"

using namespace v1_0::managed;

ManagerStubImpl::ManagerStubImpl() {
}

ManagerStubImpl::ManagerStubImpl(const std::string instanceName) {
    managerInstanceName = instanceName;
}

ManagerStubImpl::~ManagerStubImpl() {
}

void ManagerStubImpl::deviceDetected(unsigned int n) {
    std::cout << "Device " << n << " detected!" << std::endl;

    std::string deviceInstanceName = getDeviceName(n);
    myDevices[deviceInstanceName] = DevicePtr(new DeviceStubImpl);
    const bool deviceRegistered = this->registerManagedStubDevice(myDevices[deviceInstanceName], deviceInstanceName);

    if (!deviceRegistered) {
        std::cout << "Error: Unable to register device: " << deviceInstanceName << std::endl;
    }
}

void ManagerStubImpl::specialDeviceDetected(unsigned int n) {
    std::cout << "Special device " << n << " detected!" << std::endl;

    std::string specialDeviceInstanceName = getSpecialDeviceName(n);
    mySpecialDevices[specialDeviceInstanceName] = SpecialDevicePtr(new SpecialDeviceStubImpl);
    const bool specialDeviceRegistered = this->registerManagedStubSpecialDevice(mySpecialDevices[specialDeviceInstanceName],
                                                                                   specialDeviceInstanceName);

    if (!specialDeviceRegistered) {
        std::cout << "Error: Unable to register special device: " << specialDeviceInstanceName << std::endl;
    }
}

void ManagerStubImpl::deviceRemoved(unsigned int n) {
    std::cout << "Device " << n << " removed!" << std::endl;

    std::string deviceInstanceName = getDeviceName(n);
    const bool deviceDeregistered = this->deregisterManagedStubDevice(deviceInstanceName);

    if (!deviceDeregistered) {
        std::cout << "Error: Unable to deregister device: " << deviceInstanceName << std::endl;
    } else {
        myDevices.erase(deviceInstanceName);
    }
}

void ManagerStubImpl::specialDeviceRemoved(unsigned int n) {
    std::cout << "Special device " << n << " removed!" << std::endl;

    std::string specialDeviceInstanceName = getSpecialDeviceName(n);
    const bool specialDeviceDeregistered = this->deregisterManagedStubSpecialDevice(specialDeviceInstanceName);

    if (!specialDeviceDeregistered) {
        std::cout << "Error: Unable to deregister special device: " << specialDeviceInstanceName << std::endl;
    } else {
        mySpecialDevices.erase(specialDeviceInstanceName);
    }
}

std::string ManagerStubImpl::getDeviceName(unsigned int n) {
    std::stringstream ss;
    ss << managerInstanceName << ".device" << std::setw(2) << std::hex << std::setfill('0') << n;
    return ss.str();
}

std::string ManagerStubImpl::getSpecialDeviceName(unsigned int n) {
    std::stringstream ss;
    ss << managerInstanceName << ".specialDevice" << std::setw(2) << std::hex << std::setfill('0') << n;
    return ss.str();
}
