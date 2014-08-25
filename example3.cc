#include "shared_between_threads.h"

#include <cassert>

int main() {
    SharedBetweenThreads<std::pair<bool, int>> shared_object;
    std::thread([&shared_object]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    while (true) {
                        shared_object.WaitUntil([](const std::pair<bool, int>& p) { return p.first; });
                        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        {
                            auto p = shared_object.GetMutableScopedAccessor();
                            assert(p->first);
                            std::cout << "Update: " << p->second << std::endl;
                            p->first = false;
                        }
                    }
                }).detach();

    int a = 0;
    while (true) {
        shared_object.WaitUntil([](const std::pair<bool, int>& p) { return !p.first; });
        {
            auto p = shared_object.GetMutableScopedAccessor();
            assert(!p->first);
            p->first = true;
            p->second = a++;
        }
    }
}
