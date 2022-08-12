#include "KCoroutine.h"

int64_t NowInMs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return int64_t(tv.tv_sec * 1000) + tv.tv_usec / 1000;
}

KCoroutine::KCoroutine() {
    m_runningcoro = nullptr;
    m_epfd = epoll_create1(0);
    if (m_epfd < 0) {
        perror("epoll_create");
        exit(-1);
    }
}

KCoroutine::~KCoroutine() {
    close(m_epfd);
}

void KCoroutine::createCoroutine(std::function<void()> run, size_t stksize) {
    if (stksize == 0)
        stksize = 1024 * 1024;
    m_ready.push_back(new UnitCoroutine(run, this, stksize));
}

void KCoroutine::disPatch() {
    while (true) {
        if (!m_ready.empty()) {
            m_running = std::move(m_ready);
            m_ready.clear();
            printf("Running coros: %d, Waiting coros: %d\n", m_running.size(),
                   m_waiting.size());
            for (auto cur : m_running) {
                m_runningcoro = cur;
                swapcontext(&m_mainctx, cur->context());
                m_runningcoro = nullptr;
                if (cur->isFinished())
                    delete cur;
            }
            m_running.clear();
            printf("Running coros: %d, Waiting coros: %d\n", m_running.size(),
                   m_waiting.size());
        }

        int64_t now = NowInMs();
        while (!m_timeout.empty() && m_timeout.begin()->first <= now) {
            for (int fd : m_timeout.begin()->second) {
                if (m_fdexpire[fd] > m_timeout.begin()->first)
                    continue;
                wakeUpFd(fd);
            }
            m_timeout.erase(m_timeout.begin());
        }

        epoll_event events[128];
        int ready = epoll_wait(m_epfd, events, 128, 5);
        if (ready < 0) {
            perror("epoll_wait");
            exit(-1);
        }
        for (int i = 0; i < ready; ++i) {
            auto& ev = events[i];
            int fd = ev.data.fd;
            wakeUpFd(fd);
        }
    }
}

void KCoroutine::yield() {
    assert(m_runningcoro != nullptr);
    m_ready.push_back(m_runningcoro);
    switchToMainCtx();
}

void KCoroutine::registerFd(int fd) {
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl_add");
        exit(-1);
    }
}

void KCoroutine::unRegisterFd(int fd) {
    {
        auto it = m_waiting.find(fd);
        if (it != m_waiting.end()) {
            wakeUpFd(fd);
        }
    }

    {
        auto it = m_fdexpire.find(fd);
        if (it != m_fdexpire.end()) {
            m_fdexpire.erase(it);
        }
    }

    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        perror("epoll_ctl_del");
    }
}

void KCoroutine::switchToWaiting(int fd, int64_t expire_at) {
    assert(m_runningcoro != nullptr);
    m_waiting.emplace(fd, m_runningcoro);
    if (expire_at > -1 &&
        (!m_fdexpire.count(fd) || m_fdexpire[fd] < expire_at)) {
        m_fdexpire[fd] = expire_at;
        m_timeout[expire_at].emplace(fd);
    }
    switchToMainCtx();
}

void KCoroutine::switchToMainCtx() {
    swapcontext(m_runningcoro->context(), &m_mainctx);
}

void KCoroutine::wakeUpFd(int fd) {
    auto it = m_waiting.find(fd);
    if (it != m_waiting.end()) {
        printf("fd = %d wake up succeed.\n", fd);
        if (m_fdexpire.count(fd))
            m_fdexpire[fd] = -1;
        m_ready.push_back(it->second);
        m_waiting.erase(it);
    } else {
        printf("fd = %d is not waiting.\n", fd);
    }
}

ucontext_t* KCoroutine::getMainCtx() {
    return &m_mainctx;
}

UnitCoroutine::UnitCoroutine(std::function<void()> run,
                             KCoroutine* kcoro,
                             size_t stksize)
    : m_run(run), m_kcoro(kcoro), m_stksize(stksize) {
    getcontext(&m_thisctx);
    m_stkptr = new char[m_stksize];
    m_thisctx.uc_stack.ss_sp = m_stkptr;
    m_thisctx.uc_stack.ss_size = m_stksize;
    m_thisctx.uc_link = kcoro->getMainCtx();
    makecontext(&m_thisctx, (void (*)())UnitCoroutine::start, 1, this);
    m_status = UnitCoroutineStatus::INIT;
}

UnitCoroutine::~UnitCoroutine() {
    delete[] m_stkptr;
    m_stkptr = nullptr;
    m_stksize = 0;
}

ucontext_t* UnitCoroutine::context() {
    return &m_thisctx;
}

void UnitCoroutine::start(UnitCoroutine* pthis) {
    pthis->m_run();
    pthis->m_status = UnitCoroutineStatus::FINISH;
}

bool UnitCoroutine::isFinished() {
    return m_status == UnitCoroutineStatus::FINISH;
}