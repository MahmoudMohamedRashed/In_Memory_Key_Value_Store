# In-Memory Key-Value Store (C++20 / Standalone Asio)

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=cplusplus)
![Asio](https://img.shields.io/badge/Networking-Standalone%20Asio-2EA44F?style=flat)
![CMake](https://img.shields.io/badge/Build-CMake%203.16%2B-064F8C?style=flat&logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat&logo=linux&logoColor=black)
![Sanitizers](https://img.shields.io/badge/ASan%20%7C%20UBSan%20%7C%20TSan-Passing-brightgreen?style=flat)

A high-performance, asynchronous, multithreaded **In-Memory Key-Value Store** server inspired by Redis, built from scratch in **Modern C++20** and **Standalone Asio** on Linux.

Built to combine **network programming**, **concurrency**, and **modern C++ systems design** into a modular, low-latency, sanitizer-verified server.

---

## Architecture

```text
                       ┌───────────────────────────────────────┐
                       │               KvServer                │
                       │  (io_context + std::jthread pool +    │
                       │   asio::signal_set + steady_timer)    │
                       └───────────┬───────────────┬───────────┘
                                   │               │
                 Async TCP         │               │ Active Cleanup (periodic)
              ┌────────────────────▼───┐           │
              │       TcpServer        │           │
              │  (async_accept loop)   │           │
              └────────────┬───────────┘           │
                           │ creates               │
              ┌────────────▼───────────┐           │
              │       TcpSession       │           │
              │ (Zero-copy streambuf,  │           │
              │  TCP_NODELAY, 64KB cap)│           │
              └────────────┬───────────┘           │
                           │ std::string_view      │
              ┌────────────▼───────────┐           │
              │     CommandParser      │           │
              │ (PIMPL + Dispatch Map  │           │
              │  + std::from_chars)    │           │
              └────────────┬───────────┘           │
                           │                       │
              ┌────────────▼───────────────────────▼───────────┐
              │                    KvStore                     │
              │ (PIMPL + std::shared_mutex + steady_clock TTL) │
              └────────────────────────────────────────────────┘
```

---

## Key Engineering Highlights

* **Zero-Copy Command Parsing:** `TcpSession` constructs a `std::string_view` directly over `asio::streambuf` using `bytesTransferred` before consuming the line—avoiding per-request `std::istream` construction and `std::string` heap allocations on the hot path.
* **Modern C++20 Design:** Uses `std::jthread` for RAII worker thread lifecycle management, `std::from_chars` for fast locale-independent integer parsing, `std::erase_if`, designated initializers (`ServerConfig`), `std::format`, and the **PIMPL** idiom in `KvStore` and `CommandParser` for clean encapsulation.
* **Concurrency & Implicit Strand:** `KvStore` uses `std::shared_mutex` to allow concurrent readers and exclusive writers. Each `TcpSession` enforces a strict `async_read_until` → `execute` → `async_write` loop, acting as an **implicit strand** with zero extra session-level locking.
* **Low-Latency & Defensive Networking:** Enables `TCP_NODELAY` (disabling Nagle's algorithm) to keep small responses sub-millisecond, naturally handles TCP pipelining, and caps the per-session read buffer at **64 KB** to prevent unbounded memory growth.
* **Hybrid TTL Expiration:** Backed by monotonic `std::chrono::steady_clock` (immune to system wall-clock jumps):
  * **Lazy Expiration:** Expired keys are rejected immediately on access (`GET`, `EXISTS`, `TTL`, `EXPIRE`, `DEL`).
  * **Active Expiration:** Periodic background cleanup driven by `asio::steady_timer` calling `KvStore::cleanUpExpired()`.
* **Graceful Shutdown:** Captures `SIGINT` and `SIGTERM` via `asio::signal_set` and guarantees an idempotent, thread-safe shutdown using `std::atomic<bool>::exchange` (`std::memory_order_acq_rel`).

---

## Project Structure

```text
.
├── CMakeLists.txt           # CMake build configuration (C++20, Standalone Asio)
├── main.cpp                 # Entry point & ServerConfig setup
├── KvServer/                # Server orchestrator (thread pool, signals, cleanup timer)
│   ├── kvserver.h
│   └── kvserver.cpp
├── TcpServer/               # Asynchronous TCP acceptor
│   ├── tcpserver.h
│   └── tcpserver.cpp
├── Tcp Session/             # Per-client async session (zero-copy read, TCP_NODELAY)
│   ├── tcpsession.h
│   └── tcpsession.cpp
├── Command Parser/          # PIMPL command parser & static dispatch table
│   ├── commandparser.h
│   └── commandparser.cpp
├── KV-Store/                # Thread-safe in-memory storage engine with TTL support
│   ├── kvstore.h
│   └── kvstore.cpp
├── benchmark.py             # Multithreaded TCP stress test & latency benchmark
└── scripts/                 # Helper demo and shell stress-testing scripts
    ├── run_demo.sh
    └── stress_test.sh
```

---

## Supported Commands

| Command | Syntax | Description |
| :--- | :--- | :--- |
| **SET** | `SET <key> <value> [EX <seconds>]` | Stores a key-value pair (supports spaces in `<value>` and optional TTL in seconds). |
| **GET** | `GET <key>` | Returns the stored value, or `(nil)` if the key does not exist or has expired. |
| **DEL** | `DEL <key>` | Deletes `<key>`. Returns `1` if deleted, `0` otherwise. |
| **EXISTS** | `EXISTS <key>` | Returns `1` if `<key>` exists and is not expired, `0` otherwise. |
| **EXPIRE** | `EXPIRE <key> <seconds>` | Sets a TTL on an existing key (values `<= 0` expire the key immediately). |
| **TTL** | `TTL <key>` | Returns remaining seconds, `-1` if persistent, or `-2` if missing/expired. |
| **PING** | `PING` | Health check; responds with `PONG`. |
| **SIZE** | `SIZE` | Returns the current number of entries in the store. |

---

## Build & Run

### Prerequisites
* **Compiler:** GCC 13+ or Clang 16+ (with C++20 `<format>`, `<barrier>`, `std::jthread` support)
* **Build System:** CMake 3.16+
* **Networking:** Standalone Asio headers (`sudo apt install libasio-dev` on Ubuntu/Debian, or specify a custom path via `-DASIO_INCLUDE_DIR=...`)
* **Python 3:** Required only to run `benchmark.py`

### Build (Release Mode)
```bash
git clone [https://github.com/MahmoudMohamedRashed/In_Memory_Key_Value_Store.git](https://github.com/MahmoudMohamedRashed/In_Memory_Key_Value_Store.git)
cd In_Memory_Key_Value_Store

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/In_Memory_Key_Value_Store
```

*(If Standalone Asio is installed in a custom directory, pass `-DASIO_INCLUDE_DIR=/path/to/asio/include` to the `cmake -B build` step.)*

### Quick Interactive Test
```bash
nc 127.0.0.1 6379
PING
# PONG
SET user:1 hello world EX 5
# OK
TTL user:1
# 4
GET user:1
# hello world
SIZE
# 1
```

---

## Verification & Benchmarks

### Sanitizer Verification
All **71 unit, parser edge-case, TTL, and 11-thread concurrency stress tests** pass with **0 failures** and zero warnings under:
* **AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan)**
* **ThreadSanitizer (TSan)**

### TCP Benchmark Results
Run the included benchmark against a **Release** build:
```bash
python3 benchmark.py --clients 50 --requests 2000 --pipeline 16
```

Measured across **50 concurrent TCP clients** (**500,000 total requests**, **0 errors**):

| Stage | Clients | Pipeline | Total Requests | Throughput | Avg Latency | p50 Latency | p95 Latency | p99 Latency | Errors |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **PING** (Networking Baseline) | 50 | 1 | 100,000 | **45,836 req/s** | 1.077 ms | 0.888 ms | 2.644 ms | 3.745 ms | 0 |
| **SET** (Write-Heavy / `unique_lock`) | 50 | 1 | 100,000 | **26,176 req/s** | 1.893 ms | 1.128 ms | 6.809 ms | 12.355 ms | 0 |
| **GET** (Read-Heavy / `shared_lock`) | 50 | 1 | 100,000 | **23,996 req/s** | 2.032 ms | 1.172 ms | 7.682 ms | 13.650 ms | 0 |
| **Mixed + TTL** (Pipelined) | 50 | 16 | 200,000 | **66,432 req/s** | 11.695 ms | 10.342 ms | 24.964 ms | 33.802 ms | 0 |

> **Note:** Benchmarks were generated using a multithreaded Python client (bound by the Python GIL), meaning the C++20 server still has substantial throughput headroom under native C/C++ load generators.

---

## Roadmap (v3.0)
- [ ] **RESP Protocol Support:** Full compatibility with `redis-cli` and `redis-benchmark`
- [ ] **AOF Persistence:** Append-Only File logging for durability and crash recovery
- [ ] **Sharded Locking:** Partitioning the store into shards to reduce `std::shared_mutex` cache-line contention under heavy read/write concurrency
- [ ] **CLI Configuration:** Runtime flags (`--port`, `--threads`, `--cleanup-interval`)

---

## Author

**Mahmoud Mohamed Rashed**  
[![LinkedIn](https://img.shields.io/badge/LinkedIn-Connect-0A66C2?style=flat&logo=linkedin)](https://www.linkedin.com/in/mahmoudmohamedrashed2622002assuit)
[![GitHub](https://img.shields.io/badge/GitHub-MahmoudMohamedRashed-181717?style=flat&logo=github)](https://github.com/MahmoudMohamedRashed)
