#include "commandparser.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <functional>
#include <unordered_map>

using namespace std;

namespace {
struct parsedTTL
{
    std::optional<chrono::seconds> ttl{};
    bool hasError{false};
};
} // namespace

class CommandParser::Impl
{
public:
    explicit Impl(KvStore& store);

    string dispatch(string_view str);

private:
    using Handler = std::function<string(Impl*, string_view)>;

    static std::size_t findEndCaseInsensitive(string_view str, string_view target);

    std::string handleSet(std::string_view args);
    std::string handleGet(std::string_view args);
    std::string handleDel(std::string_view args);
    std::string handleExists(std::string_view args);
    std::string handleExpire(std::string_view args);
    std::string handleTtl(std::string_view args);
    std::string handlePing(std::string_view args);
    std::string handleSize(std::string_view args);

    static std::string toUpper(std::string_view str);
    static void stripLineEnding(std::string_view& str);
    static std::string_view nextToken(std::string_view& str);
    static parsedTTL getTime(std::string_view& str);
    static std::optional<chrono::seconds> convertStringToTime(string_view str);

    KvStore& m_store;
};

CommandParser::Impl::Impl(KvStore& store)
    : m_store{store}
{}

string CommandParser::Impl::dispatch(string_view str)
{
    static const std::unordered_map<string_view, Handler> Khandler = {
        {"SET", &Impl::handleSet},
        {"GET", &Impl::handleGet},
        {"DEL", &Impl::handleDel},
        {"EXISTS", &Impl::handleExists},
        {"EXPIRE", &Impl::handleExpire},
        {"TTL", &Impl::handleTtl},
        {"PING", &Impl::handlePing},
        {"SIZE", &Impl::handleSize},
    };

    stripLineEnding(str);
    string_view cmdSv = nextToken(str);
    if (cmdSv.empty()) {
        return "";
    }

    string cmd = toUpper(cmdSv);
    if (auto it = Khandler.find(cmd); it != Khandler.end()) {
        return it->second(this, str);
    }
    return "ERR unknown command\n";
}

string CommandParser::Impl::handleSet(string_view str)
{
    string ans;
    if (auto key = nextToken(str); !key.empty()) {
        auto [ttl, error] = getTime(str);
        if (error) {
            ans = "ERR invalid expire time in \'SET\'\n";
        } else if (auto pos = str.find_first_not_of(" \t"); pos != string_view::npos) {
            str.remove_prefix(pos);
            stripLineEnding(str); // to remove end space
            m_store.set(key, str, ttl);
            ans = "OK\n";
        } else {
            ans = "ERR wrong number of arguments for \'SET\'\n";
        }
    } else {
        ans = "ERR wrong number of arguments for \'SET\'\n";
    }
    return ans;
}

string CommandParser::Impl::handleGet(string_view str)
{
    string ans;
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
    return ans;
}

string CommandParser::Impl::handleDel(string_view str)
{
    string ans;
    if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
        ans = m_store.del(key) ? "1\n" : "0\n";
    } else {
        ans = "ERR wrong number of arguments for \'DEL\'\n";
    }
    return ans;
}

string CommandParser::Impl::handleExists(string_view str)
{
    string ans;
    if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
        ans = m_store.exists(key) ? "1\n" : "0\n";
    } else {
        ans = "ERR wrong number of arguments for \'EXISTS\'\n";
    }
    return ans;
}

string CommandParser::Impl::handleExpire(string_view str)
{
    auto key = nextToken(str);
    auto timeStr = nextToken(str);
    if (key.empty() or timeStr.empty() or !nextToken(str).empty()) {
        return "ERR wrong number of arguments for 'EXPIRE'\n";
    }

    auto time = convertStringToTime(timeStr);
    if (!time.has_value()) {
        return "ERR invalid expire time in 'EXPIRE'\n";
    }

    return m_store.expire(key, *time) ? "1\n" : "0\n";
}

string CommandParser::Impl::handleTtl(string_view str)
{
    string ans;
    if (auto key = nextToken(str); !key.empty() and nextToken(str).empty()) {
        ans = to_string(m_store.ttl(key));
        ans.push_back('\n');
    } else {
        ans = "ERR wrong number of arguments for \'TTL\'\n";
    }
    return ans;
}

string CommandParser::Impl::handlePing(string_view str)
{
    string ans;
    if (nextToken(str).empty()) {
        ans = "PONG\n";
    } else {
        ans = "ERR wrong number of arguments for \'PING\'\n";
    }
    return ans;
}

string CommandParser::Impl::handleSize(string_view str)
{
    string ans;
    if (nextToken(str).empty()) {
        ans = to_string(m_store.size()) + "\n";
    } else {
        ans = "ERR wrong number of arguments for \'SIZE\'\n";
    }
    return ans;
}

optional<chrono::seconds> CommandParser::Impl::convertStringToTime(string_view str)
{
    stripLineEnding(str);
    std::int64_t value{};
    const auto* first = str.data();
    const auto* last = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec == std::errc{} and ptr == last) {
        return chrono::seconds(value);
    }
    return std::nullopt;
}

std::size_t CommandParser::Impl::findEndCaseInsensitive(string_view str, string_view target)
{
    auto it = std::find_end(str.begin(),
                            str.end(),
                            target.begin(),
                            target.end(),
                            [](unsigned char a, unsigned char b) {
                                return std::tolower(a) == std::tolower(b);
                            });
    return (it == str.end()) ? string_view::npos : static_cast<size_t>(it - str.begin());
}

parsedTTL CommandParser::Impl::getTime(string_view& str)
{
    parsedTTL ttlAns;
    string_view time{};
    constexpr string_view token = " ex ";
    if (auto pos = findEndCaseInsensitive(str, token); pos != string_view::npos) {
        auto afterEX = str.substr(pos + token.size());
        time = nextToken(afterEX);
        if (auto parsed = convertStringToTime(time);
            !parsed.has_value() or *parsed <= chrono::seconds(0) or !nextToken(afterEX).empty()) {
            ttlAns.hasError = true;

        } else {
            str = str.substr(0, pos);
            ttlAns.ttl = parsed;
        }
    }
    return ttlAns;
}

string CommandParser::Impl::toUpper(string_view str)
{
    string ans{};
    for (auto ch : str) {
        ans.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return ans;
}

void CommandParser::Impl::stripLineEnding(string_view& str)
{
    while (!str.empty() and std::isspace(static_cast<unsigned char>(str.back())) != 0) {
        str.remove_suffix(1);
    }
}

string_view CommandParser::Impl::nextToken(string_view& str)
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

CommandParser::CommandParser(KvStore& store)
    : m_pimpl{make_unique<Impl>(store)}
{}

CommandParser::~CommandParser() = default;

string CommandParser::execute(string_view str)
{
    return m_pimpl->dispatch(str);
}
