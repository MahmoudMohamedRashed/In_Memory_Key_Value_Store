#include "Command Parser/commandparser.h"
#include "KV-Store/kvstore.h"
#include "TcpServer/tcpserver.h"

#include <algorithm>
#include <asio.hpp>
#include <csignal>
#include <cstdint>
#include <format>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;

int main()
{
    constexpr uint16_t kPort = 8080;

    try {
        KvStore store;
        CommandParser parser(store);
        asio::io_context io;
        TcpServer server(io, kPort, parser);

        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const asio::error_code&, int sig) {
            cout << format("\n[Server] Caught signal {}, shutting down...\n", sig);
            io.stop();
        });

        server.start();

        const unsigned int threadCount = max(1u, thread::hardware_concurrency());
        cout << format("[Server] Running with {} worker threads.\n", threadCount);

        vector<jthread> workers;
        workers.reserve(threadCount - 1);
        for (unsigned int i = 0; i < threadCount - 1; ++i) {
            workers.emplace_back([&io]() { io.run(); });
        }

        io.run();

    } catch (const exception& ex) {
        cerr << format("[Fatal] {}\n", ex.what());
        return 1;
    }

    cout << "[Server] Stopped cleanly.\n";
    return 0;
}