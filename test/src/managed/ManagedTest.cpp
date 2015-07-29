// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <thread>
#include <iostream>
#include <mutex>
#include <set>

#include <CommonAPI/CommonAPI.hpp>
#include "ManagerStubImpl.hpp"
#include "DeviceStubImpl.hpp"
#include "SpecialDeviceStubImpl.hpp"

#include <v1_0/managed/ManagerProxy.hpp>

#include <gtest/gtest.h>

using namespace v1_0::managed;

const std::string &domain = "local";
const static std::string managerInstanceName = "managed-test.Manager";
const static std::string connectionIdService = "service-sample";
const static std::string connectionIdClient = "client-sample";

const static std::string interfaceDevice = "managed.Device";
const static std::string addressDevice1 = "local:" + interfaceDevice + ":managed-test.Manager.device01";
const static std::string addressDevice2 = "local:" + interfaceDevice + ":managed-test.Manager.device02";
const static std::string interfaceSpecialDevice = "managed.SpecialDevice";
const static std::string addressSpecialDevice1 = "local:" + interfaceSpecialDevice + ":managed-test.Manager.specialDevice00";

class ManagedTest: public ::testing::Test {

public:
ManagedTest() :
    received_(false),
    serviceRegistered_(false),
    subscriptionIdSpecialDevice_(0),
    subscriptionIdDevice_(0),
    deviceAvailableCount_(0),
    specialDeviceAvailableCount_(0),
    specialDeviceAvailableDesiredValue_(0),
    deviceAvailableDesiredValue_(0),
    instanceAvailabilityStatusCallbackCalled_(false) {}

protected:
    void SetUp() {
        CommonAPI::Runtime::setProperty("LibraryBase", "SomeIPTests");
        runtime_ = CommonAPI::Runtime::get();
        ASSERT_TRUE((bool)runtime_);
        std::mutex availabilityMutex;
        std::unique_lock<std::mutex> lock(availabilityMutex);
        std::condition_variable cv;
        bool proxyAvailable = false;

        std::thread t1([this, &proxyAvailable, &cv, &availabilityMutex]() {
            std::lock_guard<std::mutex> lock(availabilityMutex);

            testProxy_ = runtime_->buildProxy<ManagerProxy>(domain, managerInstanceName, connectionIdClient);
            testProxy_->isAvailableBlocking();
            ASSERT_TRUE((bool)testProxy_);
            proxyAvailable = true;
            cv.notify_one();
        });

        testStub_ = std::make_shared<ManagerStubImpl>(managerInstanceName);
        serviceRegistered_ = runtime_->registerService(domain, managerInstanceName, testStub_, connectionIdService);
        ASSERT_TRUE(serviceRegistered_);

        while(!proxyAvailable) {
            cv.wait(lock);
        }
        t1.join();
        ASSERT_TRUE(testProxy_->isAvailable());

        // Get the events
        CommonAPI::ProxyManager::InstanceAvailabilityStatusChangedEvent& deviceEvent =
                testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusChangedEvent();
        CommonAPI::ProxyManager::InstanceAvailabilityStatusChangedEvent& specialDeviceEvent =
                testProxy_->getProxyManagerSpecialDevice().getInstanceAvailabilityStatusChangedEvent();

        // bind callbacks to member functions
        newDeviceAvailableCallbackFunc_ = std::bind(
                &ManagedTest::newDeviceAvailableCallback, this,
                std::placeholders::_1, std::placeholders::_2);
        newSpecialDeviceAvailableCallbackFunc_ = std::bind(
                &ManagedTest::newSpecialDeviceAvailableCallback, this,
                std::placeholders::_1, std::placeholders::_2);

        // register callbacks
        subscriptionIdSpecialDevice_ = specialDeviceEvent.subscribe(
                newSpecialDeviceAvailableCallbackFunc_);
        ASSERT_EQ(subscriptionIdSpecialDevice_,
                static_cast<CommonAPI::Event<>::Subscription>(0));

        subscriptionIdDevice_ = deviceEvent.subscribe(
                newDeviceAvailableCallbackFunc_);
        ASSERT_EQ(subscriptionIdDevice_,
                static_cast<CommonAPI::Event<>::Subscription>(0));
    }

