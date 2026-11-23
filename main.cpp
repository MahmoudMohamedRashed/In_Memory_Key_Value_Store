#include "Command Parser/commandparser.h"
#include "KV-Store/kvstore.h"

#include <atomic>
#include <barrier>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

std::string escapeVisible(std::string_view sv)
{
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

struct TestRunner
{
    int passed{0};
    int failed{0};

    void expectEq(std::string_view actual, std::string_view expected, std::string_view name)
    {
        if (actual == expected) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "[FAIL] " << name << "\n"
                      << "  Expected: \"" << escapeVisible(expected) << "\"\n"
                      << "  Actual:   \"" << escapeVisible(actual) << "\"\n";
        }
    }

    void expectTrue(bool cond, std::string_view name)
    {
        if (cond) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "[FAIL] " << name << "\n";
        }
    }

    void summary() const
    {
        std::cout << "\n====================================\n"
                  << "Passed: " << passed << " | Failed: " << failed << "\n"
                  << "====================================\n";
    }
};

// ------------------------------------------------------------
// Suite 1: Basic Commands & Parser Edge Cases
// ------------------------------------------------------------
void testParserEdgeCases(TestRunner& t)
{
    std::cout << "[Suite 1] Running CommandParser edge cases...\n";
    KvStore store;
    CommandParser parser(store);

    // Empty / whitespace / CRLF lines
    t.expectEq(parser.execute(""), "", "Empty string");
    t.expectEq(parser.execute("   \t  \r\n"), "", "Whitespace + CRLF only");

    // Case insensitivity & PING / SIZE
    t.expectEq(parser.execute("ping\r\n"), "PONG\n", "Lowercase ping");
    t.expectEq(parser.execute("  PiNg  \n"), "PONG\n", "Mixed case PING with spaces");
    t.expectEq(parser.execute("SIZE"), "0\n", "Initial SIZE is 0");

    // SET with spaces in value & leading/trailing whitespace
    t.expectEq(parser.execute("  set \t msg   hello   world  \r\n"), "OK\n", "SET with spaces");
    t.expectEq(parser.execute("gEt msg\n"), "hello   world\n", "GET preserves inner spaces");
    t.expectEq(parser.execute("EXISTS msg"), "1\n", "EXISTS on existing key");
    t.expectEq(parser.execute("EXISTS missing"), "0\n", "EXISTS on missing key");
    t.expectEq(parser.execute("GET missing"), "(nil)\n", "GET on missing key");
    t.expectEq(parser.execute("SIZE"), "1\n", "SIZE after 1 SET");

    // Overwrite & DEL
    t.expectEq(parser.execute("SET msg updated"), "OK\n", "Overwrite existing key");
    t.expectEq(parser.execute("GET msg"), "updated\n", "GET returns overwritten value");
    t.expectEq(parser.execute("DEL msg"), "1\n", "DEL existing key returns 1");
    t.expectEq(parser.execute("DEL msg"), "0\n", "DEL already deleted key returns 0");
    t.expectEq(parser.execute("SIZE"), "0\n", "SIZE back to 0");

    // Unknown & Wrong number of arguments
    t.expectEq(parser.execute("UNKNOWN arg1"), "ERR unknown command\n", "Unknown command");
    t.expectEq(parser.execute("SET"), "ERR wrong number of arguments for 'SET'\n", "SET no args");
    t.expectEq(parser.execute("SET onlykey"),
               "ERR wrong number of arguments for 'SET'\n",
               "SET missing value");
    t.expectEq(parser.execute("GET"), "ERR wrong number of arguments for 'GET'\n", "GET no args");
    t.expectEq(parser.execute("GET a b"),
               "ERR wrong number of arguments for 'GET'\n",
               "GET extra arg");
    t.expectEq(parser.execute("DEL"), "ERR wrong number of arguments for 'DEL'\n", "DEL no args");
    t.expectEq(parser.execute("DEL a b"),
               "ERR wrong number of arguments for 'DEL'\n",
               "DEL extra arg");
    t.expectEq(parser.execute("EXISTS"),
               "ERR wrong number of arguments for 'EXISTS'\n",
               "EXISTS no args");
    t.expectEq(parser.execute("EXISTS a b"),
               "ERR wrong number of arguments for 'EXISTS'\n",
               "EXISTS extra arg");
    t.expectEq(parser.execute("PING extra"),
               "ERR wrong number of arguments for 'PING'\n",
               "PING extra arg");
    t.expectEq(parser.execute("SIZE extra"),
               "ERR wrong number of arguments for 'SIZE'\n",
               "SIZE extra arg");
    t.expectEq(parser.execute("TTL"), "ERR wrong number of arguments for 'TTL'\n", "TTL no args");
    t.expectEq(parser.execute("TTL a b"),
               "ERR wrong number of arguments for 'TTL'\n",
               "TTL extra arg");
    t.expectEq(parser.execute("EXPIRE"),
               "ERR wrong number of arguments for 'EXPIRE'\n",
               "EXPIRE no args");
    t.expectEq(parser.execute("EXPIRE k"),
               "ERR wrong number of arguments for 'EXPIRE'\n",
               "EXPIRE missing time");
    t.expectEq(parser.execute("EXPIRE k 10 extra"),
               "ERR wrong number of arguments for 'EXPIRE'\n",
               "EXPIRE extra arg");
}

