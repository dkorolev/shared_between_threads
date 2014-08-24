#include "shared_between_threads.h"

#include <cassert>

void example1() {
    SharedBetweenThreads<int> shared_int;

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        shared_int.ImmutableUse(
                            [](int x) { std::cout << "Two seconds have passed, X = " << x << ".\n"; });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        shared_int.UseAsLock([]() { std::cout << "Five seconds have passed.\n"; });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    while (true) {
                        shared_int.UseAsLock([]() { std::cout << "Waiting for updates.\n"; });
                        shared_int.WaitForUpdates();
                        shared_int.UseAsLock([]() { std::cout << "Update detected!\n"; });
                    }
                }).detach();

    std::thread([&shared_int]() {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                        shared_int.UseAsLock([]() { std::cout << "Ten seconds have passed, poking.\n"; });
                        shared_int.Poke();
                        shared_int.UseAsLock([]() { std::cout << "Poke successful.\n"; });
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
#ifdef NOT_YET_IMPLEMENTED_SCOPED_OBJECTS
                        while (!shared_object.GetImmutableScopedObject()->first) {
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
                        if (ready) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            shared_object.MutableUse([](std::pair<bool, int>& p) {
                                assert(p.first);
                                std::cout << "Update: " << p.second << std::endl;
                                p.first = false;
                            });
                        }
                    }
                }).detach();

    int a = 0;
    while (true) {
#ifdef NOT_YET_IMPLEMENTED_SCOPED_OBJECTS
        while (shared_object.GetImmutableScopedObject()->first) {
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

        shared_object.MutableUse([&a](std::pair<bool, int>& p) {
            assert(!p.first);
            p.first = true;
            p.second = a++;
        });
    }
}

int main() {
    // example1();
    // example2_would_skip_values();
    example3_does_not_skip_values();
}
