// Comprehensive tests for HDD Control utilities

#include "catch.hpp"
#include "hdd-utils.h"

using namespace hdd;

//=============================================================================
// String Utilities Tests
//=============================================================================

TEST_CASE("TrimWhitespace std::string version", "[string][trim]") {
    SECTION("Trims leading and trailing spaces") {
        CHECK(TrimWhitespace(std::string("  hello  ")) == "hello");
        CHECK(TrimWhitespace(std::string("   hello")) == "hello");
        CHECK(TrimWhitespace(std::string("hello   ")) == "hello");
    }

    SECTION("Handles no whitespace") {
        CHECK(TrimWhitespace(std::string("hello")) == "hello");
        CHECK(TrimWhitespace(std::string("hello world")) == "hello world");
    }

    SECTION("Handles tabs") {
        CHECK(TrimWhitespace(std::string("\t\thello\t\t")) == "hello");
        CHECK(TrimWhitespace(std::string("\thello world\t")) == "hello world");
    }

    SECTION("Handles newlines and carriage returns") {
        CHECK(TrimWhitespace(std::string("\r\nhello\r\n")) == "hello");
        CHECK(TrimWhitespace(std::string("\nhello\n")) == "hello");
        CHECK(TrimWhitespace(std::string("hello\r\n")) == "hello");
    }

    SECTION("Handles mixed whitespace") {
        CHECK(TrimWhitespace(std::string(" \t\r\nhello \t\r\n")) == "hello");
    }

    SECTION("Handles empty and whitespace-only strings") {
        CHECK(TrimWhitespace(std::string("")) == "");
        CHECK(TrimWhitespace(std::string("   ")) == "");
        CHECK(TrimWhitespace(std::string("\t\t\t")) == "");
        CHECK(TrimWhitespace(std::string("\r\n")) == "");
    }

    SECTION("Preserves internal whitespace") {
        CHECK(TrimWhitespace(std::string("  hello world  ")) == "hello world");
        CHECK(TrimWhitespace(std::string("  a  b  c  ")) == "a  b  c");
    }
}

TEST_CASE("TrimWhitespace C string version", "[string][trim]") {
    SECTION("Trims basic whitespace") {
        char buf1[] = "  hello  ";
        CHECK(std::string(TrimWhitespace(buf1)) == "hello");

        char buf2[] = "hello";
        CHECK(std::string(TrimWhitespace(buf2)) == "hello");
    }

    SECTION("Handles tabs") {
        char buf[] = "\t\thello\t\t";
        CHECK(std::string(TrimWhitespace(buf)) == "hello");
    }

    SECTION("Handles empty string") {
        char buf[] = "";
        CHECK(std::string(TrimWhitespace(buf)) == "");
    }

    SECTION("Handles null pointer") {
        CHECK(TrimWhitespace(nullptr) == nullptr);
    }

    SECTION("Handles single character") {
        char buf1[] = "a";
        CHECK(std::string(TrimWhitespace(buf1)) == "a");

        char buf2[] = " a ";
        CHECK(std::string(TrimWhitespace(buf2)) == "a");
    }

    SECTION("Handles single whitespace character only") {
        // This tests line 35 - the single whitespace case
        char buf1[] = " ";
        CHECK(std::string(TrimWhitespace(buf1)) == "");

        char buf2[] = "\t";
        CHECK(std::string(TrimWhitespace(buf2)) == "");

        char buf3[] = "\n";
        CHECK(std::string(TrimWhitespace(buf3)) == "");
    }
}

TEST_CASE("EqualsIgnoreCase", "[string][compare]") {
    SECTION("Equal strings") {
        CHECK(EqualsIgnoreCase("hello", "hello"));
        CHECK(EqualsIgnoreCase("HELLO", "hello"));
        CHECK(EqualsIgnoreCase("Hello", "hELLO"));
        CHECK(EqualsIgnoreCase("HeLLo WoRLD", "hello world"));
    }

    SECTION("Unequal strings") {
        CHECK_FALSE(EqualsIgnoreCase("hello", "world"));
        CHECK_FALSE(EqualsIgnoreCase("hello", "hello!"));
        CHECK_FALSE(EqualsIgnoreCase("hello", "hell"));
    }

    SECTION("Empty strings") {
        CHECK(EqualsIgnoreCase("", ""));
        CHECK_FALSE(EqualsIgnoreCase("", "a"));
        CHECK_FALSE(EqualsIgnoreCase("a", ""));
    }

    SECTION("Numbers and special characters") {
        CHECK(EqualsIgnoreCase("abc123", "ABC123"));
        CHECK(EqualsIgnoreCase("test@123", "TEST@123"));
        CHECK_FALSE(EqualsIgnoreCase("abc123", "abc124"));
    }
}

