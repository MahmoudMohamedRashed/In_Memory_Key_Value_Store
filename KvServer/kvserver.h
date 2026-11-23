#ifndef KVSERVER_H
#define KVSERVER_H

#include "../Command Parser/commandparser.h"
#include "../KV-Store/kvstore.h"
#include "../TcpServer/tcpserver.h"

#include <algorithm>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

struct ServerConfig
{
    std::uint16_t port{6379};
    std::size_t threadCount{std::max<std::size_t>(1, std::thread::hardware_concurrency())};
    std::chrono::seconds cleanupInterval{5};
};

class KvServer
{
public:
    explicit KvServer(ServerConfig config = {});
    ~KvServer();

    KvServer(const KvServer&) = delete;
    KvServer& operator=(const KvServer&) = delete;

    void run();
    void stop();

private:
    void setupSignals();
    void scheduleCleanup();

    ServerConfig m_config;
    asio::io_context m_ioContext;
    KvStore m_store;
    CommandParser m_parser;
    TcpServer m_tcpServer;
    asio::steady_timer m_cleanupTimer;
    asio::signal_set m_signals;
    std::atomic<bool> m_stopped{false};
};

#endif // KVSERVER_H