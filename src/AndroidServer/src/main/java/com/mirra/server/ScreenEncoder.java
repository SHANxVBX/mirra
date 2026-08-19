package com.mirra.server;

import java.net.Socket;
import java.io.OutputStream;

public class ScreenEncoder {
    private Socket videoSocket;
    
    public ScreenEncoder(Socket socket) {
        this.videoSocket = socket;
    }

    public void streamScreen() {
        // MediaCodec H.264 hardware encoding
        // virtual display surface capture
        // length-prefixed packet streaming
        try {
            OutputStream os = videoSocket.getOutputStream();
            System.out.println("Screen encoder streaming to socket");
            // pseudo H.264 bitstream
            os.write(new byte[]{0, 0, 0, 1}); // NALU header
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
