#include "KTCP.h"

void ListenFd::Listen(uint16_t port) {
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        perror("socket");
        exit(0);
    }
    if (fcntl(m_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(0);
    }

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