// ------------------------------------------------------------
// Suite 2: TTL, EXPIRE, SET ... EX, Lazy & Active Expiration
// ------------------------------------------------------------
void testTtlAndExpiration(TestRunner& t)
{
    std::cout << "[Suite 2] Running TTL & expiration edge cases...\n";
    KvStore store;
    CommandParser parser(store);

    // Invalid SET ... EX edge cases
    t.expectEq(parser.execute("SET k v EX 0"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX 0 rejected");
    t.expectEq(parser.execute("SET k v EX -5"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX negative rejected");
    t.expectEq(parser.execute("SET k v EX abc"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX non-numeric rejected");
    t.expectEq(parser.execute("SET k v EX 10abc"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX trailing chars rejected");
    t.expectEq(parser.execute("SET k v EX 10 extra"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX extra token rejected");
    t.expectEq(parser.execute("SET k v EX 99999999999999999999999"),
               "ERR invalid expire time in 'SET'\n",
               "SET EX overflow rejected");
    t.expectEq(parser.execute("SET k EX 10"),
               "ERR wrong number of arguments for 'SET'\n",
               "SET missing value before EX");

    // TTL on missing & permanent keys
    t.expectEq(parser.execute("TTL nope"), "-2\n", "TTL on missing key is -2");
    t.expectEq(parser.execute("SET perm permanent_val"), "OK\n", "SET permanent key");
    t.expectEq(parser.execute("TTL perm"), "-1\n", "TTL on permanent key is -1");

    // SET with spaces + case-insensitive ex
    t.expectEq(parser.execute("SET phrase hello world eX 5"), "OK\n", "SET value with spaces + eX");
    t.expectEq(parser.execute("GET phrase"), "hello world\n", "GET value with spaces before EX");
    auto ttlVal = store.ttl("phrase");
    t.expectTrue(ttlVal >= 0 && ttlVal <= 5, "TTL for 5s key is in [0, 5]");

    // Overwriting a TTL key with non-TTL SET clears expiration
    t.expectEq(parser.execute("SET phrase now_permanent"), "OK\n", "Overwrite TTL key without EX");
    t.expectEq(parser.execute("TTL phrase"), "-1\n", "TTL becomes -1 after overwrite without EX");

    // EXPIRE edge cases
    t.expectEq(parser.execute("EXPIRE nope 10"), "0\n", "EXPIRE on missing key returns 0");
    t.expectEq(parser.execute("EXPIRE perm abc"),
               "ERR invalid expire time in 'EXPIRE'\n",
               "EXPIRE non-numeric");
    t.expectEq(parser.execute("EXPIRE perm 10abc"),
               "ERR invalid expire time in 'EXPIRE'\n",
               "EXPIRE trailing chars");

    t.expectEq(parser.execute("SET kill_neg val"), "OK\n", "SET key to kill with negative EXPIRE");
    t.expectEq(parser.execute("EXPIRE kill_neg -3"), "1\n", "EXPIRE negative deletes immediately");
    t.expectEq(parser.execute("GET kill_neg"), "(nil)\n", "GET after negative EXPIRE is (nil)");
    t.expectEq(parser.execute("TTL kill_neg"), "-2\n", "TTL after negative EXPIRE is -2");

    t.expectEq(parser.execute("SET kill_zero val"), "OK\n", "SET key to kill with 0 EXPIRE");
    t.expectEq(parser.execute("EXPIRE kill_zero 0"), "1\n", "EXPIRE 0 returns 1");
    t.expectEq(parser.execute("GET kill_zero"), "(nil)\n", "GET after EXPIRE 0 is (nil)");

    // Lazy expiration check (wait ~1.15s for 1s TTL)
    t.expectEq(parser.execute("SET temp1 v1 EX 1"), "OK\n", "SET temp1 EX 1");
    t.expectEq(parser.execute("SET temp2 v2 EX 1"), "OK\n", "SET temp2 EX 1");
    t.expectEq(parser.execute("SET temp3 v3"), "OK\n", "SET temp3 permanent");
    t.expectEq(parser.execute("EXPIRE temp3 1"), "1\n", "EXPIRE temp3 1s");

    std::cout << "  ...waiting 1.15s to test expiration...\n";
    std::this_thread::sleep_for(1150ms);

    // Expired key behaves as non-existent across all commands
    t.expectEq(parser.execute("GET temp1"), "(nil)\n", "Lazy expire on GET");
    t.expectEq(parser.execute("EXISTS temp1"), "0\n", "Lazy expire on EXISTS");
    t.expectEq(parser.execute("TTL temp1"), "-2\n", "Lazy expire on TTL");
    t.expectEq(parser.execute("EXPIRE temp1 60"), "0\n", "Cannot resurrect expired key with EXPIRE");
    t.expectEq(parser.execute("DEL temp1"), "0\n", "DEL on expired key returns 0 (was not alive)");

    // Active expiration check (temp2, temp3, and kill_zero if still in map)
    std::size_t removed = store.cleanUpExpired();
    t.expectTrue(removed >= 2, "cleanUpExpired removes remaining expired keys (temp2 & temp3)");
    t.expectEq(parser.execute("GET perm"), "permanent_val\n", "Permanent key survives cleanup");
    t.expectEq(parser.execute("SIZE"), "2\n", "Only perm and phrase remain after cleanup");
}

// ------------------------------------------------------------
// Suite 3: Multithreading Stress Test (KvStore + CommandParser)
// ------------------------------------------------------------
void testMultithreading(TestRunner& t)
{
    std::cout << "[Suite 3] Running multithreading stress test (11 threads)...\n";
    KvStore store;
    CommandParser parser(store);

    constexpr int kWriters = 4;
    constexpr int kReaders = 4;
    constexpr int kDeleters = 2;
    constexpr int kTotalWorkers = kWriters + kReaders + kDeleters + 1; // +1 cleaner
    constexpr int kOpsPerThread = 15'000;
    constexpr int kKeySpace = 50; // عدد keys قليل عمداً عشان نرفع الـ Contention لأقصى درجة

    std::barrier startSync(kTotalWorkers);
    std::atomic<bool> stopCleaner{false};
    std::atomic<int> protocolErrors{0};
    std::atomic<std::size_t> totalCleaned{0};

    std::vector<std::thread> workers;
    workers.reserve(kTotalWorkers);

    // Writer threads: SET, SET EX, EXPIRE
    for (int id = 0; id < kWriters; ++id) {
        workers.emplace_back([&, id]() {
            startSync.arrive_and_wait();
            for (int i = 0; i < kOpsPerThread; ++i) {
                std::string key = "k_" + std::to_string((i + id * 7) % kKeySpace);
                std::string res;
                if (i % 3 == 0) {
                    res = parser.execute("SET " + key + " val_" + std::to_string(i) + " EX 1\n");
                    if (res != "OK\n")
                        ++protocolErrors;
                } else if (i % 3 == 1) {
                    res = parser.execute("SET " + key + " perm_" + std::to_string(i) + "\n");
                    if (res != "OK\n")
                        ++protocolErrors;
                } else {
                    res = parser.execute("EXPIRE " + key + (i % 6 == 2 ? " 2\n" : " -1\n"));
                    if (res != "0\n" && res != "1\n")
                        ++protocolErrors;
                }
            }
        });
    }

    // Reader threads: GET, EXISTS, TTL, SIZE
    for (int id = 0; id < kReaders; ++id) {
        workers.emplace_back([&, id]() {
            startSync.arrive_and_wait();
            for (int i = 0; i < kOpsPerThread; ++i) {
                std::string key = "k_" + std::to_string((i + id * 13) % kKeySpace);
                if (i % 4 == 0) {
                    std::string res = parser.execute("GET " + key + "\n");
                    if (res.empty() || res.back() != '\n')
                        ++protocolErrors;
                } else if (i % 4 == 1) {
                    std::string res = parser.execute("EXISTS " + key + "\n");
                    if (res != "0\n" && res != "1\n")
                        ++protocolErrors;
                } else if (i % 4 == 2) {
                    std::string res = parser.execute("TTL " + key + "\n");
                    std::int64_t parsed{};
                    auto [ptr,
                          ec] = std::from_chars(res.data(), res.data() + res.size() - 1, parsed);
                    if (ec != std::errc{} || parsed < -2)
                        ++protocolErrors;
                } else {
                    std::string res = parser.execute("SIZE\n");
                    if (res.empty() || res.back() != '\n')
                        ++protocolErrors;
                }
            }
        });
    }

    // Deleter threads: DEL
    for (int id = 0; id < kDeleters; ++id) {
        workers.emplace_back([&, id]() {
            startSync.arrive_and_wait();
            for (int i = 0; i < kOpsPerThread; ++i) {
                std::string key = "k_" + std::to_string((i + id * 11) % kKeySpace);
                std::string res = parser.execute("DEL " + key + "\n");
                if (res != "0\n" && res != "1\n")
                    ++protocolErrors;
            }
        });
    }

    // Active cleanup thread (simulating asio::steady_timer)
    std::thread cleaner([&]() {
        startSync.arrive_and_wait();
        while (!stopCleaner.load(std::memory_order_relaxed)) {
            totalCleaned.fetch_add(store.cleanUpExpired(), std::memory_order_relaxed);
            std::this_thread::sleep_for(5ms);
        }
    });

    for (auto& w : workers) {
        w.join();
    }
    stopCleaner.store(true, std::memory_order_relaxed);
    cleaner.join();

    t.expectTrue(protocolErrors.load() == 0,
                 "Zero unexpected/corrupted responses under concurrency");
    t.expectEq(parser.execute("PING\n"), "PONG\n", "Store & parser healthy after stress test");

    std::cout << "  ...stress test finished. Total keys cleaned by background thread: "
              << totalCleaned.load() << "\n";
}

} // namespace

int main()
{
    TestRunner t;
    testParserEdgeCases(t);
    testTtlAndExpiration(t);
    testMultithreading(t);
    t.summary();
    return (t.failed == 0) ? 0 : 1;
}