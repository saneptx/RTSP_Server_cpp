#ifndef __TIMER_MANAGER_H__
#define __TIMER_MANAGER_H__

#include <functional>
#include <map>
#include <vector>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>

class TimerManager {
public:
    using TimerCallback = std::function<void()>;
    using TimerId = uint64_t; 

    TimerManager();
    ~TimerManager();

    // 添加一次性定时器（延时秒数）
    TimerId addTimer(int delaySec, TimerCallback &&cb);

    // 添加周期性定时器（延时、周期秒数）
    TimerId addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb);

    // 获取底层timerfd
    int getTimerFd() const { return _timerfd; }

    // 由上层 EventLoop 调用，当 timerfd 可读时触发
    void handleRead();

    void removeTimer(TimerId timerId);  // 删除定时器

private:
    struct Timer {
        int interval;  // 0 表示一次性，>0 表示周期
        TimerCallback callback;
    };

    int _timerfd;
    std::map<TimerId, std::pair<uint64_t, Timer>> _timers;  // key: expiration time
    void resetTimerfd();  // 设置下一个 timerfd 到期时间
    TimerId _nextId = 1;  // 用于生成唯一的 TimerId

    uint64_t getNowMs() const;
};

#endif  // TIMER_MANAGER_H
