#include "kvstore.h"
#include <mutex>

using namespace std;

size_t KvStore::size() const
{
    shared_lock lock(m_mtx);
    return m_store.size();
}

optional<string> KvStore::get(string_view key) const
{
    shared_lock lock(m_mtx);
    if (auto it = m_store.find(key); it != m_store.end()) {
        return it->second;
    }
    return nullopt;
}

bool KvStore::exists(string_view key) const
{
    shared_lock lock(m_mtx);
    return m_store.contains(key);
}

void KvStore::set(string_view key, string_view val)
{
    unique_lock lock(m_mtx);
    m_store.insert_or_assign(string(key), string(val));
}

bool KvStore::del(string_view key)
{
    unique_lock lock(m_mtx);
    if (auto it = m_store.find(key); it != m_store.end()) {
        m_store.erase(it);
        return true;
    }
    return false;
}
