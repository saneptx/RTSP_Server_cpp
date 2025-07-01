#include "TimerManager.h"
#include <sys/time.h>
#include <string.h>
#include <iostream>

TimerManager::TimerManager() {
    _timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
}

TimerManager::~TimerManager() {
    if (_timerfd >= 0)
        close(_timerfd);
}

uint64_t TimerManager::getNowMs() const {
    /*
        struct timespec {
            time_t tv_sec; // 秒
            long tv_nsec; // 纳秒
        };
     */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);//获取当前时间，从系统启动时间开始计时
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;//返回毫秒
}

TimerManager::TimerId TimerManager::addTimer(int delaySec, TimerCallback &&cb) {
    uint64_t expireTime = getNowMs() + delaySec;//计算到期执行时间单位毫秒
    TimerId timerId = _nextId++;
    _timers.emplace(timerId, std::make_pair(expireTime, Timer{0, std::move(cb)}));//添加到执行表，执行时间和回调函数
    resetTimerfd();
    return timerId;
}

TimerManager::TimerId TimerManager::addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb) {
    uint64_t expireTime = getNowMs() + delaySec;
    TimerId timerId = _nextId++;
    _timers.emplace(timerId, std::make_pair(expireTime, Timer{intervalSec, std::move(cb)}));
    resetTimerfd();
    return timerId;
}

void TimerManager::removeTimer(TimerId timerId) {
    auto it = _timers.find(timerId);
    if (it != _timers.end()) {
        _timers.erase(it);
        resetTimerfd();  // 删除后重置 timerfd
    }
}

void TimerManager::handleRead() {
    uint64_t expirations;
    ssize_t ret = ::read(_timerfd, &expirations, sizeof(expirations));  // 清除触发状态
    (void)ret; // 忽略返回值

    uint64_t now = getNowMs();

    std::vector<std::pair<TimerId, Timer>> expired;

    for (auto it = _timers.begin(); it != _timers.end() && it->second.first <= now;) {
        expired.push_back({it->first, it->second.second});
        it = _timers.erase(it);
    }

    for (auto& pair : expired) {
        auto& timerId = pair.first;
        auto& timer = pair.second;

        if (timer.callback) timer.callback();
        if (timer.interval > 0) {
            _timers.emplace(timerId, std::make_pair(now + timer.interval, timer));
        }
    }

    resetTimerfd();
}


void TimerManager::resetTimerfd() {
    if (_timers.empty()) {
        itimerspec spec{};
        timerfd_settime(_timerfd, 0, &spec, nullptr);  // 停用定时器
        return;
    }

    uint64_t nextExpire = _timers.begin()->second.first;
    uint64_t now = getNowMs();
    uint64_t diffMs = (nextExpire > now) ? (nextExpire - now) : 1;

    itimerspec spec{};
    spec.it_value.tv_sec = diffMs / 1000;
    spec.it_value.tv_nsec = (diffMs % 1000) * 1000000;
    timerfd_settime(_timerfd, 0, &spec, nullptr);
}