TEST_CASE("StartsWith", "[string]") {
    CHECK(StartsWith("hello world", "hello"));
    CHECK(StartsWith("hello", "hello"));
    CHECK(StartsWith("hello", ""));
    CHECK_FALSE(StartsWith("hello", "Hello")); // Case sensitive
    CHECK_FALSE(StartsWith("hello", "world"));
    CHECK_FALSE(StartsWith("hi", "hello"));
    CHECK(StartsWith("", ""));
    CHECK_FALSE(StartsWith("", "a"));
}

TEST_CASE("EndsWith", "[string]") {
    CHECK(EndsWith("hello world", "world"));
    CHECK(EndsWith("hello", "hello"));
    CHECK(EndsWith("hello", ""));
    CHECK_FALSE(EndsWith("hello", "Hello")); // Case sensitive
    CHECK_FALSE(EndsWith("hello", "ello!"));
    CHECK_FALSE(EndsWith("hi", "hello"));
    CHECK(EndsWith("", ""));
    CHECK_FALSE(EndsWith("", "a"));
}

TEST_CASE("ToLower", "[string]") {
    CHECK(ToLower("HELLO") == "hello");
    CHECK(ToLower("Hello World") == "hello world");
    CHECK(ToLower("already lowercase") == "already lowercase");
    CHECK(ToLower("MixED CaSe 123") == "mixed case 123");
    CHECK(ToLower("") == "");
}

TEST_CASE("ToUpper", "[string]") {
    CHECK(ToUpper("hello") == "HELLO");
    CHECK(ToUpper("Hello World") == "HELLO WORLD");
    CHECK(ToUpper("ALREADY UPPERCASE") == "ALREADY UPPERCASE");
    CHECK(ToUpper("MixED CaSe 123") == "MIXED CASE 123");
    CHECK(ToUpper("") == "");
}

//=============================================================================
// Drive State Tests
//=============================================================================

TEST_CASE("DriveStateToString", "[drivestate]") {
    CHECK(std::string(DriveStateToString(DriveState::Online)) == "Drive Online");
    CHECK(std::string(DriveStateToString(DriveState::Offline)) == "Drive Offline");
    CHECK(std::string(DriveStateToString(DriveState::Transitioning)) == "Transitioning...");
    CHECK(std::string(DriveStateToString(DriveState::Unknown)) == "Unknown");
}

TEST_CASE("DriveStateToStatusString", "[drivestate]") {
    CHECK(std::string(DriveStateToStatusString(DriveState::Online)) == "Status: Drive Online");
    CHECK(std::string(DriveStateToStatusString(DriveState::Offline)) == "Status: Drive Offline");
    CHECK(std::string(DriveStateToStatusString(DriveState::Transitioning)) == "Status: Transitioning...");
    CHECK(std::string(DriveStateToStatusString(DriveState::Unknown)) == "Status: Unknown");
}

TEST_CASE("GetTooltipText", "[drivestate]") {
    CHECK(GetTooltipText(DriveState::Online) == "HDD Status: Drive Online");
    CHECK(GetTooltipText(DriveState::Offline) == "HDD Status: Drive Offline");
    CHECK(GetTooltipText(DriveState::Transitioning) == "HDD Status: Drive Transitioning...");
    CHECK(GetTooltipText(DriveState::Unknown) == "HDD Status: Drive Unknown");
}

TEST_CASE("CanWake", "[drivestate][actions]") {
    CHECK_FALSE(CanWake(DriveState::Online));
    CHECK(CanWake(DriveState::Offline));
    CHECK_FALSE(CanWake(DriveState::Transitioning));
    CHECK(CanWake(DriveState::Unknown));
}

TEST_CASE("CanSleep", "[drivestate][actions]") {
    CHECK(CanSleep(DriveState::Online));
    CHECK_FALSE(CanSleep(DriveState::Offline));
    CHECK_FALSE(CanSleep(DriveState::Transitioning));
    CHECK_FALSE(CanSleep(DriveState::Unknown));
}

