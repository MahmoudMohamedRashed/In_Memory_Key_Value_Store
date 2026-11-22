#include "kvstore.h"

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace {
using namespace std;
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct HashString
{
    using is_transparent = void;
    std::size_t operator()(std::string_view str) const noexcept
    {
        return std::hash<std::string_view>{}(str);
    }
};

struct Entry
{
    string val;
    optional<TimePoint> expireAt;

    Entry() = default;
    Entry(string_view _val, optional<chrono::seconds> _expireAt)
        : val{_val}
    {
        if (_expireAt.has_value()) {
            expireAt = Clock::now() + *_expireAt;
        }
    }
};
} // namespace

class KvStore::Impl
{
public:
    std::unordered_map<std::string, Entry, HashString, std::equal_to<>> m_store;
    std::shared_mutex m_mtx;
    bool isExpired(const Entry& entry, TimePoint now = Clock::now()) const;
};

/**
 * @brief 
 * 
 * @param entry that will be checked
 * @param now time now
 * @return true only if entry has value and it's time now >= *entry.expireAt
 * @return false otherwise
 */
bool KvStore::Impl::isExpired(const Entry& entry, TimePoint now) const
{
    return entry.expireAt.has_value() and now >= *entry.expireAt;
}

KvStore::KvStore()
    : m_pimpl{make_unique<KvStore::Impl>()}
{}

KvStore::~KvStore() = default;

size_t KvStore::size() const
{
    shared_lock lock(m_pimpl->m_mtx);
    return m_pimpl->m_store.size();
}

optional<string> KvStore::get(string_view key) const
{
    shared_lock lock(m_pimpl->m_mtx);
    if (auto it = m_pimpl->m_store.find(key);
        it != m_pimpl->m_store.end() and !m_pimpl->isExpired(it->second)) {
        return it->second.val;
    }
    return nullopt;
}

bool KvStore::exists(string_view key) const
{
    shared_lock lock(m_pimpl->m_mtx);
    auto it = m_pimpl->m_store.find(key);
    return (it != m_pimpl->m_store.end()) and (!m_pimpl->isExpired(it->second));
}

void KvStore::set(string_view key, string_view val, optional<chrono::seconds> ttl)
{
    unique_lock lock(m_pimpl->m_mtx);
    m_pimpl->m_store.insert_or_assign(string(key), Entry(val, ttl));
}

bool KvStore::del(string_view key)
{
    unique_lock lock(m_pimpl->m_mtx);
    if (auto it = m_pimpl->m_store.find(key); it != m_pimpl->m_store.end()) {
        bool wasAlive = !m_pimpl->isExpired(it->second);
        m_pimpl->m_store.erase(it);
        return wasAlive;
    }
    return false;
}

bool KvStore::expire(string_view key, chrono::seconds ttl)
{
    unique_lock lock(m_pimpl->m_mtx);
    if (auto it = m_pimpl->m_store.find(key);
        it != m_pimpl->m_store.end() and !m_pimpl->isExpired(it->second)) {
        if (ttl <= chrono::seconds(0)) {
            m_pimpl->m_store.erase(it);
        } else {
            it->second.expireAt = Clock::now() + ttl;
        }
        return true;
    }
    return false;
}

std::int64_t KvStore::ttl(string_view key) const
{
    shared_lock lock(m_pimpl->m_mtx);
    auto it = m_pimpl->m_store.find(key);
    if (it == m_pimpl->m_store.end()) {
        return -2;
    }

    if (!it->second.expireAt.has_value()) {
        return -1;
    }

    const auto now = Clock::now();
    if (m_pimpl->isExpired(it->second, now)) {
        return -2;
    }

    return chrono::duration_cast<chrono::seconds>(*it->second.expireAt - now).count();
}

std::size_t KvStore::cleanUpExpired()
{
    unique_lock lock(m_pimpl->m_mtx);
    const auto now = Clock::now();
    return std::erase_if(m_pimpl->m_store, [&now, this](const auto& item) {
        return m_pimpl->isExpired(item.second, now);
    });
}