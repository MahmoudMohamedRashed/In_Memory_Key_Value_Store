#include "kvserver.h"

#include <csignal>
#include <iostream>
#include <vector>

KvServer::KvServer(ServerConfig config)
    : m_config{config}
    , m_ioContext{static_cast<int>(std::max<std::size_t>(1, m_config.threadCount))}
    , m_store{}
    , m_parser{m_store}
    , m_tcpServer{m_ioContext, static_cast<int>(m_config.port), m_parser}
    , m_cleanupTimer{m_ioContext}
    , m_signals{m_ioContext, SIGINT, SIGTERM}
{}

KvServer::~KvServer()
{
    stop();
}

void KvServer::run()
{
    m_stopped.store(false, std::memory_order_relaxed);

    setupSignals();
    m_tcpServer.start();

    if (m_config.cleanupInterval > std::chrono::seconds{0}) {
        scheduleCleanup();
    }

    const std::size_t totalThreads = std::max<std::size_t>(1, m_config.threadCount);

    std::cout << "KvServer listening on port " << m_config.port << " with " << totalThreads
              << " thread(s) "
              << "(cleanup every " << m_config.cleanupInterval.count() << "s)\n";

    auto runWorker = [this]() {
        try {
            m_ioContext.run();
        } catch (const std::exception& ex) {
            std::cerr << "[Worker Error] " << ex.what() << '\n';
        }
    };

    std::vector<std::jthread> workers;
    workers.reserve(totalThreads - 1);
    for (std::size_t i = 1; i < totalThreads; ++i) {
        workers.emplace_back(runWorker);
    }

    runWorker();
}

void KvServer::stop()
{
    if (m_stopped.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    m_cleanupTimer.cancel();
    asio::error_code ec;
    m_signals.cancel(ec);
    m_ioContext.stop();
}

void KvServer::setupSignals()
{
    m_signals.async_wait([this](const asio::error_code& ec, int signalNumber) {
        if (!ec) {
            std::cout << "\nSignal " << signalNumber << " received. Shutting down...\n";
            stop();
        }
    });
}

void KvServer::scheduleCleanup()
{
    m_cleanupTimer.expires_after(m_config.cleanupInterval);
    m_cleanupTimer.async_wait([this](const asio::error_code& ec) {
        if (ec) {
            return;
        }

        if (const std::size_t removed = m_store.cleanUpExpired(); removed > 0) {
            std::cout << "[Cleanup] Removed " << removed << " expired key(s)\n";
        }

        scheduleCleanup();
    });
}