TEST_CASE("IsTransitioning", "[drivestate]") {
    CHECK_FALSE(IsTransitioning(DriveState::Online));
    CHECK_FALSE(IsTransitioning(DriveState::Offline));
    CHECK(IsTransitioning(DriveState::Transitioning));
    CHECK_FALSE(IsTransitioning(DriveState::Unknown));
}

TEST_CASE("GetPrimaryActionText", "[drivestate][actions]") {
    CHECK(std::string(GetPrimaryActionText(DriveState::Online)) == "Sleep Drive");
    CHECK(std::string(GetPrimaryActionText(DriveState::Offline)) == "Wake Drive");
    CHECK(std::string(GetPrimaryActionText(DriveState::Transitioning)) == "Wake Drive");
    CHECK(std::string(GetPrimaryActionText(DriveState::Unknown)) == "Wake Drive");
}

//=============================================================================
// Animation Tests
//=============================================================================

TEST_CASE("GetAnimationDots", "[animation]") {
    CHECK(std::string(GetAnimationDots(0)) == "");
    CHECK(std::string(GetAnimationDots(1)) == ".");
    CHECK(std::string(GetAnimationDots(2)) == "..");
    CHECK(std::string(GetAnimationDots(3)) == "...");

    SECTION("Wraps correctly") {
        CHECK(std::string(GetAnimationDots(4)) == "");
        CHECK(std::string(GetAnimationDots(5)) == ".");
        CHECK(std::string(GetAnimationDots(8)) == "");
    }

    SECTION("Handles negative values") {
        // Negative mod can be negative in C++, but we handle it
        CHECK(std::string(GetAnimationDots(-1)) == "");  // Falls through to 0 due to < 0 check
    }
}

TEST_CASE("GetAnimatedTooltip", "[animation]") {
    CHECK(GetAnimatedTooltip(0) == "HDD Control - Working");
    CHECK(GetAnimatedTooltip(1) == "HDD Control - Working.");
    CHECK(GetAnimatedTooltip(2) == "HDD Control - Working..");
    CHECK(GetAnimatedTooltip(3) == "HDD Control - Working...");
    CHECK(GetAnimatedTooltip(4) == "HDD Control - Working");
}

TEST_CASE("NextAnimationFrame", "[animation]") {
    CHECK(NextAnimationFrame(0) == 1);
    CHECK(NextAnimationFrame(1) == 2);
    CHECK(NextAnimationFrame(2) == 3);
    CHECK(NextAnimationFrame(3) == 0);  // Wraps
    CHECK(NextAnimationFrame(4) == 1);  // Already wrapped, continues
}

//=============================================================================
// Timing Utilities Tests
//=============================================================================

TEST_CASE("HasDebounceElapsed", "[timing]") {
    SECTION("Normal cases") {
        CHECK_FALSE(HasDebounceElapsed(0, 100, 200));   // Not enough time
        CHECK(HasDebounceElapsed(0, 200, 200));          // Exactly enough
        CHECK(HasDebounceElapsed(0, 300, 200));          // More than enough
    }

    SECTION("Same time") {
        CHECK_FALSE(HasDebounceElapsed(100, 100, 200));
    }

    SECTION("Time wrap-around") {
        // If current < last (hypothetical wrap), should return true
        CHECK(HasDebounceElapsed(1000, 100, 200));
    }

    SECTION("Zero debounce") {
        CHECK(HasDebounceElapsed(100, 100, 0));
        CHECK(HasDebounceElapsed(100, 101, 0));
    }
}

TEST_CASE("MinutesToMs", "[timing]") {
    CHECK(MinutesToMs(0) == 0);
    CHECK(MinutesToMs(1) == 60000);
    CHECK(MinutesToMs(10) == 600000);
    CHECK(MinutesToMs(60) == 3600000);
}

TEST_CASE("SecondsToMs", "[timing]") {
    CHECK(SecondsToMs(0) == 0);
    CHECK(SecondsToMs(1) == 1000);
    CHECK(SecondsToMs(10) == 10000);
    CHECK(SecondsToMs(60) == 60000);
}

