// Tests for HDD Control utilities

#include "catch.hpp"
#include "hdd-utils.h"

using namespace hdd;

TEST_CASE("TrimWhitespace std::string version", "[trim]") {
    CHECK(TrimWhitespace(std::string("  hello  ")) == "hello");
    CHECK(TrimWhitespace(std::string("hello")) == "hello");
    CHECK(TrimWhitespace(std::string("  hello")) == "hello");
    CHECK(TrimWhitespace(std::string("hello  ")) == "hello");
    CHECK(TrimWhitespace(std::string("\t\thello\t\t")) == "hello");
    CHECK(TrimWhitespace(std::string("hello world")) == "hello world");
    CHECK(TrimWhitespace(std::string("  hello world  ")) == "hello world");
    CHECK(TrimWhitespace(std::string("\r\nhello\r\n")) == "hello");
    CHECK(TrimWhitespace(std::string("")) == "");
    CHECK(TrimWhitespace(std::string("   ")) == "");
}

TEST_CASE("TrimWhitespace C string version", "[trim]") {
    char buf1[] = "  hello  ";
    CHECK(std::string(TrimWhitespace(buf1)) == "hello");

    char buf2[] = "hello";
    CHECK(std::string(TrimWhitespace(buf2)) == "hello");

    char buf3[] = "\t\thello\t\t";
    CHECK(std::string(TrimWhitespace(buf3)) == "hello");

    char buf4[] = "";
    CHECK(std::string(TrimWhitespace(buf4)) == "");

    CHECK(TrimWhitespace(nullptr) == nullptr);
}

TEST_CASE("DriveState conversion functions", "[drivestate]") {
    SECTION("DriveStateToString returns correct strings") {
        CHECK(std::string(DriveStateToString(DriveState::Online)) == "Drive Online");
        CHECK(std::string(DriveStateToString(DriveState::Offline)) == "Drive Offline");
        CHECK(std::string(DriveStateToString(DriveState::Transitioning)) == "Transitioning...");
        CHECK(std::string(DriveStateToString(DriveState::Unknown)) == "Unknown");
    }

    SECTION("DriveStateToStatusString returns correct status strings") {
        CHECK(std::string(DriveStateToStatusString(DriveState::Online)) == "Status: Drive Online");
        CHECK(std::string(DriveStateToStatusString(DriveState::Offline)) == "Status: Drive Offline");
        CHECK(std::string(DriveStateToStatusString(DriveState::Transitioning)) == "Status: Transitioning...");
        CHECK(std::string(DriveStateToStatusString(DriveState::Unknown)) == "Status: Unknown");
    }
}

TEST_CASE("Config has sensible defaults", "[config]") {
    Config config;

    CHECK(config.targetSerial == "2VH7TM9L");
    CHECK(config.wakeCommand == "wake-hdd.exe");
    CHECK(config.sleepCommand == "sleep-hdd.exe");
    CHECK(config.periodicCheckMinutes == 10);
    CHECK(config.postOperationCheckSeconds == 3);
    CHECK(config.showNotifications == true);
    CHECK(config.debugMode == false);
}

TEST_CASE("Config can be modified", "[config]") {
    Config config;

    config.targetSerial = "NEWSERIAL";
    config.periodicCheckMinutes = 30;
    config.debugMode = true;

    CHECK(config.targetSerial == "NEWSERIAL");
    CHECK(config.periodicCheckMinutes == 30);
    CHECK(config.debugMode == true);
    // Other values should remain at defaults
    CHECK(config.wakeCommand == "wake-hdd.exe");
}
