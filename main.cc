#include "shared_between_threads.h"

#include <cassert>

#define USING_SCOPED_ACCESSORS

void example1() {
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
                    while (true) {
                        shared_int.UseAsLock([]() { std::cout << "Waiting for updates." << std::endl; });
                        shared_int.WaitForUpdates();
                        shared_int.UseAsLock([]() { std::cout << "Update detected!" << std::endl; });
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

void example2_would_skip_values() {
    SharedBetweenThreads<int> shared_int;
    std::thread([&shared_int]() {
                    while (true) {
                        shared_int.WaitForUpdates();
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        shared_int.ImmutableUse([](const int& x) { std::cout << "Update: " << x << std::endl; });
                    }
                }).detach();

    int a = 0;
    while (true) {
        shared_int.MutableUse([&a](int& x) { x = a++; });
    }
}

void example3_does_not_skip_values() {
    SharedBetweenThreads<std::pair<bool, int>> shared_object;
    std::thread([&shared_object]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    while (true) {
#ifdef USING_SCOPED_ACCESSORS
                        while (!shared_object.GetImmutableScopedAccessor()->first) {
                            shared_object.WaitForUpdates();
                        }
#else
                        bool ready = false;
                        while (!ready) {
                            shared_object.ImmutableUse([&ready](const std::pair<bool, int>& p) {
                                if (p.first) {
                                    ready = true;
                                }
                            });
                            if (!ready) {
                                shared_object.WaitForUpdates();
                            }
                        }
#endif

                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

#ifdef USING_SCOPED_ACCESSORS
                        {
                            auto p = shared_object.GetMutableScopedAccessor();
                            assert(p->first);
                            std::cout << "Update: " << p->second << std::endl;
                            p->first = false;
                        }
#else
                        shared_object.MutableUse([](std::pair<bool, int>& p) {
                            assert(p.first);
                            std::cout << "Update: " << p.second << std::endl;
                            p.first = false;
                        });
#endif
                    }
                }).detach();

    int a = 0;
    while (true) {
#ifdef USING_SCOPED_ACCESSORS
        while (shared_object.GetImmutableScopedAccessor()->first) {
            shared_object.WaitForUpdates();
        }
#else
        bool done = false;
        while (!done) {
            shared_object.ImmutableUse([&done](const std::pair<bool, int>& p) {
                if (!p.first) {
                    done = true;
                }
            });
            if (!done) {
                shared_object.WaitForUpdates();
            }
        }
#endif

#ifdef USING_SCOPED_ACCESSORS
        {
            auto p = shared_object.GetMutableScopedAccessor();
            assert(!p->first);
            p->first = true;
            p->second = a++;
        }
#else
        shared_object.MutableUse([&a](std::pair<bool, int>& p) {
            assert(!p.first);
            p.first = true;
            p.second = a++;
        });
#endif
    }
}

int main() {
    example1();
    // example2_would_skip_values();
    // example3_does_not_skip_values();
}
