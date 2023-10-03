# Cannot convert to Android.bp as resource copying has not
# yet implemented for soong as of 12/16/2016

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libcommonapi_someip_dlt
LOCAL_MODULE_TAGS := optional
LOCAL_CLANG := true
LOCAL_PROPRIETARY_MODULE := true

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_SRC_FILES += \
    src/CommonAPI/SomeIP/Address.cpp \
    src/CommonAPI/SomeIP/AddressTranslator.cpp \
    src/CommonAPI/SomeIP/ClientId.cpp \
    src/CommonAPI/SomeIP/Configuration.cpp \
    src/CommonAPI/SomeIP/Connection.cpp \
    src/CommonAPI/SomeIP/DispatchSource.cpp \
    src/CommonAPI/SomeIP/Factory.cpp \
    src/CommonAPI/SomeIP/InputStream.cpp \
    src/CommonAPI/SomeIP/InstanceAvailabilityStatusChangedEvent.cpp \
    src/CommonAPI/SomeIP/Message.cpp \
    src/CommonAPI/SomeIP/OutputStream.cpp \
    src/CommonAPI/SomeIP/Proxy.cpp \
    src/CommonAPI/SomeIP/ProxyBase.cpp \
    src/CommonAPI/SomeIP/ProxyManager.cpp \
    src/CommonAPI/SomeIP/StringEncoder.cpp \
    src/CommonAPI/SomeIP/StubAdapter.cpp \
    src/CommonAPI/SomeIP/StubManager.cpp \
    src/CommonAPI/SomeIP/SubscriptionStatusWrapper.cpp \
    src/CommonAPI/SomeIP/Watch.cpp \

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/internal

LOCAL_SHARED_LIBRARIES := \
    libboost_log \
    libboost_system \
    libboost_thread \
    libvsomeip_dlt \
    libcommonapi_dlt \

LOCAL_CFLAGS :=  \
    -frtti -fexceptions \
    -Wno-ignored-attributes \
    -Wno-unused-private-field \
    -D_CRT_SECURE_NO_WARNINGS \
    -DCOMMONAPI_INTERNAL_COMPILATION \
    -DCOMMONAPI_LOGLEVEL=COMMONAPI_LOGLEVEL_VERBOSE \

include $(BUILD_SHARED_LIBRARY)

