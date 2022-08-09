#pragma once

#include <stdio.h>
#include <sys/epoll.h>
#include <ucontext.h>
#include <functional>
#include <list>
#include <map>

typedef enum {
    INIT = 0,
    READY = 1,
    WAITING = 2,
    FINISH = 3
} UnitCoroutineStatus;

class UnitCoroutine;

class KCoroutine {
   public:
    virtual ~KCoroutine();

    void createCoroutine(std::function<void()> run);
    void disPatch();
    void yield();
    void registerFd(int fd, bool is_write);
    void switchToMainCtx();
    ucontext_t* schedule();

   public:
    static KCoroutine* getInstance() {
        static thread_local KCoroutine KC;
        return &KC;
    }

   private:
    KCoroutine();

   private:
    int m_epfd;
    std::list<UnitCoroutine*> m_ready, m_running;
    ucontext_t m_mainctx;
    UnitCoroutine* m_runningcoro;
    typedef struct {
        UnitCoroutine *write, *read;
    } WaitingCoros;
    std::map<int, WaitingCoros> m_io_waitingcoros;
};

class UnitCoroutine {
   public:
    UnitCoroutine(std::function<void()> run, KCoroutine* kcoro);
    virtual ~UnitCoroutine();

    ucontext_t* context();
    static void start(UnitCoroutine* pthis);
    bool isFinished();

   private:
    u_int64_t m_status;
    KCoroutine* m_kcoro;
    std::function<void()> m_run;
    ucontext_t m_thisctx;
    char* m_stkptr;
    size_t m_stksize;
};