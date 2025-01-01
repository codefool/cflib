#include <iostream>
#include <cstdlib>
#include <condition_variable>

#include "/usr/local/libcf/include/libcf.h"

#define MAX_THREADS 8

std::mutex m_closing;
std::mutex m_cout;
bool closing = false;
std::condition_variable cv;

libcf::dq<uint64_t> work("/tmp/bang", "bang", sizeof(uint64_t) * 1024);

void thread_proc(int idx) {
    while ( !closing ) {
        std::unique_lock lk(m_closing);
        cv.wait(lk, []{ return !work.empty() || closing; });
        lk.unlock();
        cv.notify_one();
        if ( !closing ) {
            uint64_t data;
            if ( work.pop(data) ) {
                std::lock_guard<std::mutex> _ (m_cout);
                std::cout << idx << " pull " << data << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(idx*100));
    }
    {
        std::lock_guard<std::mutex> _ (m_cout);
        std::cout << idx << " leaving" << std::endl;
    }
}

int main() {
    std::thread t0(thread_proc, 0);
    std::thread t1(thread_proc, 1);
    std::thread t2(thread_proc, 2);
    std::thread t3(thread_proc, 3);

    std::srand(std::time(nullptr));
    for( int i = 0; i < 100; ++i ) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        uint64_t data = std::rand();
        work.push(data);
        {
            std::lock_guard<std::mutex> _ (m_cout);
            std::cout << "push " << data << std::endl;
        }
        cv.notify_one();
    }
    while ( !work.empty() )
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    {
        std::lock_guard<std::mutex> _(m_closing);
        closing = true;
        std::cout << "closing" << std::endl;
        cv.notify_one();
    }
    t0.join();
    t1.join();
    t2.join();
    t3.join();

    std::cout << "queue " << work.size() << std::endl;
    return 0;
}