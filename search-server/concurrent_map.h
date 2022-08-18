#pragma once
#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <cmath>


template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : val_mutex_(bucket_count), maps_(bucket_count) {}

    Access operator[](const Key& key) {
        uint64_t temp = (uint64_t)key;

        auto index = temp % (maps_.size());
        return {std::lock_guard(val_mutex_[index]), maps_[index][key]};
    }

    void erase(const Key& key) {
        uint64_t temp = (uint64_t)key;

        auto index = temp % (maps_.size());
        val_mutex_[index].lock();
        maps_[index].erase(key);
        val_mutex_[index].unlock();
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> answer;
        int i = 0;
        for (const auto& m : maps_) {
            std::lock_guard<std::mutex> g(val_mutex_[i]);
            for (const auto& [key, value] : m) {
                answer[key] = value;
            }
            ++i;
        }
        return answer;
    }

private:
    std::vector<std::mutex> val_mutex_;
    std::vector<std::map<Key, Value>> maps_;
};