    void TearDown() {
        // Unregister callbacks
        CommonAPI::ProxyManager::InstanceAvailabilityStatusChangedEvent& deviceEvent =
                testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusChangedEvent();
        CommonAPI::ProxyManager::InstanceAvailabilityStatusChangedEvent& specialDeviceEvent =
                testProxy_->getProxyManagerSpecialDevice().getInstanceAvailabilityStatusChangedEvent();
        specialDeviceEvent.unsubscribe(subscriptionIdSpecialDevice_);
        deviceEvent.unsubscribe(subscriptionIdDevice_);
        runtime_->unregisterService(domain, ManagerStub::StubInterface::getInterface(), managerInstanceName);
    }

    void newDeviceAvailableCallback(const std::string _address,
                            const CommonAPI::AvailabilityStatus _status) {
        ASSERT_TRUE(_address == addressDevice1 || _address == addressDevice2);
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        if(_status == CommonAPI::AvailabilityStatus::AVAILABLE) {
            std::cout << "New device available: " << _address << std::endl;
            deviceAvailableCount_++;
            devicesAvailable_.insert(_address);
        }

        if(_status == CommonAPI::AvailabilityStatus::NOT_AVAILABLE) {
            std::cout << "Device removed: " << _address << std::endl;
            deviceAvailableCount_--;
            devicesAvailable_.erase(_address);
        }
    }

    void newSpecialDeviceAvailableCallback(
            const std::string _address,
            const CommonAPI::AvailabilityStatus _status) {
        ASSERT_TRUE(_address == addressSpecialDevice1);
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        if(_status == CommonAPI::AvailabilityStatus::AVAILABLE) {
            std::cout << "New device available: " << _address << std::endl;
            specialDeviceAvailableCount_++;
            specialDevicesAvailable_.insert(_address);
        }

        if(_status == CommonAPI::AvailabilityStatus::NOT_AVAILABLE) {
            std::cout << "Device removed: " << _address << std::endl;
            specialDeviceAvailableCount_--;
            specialDevicesAvailable_.erase(_address);
        }
    }

    bool checkInstanceAvailabilityStatus(
            CommonAPI::ProxyManager* _proxyMananger,
            const std::string& _instanceAddress) {
        CommonAPI::CallStatus callStatus(CommonAPI::CallStatus::UNKNOWN);
        CommonAPI::AvailabilityStatus availabilityStatus(
                CommonAPI::AvailabilityStatus::UNKNOWN);
        _proxyMananger->getInstanceAvailabilityStatus(_instanceAddress,
                callStatus, availabilityStatus);
        if(callStatus == CommonAPI::CallStatus::SUCCESS
                && availabilityStatus
                        == CommonAPI::AvailabilityStatus::AVAILABLE)
            return true;
        else
            return false;
    }

