#include "KTCP.h"

void ListenFd::Listen(uint16_t port) {
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("fd = %d listenning...\n", m_fd);
    if (m_fd < 0) {
        perror("socket");
        exit(0);
    }
    if (fcntl(m_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(0);
    }

    int flag = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    if (bind(m_fd, (sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        exit(0);
    }

    if (listen(m_fd, 32) < 0) {
        perror("listen");
        exit(0);
    }
}

int ListenFd::Accept() {
    KCoroutine* kcoro = KCoroutine::getInstance();
    while (true) {
        int fd_client = accept(m_fd, nullptr, nullptr);
        if (fd_client > 0) {
            int flag = 1;
            if (setsockopt(fd_client, IPPROTO_TCP, TCP_NODELAY, &flag,
                           sizeof(flag)) < 0) {
                perror("setsockopt_tcp_nodelay");
                return -1;
            }
            printf("Accept fd = %d\n", fd_client);
            if (fcntl(fd_client, F_SETFL, O_NONBLOCK) < 0) {
                perror("fcntl");
                exit(0);
            }
            return fd_client;
        } else if (errno == EAGAIN) {
            kcoro->registerFd(m_fd, false);
            kcoro->switchToMainCtx();
        } else if (errno == EINTR) {
            continue;
        } else {
            perror("accept");
            return -1;
        }
    }
    return -1;
}

OpFd::OpFd(int fd) {
    m_fd = fd;
}

int OpFd::Read(char* buffer, size_t size) {
    KCoroutine* kcoro = KCoroutine::getInstance();

    while (true) {
        int n = read(m_fd, buffer, size);
        if (n >= 0) {
            return n;
        } else if (errno == EAGAIN) {
            printf("fd = %d read yield.\n", m_fd);
            kcoro->registerFd(m_fd, false);
            kcoro->switchToMainCtx();
        } else if (errno != EINTR) {
            perror("read");
            return -1;
        }
    }
    return -1;
}

int OpFd::Write(const char* buffer, size_t size) {
    KCoroutine* kcoro = KCoroutine::getInstance();
    size_t nwrited = 0;

    while (nwrited < size) {
        int n = write(m_fd, buffer + nwrited, size - nwrited);
        if (n > 0) {
            nwrited += n;
        } else if (n == 0) {
            return 0;
        } else if (errno == EAGAIN) {
            printf("fd = %d write yield.\n", m_fd);
            kcoro->registerFd(m_fd, true);
            kcoro->switchToMainCtx();
        } else if (errno != EINTR) {
            perror("write");
            return -1;
        }
    }
    return size;
}

void OpFd::Close() {
    if (m_fd < 0) {
        return;
    }
    KCoroutine* kcoro = KCoroutine::getInstance();
    kcoro->unRegisterFd(m_fd);
    close(m_fd);
    m_fd = -1;
}