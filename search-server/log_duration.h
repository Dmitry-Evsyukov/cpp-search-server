#pragma once

#include <chrono>
#include <iostream>
#include <string_view>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)

/**
 * РњР°РєСЂРѕСЃ Р·Р°РјРµСЂСЏРµС‚ РІСЂРµРјСЏ, РїСЂРѕС€РµРґС€РµРµ СЃ РјРѕРјРµРЅС‚Р° СЃРІРѕРµРіРѕ РІС‹Р·РѕРІР°
 * РґРѕ РєРѕРЅС†Р° С‚РµРєСѓС‰РµРіРѕ Р±Р»РѕРєР°, Рё РІС‹РІРѕРґРёС‚ РІ РїРѕС‚РѕРє std::cerr.
 *
 * РџСЂРёРјРµСЂ РёСЃРїРѕР»СЊР·РѕРІР°РЅРёСЏ:
 *
 *  void Task1() {
 *      LOG_DURATION("Task 1"s); // Р’С‹РІРµРґРµС‚ РІ cerr РІСЂРµРјСЏ СЂР°Р±РѕС‚С‹ С„СѓРЅРєС†РёРё Task1
 *      ...
 *  }
 *
 *  void Task2() {
 *      LOG_DURATION("Task 2"s); // Р’С‹РІРµРґРµС‚ РІ cerr РІСЂРµРјСЏ СЂР°Р±РѕС‚С‹ С„СѓРЅРєС†РёРё Task2
 *      ...
 *  }
 *
 *  int main() {
 *      LOG_DURATION("main"s);  // Р’С‹РІРµРґРµС‚ РІ cerr РІСЂРµРјСЏ СЂР°Р±РѕС‚С‹ С„СѓРЅРєС†РёРё main
 *      Task1();
 *      Task2();
 *  }
 */
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

/**
 * РџРѕРІРµРґРµРЅРёРµ Р°РЅР°Р»РѕРіРёС‡РЅРѕ РјР°РєСЂРѕСЃСѓ LOG_DURATION, РїСЂРё СЌС‚РѕРј РјРѕР¶РЅРѕ СѓРєР°Р·Р°С‚СЊ РїРѕС‚РѕРє,
 * РІ РєРѕС‚РѕСЂС‹Р№ РґРѕР»Р¶РЅРѕ Р±С‹С‚СЊ РІС‹РІРµРґРµРЅРѕ РёР·РјРµСЂРµРЅРЅРѕРµ РІСЂРµРјСЏ.
 *
 * РџСЂРёРјРµСЂ РёСЃРїРѕР»СЊР·РѕРІР°РЅРёСЏ:
 *
 *  int main() {
 *      // Р’С‹РІРµРґРµС‚ РІСЂРµРјСЏ СЂР°Р±РѕС‚С‹ main РІ РїРѕС‚РѕРє std::cout
 *      LOG_DURATION("main"s, std::cout);
 *      ...
 *  }
 */
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
    // Р·Р°РјРµРЅРёРј РёРјСЏ С‚РёРїР° std::chrono::steady_clock
    // СЃ РїРѕРјРѕС‰СЊСЋ using РґР»СЏ СѓРґРѕР±СЃС‚РІР°
    using Clock = std::chrono::steady_clock;

    LogDuration(std::string_view id, std::ostream& dst_stream = std::cerr)
            : id_(id)
            , dst_stream_(dst_stream) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        dst_stream_ << id_ << ": "sv << duration_cast<milliseconds>(dur).count() << " ms"sv << std::endl;
    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& dst_stream_;
};