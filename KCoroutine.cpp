#include "KCoroutine.h"

KCoroutine::KCoroutine() {
    m_runningcoro = nullptr;
    m_epfd = epoll_create1(0);
    if (m_epfd < 0) {
        perror("epoll_create");
        exit(0);
    }
}

KCoroutine::~KCoroutine() {}

void KCoroutine::createCoroutine(std::function<void()> run) {
    m_ready.push_back(new UnitCoroutine(run, this));
}

void KCoroutine::disPatch() {
    while (true) {
        // if(m_ready.empty()) continue;
        if (m_ready.size() > 0) {
            m_running = std::move(m_ready);
            m_ready.clear();
            printf("Running coros: %d, Waiting coros: %d\n", m_running.size(),
                   m_io_waitingcoros.size());
            for (auto cur : m_running) {
                m_runningcoro = cur;
                swapcontext(&m_mainctx, cur->context());
                m_runningcoro = nullptr;
                if (cur->isFinished())
                    delete cur;
            }
            m_running.clear();
        }

        epoll_event events[128];
        int ready = epoll_wait(m_epfd, events, 128, 5);
        if (ready < 0) {
            perror("epoll_wait");
            exit(0);
        }
        for (int i = 0; i < ready; ++i) {
            auto& ev = events[i];
            int fd = ev.data.fd;
            auto it = m_io_waitingcoros.find(fd);
            if (it == m_io_waitingcoros.end())
                continue;
            else {
                if (ev.events | EPOLLIN && it->second.read) {
                    m_ready.push_back(it->second.read);
                }
                if (ev.events | EPOLLOUT && it->second.write) {
                    m_ready.push_back(it->second.write);
                }
            }
        }
    }
}

void KCoroutine::yield() {
    m_ready.push_back(m_runningcoro);
    swapcontext(m_runningcoro->context(), &m_mainctx);
}

void KCoroutine::registerFd(int fd, bool is_write) {
    auto it = m_io_waitingcoros.find(fd);
    if (it == m_io_waitingcoros.end()) {
        WaitingCoros cur;
        if (is_write) {
            cur.read = nullptr;
            cur.write = m_runningcoro;
        } else {
            cur.read = m_runningcoro;
            cur.write = nullptr;
        }
        m_io_waitingcoros.emplace(fd, cur);
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            perror("epoll_ctl_add");
            exit(0);
        }
    } else {
        if (is_write) {
            it->second.read = nullptr;
            it->second.write = m_runningcoro;
        } else {
            it->second.read = m_runningcoro;
            it->second.write = nullptr;
        }
    }
}

void KCoroutine::unRegisterFd(int fd) {
    auto it = m_io_waitingcoros.find(fd);
    if (it == m_io_waitingcoros.end()) {
        return;
    }
    m_io_waitingcoros.erase(it);
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        perror("epoll_ctl_del");
    }
}

void KCoroutine::switchToMainCtx() {
    swapcontext(m_runningcoro->context(), &m_mainctx);
}

ucontext_t* KCoroutine::schedule() {
    return &m_mainctx;
}

UnitCoroutine::UnitCoroutine(std::function<void()> run, KCoroutine* kcoro)
    : m_run(run), m_kcoro(kcoro) {
    m_status = UnitCoroutineStatus::INIT;
    getcontext(&m_thisctx);
    m_stksize = 1024 * 128;
    m_stkptr = new char[m_stksize];
    m_thisctx.uc_link = kcoro->schedule();
    m_thisctx.uc_stack.ss_sp = m_stkptr;
    m_thisctx.uc_stack.ss_size = m_stksize;
    makecontext(&m_thisctx, (void (*)())UnitCoroutine::start, 1, this);
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