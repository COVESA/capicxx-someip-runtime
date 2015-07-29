// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SPECIALDEVICESTUBIMPL_H_
#define SPECIALDEVICESTUBIMPL_H_

#include <v1_0/managed/SpecialDeviceStubDefault.hpp>

using namespace v1_0::managed;

class SpecialDeviceStubImpl: public SpecialDeviceStubDefault {
public:
    SpecialDeviceStubImpl();
    virtual ~SpecialDeviceStubImpl();

    void doSomethingSpecial();
};

#endif /* SPECIALDEVICESTUBIMPL_H_ */
