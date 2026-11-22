#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "../Command Parser/commandparser.h"
#include <asio.hpp>

class TcpServer
{
public:
    explicit TcpServer(asio::io_context& io, int port, CommandParser& parser);
    void start();

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    CommandParser& m_parser;
};

#endif // TCPSERVER_H
