// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <CommonAPI/SomeIP/Watch.hpp>

#include <fcntl.h>

#ifdef WIN32
#include <Winsock2.h>
#else
#include <unistd.h>
#endif

#include <CommonAPI/SomeIP/Connection.hpp>

namespace CommonAPI {
namespace SomeIP {

Watch::Watch(const std::shared_ptr<Connection>& _connection) : connection_(_connection), pipeValue_(4) {
#ifdef WIN32
	std::string pipeName = "\\\\.\\pipe\\CommonAPI-SomeIP-";

	UUID uuid;
	CHAR* uuidString = NULL;
	UuidCreate(&uuid);
	UuidToString(&uuid, (RPC_CSTR*)&uuidString);
	pipeName += uuidString;
	RpcStringFree((RPC_CSTR*)&uuidString);

	HANDLE hPipe = ::CreateNamedPipe(
		pipeName.c_str(),
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		1,
		4096,
		4096,
		100,
		nullptr);

	if (hPipe == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			printf("Could not open pipe %d\n", GetLastError());
		}

		// All pipe instances are busy, so wait for sometime.
		else if (!WaitNamedPipe(pipeName.c_str(), NMPWAIT_USE_DEFAULT_WAIT))
		{
			printf("Could not open pipe: wait timed out.\n");
		}
	}

	HANDLE hPipe2 = CreateFile(
		pipeName.c_str(),   // pipe name 
		GENERIC_READ |  // read and write access 
		GENERIC_WRITE,
		0,              // no sharing 
		NULL,           // default security attributes
		OPEN_EXISTING,  // opens existing pipe 
		0,              // default attributes 
		NULL);          // no template file 

	if (hPipe2 == INVALID_HANDLE_VALUE) {
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			printf("Could not open pipe2 %d\n", GetLastError());
		}

		// All pipe instances are busy, so wait for sometime.
		else if (!WaitNamedPipe(pipeName.c_str(), NMPWAIT_USE_DEFAULT_WAIT))
		{
			printf("Could not open pipe2: wait timed out.\n");
		}
	}

	pipeFileDescriptors_[0] = (int)hPipe;
	pipeFileDescriptors_[1] = (int)hPipe2;

	wsaEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

	if (wsaEvent_ == WSA_INVALID_EVENT) {
		printf("Invalid Event Created!\n");
	}

	ov = { 0 };
	ov.hEvent = wsaEvent_;

	BOOL retVal = ::ConnectNamedPipe(hPipe, &ov);

	if (retVal == 0) {
		int error = GetLastError();

		if (error != 535) {
			printf("ERROR: ConnectNamedPipe failed with (%d)\n", error);
		}
	}
#else
	pipe2(pipeFileDescriptors_, O_NONBLOCK);
#endif
    pollFileDescriptor_.fd = pipeFileDescriptors_[0];
    pollFileDescriptor_.events = POLLRDNORM;
}

Watch::~Watch() {
#ifdef WIN32
	BOOL retVal = DisconnectNamedPipe((HANDLE)pipeFileDescriptors_[0]);

	if (!retVal) {
		printf(TEXT("DisconnectNamedPipe failed. GLE=%d\n"), GetLastError());
	}

	retVal = CloseHandle((HANDLE)pipeFileDescriptors_[0]);

	if (!retVal) {
		printf(TEXT("CloseHandle failed. GLE=%d\n"), GetLastError());
	}

	retVal = CloseHandle((HANDLE)pipeFileDescriptors_[1]);

	if (!retVal) {
		printf(TEXT("CloseHandle2 failed. GLE=%d\n"), GetLastError());
	}
#endif
}

void Watch::dispatch(unsigned int eventFlags) {
}

const pollfd& Watch::getAssociatedFileDescriptor() {
	return pollFileDescriptor_;
}

#ifdef WIN32
const HANDLE& Watch::getAssociatedEvent() {
	return wsaEvent_;
}
#endif

const std::vector<CommonAPI::DispatchSource*>& Watch::getDependentDispatchSources() {
	return dependentDispatchSources_;
}

void Watch::addDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource) {
	dependentDispatchSources_.push_back(_dispatchSource);
}

void Watch::removeDependentDispatchSource(CommonAPI::DispatchSource* _dispatchSource) {
	std::vector<CommonAPI::DispatchSource*>::iterator it;

	for (it = dependentDispatchSources_.begin(); it != dependentDispatchSources_.end(); it++) {
		if ( (*it) == _dispatchSource ) {
			dependentDispatchSources_.erase(it);
			break;
		}
	}
}

void Watch::pushQueue(Watch::msgQueueEntry _msgQueueEntry) {
	std::unique_lock<std::mutex> itsLock(msgQueueMutex_);
	msgQueue_.push(_msgQueueEntry);

#ifdef WIN32
	char writeValue[sizeof(pipeValue_)];
	*reinterpret_cast<int*>(writeValue) = pipeValue_;
	DWORD cbWritten;

	int fSuccess = WriteFile(
		(HANDLE)pipeFileDescriptors_[1],                  // pipe handle 
		writeValue,             // message 
		sizeof(pipeValue_),              // message length 
		&cbWritten,             // bytes written 
		&ov);                  // overlapped 

	if (!fSuccess)
	{
		printf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
	}
#else
	write(pipeFileDescriptors_[1], &pipeValue_, sizeof(pipeValue_));
#endif
}

void Watch::popQueue() {
	std::unique_lock<std::mutex> itsLock(msgQueueMutex_);

#ifdef WIN32
	char readValue[sizeof(pipeValue_)];
	DWORD cbRead;

	int fSuccess = ReadFile(
		(HANDLE)pipeFileDescriptors_[0],    // pipe handle 
		readValue,    // buffer to receive reply 
		sizeof(pipeValue_),  // size of buffer 
		&cbRead,  // number of bytes read 
		&ov);    // overlapped 

	if (!fSuccess)
	{
		printf(TEXT("ReadFile to pipe failed. GLE=%d\n"), GetLastError());
	}
#else
	int readValue = 0;
	read(pipeFileDescriptors_[0], &readValue, sizeof(readValue));
#endif

	msgQueue_.pop();
}

Watch::msgQueueEntry& Watch::frontQueue() {
	std::unique_lock<std::mutex> itsLock(msgQueueMutex_);

	return msgQueue_.front();
}

bool Watch::emptyQueue() {
	std::unique_lock<std::mutex> itsLock(msgQueueMutex_);

	return msgQueue_.empty();
}

void Watch::processMsgQueueEntry(msgQueueEntry &_msgQueueEntry) {
	connection_->processMsgQueueEntry(_msgQueueEntry);
}

} // namespace SomeIP
} // namespace CommonAPI
