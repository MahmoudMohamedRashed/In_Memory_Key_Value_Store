#ifndef TCPSESSION_H
#define TCPSESSION_H

#include "../Command Parser/commandparser.h"
#include <asio.hpp>
#include <memory>

class TcpSession : public std::enable_shared_from_this<TcpSession>
{
public:
    explicit TcpSession(asio::ip::tcp::socket socket, CommandParser& parser);

    void start();

private:
    void doRead();
    void doWrite(std::string msg);

    asio::ip::tcp::socket m_socket;
    CommandParser& m_parser;
    asio::streambuf m_readBuffer;
    std::string m_writeBuffer;

    constexpr static std::size_t m_MAXBUFFERSIZE = 64 * 1024; // 64kB
};

#endif // TCPSESSION_H
