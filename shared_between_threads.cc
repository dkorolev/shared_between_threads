// g++ -std=c++11 -o shared_between_threads shared_between_threads.cc -lpthread && ./shared_between_threads

#include <iostream>      
#include <thread>        
#include <mutex>         
#include <condition_variable>

// A universal shared primitive.
// Used for simple locking, state sharing, semaphores and async waiting.
template<typename DATA> class SharedBetweenThreads {
  public:
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
        if(f(data_)) {
            cv_.notify_all(); 
        }
    }

  private:
    DATA data_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// Example usage.
int main() {
    SharedBetweenThreads<int> shared_int;

    std::thread([&shared_int]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            shared_int.ImmutableUse([](int x){
                std::cout << "Two seconds have passed, X = " << x << ".\n";
            });
        }
    }).detach();

    std::thread([&shared_int]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            shared_int.UseAsLock([](){
                std::cout << "Five seconds have passed.\n";
            });
        }
    }).detach();

    std::thread([&shared_int]() {
        while (true) {
            shared_int.UseAsLock([](){
                std::cout << "Waiting for updates.\n";
            });
            shared_int.WaitForUpdates();
            shared_int.UseAsLock([](){
                std::cout << "Update detected!\n";
            });
        }
    }).detach();

    std::thread([&shared_int]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            shared_int.UseAsLock([](){
                std::cout << "Ten seconds have passed, poking.\n";
            });
            shared_int.Poke();
            shared_int.UseAsLock([](){
                std::cout << "Poke successful.\n";
            });
        }
    }).detach();

    while (true) {
        int a;
        std::cin >> a;
        shared_int.MutableUse([&a](int& x) {
            x = a;
            std::cout << "Changed X to " << a << ".\n";
        });
    }
}
