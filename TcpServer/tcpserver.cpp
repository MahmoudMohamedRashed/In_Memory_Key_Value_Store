#include "tcpserver.h"
#include "../Tcp Session/tcpsession.h"
#include <format>
#include <iostream>
#include <memory>

using tcp = asio::ip::tcp;
using namespace std;

TcpServer::TcpServer(asio::io_context& io, int port, CommandParser& parser)
    : m_acceptor(io, tcp::endpoint(tcp::v4(), port))
    , m_parser{parser}
{}

void TcpServer::doAccept()
{
    m_acceptor.async_accept([this](const asio::error_code& ec, tcp::socket socket) {
        if (ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            cerr << format("Server Accepter error : {}\n", ec.message());
        } else {
            make_shared<TcpSession>(std::move(socket), m_parser)->start();
        }
        doAccept();
    });
}

void TcpServer::start()
{
    cout << format("[TcpServer] Listening on port {}...\n", m_acceptor.local_endpoint().port());
    doAccept();
}