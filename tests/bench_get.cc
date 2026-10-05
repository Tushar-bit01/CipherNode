#include "../include/engine.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// Frozen Benchmark Configuration
// ============================================================

constexpr int NUM_RUNS = 5;

constexpr int SMALL_DATASET = 100'000;
constexpr int LARGE_DATASET = 1'000'000;

constexpr size_t KEY_SIZE = 16;
constexpr size_t VALUE_SIZE = 100;

constexpr uint32_t RANDOM_SEED = 42;

// ============================================================
// Result Structure
// ============================================================

struct BenchmarkResult
{
    std::string workload;
    int operations;

    double total_time_ms;
    double throughput;

    double p50_us;
    double p95_us;
    double p99_us;

    uintmax_t db_size;
    uintmax_t wal_size;
    uintmax_t sstable_size;
};

// ============================================================
// Database Cleanup
// ============================================================

void cleanDatabase()
{
    std::error_code ec;

    fs::remove("tusu.db", ec);

    for (const auto &entry : fs::directory_iterator(".", ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file())
            continue;

        std::string filename = entry.path().filename().string();

        if (filename.rfind("sstable_", 0) == 0 &&
            entry.path().extension() == ".db")
        {
            fs::remove(entry.path(), ec);
        }
    }
}

// ============================================================
// Key / Value Generation
// ============================================================

std::string makeKey(int number)
{
    std::string key = "key" + std::to_string(number);

    if (key.size() < KEY_SIZE)
        key.append(KEY_SIZE - key.size(), '_');
    else if (key.size() > KEY_SIZE)
        key.resize(KEY_SIZE);

    return key;
}

std::string makeValue(int number)
{
    std::string value = "value" + std::to_string(number);

    if (value.size() < VALUE_SIZE)
        value.append(VALUE_SIZE - value.size(), 'x');
    else if (value.size() > VALUE_SIZE)
        value.resize(VALUE_SIZE);

    return value;
}

// ============================================================
// Storage Size Measurement
// ============================================================

uintmax_t getFileSize(const std::string &filename)
{
    std::error_code ec;

    if (!fs::exists(filename, ec))
        return 0;

    return fs::file_size(filename, ec);
}

uintmax_t getSSTableSize()
{
    uintmax_t total = 0;

    std::error_code ec;

    for (const auto &entry : fs::directory_iterator(".", ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file())
            continue;

        std::string filename = entry.path().filename().string();

        if (filename.rfind("sstable_", 0) == 0 &&
            entry.path().extension() == ".db")
        {
            total += entry.file_size(ec);

            if (ec)
                ec.clear();
        }
    }

    return total;
}

// ============================================================
// Percentile Calculation
// ============================================================

double percentile(std::vector<double> values, double percentile_value)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());

    double position =
        (percentile_value / 100.0) * (values.size() - 1);

    size_t lower = static_cast<size_t>(position);
    size_t upper = lower + 1;

    if (upper >= values.size())
        return values[lower];

    double fraction = position - lower;

    return values[lower] +
           fraction * (values[upper] - values[lower]);
}

// ============================================================
// Result Construction
// ============================================================

BenchmarkResult buildResult(
    const std::string &workload,
    int operations,
    double total_time_ms,
    const std::vector<double> &latencies)
{
    BenchmarkResult result;

    result.workload = workload;
    result.operations = operations;

    result.total_time_ms = total_time_ms;

    double total_seconds = total_time_ms / 1000.0;

    result.throughput =
        static_cast<double>(operations) / total_seconds;

    result.p50_us = percentile(latencies, 50.0);
    result.p95_us = percentile(latencies, 95.0);
    result.p99_us = percentile(latencies, 99.0);

    result.db_size =
        getFileSize("tusu.db") + getSSTableSize();

    result.wal_size =
        getFileSize("tusu.db");

    result.sstable_size =
        getSSTableSize();

    return result;
}

// ============================================================
// Existing-Key GET
// ============================================================

BenchmarkResult benchmarkExistingGet(int operations)
{
    cleanDatabase();

    TusuEngine db("tusu.db");

    // Prepare database.
    for (int i = 0; i < operations; ++i)
    {
        db.put(makeKey(i), makeValue(i));
    }

    std::vector<int> order(operations);

    std::iota(order.begin(), order.end(), 0);

    std::mt19937 gen(RANDOM_SEED);

    std::shuffle(order.begin(), order.end(), gen);

    std::vector<double> latencies;
    latencies.reserve(operations);

    auto total_start = std::chrono::steady_clock::now();

    for (int number : order)
    {
        std::string key = makeKey(number);

        auto start = std::chrono::steady_clock::now();

        std::string value = db.get(key);

        auto end = std::chrono::steady_clock::now();

        double latency =
            std::chrono::duration<double, std::micro>(
                end - start)
                .count();

        latencies.push_back(latency);

        if (value.empty())
        {
            std::cerr << "[ERROR] Existing key returned empty value\n";
        }
    }

    auto total_end = std::chrono::steady_clock::now();

    double total_time =
        std::chrono::duration<double, std::milli>(
            total_end - total_start)
            .count();

    return buildResult(
        "Existing-key GET",
        operations,
        total_time,
        latencies);
}

// ============================================================
// Printing
// ============================================================

void printResult(const BenchmarkResult &result)
{
    std::cout
        << std::left
        << std::setw(22)
        << result.workload
        << std::setw(12)
        << result.operations
        << std::fixed
        << std::setprecision(2)
        << std::setw(15)
        << result.total_time_ms
        << std::setw(18)
        << result.throughput
        << std::setw(12)
        << result.p50_us
        << std::setw(12)
        << result.p95_us
        << std::setw(12)
        << result.p99_us
        << '\n';
}

// ============================================================
// Main
// ============================================================

int main()
{
    constexpr int operations = LARGE_DATASET; // 1,000,000

    std::cout << "[Existing GET] Single Run\n";

    BenchmarkResult result =
        benchmarkExistingGet(operations);

    std::cout
        << "\n========== Existing GET ==========\n";

    printResult(result);

    cleanDatabase();

    return 0;
}