// Copyright (C) 2015-2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/Configuration.hpp>
#include <CommonAPI/SomeIP/Watch.hpp>

#include <fcntl.h>

#include <cstdio>
#include <sstream>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/eventfd.h>
#endif

#if ANDROID
#if !defined(EFD_SEMAPHORE)
#define EFD_SEMAPHORE (1 << 0)
#endif
#endif

#include <CommonAPI/SomeIP/Connection.hpp>

namespace CommonAPI {
namespace SomeIP {

Watch::Watch(const std::shared_ptr<Connection> &_connection) :
#ifdef _WIN32
        pipeValue_(4),
#else
        eventFd_(0),
        eventFdValue_(1),
#endif
        lastProcessing_()
{
#ifdef _WIN32
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, "0", &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
    }

    sockaddr* connected_addr = new sockaddr();
    USHORT port = 0;
    int namelength = sizeof(sockaddr);
    iResult = getsockname(ListenSocket, connected_addr, &namelength);
    if (iResult == SOCKET_ERROR) {
        printf("getsockname failed with error: %d\n", WSAGetLastError());
    } else if (connected_addr->sa_family == AF_INET) {
        port = ((struct sockaddr_in*)connected_addr)->sin_port;
    }
    delete connected_addr;

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
    }

    wsaData;
    pipeFileDescriptors_[0] = INVALID_SOCKET;
    struct addrinfo *ptr = NULL;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", std::to_string(ntohs(port)).c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        pipeFileDescriptors_[0] = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (pipeFileDescriptors_[0] == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
        }

        // Connect to server.
        iResult = connect(pipeFileDescriptors_[0], ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("connect failed with error: %ld\n", WSAGetLastError());
            closesocket(pipeFileDescriptors_[0]);
            pipeFileDescriptors_[0] = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (pipeFileDescriptors_[0] == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
    }

    // Accept a client socket
    pipeFileDescriptors_[1] = accept(ListenSocket, NULL, NULL);
    if (pipeFileDescriptors_[1] == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
    }
    pollFileDescriptor_.fd = pipeFileDescriptors_[0];
#else
    eventFd_ = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    if (eventFd_ == -1) {
        std::perror(__func__);
    }
    pollFileDescriptor_.fd = eventFd_;
#endif
    pollFileDescriptor_.events = POLLIN;

    connection_ = _connection;

    auto itsConfiguration = Configuration::get();
    max_processing_time_ = itsConfiguration->getMaxProcessingTime(_connection->getName());
    max_queue_size_ = itsConfiguration->getMaxQueueSize(_connection->getName());

    is_supervising_ = (max_processing_time_ > 0 || max_queue_size_ > 0);
    supervisor_ = std::make_shared<std::thread>(
            std::bind(&Watch::supervise, this));
}

Watch::~Watch() {
    {
        std::lock_guard<std::mutex> itsLock(superviseMutex_);
        is_supervising_ = false;
        superviseCondition_.notify_one();
    }
    if (supervisor_ && supervisor_->joinable())
        supervisor_->join();

#ifdef _WIN32
    // shutdown the connection since no more data will be sent
    int iResult = shutdown(pipeFileDescriptors_[0], SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(pipeFileDescriptors_[0]);
        WSACleanup();
    }

    // cleanup
    closesocket(pipeFileDescriptors_[0]);
    WSACleanup();
#else
    close(eventFd_);
#endif
}

void Watch::dispatch(unsigned int) {
}

const pollfd& Watch::getAssociatedFileDescriptor() {
    return pollFileDescriptor_;
}

#ifdef _WIN32
const HANDLE& Watch::getAssociatedEvent() {
    return wsaEvent_;
}
#endif

const std::vector<CommonAPI::DispatchSource*>& Watch::getDependentDispatchSources() {
    std::lock_guard<std::mutex> itsLock(dependentDispatchSourcesMutex_);
    return dependentDispatchSources_;
}

void Watch::addDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource) {
    std::lock_guard<std::mutex> itsLock(dependentDispatchSourcesMutex_);
    dependentDispatchSources_.push_back(_dispatchSource);
}

void Watch::removeDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource) {
    std::lock_guard<std::mutex> itsLock(dependentDispatchSourcesMutex_);
    std::vector<CommonAPI::DispatchSource*>::iterator it;

    for (it = dependentDispatchSources_.begin(); it != dependentDispatchSources_.end(); it++) {
        if ( (*it) == _dispatchSource ) {
            dependentDispatchSources_.erase(it);
            break;
        }
    }
}