    void getAvailableInstancesAsyncSpecialDeviceCallback(
            const CommonAPI::CallStatus &_callStatus,
            const std::vector<std::string> &_availableSpecialDevices) {
        ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, _callStatus);
        ASSERT_EQ(_availableSpecialDevices.size(),
                static_cast<std::vector<std::string>::size_type>(specialDeviceAvailableDesiredValue_));
        bool allDetectedAreAvailable = false;
        for(auto &i : _availableSpecialDevices) {
            for(auto &j : specialDevicesAvailable_) {
                if(i == j) {
                    allDetectedAreAvailable = true;
                    break;
                }
            }
            ASSERT_TRUE(allDetectedAreAvailable);
        }
    }

    void getAvailableInstancesAsyncDeviceCallback(
            const CommonAPI::CallStatus &_callStatus,
            const std::vector<std::string> &_availableDevices) {
        ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, _callStatus);
        ASSERT_EQ(_availableDevices.size(),
                static_cast<std::vector<std::string>::size_type>(deviceAvailableDesiredValue_));
        bool allDetectedAreAvailable = false;
        for(auto &i : _availableDevices) {
            for(auto &j : devicesAvailable_) {
                if(i == j) {
                    allDetectedAreAvailable = true;
                    break;
                }
            }
            ASSERT_TRUE(allDetectedAreAvailable);
        }
    }

    void getInstanceAvailabilityStatusAsyncCallbackAvailable(
            const CommonAPI::CallStatus &_callStatus,
            const CommonAPI::AvailabilityStatus &_availabilityStatus) {
        ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, _callStatus);
        ASSERT_EQ(CommonAPI::AvailabilityStatus::AVAILABLE, _availabilityStatus);
        instanceAvailabilityStatusCallbackCalled_ = true;
    }

    void getInstanceAvailabilityStatusAsyncCallbackNotAvailable(
            const CommonAPI::CallStatus &_callStatus,
            const CommonAPI::AvailabilityStatus &_availabilityStatus) {
        ASSERT_EQ(CommonAPI::CallStatus::SUCCESS, _callStatus);
        ASSERT_EQ(CommonAPI::AvailabilityStatus::NOT_AVAILABLE, _availabilityStatus);
        instanceAvailabilityStatusCallbackCalled_ = true;
    }

    bool received_;
    bool serviceRegistered_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<ManagerProxy<>> testProxy_;
    std::shared_ptr<ManagerStubImpl> testStub_;

    std::function<void(const std::string, const CommonAPI::AvailabilityStatus)> newDeviceAvailableCallbackFunc_;
    std::function<void(const std::string, const CommonAPI::AvailabilityStatus)> newSpecialDeviceAvailableCallbackFunc_;
    CommonAPI::Event<>::Subscription subscriptionIdSpecialDevice_;
    CommonAPI::Event<>::Subscription subscriptionIdDevice_;

    int deviceAvailableCount_;
    int specialDeviceAvailableCount_;

    std::mutex deviceAvailableCountMutex_;
    std::mutex specialDeviceAvailableCountMutex_;

    std::set<std::string> devicesAvailable_;
    std::set<std::string> specialDevicesAvailable_;

    int specialDeviceAvailableDesiredValue_;
    int deviceAvailableDesiredValue_;

    bool instanceAvailabilityStatusCallbackCalled_;
};

/**
 * @test
 *  - Subscribe on the events about availability status changes at the manager
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Remove the managed interface from the manager
 *  - Check that the client is notified about the removed interface
 */
TEST_F(ManagedTest, AddRemoveManagedInterfaceSingle) {
    // Add
    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }

    // Remove
    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 0);
    }
}

/**
 * @test
 *  - Subscribe on the events about availability status changes at the manager
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Add a second instance of the same managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, AddRemoveManagedInterfaceMultiple) {
    // Add
    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }

    testStub_->deviceDetected(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 2);
    }

    // Remove
    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }

    testStub_->deviceRemoved(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 0);
    }
}

/**
 * @test
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Add a different managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, AddRemoveMultipleManagedInterfacesSingle) {
    // Add
    testStub_->specialDeviceDetected(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        ASSERT_EQ(specialDeviceAvailableCount_, 1);
    }

    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }

    // Remove
    testStub_->specialDeviceRemoved(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(specialDeviceAvailableCount_, 0);
    }

    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }
}

/**
 * @test
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Add a different managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Add a second instance of the same managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, AddRemoveMultipleManagedInterfacesMultiple) {
    // Add
    testStub_->specialDeviceDetected(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        ASSERT_EQ(specialDeviceAvailableCount_, 1);
    }

    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }

    testStub_->deviceDetected(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 2);
    }

    // Remove
    testStub_->specialDeviceRemoved(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(specialDeviceAvailableCount_, 0);
    }

    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }

    testStub_->deviceRemoved(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 0);
    }
}

/**
 * @test
 *  - Test the getConnectionId, getDomain and getInteface methods
 *    available via the ProxyManager of the respective managed interfaces
 *    of the manager
 */
