package com.mirra.server;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import java.net.Socket;
import java.io.OutputStream;

public class AudioCapture {
    private Socket audioSocket;

    public AudioCapture(Socket socket) {
        this.audioSocket = socket;
    }

    public void streamAudio() {
        try {
            OutputStream os = audioSocket.getOutputStream();
            System.out.println("Audio capture streaming to socket");
            
            int sampleRate = 48000;
            int channelConfig = AudioFormat.CHANNEL_IN_STEREO;
            int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
            int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
            
            AudioRecord audioRecord = new AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.REMOTE_SUBMIX)
                .setAudioFormat(new AudioFormat.Builder()
                    .setEncoding(audioFormat)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig)
                    .build())
                .setBufferSizeInBytes(minBufferSize)
                .build();
                
            audioRecord.startRecording();
            byte[] buffer = new byte[minBufferSize];
            while (true) {
                int read = audioRecord.read(buffer, 0, buffer.length);
                if (read > 0) {
                    os.write(buffer, 0, read);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
