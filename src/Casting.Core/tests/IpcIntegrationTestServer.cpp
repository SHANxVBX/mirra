#include "ipc/PipeServer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using namespace mirra;

int main(int argc, char** argv) {
    std::string pipeName = "mirra-test-pipe";
    if (argc > 1) {
        pipeName = argv[1];
    }

    PipeServer server(pipeName, "test-session");
    server.onMessage([&](const IpcMessage& msg) {
        if (msg.type == "quit") {
            server.stop();
        } else {
            IpcMessage reply;
            reply.type = msg.type + "-reply";
            reply.payload = msg.payload;
            server.send(reply);
        }
    });

    if (server.start()) {
        std::cout << "READY" << std::endl;
        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } else {
        std::cerr << "FAILED" << std::endl;
        return 1;
    }
    return 0;
}
