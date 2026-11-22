#ifndef KVSTORE_H
#define KVSTORE_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class KvStore
{
public:
    KvStore();
    ~KvStore();

    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool exists(std::string_view key) const;
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
    [[nodiscard]] std::int64_t ttl(std::string_view key) const;
    [[nodiscard]] bool expire(std::string_view key, std::chrono::seconds ttl);

    std::size_t cleanUpExpired();

    void set(std::string_view key,
             std::string_view val,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    bool del(std::string_view key);

private:
    class Impl;
    const std::unique_ptr<Impl> m_pimpl;
};

#endif // KVSTORE_H
