#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <memory>
#include <optional>
#include "KCoroutine.h"

class Fd {
   public:
    Fd(int fd = -1);
    virtual ~Fd();

    void registerFd();
    bool isValid();

   protected:
    int m_fd;
};

class OpFd;

class ListenFd : public Fd {
   public:
    ListenFd(int fd);
    ~ListenFd();
    static ListenFd Listen(uint16_t port);
    std::optional<std::shared_ptr<OpFd>> Accept();
};

class OpFd : public Fd {
   public:
    OpFd(int fd);
    ~OpFd();
    int Read(char* buffer, size_t size);
    int Write(const char* buffer, size_t size);
};