// Defines SharedBetweenThreads: a universal shared primitive.
// Used for simple locking, state sharing, semaphores and async waiting.

#ifndef SHARED_BETWEEN_THREADS_H
#define SHARED_BETWEEN_THREADS_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

struct ParentObjectGoneException : std::exception {};

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

    struct NoneDestructionPolicy {
        struct ScopedCall {
            inline ScopedCall(NoneDestructionPolicy*) {
            }
            inline operator bool() const {
                return true;
            }
        };
    };

    struct WaitDestructionPolicy {
        struct ScopedCall {
            inline explicit ScopedCall(WaitDestructionPolicy* parent) : parent_(parent) {
                std::cerr << "DEBUG: ScopedCall::ScopedCall()." << std::endl;
                std::lock_guard<std::mutex> guard(parent_->access_mutex_);
                std::cerr << "DEBUG: ScopedCall::ScopedCall(): ref_count_ = " << parent_->ref_count_ << "."
                          << std::endl;
                std::cerr << "DEBUG: ScopedCall::ScopedCall(): Acquired the mutex." << std::endl;
                if (parent_->destructing_) {
                    std::cerr << "DEBUG: ScopedCall::ScopedCall(): Throwing." << std::endl;
                    throw ParentObjectGoneException();
                }
                if (!parent_->ref_count_) {
                    std::cerr << "DEBUG: ScopedCall::ScopedCall(): Acquiring non-zero lock." << std::endl;
                    parent_->ref_count_is_zero_mutex_.lock();
                }
                ++parent_->ref_count_;
                std::cerr << "DEBUG: ScopedCall::ScopedCall(): OK." << std::endl;
            }
            inline ~ScopedCall() {
                std::lock_guard<std::mutex> guard(parent_->access_mutex_);
                std::cerr << "DEBUG: ScopedCall::~ScopedCall(): ref_count_ = " << parent_->ref_count_ << "."
                          << std::endl;
                --parent_->ref_count_;
                if (!parent_->ref_count_) {
                    std::cerr << "DEBUG: ScopedCall::~ScopedCall(): Releasing non-zero lock." << std::endl;
                    parent_->ref_count_is_zero_mutex_.unlock();
                }
            }

            inline operator bool() const {
                std::lock_guard<std::mutex> guard(parent_->access_mutex_);
                return !parent_->destructing_;
            }

            ScopedCall(ScopedCall&&) = default;
            ScopedCall(const ScopedCall&) = delete;
            void operator=(const ScopedCall&) = delete;

            mutable WaitDestructionPolicy* parent_;
        };
        ~WaitDestructionPolicy() {
            // Mark this object as the one being destructed.
            // This would prevent any new ScopedCall-s from being created.
            {
                std::lock_guard<std::mutex> guard(access_mutex_);
                std::cerr << "DEBUG: Setting `destructing_` to true." << std::endl;
                destructing_ = true;
            }
            // Wait until all active instaces of ScopedCall are destructed.
            std::cerr << "DEBUG: WaitDestructionPolicy::~WaitDestructionPolicy(): Waiting for the final lock."
                      << std::endl;
            ref_count_is_zero_mutex_.lock();
            std::cerr << "DEBUG: WaitDestructionPolicy::~WaitDestructionPolicy(): Final lock acquired." << std::endl;
        }
        std::mutex access_mutex_;
        size_t ref_count_ = 0;
        bool destructing_ = false;
        std::mutex ref_count_is_zero_mutex_;
    };

    template <typename T> struct DataHolder {
      public:
        DataHolder() : data_() {
        }

        explicit DataHolder(const T& data) : data_(data) {
        }

        ~DataHolder() {
        }

      protected:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        T data_;
    };

    // template <typename DATA, typename DESTRUCTION_POLICY = NoneDestructionPolicy>
    template <typename DATA, typename DESTRUCTION_POLICY = WaitDestructionPolicy>
    class SharedBetweenThreads : protected DataHolder<DATA>, protected DESTRUCTION_POLICY {
      public:
        typedef DATA T_DATA;
        typedef DataHolder<T_DATA> T_DATA_HOLDER;
        typedef DESTRUCTION_POLICY T_DESTRUCTION_POLICY;

        inline SharedBetweenThreads() : T_DATA_HOLDER() {
        }

        inline explicit SharedBetweenThreads(const T_DATA& data) : T_DATA_HOLDER(data) {
        }

        SharedBetweenThreads(const SharedBetweenThreads&) = delete;
        SharedBetweenThreads(SharedBetweenThreads&&) = delete;
        void operator=(const SharedBetweenThreads&) = delete;

        typename T_DESTRUCTION_POLICY::ScopedCall ScopedUser() {
            return typename DESTRUCTION_POLICY::ScopedCall(this);
        }

        // WaitUntil() blocks the current thread until an update satisfying the predicate happens.
        inline void WaitUntil(const std::function<bool(const T_DATA&)>& f) {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            while (!f(T_DATA_HOLDER::data_)) {
                T_DATA_HOLDER::cv_.wait(lock);
            }
        }

        // Poke() notifies all waiting processes that something has happened.

        // Should be called from outside the locked function; calling it from a locked one would cause a deadlock.
        inline void Poke() {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            T_DATA_HOLDER::cv_.notify_all();
        }

        // PokeFromLockedSection() notifies all waiting processes that something has happened.
        // Should be called from inside the locked function.
        inline void PokeFromLockedSection() {
            T_DATA_HOLDER::cv_.notify_all();
        }

        // Simple lock usecase, locked, not notifying others.
        inline void UseAsLock(const std::function<void()>& f) const {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            f();
        }

        // Immutable usecase, locked, not notifying others.
        inline void ImmutableUse(const std::function<void(const T_DATA&)>& f) const {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            f(T_DATA_HOLDER::data_);
        }

        // Mutable access, locked, allowing to change the object and notifying others upon returning.
        inline void MutableUse(const std::function<void(T_DATA&)>& f) {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            f(T_DATA_HOLDER::data_);
            T_DATA_HOLDER::cv_.notify_all();
        }

        // Mutable access, locked, only notifying the others if `true` was returned.
        inline void MutableUse(const std::function<bool(T_DATA&)>& f) {
            std::unique_lock<std::mutex> lock(T_DATA_HOLDER::mutex_);
            if (f(T_DATA_HOLDER::data_)) {
                T_DATA_HOLDER::cv_.notify_all();
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
    };
}  // namespace shared_between_threads.

using shared_between_threads::SharedBetweenThreads;

#endif
