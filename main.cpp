#include "KvServer/kvserver.h"

#include <iostream>

int main()
{
    try {
        ServerConfig config{
            .port = 6379,
            .threadCount = 4,
            .cleanupInterval = std::chrono::seconds{5},
        };

        KvServer server(config);
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}