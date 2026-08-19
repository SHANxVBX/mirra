#include <gtest/gtest.h>
#include "adb/AdbManager.h"
#include <string>

using namespace mirra;

TEST(AdbParser, ParsesReadyDevice) {
    std::string raw =
        "List of devices attached\n"
        "R3CN901ABCD\tdevice product:cheetah model:Pixel_7_Pro\n";

    auto devs = AdbManager::parseDeviceList(raw);
    ASSERT_EQ(devs.size(), 1u);
    EXPECT_EQ(devs[0].serial, "R3CN901ABCD");
    EXPECT_EQ(devs[0].state, AdbDeviceState::Ready);
    EXPECT_EQ(devs[0].model, "Pixel_7_Pro");
}

TEST(AdbParser, ParsesUnauthorizedDevice) {
    std::string raw =
        "List of devices attached\n"
        "emulator-5554\tunauthorized\n";

    auto devs = AdbManager::parseDeviceList(raw);
    ASSERT_EQ(devs.size(), 1u);
    EXPECT_EQ(devs[0].state, AdbDeviceState::Unauthorized);
}

TEST(AdbParser, ParsesOfflineDevice) {
    std::string raw =
        "List of devices attached\n"
        "ZY224XXXXX\toffline\n";

    auto devs = AdbManager::parseDeviceList(raw);
    ASSERT_EQ(devs.size(), 1u);
    EXPECT_EQ(devs[0].state, AdbDeviceState::Offline);
}

TEST(AdbParser, ParsesMultipleDevices) {
    std::string raw =
        "List of devices attached\n"
        "ABC123\tdevice model:Pixel_6\n"
        "DEF456\tunauthorized\n"
        "GHI789\toffline\n";

    auto devs = AdbManager::parseDeviceList(raw);
    ASSERT_EQ(devs.size(), 3u);
    EXPECT_EQ(devs[0].state, AdbDeviceState::Ready);
    EXPECT_EQ(devs[1].state, AdbDeviceState::Unauthorized);
    EXPECT_EQ(devs[2].state, AdbDeviceState::Offline);
}

TEST(AdbParser, ReturnsEmptyListWhenNoDevices) {
    std::string raw = "List of devices attached\n";
    auto devs = AdbManager::parseDeviceList(raw);
    EXPECT_TRUE(devs.empty());
}
