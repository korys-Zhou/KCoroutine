#include <string.h>
#include <iostream>
#include "KCoroutine.h"
#include "KTCP.h"

int main() {
    KCoroutine* kcoro = KCoroutine::getInstance();

    kcoro->createCoroutine(
        [&]() {
            std::cout << "Hello World!\n";
            kcoro->yield();
            std::cout << "Hello Third Time!\n";
        },
        0);
    kcoro->createCoroutine([]() { std::cout << "Hello Again!\n"; }, 0);

    kcoro->createCoroutine(
        [&]() {
            ListenFd fd_listen = ListenFd::Listen(5005);
            while (true) {
                std::optional<std::shared_ptr<OpFd>> fd_op = fd_listen.Accept();
                if (!fd_op) {
                    continue;
                }
                kcoro->createCoroutine(
                    [fd_op]() {
                        while (true) {
                            char buffer[1024];
                            int n = (*fd_op)->Read(buffer, 1024);
                            if (n <= 0) {
                                break;
                            }
                            buffer[n] = '\0';
                            std::cout << "recv: " << buffer << std::endl;
                            char add[] = " // CHECK OK!";
                            strcat(buffer, add);
                            if ((*fd_op)->Write(buffer, sizeof(buffer)) <= 0) {
                                break;
                            }
                        }
                    },
                    0);
            }
        },
        0);

    kcoro->disPatch();

    return 0;
}