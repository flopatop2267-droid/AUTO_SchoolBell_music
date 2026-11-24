// Config.h - data model for bell schedules and playback settings.
// Everything is stored as JSON in data/config.json so it is easy to inspect and back up.
#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

// One bell: a time of day in "HH:MM" (24h, local time) that can be switched off.
struct Bell {
    std::string time;
    bool enabled = true;
};

// A schedule profile, e.g. "Regular Day", "Shortened Day", "Holiday (silence)".
struct Profile {
    std::string name;
    std::vector<Bell> bells;
};

// Weekly plan: which profile applies on each day of the week.
// Index 0 = Monday ... 6 = Sunday. Empty string means "no profile" (silence).
struct WeekPlan {
    std::vector<std::string> days = {"Regular Day", "Regular Day", "Regular Day",
                                     "Regular Day", "Regular Day", "Silence", "Silence"};
};

struct Config {
    std::string musicFolder = "music";      // folder with MP3/WAV tracks (UTF-8)
    std::string playMode = "sequential";    // "sequential" or "random"
    int durationSec = 15;                   // how long each bell plays
    int fadeSec = 3;                        // fade-out at the end of the bell
    double volume = 0.8;                    // 0.0 .. 1.0
    std::string activeProfile = "Regular Day";   // manual override, valid only on overrideDate
    std::string overrideDate;               // "YYYY-MM-DD"; empty = no override
    int nextTrackIndex = 0;                 // used by sequential mode
    std::vector<Profile> profiles;
    WeekPlan weekPlan;                      // automatic profile per weekday
};

// JSON (de)serialisation. WITH_DEFAULT means missing keys fall back to defaults.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Bell, time, enabled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Profile, name, bells)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WeekPlan, days)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Config, musicFolder, playMode, durationSec,
                                                fadeSec, volume, activeProfile, overrideDate,
                                                nextTrackIndex, profiles, weekPlan)

// Example schedules. Times are placeholders: edit them in the web UI to match your timetable.
inline std::vector<Profile> defaultProfiles() {
    auto make = [](const std::string& name, std::vector<std::string> times) {
        Profile p;
        p.name = name;
        for (auto& t : times) p.bells.push_back(Bell{t, true});
        return p;
    };
    return {
        make("Regular Day", {"08:30", "09:20", "10:20", "11:20", "12:20", "13:20", "14:20"}),
        make("Shortened Day", {"08:30", "09:10", "09:50", "10:30"}),
        make("Silence", {}),   // holiday / silence mode: no bells
    };
}

inline const Profile* findProfile(const Config& c, const std::string& name) {
    for (const auto& p : c.profiles)
        if (p.name == name) return &p;
    return nullptr;
}

// Maps std::tm::tm_wday (0 = Sunday) to our index (0 = Monday ... 6 = Sunday).
inline int weekdayIndex(int tmWday) {
    return (tmWday + 6) % 7;
}

// Name of the profile that applies right now.
// A manual override wins, but only on the day it was set (today == overrideDate).
// Otherwise the weekly plan entry for this weekday is used.
inline std::string effectiveProfile(const Config& c, const std::string& today, int tmWday) {
    if (!c.overrideDate.empty() && c.overrideDate == today)
        return c.activeProfile;
    int idx = weekdayIndex(tmWday);
    if (idx < 0 || idx >= static_cast<int>(c.weekPlan.days.size())) return "";
    return c.weekPlan.days[idx];
}

// Loads the config; falls back to defaults if the file is missing or broken.
inline Config loadConfig(const std::string& path) {
    Config c;
    std::ifstream in(path);
    if (in) {
        try {
            c = nlohmann::json::parse(in).get<Config>();
        } catch (...) {
            // Corrupt file: keep defaults (the original file is overwritten on next save)
        }
    }
    if (c.profiles.empty()) c.profiles = defaultProfiles();
    return c;
}

inline void saveConfig(const std::string& path, const Config& c) {
    std::ofstream out(path, std::ios::trunc);
    out << nlohmann::json(c).dump(2);
}
