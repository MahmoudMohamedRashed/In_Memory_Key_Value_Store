#ifndef KVSTORE_H
#define KVSTORE_H

#include <cstddef>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

struct HashString
{
    using is_transparent = void;
    std::size_t operator()(std::string_view str) const noexcept
    {
        return std::hash<std::string_view>{}(str);
    }
};

class KvStore
{
public:
    KvStore() = default;
    ~KvStore() = default;

    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool exists(std::string_view key) const;
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const;

    void set(std::string_view key, std::string_view val);
    bool del(std::string_view key);

private:
    std::unordered_map<std::string, std::string, HashString, std::equal_to<>> m_store;
    mutable std::shared_mutex m_mtx;
};

#endif // KVSTORE_H
