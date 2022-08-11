#include "KTCP.h"

Fd::Fd(int fd) {
    m_fd = fd;
}

Fd::~Fd() {
    if (isValid()) {
        close(m_fd);
        printf("fd = %d closed.", m_fd);
    } else {
        printf("fd = %d is destructed.", m_fd);
    }
    m_fd = -1;
}

bool Fd::isValid() {
    return m_fd > 0;
}

void Fd::registerFd() {
    KCoroutine* kcoro = KCoroutine::getInstance();
    kcoro->registerFd(m_fd);
}

ListenFd::ListenFd(int fd) : Fd(fd) {}

ListenFd::~ListenFd() {
    KCoroutine* kcoro = KCoroutine::getInstance();
    kcoro->unRegisterFd(m_fd);
}

ListenFd ListenFd::Listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        exit(-1);
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(-1);
    }
    int flag = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        perror("set_reuse");
        exit(-1);
    };

    sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    if (bind(fd, (sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        exit(-1);
    }

    if (listen(fd, 32) < 0) {
        perror("listen");
        exit(-1);
    }

    ListenFd ret(fd);
    ret.registerFd();

    printf("fd = %d listenning...\n", fd);
    return ret;
}

std::optional<std::shared_ptr<OpFd>> ListenFd::Accept() {
    KCoroutine* kcoro = KCoroutine::getInstance();
    while (true) {
        int fd_client = accept(m_fd, nullptr, nullptr);
        if (fd_client > 0) {
            if (fcntl(fd_client, F_SETFL, O_NONBLOCK) < 0) {
                perror("fcntl");
                exit(-1);
            }
            int flag = 1;
            if (setsockopt(fd_client, IPPROTO_TCP, TCP_NODELAY, &flag,
                           sizeof(flag)) < 0) {
                close(fd_client);
                fd_client = -1;
            }
            printf("Accept fd = %d\n", fd_client);
            kcoro->registerFd(fd_client);
            return std::shared_ptr<OpFd>(new OpFd(fd_client));
        } else if (errno == EAGAIN) {
            kcoro->switchToWaiting(m_fd);
        } else if (errno == EINTR) {
            continue;
        } else {
            perror("accept");
        }
    }
    return std::nullopt;
}

OpFd::OpFd(int fd) : Fd(fd) {}

OpFd::~OpFd() {
    KCoroutine* kcoro = KCoroutine::getInstance();
    kcoro->unRegisterFd(m_fd);
}

int OpFd::Read(char* buffer, size_t size) {
    KCoroutine* kcoro = KCoroutine::getInstance();

    while (true) {
        int n = read(m_fd, buffer, size);
        if (n >= 0) {
            return n;
        } else if (errno == EAGAIN) {
            printf("fd = %d read yield.\n", m_fd);
            kcoro->switchToWaiting(m_fd);
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
            kcoro->switchToWaiting(m_fd);
        } else if (errno != EINTR) {
            perror("write");
            return -1;
        }
    }
    return size;
}