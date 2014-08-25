#include "shared_between_threads.h"

#include <cassert>

#define USING_SCOPED_ACCESSORS

int main() {
    SharedBetweenThreads<int> shared_int;
    std::thread([&shared_int]() {
                    int last_value;
                    { last_value = *shared_int.GetImmutableScopedAccessor(); }
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
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }).detach();

    int a = 0;
    while (true) {
        shared_int.MutableUse([&a](int& x) { x = a++; });
    }
}