void Watch::pushQueue(std::shared_ptr<QueueEntry> _queueEntry) {
    {
        std::lock_guard<std::mutex> itsLock(lastProcessingMutex_);
        // Record the time when the first entry is queued so that the supervisor can detect
        // when the main loop becomes blocked.
        if (!lastProcessing_.time_since_epoch().count()) {
            lastProcessing_ = std::chrono::steady_clock::now();
        }
    }

    {
        std::lock_guard<std::mutex> itsLock(queueMutex_);
        queue_.push(_queueEntry);
    }

#ifdef _WIN32
    // Send an initial buffer
    char *sendbuf = "1";

    int iResult = send(pipeFileDescriptors_[1], sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        int error = WSAGetLastError();

        if (error != WSANOTINITIALISED) {
            printf("send failed with error: %d\n", error);
        }
    }
#else
    while (write(eventFd_, &eventFdValue_, sizeof(eventFdValue_)) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            std::perror(__func__);
            break;
        }
        std::this_thread::yield();
    }
#endif
}

void Watch::popQueue() {
#ifdef _WIN32
    // Receive until the peer closes the connection
    int iResult;
    char recvbuf[1];
    int recvbuflen = 1;

    iResult = recv(pipeFileDescriptors_[0], recvbuf, recvbuflen, 0);
    if (iResult > 0) {
        //printf("Bytes received from %d: %d\n", wakeFd_.fd, iResult);
    }
    else if (iResult == 0) {
        printf("Connection closed\n");
    }
    else {
        printf("recv failed with error: %d\n", WSAGetLastError());
    }
#else
    std::uint64_t readValue(0);
    while (read(eventFd_, &readValue, sizeof(readValue)) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            std::perror(__func__);
            break;
        }
        std::this_thread::yield();
    }
#endif

    {
        std::lock_guard<std::mutex> itsLock(queueMutex_);
        queue_.pop();
    }
}

std::shared_ptr<QueueEntry> Watch::frontQueue() {
    std::lock_guard<std::mutex> itsLock(queueMutex_);

    return queue_.front();
}

bool Watch::emptyQueue() {
    std::lock_guard<std::mutex> itsLock(queueMutex_);

    return queue_.empty();
}

void Watch::processQueueEntry(std::shared_ptr<QueueEntry> _queueEntry) {
    if (auto connection = connection_.lock()) {
        _queueEntry->process(connection);

        std::lock_guard<std::mutex> itsLock(lastProcessingMutex_);
        lastProcessing_ = std::chrono::steady_clock::now();
    }
}

void Watch::supervise() {
    std::string applicationName = "<unknown>";
    {
        auto itsConnection = connection_.lock();
        if (itsConnection) {
            applicationName = itsConnection->getName();
            COMMONAPI_INFO("Supervising watch[", applicationName, "] --> (",
                max_processing_time_, ", ", max_queue_size_, ")");
        } else {
            COMMONAPI_WARNING("Supervising watch[", applicationName, "] --> (",
                        max_processing_time_, ", ", max_queue_size_, ")");
        }
    }

    std::unique_lock<std::mutex> itsSuperviseLock(superviseMutex_);
    while (is_supervising_) {
        if (!emptyQueue()) {
            std::chrono::milliseconds itsDuration;
            std::size_t itsQueueSize;

            {
                std::lock_guard<std::mutex> itsLock(lastProcessingMutex_);
                itsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - lastProcessing_);
            }
            {
                std::lock_guard<std::mutex> itsLock(queueMutex_);
                itsQueueSize = queue_.size();
            }

#ifdef MAINLOOP_WATCH_ELEMENTS_SLOW_THRESHOLD
                if (MAINLOOP_WATCH_ELEMENTS_SLOW_THRESHOLD >= itsQueueSize) {
                    COMMONAPI_WARNING("DEBUG -> Aborting application[", applicationName, "] watch elements threshold defined.");
                    abort();
                }
#endif

#ifdef MAINLOOP_DID_NOT_PROCESS_MESSAGE_MS_THRESHOLD
                if (MAINLOOP_DID_NOT_PROCESS_MESSAGE_MS_THRESHOLD >= itsDuration.count()) {
                    COMMONAPI_WARNING("DEBUG -> Aborting application[", applicationName, "] ms threshold defined.");
                    abort();
                }
#endif

            if (0 < max_processing_time_
                    && itsDuration.count() > max_processing_time_) {

                std::stringstream itsMessage;
                itsMessage << "Mainloop did not process a message for "
                        << itsDuration.count() << "ms (#elements="
                        << itsQueueSize << ")"
                        << " Application[" << applicationName << "]";

                COMMONAPI_WARNING(itsMessage.str());
            } else if (0 < max_queue_size_
                    && itsQueueSize > max_queue_size_) {

                std::stringstream itsMessage;
                itsMessage << "Mainloop watch contains "
                        << itsQueueSize << " elements."
                        << " Maybe processing is too slow"
                        << " Application[" << applicationName << "]";

                COMMONAPI_WARNING(itsMessage.str());
            }
        }

        if (is_supervising_)
            superviseCondition_.wait_for(itsSuperviseLock, std::chrono::seconds(1));
    }
}

} // namespace SomeIP
} // namespace CommonAPI
