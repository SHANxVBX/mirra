#include <gtest/gtest.h>
#include "ipc/MessageFramer.h"

using namespace mirra;

TEST(MessageFramer, EncodesLengthPrefixedFrame) {
    std::string json = R"({"type":"StateChanged"})";
    auto frame = MessageFramer::encode(json);

    // First 4 bytes = length (little-endian)
    uint32_t len = frame[0] | (frame[1] << 8) | (frame[2] << 16) | (frame[3] << 24);
    EXPECT_EQ(len, json.size());
    EXPECT_EQ(frame.size(), 4 + json.size());

    // Payload matches
    std::string decoded(reinterpret_cast<char*>(frame.data() + 4), len);
    EXPECT_EQ(decoded, json);
}

TEST(MessageFramer, ExtractsCompleteMessage) {
    std::string json = R"({"type":"HealthTick","payload":{"fps":60}})";
    auto frame = MessageFramer::encode(json);

    MessageFramer framer;
    framer.feed(std::span<const uint8_t>{frame.data(), frame.size()});

    auto msg = framer.extractMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, json);

    // No more messages
    EXPECT_FALSE(framer.extractMessage().has_value());
}

TEST(MessageFramer, HandlesFragmentedInput) {
    std::string json = R"({"type":"FirstFrame"})";
    auto frame = MessageFramer::encode(json);

    MessageFramer framer;

    // Feed in two chunks
    framer.feed(std::span<const uint8_t>{frame.data(), 5});
    EXPECT_FALSE(framer.extractMessage().has_value()); // incomplete

    framer.feed(std::span<const uint8_t>{frame.data() + 5, frame.size() - 5});
    auto msg = framer.extractMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, json);
}

TEST(MessageFramer, ExtractsMultipleSequentialMessages) {
    std::string json1 = R"({"type":"A"})";
    std::string json2 = R"({"type":"B"})";

    auto f1 = MessageFramer::encode(json1);
    auto f2 = MessageFramer::encode(json2);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), f1.begin(), f1.end());
    combined.insert(combined.end(), f2.begin(), f2.end());

    MessageFramer framer;
    framer.feed(std::span<const uint8_t>{combined.data(), combined.size()});

    EXPECT_EQ(framer.extractMessage(), json1);
    EXPECT_EQ(framer.extractMessage(), json2);
    EXPECT_FALSE(framer.extractMessage().has_value());
}

TEST(MessageFramer, ThrowsOnExcessiveLength) {
    // Craft a frame with a bogus 32MB length
    std::vector<uint8_t> malicious = {0xFF, 0xFF, 0xFF, 0x01}; // 33,554,431 bytes

    MessageFramer framer;
    framer.feed(std::span<const uint8_t>{malicious.data(), malicious.size()});
    EXPECT_THROW(framer.extractMessage(), std::runtime_error);
}

TEST(MessageFramer, ResetClearsBuffer) {
    std::string json = R"({"type":"X"})";
    auto frame = MessageFramer::encode(json);

    MessageFramer framer;
    framer.feed(std::span<const uint8_t>{frame.data(), 2}); // partial
    framer.reset();

    // Feed a complete new message after reset
    framer.feed(std::span<const uint8_t>{frame.data(), frame.size()});
    auto msg = framer.extractMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(*msg, json);
}
