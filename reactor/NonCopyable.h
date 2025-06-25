#ifndef __NONCOPYABLE_H__
#define __NONCOPYABLE_H__

class NonCopyable{
protected:
    NonCopyable() = default;
    NonCopyable(const NonCopyable &rhs) = delete;
    NonCopyable &operator=(const NonCopyable &rhs) = delete;

};

#endif