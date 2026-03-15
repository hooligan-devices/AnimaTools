#pragma once
#include <Arduino.h>

class LockGuard
{
    SemaphoreHandle_t mutex;

public:
    LockGuard(SemaphoreHandle_t m) : mutex(m)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
    ~LockGuard()
    {
        xSemaphoreGive(mutex);
    }
};

class MutexProtected
{
protected:
    SemaphoreHandle_t mutex;

public:
    MutexProtected()
    {
        mutex = xSemaphoreCreateMutex();
    }

    MutexProtected(const MutexProtected &) = delete;
    MutexProtected &operator=(const MutexProtected &) = delete;
    MutexProtected(MutexProtected &&) = delete;
    MutexProtected &operator=(MutexProtected &&) = delete;

    ~MutexProtected()
    {
        if (mutex)
            vSemaphoreDelete(mutex);
    }

protected:
    LockGuard guard() const
    {
        return LockGuard(mutex);
    }
};

#define MUTEX_PROTECTED_STRUCT(T) \
private: \
T data; \
\
public: \
T read() const \
{ \
    auto g = guard(); \
    return data; \
} \
\
template <typename F> \
auto read(F &&f) const -> const decltype(f(data)) \
{ \
    auto g = guard(); \
    return f(data); \
} \
\
void update(T &src) \
{ \
    auto g = guard(); \
    data = src; \
} \
\
void update(const T &src) \
{ \
    auto g = guard(); \
    data = src; \
} \
\
template <typename F> \
auto update(F &&f) -> decltype(f(data)) \
{ \
    auto g = guard(); \
    return f(data); \
}

/*
== USAGE EXAMPLE ==

++++ PROTECTED STRUCT CREATION

struct Main
{
    int a;
    float b;
    bool c;
    uint32_t d;
    int e;
};

struct MainProtected : private MutexProtected
{
private:
    Main data;

public:
    // full snapshot: copy protected → local
    Main read()
    {
        auto g = guard();
        return data;
    }

    // partial read
    template <typename F>
    auto read(F &&f) -> const decltype(f(data))
    {
        auto g = guard();
        return f(data);
    }

    // update: copy local → protected
    void update(const Main& src)
    {
        auto g = guard();
        data = src;
    }

    // same without const, to avoid compiler to choose template version if non-const passed
    void update(Main &src)
    {
        auto g = guard();
        data = src;
    }

    // partial update
    template <typename F>
    void update(F &&f)
    {
        auto g = guard();
        f(data);
    }
};

++++ USAGE

MainProtected shared;

{
    Main local = shared.read();
    local.a += 10;
    local.b *= 0.5f;
    shared.update(local);
}

// partial update:

shared.update([](Main& m){
    m.a += 10;
    m.b *= 0.5f;
});

// partial read:
float v = shared.read([](const Main& m){
    return m.velocity;
});





*/