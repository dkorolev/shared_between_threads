#ifndef SHARED_BETWEEN_THREADS_H
#define SHARED_BETWEEN_THREADS_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

// A universal shared primitive.
// Used for simple locking, state sharing, semaphores and async waiting.
template <typename DATA> class SharedBetweenThreads {
  public:
    SharedBetweenThreads() : data_() {
    }

    SharedBetweenThreads(const SharedBetweenThreads&) = delete;
    SharedBetweenThreads(SharedBetweenThreads&&) = delete;
    void operator=(const SharedBetweenThreads&) = delete;

    // WaitForUpdates() blocks the current thread until an update happens.
    void WaitForUpdates() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock);
    }

    // Poke() notifies all waiting processes that something has happened.
    // Should be called from outside the locked function; calling it from a locked one would cause a deadlock.
    void Poke() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.notify_all();
    }

    // Simple lock usecase, locked, not notifying others.
    void UseAsLock(const std::function<void()>& f) const {
        std::unique_lock<std::mutex> lock(mutex_);
        f();
    }

    // Immutable usecase, locked, not notifying others.
    void ImmutableUse(const std::function<void(const DATA&)>& f) const {
        std::unique_lock<std::mutex> lock(mutex_);
        f(data_);
    }

    // Mutable access, locked, allowing to change the object and notifying others upon returning.
    void MutableUse(const std::function<void(DATA&)>& f) {
        std::unique_lock<std::mutex> lock(mutex_);
        f(data_);
        cv_.notify_all();
    }

    // Mutable access, locked, only notifying the others if `true` was returned.
    void MutableUse(const std::function<bool(DATA&)>& f) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (f(data_)) {
            cv_.notify_all();
        }
    }

  private:
    DATA data_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

#endif
