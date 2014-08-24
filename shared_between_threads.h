// Defines SharedBetweenThreads: a universal shared primitive.
// Used for simple locking, state sharing, semaphores and async waiting.

#ifndef SHARED_BETWEEN_THREADS_H
#define SHARED_BETWEEN_THREADS_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace shared_between_threads {

    template <typename POINTER, bool> struct PokeIfPointerIsMutable;

    template <typename POINTER> struct PokeIfPointerIsMutable<POINTER, true> {
        static void Run(POINTER pointer) {
        }
    };

    template <typename POINTER> struct PokeIfPointerIsMutable<POINTER, false> {
        static void Run(POINTER pointer) {
            pointer->PokeFromLockedSection();
        }
    };

    template <typename DATA> class SharedBetweenThreads {
      public:
        typedef DATA T_DATA;

        inline SharedBetweenThreads() : data_() {
        }

        inline explicit SharedBetweenThreads(const T_DATA& data) : data_(data) {
        }

        SharedBetweenThreads(const SharedBetweenThreads&) = delete;
        SharedBetweenThreads(SharedBetweenThreads&&) = delete;
        void operator=(const SharedBetweenThreads&) = delete;

        // WaitForUpdates() blocks the current thread until an update happens.
        inline void WaitForUpdates() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock);
        }

        // Poke() notifies all waiting processes that something has happened.
        // Should be called from outside the locked function; calling it from a locked one would cause a deadlock.
        inline void Poke() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.notify_all();
        }

        // PokeFromLockedSection() notifies all waiting processes that something has happened.
        // Should be called from inside the locked function.
        inline void PokeFromLockedSection() {
            cv_.notify_all();
        }

        // Simple lock usecase, locked, not notifying others.
        inline void UseAsLock(const std::function<void()>& f) const {
            std::unique_lock<std::mutex> lock(mutex_);
            f();
        }

        // Immutable usecase, locked, not notifying others.
        inline void ImmutableUse(const std::function<void(const T_DATA&)>& f) const {
            std::unique_lock<std::mutex> lock(mutex_);
            f(data_);
        }

        // Mutable access, locked, allowing to change the object and notifying others upon returning.
        inline void MutableUse(const std::function<void(T_DATA&)>& f) {
            std::unique_lock<std::mutex> lock(mutex_);
            f(data_);
            cv_.notify_all();
        }

        // Mutable access, locked, only notifying the others if `true` was returned.
        inline void MutableUse(const std::function<bool(T_DATA&)>& f) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (f(data_)) {
                cv_.notify_all();
            }
        }

        // Mutable and immutable scoped accessors.
        template <class PARENT> class TemplatedScopedAccessor {
          public:
            typedef PARENT T_PARENT;
            typedef typename std::conditional<std::is_const<T_PARENT>::value,
                                              const typename T_PARENT::T_DATA,
                                              typename T_PARENT::T_DATA>::type T_DATA;

            explicit inline TemplatedScopedAccessor(T_PARENT* parent)
                : parent_(parent), lock_(parent->mutex_), pdata_(&parent->data_) {
            }

            inline TemplatedScopedAccessor(TemplatedScopedAccessor&& rhs)
                : parent_(rhs.parent_), lock_(std::move(rhs.lock_)), pdata_(rhs.pdata_) {
            }

            inline ~TemplatedScopedAccessor() {
                PokeIfPointerIsMutable<T_PARENT*, std::is_const<T_PARENT>::value>::Run(parent_);
            }

            TemplatedScopedAccessor() = delete;
            TemplatedScopedAccessor(const TemplatedScopedAccessor&) = delete;
            void operator=(const TemplatedScopedAccessor&) = delete;

            inline T_DATA* operator->() {
                return pdata_;
            }

            inline T_DATA& operator*() {
                return *pdata_;
            }

          private:
            T_PARENT* parent_;
            std::unique_lock<std::mutex> lock_;
            T_DATA* pdata_;
        };

        typedef TemplatedScopedAccessor<SharedBetweenThreads> MutableScopedAccessor;
        typedef TemplatedScopedAccessor<const SharedBetweenThreads> ImmutableScopedAccessor;

        inline ImmutableScopedAccessor GetImmutableScopedAccessor() {
            return ImmutableScopedAccessor(this);
        }

        inline MutableScopedAccessor GetMutableScopedAccessor() {
            return MutableScopedAccessor(this);
        }

      private:
        T_DATA data_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
    };

}  // namespace shared_between_threads.

using shared_between_threads::SharedBetweenThreads;

#endif
