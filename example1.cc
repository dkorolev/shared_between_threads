#include "shared_between_threads.h"

#include <cassert>

//#define USING_SCOPED_ACCESSORS

int main() {
    SharedBetweenThreads<int> shared_int;

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        shared_int.ImmutableUse(
                            [](int x) { std::cout << "Two seconds have passed, X = " << x << "." << std::endl; });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        shared_int.UseAsLock([]() { std::cout << "Five seconds have passed." << std::endl; });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    int last_value;
                    { last_value = *shared_int.GetImmutableScopedAccessor(); }
                    shared_int.UseAsLock([]() { std::cout << "Waiting for updates." << std::endl; });
                    while (true) {
                        shared_int.WaitUntil([&last_value](int current_value) {
                            if (current_value == last_value) {
                                return false;
                            } else {
                                std::cout << "Update from " << last_value << " to " << current_value << " detected!"
                                          << std::endl;
                                last_value = current_value;
                                return true;
                            }
                        });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                        shared_int.UseAsLock([]() { std::cout << "Ten seconds have passed, poking." << std::endl; });
                        shared_int.Poke();
                        shared_int.UseAsLock([]() { std::cout << "Poke successful." << std::endl; });
                    }
                }).detach();

    while (true) {
        int a;
        std::cin >> a;
#ifdef USING_SCOPED_ACCESSORS
        { *shared_int.GetMutableScopedAccessor() = a; }
#else
        shared_int.MutableUse([&a](int& x) {
            x = a;
            std::cout << "Changed X to " << a << "." << std::endl;
        });
#endif
    }
}
