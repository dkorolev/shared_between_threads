#include "shared_between_threads.h"

/*
int foo() {
    try {
    } catch (ParentObjectGoneException& e) {
        std::cerr << "Caught the magic exception." << std::endl;
    }
}
*/

int main() {
    {
        SharedBetweenThreads<int> shared_object(42);
        std::thread([&shared_object]() {
                        auto scope = shared_object.ScopedUser();
                        while (scope) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(250));
                            {
                                auto p = shared_object.GetMutableScopedAccessor();
                                std::cout << "Value: " << (*p)++ << std::endl;
                                // std::cout << "Value: " << shared_object.data_++ << std::endl;
                            }
                        }
                        std::cout << "We are out of scope. Give it another 2s and then be done with the thread."
                                  << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        std::cout << "Terminating the inner thread." << std::endl;
                    }).detach();

        std::cerr << "Waiting for 1s with `shared_object` in scope." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cerr << "Leaving the scope." << std::endl;
    }

    std::cerr << "Scope is done. Waiting for another 3s." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cerr << "Everything is done." << std::endl;
}
