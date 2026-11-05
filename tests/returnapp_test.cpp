#include "so/returnapp.h"
#include "io/rawsave.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#ifdef NDEBUG
#error Host tests require assertions
#endif

struct Host {
    bool exists, saved;
    std::string events, target;
    Host() : exists(true), saved(true) {}
    bool isFile(const std::string& path) {
        target = path;
        events += "validate ";
        return exists;
    }
    bool save() { events += "save "; return saved; }
    void close() { events += "close "; }
    void launch(const std::string& path) {
        assert(path == target);
        events += "launch";
    }
};

static void pathTests() {
    std::string path;
    assert(ReturnApp::normalizePath("Hdd:/Apps/EmulationStation/emulationstation.xex", path));
    assert(path == "Hdd:\\Apps\\EmulationStation\\emulationstation.xex");
    assert(ReturnApp::normalizePath("Usb0:\\Front End\\default.XEX", path));
    assert(ReturnApp::normalizePath("game:\\other.xex", path));
    const char* invalid[] = {
        "", "default.xex", "Apps/default.xex", "Hdd:default.xex", "Hdd:\\",
        "Hdd:\\Apps\\default.exe", "Hdd:\\Apps\\default.xex extra",
        "\"Hdd:\\Apps\\default.xex\"", "Hdd:\\Apps\\*default.xex",
        "Hdd:\\Apps\\..\\default.xex", "Hdd:\\Apps\\.\\default.xex",
        "Hdd:\\Apps\\\\default.xex", "Hdd:\\Apps\\default.xex\\",
        "Hdd:\\Apps \\default.xex", "Hdd:\\Apps.\\default.xex",
        "Hdd:\\Apps\\default.xex\n", "Hdd:\\Apps\\default.xex\r",
        "Hdd:\\Apps\\default.xex\t", "Hdd:\\Apps:default.xex",
        "https://example.com/default.xex", "\\\\host\\default.xex",
        "1Hdd:\\default.xex"
    };
    for (std::size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        path = "stale";
        assert(!ReturnApp::normalizePath(invalid[i], path));
        assert(path.empty());
    }
    const std::string prefix = "Hdd:\\";
    const std::string atLimit = prefix + std::string(259 - prefix.size() - 4, 'a') + ".xex";
    assert(ReturnApp::normalizePath(atLimit, path));
    assert(!ReturnApp::normalizePath(prefix + "a" + atLimit.substr(prefix.size()), path));
    assert(!ReturnApp::normalizePath(std::string("Hdd:\\a\0b.xex", 12), path));
}

static void handoffTests() {
    const std::string target = "Usb1:/Launcher/default.xex";
    Host unset;
    assert(ReturnApp::run("", unset) == ReturnApp::UNSET);
    assert(unset.events.empty());
    Host invalid;
    assert(ReturnApp::run("default.xex", invalid) == ReturnApp::INVALID_TARGET);
    assert(invalid.events.empty());
    Host missing;
    missing.exists = false;
    assert(ReturnApp::run(target, missing) == ReturnApp::INVALID_TARGET);
    assert(missing.events == "validate ");
    Host saveFailure;
    saveFailure.saved = false;
    assert(ReturnApp::run(target, saveFailure) == ReturnApp::SAVE_FAILED);
    assert(saveFailure.events == "validate save ");
    Host launchFailure;
    assert(ReturnApp::run(target, launchFailure) == ReturnApp::LAUNCH_FAILED);
    assert(launchFailure.events == "validate save close launch");
    assert(launchFailure.target == "Usb1:\\Launcher\\default.xex");
    // Do not retry a failed handoff automatically: another action is required.
    assert(launchFailure.events.find("launch") == launchFailure.events.rfind("launch"));
}

static void boundaryTests() {
    std::string normalized;
    for (unsigned mask = 0; mask < 8; ++mask) {
        std::string target = "Hdd:/Apps/default.xex";
        for (unsigned bit = 0; bit < 3; ++bit)
            if (mask & (1u << bit)) target[target.size() - 3 + bit] -= 'a' - 'A';
        assert(ReturnApp::normalizePath(target, normalized));
    }
    for (unsigned c = 0; c <= 127; ++c) {
        if (c >= 32 && c != 127) continue;
        const std::string target = "Hdd:/Apps/" + std::string(1, static_cast<char>(c)) + "default.xex";
        assert(!ReturnApp::normalizePath(target, normalized) && normalized.empty());
    }
    for (std::size_t nameLength = 0; nameLength <= 300; ++nameLength) {
        const std::string target = "Hdd:/" + std::string(nameLength, 'a') + ".xex";
        assert(ReturnApp::normalizePath(target, normalized) == (target.size() < 260));
    }
    std::string utf8 = "Hdd:/";
    for (int i = 0; i < 125; ++i) utf8 += "\xc3\xa9";
    utf8 += ".xex";
    assert(utf8.size() == 259 && ReturnApp::normalizePath(utf8, normalized));
    utf8.insert(5, "a");
    assert(utf8.size() == 260 && !ReturnApp::normalizePath(utf8, normalized));

    // Sweep every possible byte at every position, including embedded NUL and
    // non-ASCII bytes. Accepted paths must normalize idempotently; failures must
    // never leave a prior target in the output. Sanitizers check these bounds.
    const std::string base = "Usb0:/Front End/default.xex";
    for (std::size_t i = 0; i < base.size(); ++i) {
        for (unsigned c = 0; c < 256; ++c) {
            std::string target = base;
            target[i] = static_cast<char>(c);
            normalized = "stale";
            if (ReturnApp::normalizePath(target, normalized)) {
                std::string twice;
                assert(ReturnApp::normalizePath(normalized, twice) && twice == normalized);
            } else {
                assert(normalized.empty());
            }
        }
    }
}

static void rawSaveTests(const std::string& directory) {
    const std::string path = directory + "/returnapp-test.srm";
    const unsigned char bytes[] = { 0, 0xff, 1, 2, 3, 0x80 };
    assert(writeRawSave(path.c_str(), bytes, sizeof(bytes)));
    std::ifstream input(path.c_str(), std::ios::binary);
    const std::vector<unsigned char> actual((std::istreambuf_iterator<char>(input)),
                                           std::istreambuf_iterator<char>());
    assert(actual.size() == sizeof(bytes));
    for (std::size_t i = 0; i < actual.size(); ++i) assert(actual[i] == bytes[i]);
    input.close();
    assert(std::remove(path.c_str()) == 0);
    assert(!writeRawSave(directory.c_str(), bytes, sizeof(bytes)));
    assert(!writeRawSave(NULL, bytes, sizeof(bytes)));
    assert(!writeRawSave(path.c_str(), NULL, sizeof(bytes)));
    assert(!writeRawSave(path.c_str(), bytes, 0));
#ifndef _WIN32
    // Opening /dev/full succeeds; the buffered write fails at flush/close.
    assert(!writeRawSave("/dev/full", bytes, sizeof(bytes)));
#endif
}

int main(int argc, char** argv) {
    assert(argc == 2); // An existing scratch directory for the raw SRAM test.
    pathTests();
    boundaryTests();
    handoffTests();
    rawSaveTests(argv[1]);
    std::cout << "portable path/boundary sweeps, handoff and raw SRAM groups passed\n";
}
