// Mirra — IPC Message Framer
// Handles length-prefixed JSON framing: [4-byte LE uint32 length][UTF-8 JSON]

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <span>

namespace mirra {

class MessageFramer {
public:
    // Encode a JSON string into a length-prefixed frame
    static std::vector<uint8_t> encode(const std::string& json);

    // Feed raw bytes; call extractMessage() repeatedly until nullopt
    void feed(std::span<const uint8_t> data);

    // Returns the next complete JSON message, or nullopt if incomplete
    std::optional<std::string> extractMessage();

    // Reset state (e.g., on reconnect)
    void reset();

private:
    std::vector<uint8_t> m_buffer;
};

} // namespace mirra
