#include <string.h>
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

            OpFd fd_op(fd_client);
            kcoro->createCoroutine([&]() {
                char buffer[1024];
                int n = fd_op.Read(buffer, 1024);
                if (n <= 0) {
                    fd_op.Close();
                }
                buffer[n] = '\0';
                std::cout << "recv: " << buffer << std::endl;
                if (fd_op.Write(buffer, n) <= 0) {
                    fd_op.Close();
                }

                // Writing yield test...
                // static char msg[4200000];
                // memset(msg, 0, sizeof(msg));
                // if (fd_op.Write(msg, sizeof(msg)) <= 0) {
                //     fd_op.Close();
                // }

                fd_op.Close();
            });
        }
    });

    kcoro->disPatch();

    return 0;
}