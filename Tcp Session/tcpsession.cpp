#include "tcpsession.h"
#include <format>
#include <iostream>

using tcp = asio::ip::tcp;
using namespace std;

TcpSession::TcpSession(tcp::socket socket, CommandParser& parser)
    : m_socket{std::move(socket)}
    , m_parser{parser}
    , m_readBuffer{m_MAXBUFFERSIZE}
{}

void TcpSession::start()
{
    doRead();
}

void TcpSession::doRead()
{
    auto self = shared_from_this();
    asio::async_read_until(m_socket,
                           m_readBuffer,
                           '\n',
                           [self](const asio::error_code& ec, [[maybe_unused]] size_t) {
                               if (ec) {
                                   if (ec != asio::error::eof
                                       and ec != asio::error::connection_reset) {
                                       cerr << format("[TcpSession] read error : {}\n",
                                                      ec.message());
                                   }
                                   return;
                               }
                               istream is(&self->m_readBuffer);
                               string line{};
                               getline(is, line);

                               if (auto response = self->m_parser.execute(line); !response.empty()) {
                                   self->doWrite(std::move(response));
                               } else {
                                   self->doRead();
                               }
                           });
}

void TcpSession::doWrite(string msg)
{
    m_writeBuffer = std::move(msg);
    asio::async_write(m_socket,
                      asio::buffer(m_writeBuffer),
                      [self = shared_from_this()](const asio::error_code& ec,
                                                  [[maybe_unused]] size_t) {
                          if (ec) {
                              if (ec != asio::error::eof and ec != asio::error::connection_reset
                                  and ec != asio::error::broken_pipe) {
                                  cerr << format("[TcpSession] write error : {}\n", ec.message());
                              }
                              return;
                          }
                          self->doRead();
                      });
}