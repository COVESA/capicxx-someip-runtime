### CommonAPI C++ SOME/IP Runtime

##### Copyright
Copyright (C) 2016-2020, Bayerische Motoren Werke Aktiengesellschaft (BMW AG).
Copyright (C) 2016-2020, GENIVI Alliance, Inc.

This file is part of GENIVI Project IPC Common API C++.
Contributions are licensed to the GENIVI Alliance under one or more Contribution License Agreements or MPL 2.0.

##### License
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, you can obtain one at http://mozilla.org/MPL/2.0/.

== CommonAPI SOME/IP C++ User Guide
The user guide can be found in the documentation directory of the CommonAPI-SomeIP-Tools project as AsciiDoc document. A pdf version can be found at https://github.com/GENIVI/capicxx-someip-tools/releases.

##### Further information
https://genivi.github.io/capicxx-core-tools/

##### Build Instructions for Linux

###### CommonAPI Core Runtime and vsomeip

In order to build the CommonAPI SOME/IP Runtime library the CommonAPI Core Runtime library and the vsomeip library must be available on your system. Please consult the CommonAPI and vsomeip documentation for information on building CommonAPI Core Runtime/vsomeip for Android.

###### Build CommonAPI-SomeIP Runtime

Use CMake to build the CommonAPI SOME/IP runtime library. We assume that your source directory is common-api-someip-runtime:

```bash
$ cd common-api-someip-runtime
$ mkdir build
$ cmake -D USE_INSTALLED_COMMONAPI=ON -D CMAKE_INSTALL_PREFIX=/usr/local ..
$ make
$ make install
```

You can change the installation directory by the CMake variable _CMAKE_INSTALL_PREFIX_ or you can let it uninstalled (skip the _make install_ command). If you want to use the uninstalled version of CommonAPI set the CMake variable _USE_INSTALLED_COMMONAPI_ to _OFF_.

For further build instructions (build for windows, build documentation, tests etc.) please refer to the CommonAPI SOME/IP tutorial.

##### Build Instructions for Android

###### CommonAPI Core Runtime and vsomeip

In order to build the CommonAPI SOME/IP Runtime library the CommonAPI Core Runtime library and the vsomeip library must be included in the Android source tree. Please consult the CommonAPI and vsomeip documentation for information on building CommonAPI Core Runtime/vsomeip for Android.

###### Build CommonAPI-SomeIP Runtime

In general for building the Android source tree the instructions found on the pages from the Android Open Source Project (AOSP) apply (https://source.android.com/setup/build/requirements).

To integrate the CommonAPI SOME/IP Runtime library into the build process, the source code together with the Android.bp file has to be inserted into the Android source tree (by simply copying or by fetching with a custom platform manifest).
When building the Android source tree, the Android.bp file is automatically found and considered by the build system.

In order that the CommonAPI SOME/IP Runtime library is also included in the Android image, the library has to be added to the PRODUCT_PACKAGES variable in one of a device/target specific makefile:

```
PRODUCT_PACKAGES += \
    libCommonAPI-SomeIP
```
