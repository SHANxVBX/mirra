#include "H264Decoder.h"
#include "../diag/DiagLogger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <stdexcept>

namespace mirra {

H264Decoder::H264Decoder() = default;

H264Decoder::~H264Decoder() {
    if (m_frame)    av_frame_free(&m_frame);
    if (m_packet)   av_packet_free(&m_packet);
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
}

bool H264Decoder::init() {
    auto& log = DiagLogger::get();

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        log.error("FFmpeg: H264 decoder not found");
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        log.error("FFmpeg: Failed to allocate codec context");
        return false;
    }

    // Configure ultra-low-latency real-time stream decoding
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
    m_codecCtx->thread_count = 1; // Single-thread eliminates multi-frame buffer queuing delay

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        log.error("FFmpeg: Failed to open H264 codec");
        return false;
    }

    m_packet = av_packet_alloc();
    m_frame  = av_frame_alloc();

    if (!m_packet || !m_frame) {
        log.error("FFmpeg: Failed to allocate packet/frame buffers");
        return false;
    }

    log.info("H264Decoder initialized ({}, low-latency mode)", codec->name);
    return true;
}

void H264Decoder::onFrame(FrameCallback cb) {
    m_callback = std::move(cb);
}

void H264Decoder::feed(const uint8_t* data, size_t size, int64_t pts) {
    if (!m_codecCtx || !m_packet || !m_frame) return;

    auto t0 = std::chrono::steady_clock::now();

    av_packet_unref(m_packet);
    m_packet->data = const_cast<uint8_t*>(data);
    m_packet->size = static_cast<int>(size);
    m_packet->pts  = pts;

    int ret = avcodec_send_packet(m_codecCtx, m_packet);
    if (ret < 0) {
        DiagLogger::get().warn("FFmpeg: send_packet error {}", ret);
        m_frameDropCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_codecCtx, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            DiagLogger::get().warn("FFmpeg: receive_frame error {}", ret);
            m_frameDropCount.fetch_add(1, std::memory_order_relaxed);
            break;
        }

        auto t1 = std::chrono::steady_clock::now();
        int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        m_totalDecodeUs.fetch_add(us, std::memory_order_relaxed);
        m_decodeCount.fetch_add(1, std::memory_order_relaxed);

        // Copy frame to DecodedFrame (YUV420P)
        if (m_callback && m_frame->data[0] && m_frame->data[1] && m_frame->data[2]) {
            auto df = std::make_shared<DecodedFrame>();
            df->width  = m_frame->width;
            df->height = m_frame->height;
            df->pts    = m_frame->pts;

            // Copy Y plane
            df->strideY = m_frame->linesize[0];
            df->dataY.assign(m_frame->data[0], m_frame->data[0] + df->strideY * m_frame->height);

            // Copy U plane
            int chromaH = (m_frame->height + 1) / 2;
            df->strideU = m_frame->linesize[1];
            df->dataU.assign(m_frame->data[1], m_frame->data[1] + df->strideU * chromaH);

            // Copy V plane
            df->strideV = m_frame->linesize[2];
            df->dataV.assign(m_frame->data[2], m_frame->data[2] + df->strideV * chromaH);

            m_callback(std::move(df));
        }

        av_frame_unref(m_frame);
    }
}

void H264Decoder::flush() {
    if (m_codecCtx) avcodec_flush_buffers(m_codecCtx);
}

float H264Decoder::avgDecodeMs() const {
    int64_t count = m_decodeCount.load(std::memory_order_relaxed);
    if (count == 0) return 0.0f;
    return static_cast<float>(m_totalDecodeUs.load(std::memory_order_relaxed)) / count / 1000.0f;
}

} // namespace mirra
