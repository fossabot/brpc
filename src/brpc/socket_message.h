// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef BRPC_SOCKET_MESSAGE_H
#define BRPC_SOCKET_MESSAGE_H

#include "butil/status.h"  // butil::Status

namespace brpc {

// Generate the IOBuf to write dynamically, for implementing complex protocols.
// Used in RTMP and HTTP2 right now.
class SocketMessage {
public:
    virtual ~SocketMessage() {}

    // Called once and only once *sequentially* to generate the buffer to
    // write. This object should destroy itself at the end of this method.
    // AppendAndDestroySelf() to the same Socket are called one by one in the
    // same sequence as their generated data are written into the file
    // descriptor. AppendAndDestroySelf() are called _after_ completion of
    // connecting including AppConnect.
    // Params:
    //   out  - The buffer to be generated, being empty initially, could
    //          remain empty after being called.
    //   sock - the socket to write. NULL when the message is abandoned,
    //          namely the socket is broken or AppendAndDestroySelf() is
    //          called by SocketMessagePtr<T>
    // If the status returned is an error, WriteOptions.id_wait (if absent)
    // will be signalled with the error. Other messages are not affected.
    virtual butil::Status AppendAndDestroySelf(butil::IOBuf* out,
                                               Socket* sock) = 0;

    // Estimated size of the buffer generated by AppendAndDestroySelf()
    virtual size_t EstimatedByteSize() { return 0; }
};

namespace details {
struct SocketMessageDeleter {
    void operator()(SocketMessage* msg) const {
        butil::IOBuf dummy_buf;
        // We don't care about the return value since the message is abandoned
        (void)msg->AppendAndDestroySelf(&dummy_buf, NULL);
    }
};
}  // namespace details

// A RAII pointer to make sure SocketMessage.AppendAndDestroySelf() is always
// called even if the message is rejected by Socket.Write()
// Property: Any SocketMessagePtr<T> can be casted to SocketMessagePtr<> which
// is accepted by Socket.Write()
template <typename T = void>
struct SocketMessagePtr;

template <>
struct SocketMessagePtr<void>
    : public std::unique_ptr<SocketMessage, details::SocketMessageDeleter> {
    SocketMessagePtr() {}
    SocketMessagePtr(SocketMessage* p)
        : std::unique_ptr<SocketMessage, details::SocketMessageDeleter>(p) {}
};

template <typename T>
struct SocketMessagePtr : public SocketMessagePtr<> {
    SocketMessagePtr() {}
    SocketMessagePtr(T* p) : SocketMessagePtr<>(p) {}
    T& operator*() { return static_cast<T&>(SocketMessagePtr<>::operator*()); }
    T* operator->() {
        return static_cast<T*>(SocketMessagePtr<>::operator->());
    }
    T* release() { return static_cast<T*>(SocketMessagePtr<>::release()); }
};

}  // namespace brpc

#endif  // BRPC_SOCKET_MESSAGE_H
