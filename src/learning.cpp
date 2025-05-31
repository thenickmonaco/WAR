#include <iostream>
#include <mutex>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

std::mutex mtx;

void print_hello(int id) {
    std::lock_guard<std::mutex> lock(mtx); 
    std::cout << "Hello from thread: " << id << std::endl;
}

int main() {
    std::thread t1(print_hello, 0);
    std::thread t2(print_hello, 0);

    t1.join();
    t2.join();

    return 0;
}
