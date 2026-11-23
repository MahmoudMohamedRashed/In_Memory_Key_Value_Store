#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include "../KV-Store/kvstore.h"
#include <memory>
#include <string>
#include <string_view>

class CommandParser
{
public:
    explicit CommandParser(KvStore& store);
    ~CommandParser();

    [[nodiscard]] std::string execute(std::string_view str);

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

#endif // COMMANDPARSER_H
