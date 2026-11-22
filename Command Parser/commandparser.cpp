#include "commandparser.h"

using namespace std;

CommandParser::CommandParser(KvStore& store)
    : m_store{store}
{}

string CommandParser::toUpper(string_view str)
{
    string ans{};
    for (auto ch : str) {
        ans.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return ans;
}

void CommandParser::stripLineEnding(string_view& str)
{
    while (!str.empty() and (str.back() == '\r' or str.back() == '\n')) {
        str.remove_suffix(1);
    }
}

string_view CommandParser::nextToken(string_view& str)
{
    string_view token{};
    if (auto pos = str.find_first_not_of(" \t"); pos == string_view::npos) {
        str = {};
        return token;
    } else {
        str.remove_prefix(pos);
    }
    if (auto pos = str.find_first_of(" \t"); pos == string_view::npos) {
        token = str;
        str = {};
    } else {
        token = str.substr(0, pos);
        str.remove_prefix(pos);
    }
    return token;
}

string CommandParser::execute(string_view str)
{
    stripLineEnding(str);
    string_view cmdSv = nextToken(str);
    string cmd = toUpper(cmdSv);
    string ans;
    if (cmd == "SET") {
        if (auto key = nextToken(str); !key.empty()) {
            if (auto pos = str.find_first_not_of(" \t"); pos != string_view::npos) {
                str.remove_prefix(pos);
                m_store.set(key, str);
                ans = "OK\n";
            } else {
                ans = "ERR wrong number of arguments for \'SET\'\n";
            }
        } else {
            ans = "ERR wrong number of arguments for \'SET\'\n";
        }
    } else if (cmd == "GET") {
        if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
            if (optional<string> ret = m_store.get(key); ret.has_value()) {
                ret->push_back('\n');
                ans = std::move(*ret);
            } else {
                ans = "(nil)\n";
            }
        } else {
            ans = "ERR wrong number of arguments for \'GET\'\n";
        }
    } else if (cmd == "DEL") {
        if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
            ans = m_store.del(key) ? "1\n" : "0\n";
        } else {
            ans = "ERR wrong number of arguments for \'DEL\'\n";
        }
    } else if (cmd == "EXISTS") {
        if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
            ans = m_store.exists(key) ? "1\n" : "0\n";
        } else {
            ans = "ERR wrong number of arguments for \'EXISTS\'\n";
        }
    } else if (cmd == "PING") {
        if (nextToken(str).empty()) {
            ans = "PONG\n";
        } else {
            ans = "ERR wrong number of arguments for \'PING\'\n";
        }
    } else if (cmd == "SIZE") {
        if (nextToken(str).empty()) {
            ans = to_string(m_store.size()) + "\n";
        } else {
            ans = "ERR wrong number of arguments for \'SIZE\'\n";
        }
    } else if (cmd == "") {
        ans = "";
    } else {
        ans = "ERR unknown command\n";
    }
    return ans;
}
