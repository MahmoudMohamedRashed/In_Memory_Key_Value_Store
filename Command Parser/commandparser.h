#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include "../KV-Store/kvstore.h"

class CommandParser
{
public:
    explicit CommandParser(KvStore& store);

    [[nodiscard]] std::string execute(std::string_view str);

private:
    static std::string toUpper(std::string_view str);
    static void stripLineEnding(std::string_view& str);
    static std::string_view nextToken(std::string_view& str);

    KvStore& m_store;
};

#endif // COMMANDPARSER_H
