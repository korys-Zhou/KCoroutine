#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <deque>
#include <functional>
#include <map>
#include <vector>

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

    void createCoroutine(std::function<void()> run, size_t stksize);
    void disPatch();
    void yield();
    void registerFd(int fd);
    void unRegisterFd(int fd);
    void switchToWaiting(int fd);
    void switchToMainCtx();
    void wakeUpFd(int fd);
    ucontext_t* getMainCtx();

   public:
    static KCoroutine* getInstance() {
        static thread_local KCoroutine KC;
        return &KC;
    }

   private:
    KCoroutine();
    int64_t NowInMs();

   private:
    int m_epfd;
    std::deque<UnitCoroutine*> m_ready, m_running;
    std::map<int, UnitCoroutine*> m_waiting;
    std::map<int, int> m_timeout;
    ucontext_t m_mainctx;
    UnitCoroutine* m_runningcoro;
};

class UnitCoroutine {
   public:
    UnitCoroutine(std::function<void()> run, KCoroutine* kcoro, size_t stksize);
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