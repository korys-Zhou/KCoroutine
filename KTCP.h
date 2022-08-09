#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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