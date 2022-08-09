#include <iostream>
#include "KCoroutine.h"
#include "KTCP.h"

int main() {
    KCoroutine* kcoro = KCoroutine::getInstance();

    kcoro->createCoroutine([&]() {
        std::cout << "Hello World!\n";
        kcoro->yield();
        std::cout << "Hello Third Time!\n";
    });
    kcoro->createCoroutine([]() { std::cout << "Hello Again!\n"; });

    kcoro->createCoroutine([&]() {
        ListenFd fd_listen;
        fd_listen.Listen(5005);
        while (true) {
            int fd_client = fd_listen.Accept();
            if (fd_client < 0) {
                continue;
            }

            kcoro->createCoroutine([&]() {
                char buffer[1024];
                int n = read(fd_client, buffer, 1024);
                buffer[n] = '\0';
                std::cout << "recv: " << buffer << std::endl;
            });
        }
    });

    kcoro->disPatch();

    return 0;
}