TEST_F(ManagedTest, ProxyManagerTestPrimitiveMethods) {
    ASSERT_EQ(testProxy_->getProxyManagerDevice().getConnectionId(), connectionIdClient);
    ASSERT_EQ(testProxy_->getProxyManagerSpecialDevice().getConnectionId(), connectionIdClient);

    ASSERT_EQ(testProxy_->getProxyManagerDevice().getDomain(), domain);
    ASSERT_EQ(testProxy_->getProxyManagerSpecialDevice().getDomain(), domain);

    ASSERT_EQ(testProxy_->getProxyManagerDevice().getInterface(), interfaceDevice);
    ASSERT_EQ(testProxy_->getProxyManagerSpecialDevice().getInterface(), interfaceSpecialDevice);
}

/**
 * @test
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatus method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Add a different managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatus method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Add a second instance of the same managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatus method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, ProxyManagerTestNonPrimitiveMethodsSync) {
    // Add
    testStub_->specialDeviceDetected(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        ASSERT_EQ(specialDeviceAvailableCount_, 1);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    CommonAPI::CallStatus callStatus = CommonAPI::CallStatus::UNKNOWN;
    std::vector<std::string> availableSpecialDevices;
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstances(callStatus,
            availableSpecialDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableSpecialDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    bool allDetectedAreAvailable = false;
    for(auto &i : availableSpecialDevices) {
        for(auto &j : specialDevicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                ASSERT_TRUE(checkInstanceAvailabilityStatus(&testProxy_->getProxyManagerSpecialDevice(), j));
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    std::vector<std::string> availableDevices;
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                ASSERT_TRUE(checkInstanceAvailabilityStatus(&testProxy_->getProxyManagerDevice(), j));
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    testStub_->deviceDetected(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 2);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(2));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(2));
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                ASSERT_TRUE(checkInstanceAvailabilityStatus(&testProxy_->getProxyManagerDevice(), j));
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    // Remove
    testStub_->specialDeviceRemoved(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(specialDeviceAvailableCount_, 0);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableSpecialDevices.clear();
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstances(callStatus,
            availableSpecialDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableSpecialDevices.size(), static_cast<std::vector<std::string>::size_type>(0));

    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }
    ASSERT_LE(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                ASSERT_TRUE(checkInstanceAvailabilityStatus(&testProxy_->getProxyManagerDevice(), j));
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    testStub_->deviceRemoved(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 0);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(0));
}

/**
 * @test
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstancesAsync method to check that all
 *    registered instances are returned
 *  - Add a different managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstancesAsync method to check that all
 *    registered instances are returned
 *  - Add a second instance of the same managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstancesAsync method to check that all
 *    registered instances are returned
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, ProxyManagerTestNonPrimitiveMethodsAsync) {
    std::function<
            void(const CommonAPI::CallStatus &,
                 const std::vector<std::string> &)> getAvailableInstancesAsyncSpecialDeviceCallbackFunc =
            std::bind(
                    &ManagedTest_ProxyManagerTestNonPrimitiveMethodsAsync_Test::getAvailableInstancesAsyncSpecialDeviceCallback,
                    this, std::placeholders::_1, std::placeholders::_2);

    std::function<
            void(const CommonAPI::CallStatus &,
                 const std::vector<std::string> &)> getAvailableInstancesAsyncDeviceCallbackFunc =
            std::bind(
                    &ManagedTest_ProxyManagerTestNonPrimitiveMethodsAsync_Test::getAvailableInstancesAsyncDeviceCallback,
                    this, std::placeholders::_1, std::placeholders::_2);

    // Add
    testStub_->specialDeviceDetected(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        ASSERT_EQ(specialDeviceAvailableCount_, 1);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    specialDeviceAvailableDesiredValue_ = 1;
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstancesAsync(getAvailableInstancesAsyncSpecialDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    deviceAvailableDesiredValue_ = 1;
    testProxy_->getProxyManagerDevice().getAvailableInstancesAsync(
            getAvailableInstancesAsyncDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    testStub_->deviceDetected(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 2);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(2));
    deviceAvailableDesiredValue_ = 2;
    testProxy_->getProxyManagerDevice().getAvailableInstancesAsync(
            getAvailableInstancesAsyncDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    // Remove
    testStub_->specialDeviceRemoved(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(specialDeviceAvailableCount_, 0);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    specialDeviceAvailableDesiredValue_ = 0;
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstancesAsync(getAvailableInstancesAsyncSpecialDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }
    ASSERT_LE(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    deviceAvailableDesiredValue_ = 1;
    testProxy_->getProxyManagerDevice().getAvailableInstancesAsync(getAvailableInstancesAsyncDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));

    testStub_->deviceRemoved(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }
    ASSERT_LE(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    deviceAvailableDesiredValue_ = 0;
    testProxy_->getProxyManagerDevice().getAvailableInstancesAsync(getAvailableInstancesAsyncDeviceCallbackFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
}

/**
 * @test
 *  - Add a managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatusAsync method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Add a different managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatusAsync method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Add a second instance of the same managed interface to the manager
 *  - Check that the client is notified about the newly added interface
 *  - Use the ProxyManager's getAvailableInstances method to check that all
 *    registered instances are returned
 *  - Use the ProxyManager's checkInstanceAvailabilityStatusAsync method to check that all
 *    returned instances by getAvailableInstances are available
 *  - Remove all the managed interfaces from the manager
 *  - Check that the client is notified about the removed interfaces
 */
