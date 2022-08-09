#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "KCoroutine.h"

class ListenFd {
   public:
    void Listen(uint16_t port);
    int Accept();

   private:
    int m_fd;
};

class OpFd {
   public:
    OpFd(int fd);
    int Read(char* buffer, size_t size);
    int Write(const char* buffer, size_t size);
    void Close();

   private:
    int m_fd;
};