TEST_CASE("ShouldShowMenu", "[timing][menu]") {
    CHECK_FALSE(ShouldShowMenu(0, 100));     // 100ms < 200ms debounce
    CHECK_FALSE(ShouldShowMenu(0, 199));     // 199ms < 200ms debounce
    CHECK(ShouldShowMenu(0, 200));           // Exactly 200ms
    CHECK(ShouldShowMenu(0, 300));           // More than 200ms
    CHECK(ShouldShowMenu(0, 1000));          // Much more
}

TEST_CASE("ShouldPeriodicCheck", "[timing]") {
    SECTION("Blocks during transition") {
        CHECK_FALSE(ShouldPeriodicCheck(0, 100000, true));  // isTransitioning = true
    }

    SECTION("Requires 1 minute minimum") {
        CHECK_FALSE(ShouldPeriodicCheck(0, 30000, false));   // 30 sec < 1 min
        CHECK_FALSE(ShouldPeriodicCheck(0, 59999, false));   // Just under 1 min
        CHECK(ShouldPeriodicCheck(0, 60000, false));         // Exactly 1 min
        CHECK(ShouldPeriodicCheck(0, 120000, false));        // 2 min
    }
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_CASE("Config has sensible defaults", "[config]") {
    Config config;

    CHECK(config.targetSerial == "2VH7TM9L");
    CHECK(config.targetModel == "WDC WD181KFGX-68AFPN0");
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

TEST_CASE("ValidatePeriodicCheckMinutes", "[config][validation]") {
    CHECK(ValidatePeriodicCheckMinutes(0) == 1);   // Below minimum
    CHECK(ValidatePeriodicCheckMinutes(1) == 1);   // At minimum
    CHECK(ValidatePeriodicCheckMinutes(10) == 10); // Normal
    CHECK(ValidatePeriodicCheckMinutes(100) == 100); // Large value
}

TEST_CASE("ValidatePostOperationSeconds", "[config][validation]") {
    CHECK(ValidatePostOperationSeconds(0) == 1);   // Below minimum
    CHECK(ValidatePostOperationSeconds(1) == 1);   // At minimum
    CHECK(ValidatePostOperationSeconds(5) == 5);   // Normal
    CHECK(ValidatePostOperationSeconds(60) == 60); // Large value
}

TEST_CASE("SerialMatches", "[config]") {
    SECTION("Exact match") {
        CHECK(SerialMatches("2VH7TM9L", "2VH7TM9L"));
    }

    SECTION("Case insensitive") {
        CHECK(SerialMatches("2vh7tm9l", "2VH7TM9L"));
        CHECK(SerialMatches("2VH7TM9L", "2vh7tm9l"));
    }

    SECTION("Whitespace handling") {
        CHECK(SerialMatches("  2VH7TM9L  ", "2VH7TM9L"));
        CHECK(SerialMatches("2VH7TM9L", "  2VH7TM9L  "));
        CHECK(SerialMatches("  2VH7TM9L  ", "  2VH7TM9L  "));
    }

    SECTION("Non-matches") {
        CHECK_FALSE(SerialMatches("2VH7TM9L", "2VH7TM9X"));
        CHECK_FALSE(SerialMatches("SERIAL1", "SERIAL2"));
        CHECK_FALSE(SerialMatches("", "2VH7TM9L"));
    }
}

//=============================================================================
// Path Utilities Tests
//=============================================================================

TEST_CASE("GetExtension", "[path]") {
    SECTION("Normal files") {
        CHECK(GetExtension("file.txt") == "txt");
        CHECK(GetExtension("file.EXE") == "exe");  // Lowercase output
        CHECK(GetExtension("file.TxT") == "txt");
        CHECK(GetExtension("document.pdf") == "pdf");
    }

    SECTION("Multiple dots") {
        CHECK(GetExtension("file.backup.txt") == "txt");
        CHECK(GetExtension("archive.tar.gz") == "gz");
    }

    SECTION("No extension") {
        CHECK(GetExtension("file") == "");
        CHECK(GetExtension("Makefile") == "");
    }

    SECTION("Dot in directory name") {
        CHECK(GetExtension("dir.name/file") == "");
        CHECK(GetExtension("dir.name\\file") == "");
        CHECK(GetExtension("dir.name/file.txt") == "txt");
    }

    SECTION("Mixed path separators (both / and \\)") {
        // This tests line 272 - when both separators exist
        CHECK(GetExtension("C:\\path/to\\file.exe") == "exe");
        CHECK(GetExtension("dir/sub\\file.txt") == "txt");
        CHECK(GetExtension("a/b\\c/d\\file.doc") == "doc");
    }

    SECTION("Paths") {
        CHECK(GetExtension("C:\\path\\to\\file.exe") == "exe");
        CHECK(GetExtension("/path/to/file.sh") == "sh");
        CHECK(GetExtension("relative/path/file.bat") == "bat");
    }

    SECTION("Edge cases") {
        CHECK(GetExtension("") == "");
        CHECK(GetExtension(".") == "");
        CHECK(GetExtension("..") == "");
        CHECK(GetExtension(".hidden") == "hidden");
    }
}

TEST_CASE("GetFilename", "[path]") {
    SECTION("Windows paths") {
        CHECK(GetFilename("C:\\path\\to\\file.exe") == "file.exe");
        CHECK(GetFilename("C:\\file.txt") == "file.txt");
        CHECK(GetFilename("\\\\server\\share\\file.doc") == "file.doc");
    }

    SECTION("Unix paths") {
        CHECK(GetFilename("/path/to/file.sh") == "file.sh");
        CHECK(GetFilename("/file.txt") == "file.txt");
    }

    SECTION("Mixed separators") {
        CHECK(GetFilename("C:/path/to/file.exe") == "file.exe");
        CHECK(GetFilename("path\\to/file.txt") == "file.txt");
    }

    SECTION("No path") {
        CHECK(GetFilename("file.txt") == "file.txt");
        CHECK(GetFilename("file") == "file");
    }

    SECTION("Edge cases") {
        CHECK(GetFilename("") == "");
        CHECK(GetFilename("\\") == "");
        CHECK(GetFilename("/") == "");
    }
}

TEST_CASE("IsExecutable", "[path]") {
    SECTION("Executable extensions") {
        CHECK(IsExecutable("program.exe"));
        CHECK(IsExecutable("script.bat"));
        CHECK(IsExecutable("script.cmd"));
        CHECK(IsExecutable("script.ps1"));
    }

    SECTION("Case insensitive") {
        CHECK(IsExecutable("program.EXE"));
        CHECK(IsExecutable("script.BAT"));
        CHECK(IsExecutable("script.PS1"));
    }

    SECTION("Non-executable") {
        CHECK_FALSE(IsExecutable("document.txt"));
        CHECK_FALSE(IsExecutable("image.png"));
        CHECK_FALSE(IsExecutable("file"));
        CHECK_FALSE(IsExecutable(""));
    }

    SECTION("Full paths") {
        CHECK(IsExecutable("C:\\bin\\program.exe"));
        CHECK(IsExecutable("/usr/bin/script.ps1"));
    }
}

TEST_CASE("JoinPath", "[path]") {
    SECTION("Normal joining") {
        CHECK(JoinPath("C:\\path", "file.txt") == "C:\\path\\file.txt");
        CHECK(JoinPath("C:\\path\\", "file.txt") == "C:\\path\\file.txt");
        CHECK(JoinPath("path", "file.txt") == "path\\file.txt");
    }

    SECTION("Forward slash handling") {
        CHECK(JoinPath("C:/path/", "file.txt") == "C:/path/file.txt");
    }

    SECTION("Empty components") {
        CHECK(JoinPath("", "file.txt") == "file.txt");
        CHECK(JoinPath("path", "") == "path");
        CHECK(JoinPath("", "") == "");
    }

    SECTION("Multiple levels") {
        std::string path = JoinPath("C:\\base", "sub");
        path = JoinPath(path, "file.txt");
        CHECK(path == "C:\\base\\sub\\file.txt");
    }
}

//=============================================================================
// Notification Message Tests
//=============================================================================

TEST_CASE("GetCompletionMessage", "[notification]") {
    SECTION("Wake operations") {
        CHECK(std::string(GetCompletionMessage(true, true)) == "Drive wake completed");
        CHECK(std::string(GetCompletionMessage(true, false)) == "Drive wake failed");
    }

    SECTION("Sleep operations") {
        CHECK(std::string(GetCompletionMessage(false, true)) == "Drive shutdown completed");
        CHECK(std::string(GetCompletionMessage(false, false)) == "Drive shutdown failed");
    }
}

TEST_CASE("GetStartMessage", "[notification]") {
    CHECK(std::string(GetStartMessage(true)) == "Waking drive...");
    CHECK(std::string(GetStartMessage(false)) == "Sleeping drive...");
}
