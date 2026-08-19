// Mirra — H.264 Decoder (FFmpeg)
// Demuxes raw H.264 NAL units from the ADB socket and decodes via FFmpeg.
// Presents decoded frames to the SDL renderer via atomic frame swap.

#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

// Forward-declare FFmpeg types to avoid polluting headers
struct AVCodecContext;
struct AVPacket;
struct AVFrame;

namespace mirra {

struct DecodedFrame {
    int width   = 0;
    int height  = 0;
    int64_t pts = 0;
    // YUV420P planes
    std::vector<uint8_t> dataY, dataU, dataV;
    int strideY = 0, strideU = 0, strideV = 0;
};

using FrameCallback = std::function<void(std::shared_ptr<DecodedFrame>)>;

class H264Decoder {
public:
    H264Decoder();
    ~H264Decoder();

    // Initialize FFmpeg codec context (prefer DXVA2/D3D11VA hw decode)
    bool init();

    // Feed a raw H.264 access unit (one complete NALU or annex-B frame)
    // Invokes callback when a frame is ready.
    void feed(const uint8_t* data, size_t size, int64_t pts);

    // Register frame output callback
    void onFrame(FrameCallback cb);

    // Flush decoder (e.g., on reconnect)
    void flush();

    // Diagnostics
    int  frameDropCount()   const { return m_frameDropCount.load(); }
    float avgDecodeMs()     const;

private:
    AVCodecContext*             m_codecCtx   = nullptr;
    AVPacket*                   m_packet     = nullptr;
    AVFrame*                    m_frame      = nullptr;
    FrameCallback               m_callback;
    std::atomic<int>            m_frameDropCount{0};
    std::atomic<int64_t>        m_totalDecodeUs{0};
    std::atomic<int64_t>        m_decodeCount{0};
};

} // namespace mirra