TEST_F(ManagedTest, ProxyManagerTestGetInstanceAvailabilityStatusAsync) {
    std::function<
            void(const CommonAPI::CallStatus &,
                 const CommonAPI::AvailabilityStatus &)> getInstanceAvailabilityStatusAsyncCallbackAvailableFunc =
            std::bind(
                    &ManagedTest_ProxyManagerTestGetInstanceAvailabilityStatusAsync_Test::getInstanceAvailabilityStatusAsyncCallbackAvailable,
                    this, std::placeholders::_1, std::placeholders::_2);

    std::function<
            void(const CommonAPI::CallStatus &,
                 const CommonAPI::AvailabilityStatus &)> getInstanceAvailabilityStatusAsyncCallbackNotAvailableFunc =
            std::bind(
                    &ManagedTest_ProxyManagerTestGetInstanceAvailabilityStatusAsync_Test::getInstanceAvailabilityStatusAsyncCallbackNotAvailable,
                    this, std::placeholders::_1, std::placeholders::_2);

    // Add
    testStub_->specialDeviceDetected(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(specialDeviceAvailableCountMutex_);
        ASSERT_EQ(specialDeviceAvailableCount_, 1);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    CommonAPI::CallStatus callStatus = CommonAPI::CallStatus::UNKNOWN;
    std::vector<std::string> availableSpecialDevices;
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstances(callStatus,
            availableSpecialDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableSpecialDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    bool allDetectedAreAvailable = false;
    instanceAvailabilityStatusCallbackCalled_ = false;
    for(auto &i : availableSpecialDevices) {
        for(auto &j : specialDevicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                testProxy_->getProxyManagerSpecialDevice().getInstanceAvailabilityStatusAsync(
                        j,
                        getInstanceAvailabilityStatusAsyncCallbackAvailableFunc);
                std::this_thread::sleep_for(
                        std::chrono::milliseconds(1 * 1000));
                ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
                instanceAvailabilityStatusCallbackCalled_ = false;
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    testStub_->deviceDetected(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 1);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    std::vector<std::string> availableDevices;
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusAsync(
                        j,
                        getInstanceAvailabilityStatusAsyncCallbackAvailableFunc);
                std::this_thread::sleep_for(
                        std::chrono::milliseconds(1 * 1000));
                ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
                instanceAvailabilityStatusCallbackCalled_ = false;
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    testStub_->deviceDetected(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_EQ(deviceAvailableCount_, 2);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(2));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(2));
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusAsync(
                        j,
                        getInstanceAvailabilityStatusAsyncCallbackAvailableFunc);
                std::this_thread::sleep_for(
                        std::chrono::milliseconds(1 * 1000));
                ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
                instanceAvailabilityStatusCallbackCalled_ = false;
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }

    // Remove
    testStub_->specialDeviceRemoved(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(specialDeviceAvailableCount_, 0);
    }
    ASSERT_EQ(specialDevicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableSpecialDevices.clear();
    testProxy_->getProxyManagerSpecialDevice().getAvailableInstances(callStatus,
            availableSpecialDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableSpecialDevices.size(), static_cast<std::vector<std::string>::size_type>(0));
    testProxy_->getProxyManagerSpecialDevice().getInstanceAvailabilityStatusAsync(
            "local:managed.SpecialDevice:managed-test.Manager.specialDevice00",
            getInstanceAvailabilityStatusAsyncCallbackNotAvailableFunc);
    std::this_thread::sleep_for(
            std::chrono::milliseconds(1 * 1000));
    ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
    instanceAvailabilityStatusCallbackCalled_ = false;

    testStub_->deviceRemoved(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 1);
    }
    ASSERT_LE(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(1));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(1));
    testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusAsync(
            "local:managed.Device:managed-test.Manager.device01",
            getInstanceAvailabilityStatusAsyncCallbackNotAvailableFunc);
    std::this_thread::sleep_for(
            std::chrono::milliseconds(1 * 1000));
    ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
    instanceAvailabilityStatusCallbackCalled_ = false;
    allDetectedAreAvailable = false;
    for(auto &i : availableDevices) {
        for(auto &j : devicesAvailable_) {
            if(i == j) {
                allDetectedAreAvailable = true;
                testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusAsync(
                        j,
                        getInstanceAvailabilityStatusAsyncCallbackAvailableFunc);
                std::this_thread::sleep_for(
                        std::chrono::milliseconds(1 * 1000));
                ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
                instanceAvailabilityStatusCallbackCalled_ = false;
                break;
            }
        }
        ASSERT_TRUE(allDetectedAreAvailable);
    }


    testStub_->deviceRemoved(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 * 1000));
    {
        std::lock_guard<std::mutex> lock(deviceAvailableCountMutex_);
        ASSERT_LE(deviceAvailableCount_, 0);
    }
    ASSERT_EQ(devicesAvailable_.size(), static_cast<std::set<std::string>::size_type>(0));
    callStatus = CommonAPI::CallStatus::UNKNOWN;
    availableDevices.clear();
    testProxy_->getProxyManagerDevice().getAvailableInstances(callStatus,
            availableDevices);
    ASSERT_EQ(callStatus, CommonAPI::CallStatus::SUCCESS);
    ASSERT_EQ(availableDevices.size(), static_cast<std::vector<std::string>::size_type>(0));
    testProxy_->getProxyManagerDevice().getInstanceAvailabilityStatusAsync(
            "local:managed.Device:managed-test.Manager.device02",
            getInstanceAvailabilityStatusAsyncCallbackNotAvailableFunc);
    std::this_thread::sleep_for(
            std::chrono::milliseconds(1 * 1000));
    ASSERT_TRUE(instanceAvailabilityStatusCallbackCalled_);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new ::testing::Environment());
    return RUN_ALL_TESTS();
}
