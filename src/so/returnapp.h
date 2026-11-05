#pragma once

#include <string>

// No platform headers: the handoff policy can be tested without an Xbox SDK.
namespace ReturnApp {
enum Result { UNSET, INVALID_TARGET, SAVE_FAILED, LAUNCH_FAILED };

inline bool normalizePath(const std::string& configured, std::string& path) {
    path.clear();
    if (configured.empty() || configured.size() >= 260) return false;
    std::string candidate = configured;
    if (candidate.find_first_of("\"<>|?*") != std::string::npos) return false;
    for (std::size_t i = 0; i < candidate.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(candidate[i]);
        if (c < 32 || c == 127) return false;
        if (candidate[i] == '/') candidate[i] = '\\';
    }
    const std::size_t colon = candidate.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 2 >= candidate.size() ||
        candidate[colon + 1] != '\\' || candidate.find(':', colon + 1) != std::string::npos)
        return false;
    // Require a mounted drive alias, never a relative path or a shell command.
    for (std::size_t i = 0; i < colon; ++i) {
        const char c = candidate[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (i > 0 && c >= '0' && c <= '9'))) return false;
    }
    std::size_t begin = colon + 2;
    while (begin < candidate.size()) {
        const std::size_t end = candidate.find('\\', begin);
        const std::string part = candidate.substr(begin, end - begin);
        if (part.empty() || part == "." || part == ".." ||
            part[part.size() - 1] == '.' || part[part.size() - 1] == ' ') return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    const std::size_t size = candidate.size();
    if (size < 4 || candidate[size - 4] != '.' ||
        (candidate[size - 3] != 'x' && candidate[size - 3] != 'X') ||
        (candidate[size - 2] != 'e' && candidate[size - 2] != 'E') ||
        (candidate[size - 1] != 'x' && candidate[size - 1] != 'X')) return false;
    path = candidate;
    return true;
}

// Host methods run on the frontend thread, outside retro_run and menu callbacks.
// A successful Xbox launch replaces the process; any returned launch is failure.
template<class Host>
Result run(const std::string& configured, Host& host) {
    if (configured.empty()) return UNSET;
    std::string path;
    if (!normalizePath(configured, path) || !host.isFile(path)) return INVALID_TARGET;
    if (!host.save()) return SAVE_FAILED;
    host.close();
    host.launch(path);
    return LAUNCH_FAILED;
}
}
