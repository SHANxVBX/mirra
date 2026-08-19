#include "MessageFramer.h"
#include <cstring>
#include <stdexcept>

namespace mirra {

// ── Encode ────────────────────────────────────────────────────────────────────
std::vector<uint8_t> MessageFramer::encode(const std::string& json) {
    const uint32_t len = static_cast<uint32_t>(json.size());
    std::vector<uint8_t> frame(4 + len);

    // Little-endian length prefix
    frame[0] = static_cast<uint8_t>(len & 0xFF);
    frame[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
    frame[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
    frame[3] = static_cast<uint8_t>((len >> 24) & 0xFF);

    std::memcpy(frame.data() + 4, json.data(), len);
    return frame;
}

// ── Feed ──────────────────────────────────────────────────────────────────────
void MessageFramer::feed(std::span<const uint8_t> data) {
    m_buffer.insert(m_buffer.end(), data.begin(), data.end());
}

// ── Extract ───────────────────────────────────────────────────────────────────
std::optional<std::string> MessageFramer::extractMessage() {
    // Need at least 4 bytes for the length header
    if (m_buffer.size() < 4) return std::nullopt;

    // Read little-endian uint32 length
    const uint32_t msgLen =
        static_cast<uint32_t>(m_buffer[0])        |
        (static_cast<uint32_t>(m_buffer[1]) << 8)  |
        (static_cast<uint32_t>(m_buffer[2]) << 16) |
        (static_cast<uint32_t>(m_buffer[3]) << 24);

    // Sanity check: reject absurdly large messages (>16 MB)
    if (msgLen > 16 * 1024 * 1024) {
        throw std::runtime_error("IPC message length exceeds maximum (16 MB). Protocol error.");
    }

    // Wait for full payload
    if (m_buffer.size() < 4 + msgLen) return std::nullopt;

    // Extract message
    std::string json(reinterpret_cast<char*>(m_buffer.data() + 4), msgLen);

    // Consume from buffer
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + 4 + msgLen);

    return json;
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void MessageFramer::reset() {
    m_buffer.clear();
}

} // namespace mirra
