#include "shared_between_threads.h"

void example1() {
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

int main() {
    example1();
}
