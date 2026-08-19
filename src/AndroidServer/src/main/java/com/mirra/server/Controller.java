package com.mirra.server;

import java.net.Socket;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.ByteBuffer;

public class Controller {
    public static final int TYPE_SET_CLIPBOARD = 1;
    
    private Socket controlSocket;
    private Device device;

    public Controller(Socket socket, Device device) {
        this.controlSocket = socket;
        this.device = device;
    }

    public void controlLoop() {
        try {
            InputStream is = controlSocket.getInputStream();
            OutputStream os = controlSocket.getOutputStream();
            System.out.println("Controller ready for input commands");
            
            // Add clipboard listener to push to Windows
            device.addClipboardListener(() -> {
                String text = device.getClipboardText();
                if (text != null) {
                    try {
                        byte[] textBytes = text.getBytes(StandardCharsets.UTF_8);
                        ByteBuffer buffer = ByteBuffer.allocate(1 + 4 + textBytes.length);
                        buffer.put((byte) TYPE_SET_CLIPBOARD);
                        buffer.putInt(textBytes.length);
                        buffer.put(textBytes);
                        os.write(buffer.array());
                        os.flush();
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            });

            byte[] header = new byte[5]; // 1 byte type + 4 byte length
            while (true) {
                int read = is.read(header);
                if (read == -1) break;
                if (read < 5) continue;
                
                int type = header[0];
                ByteBuffer lengthBuffer = ByteBuffer.wrap(header, 1, 4);
                int length = lengthBuffer.getInt();
                
                if (type == TYPE_SET_CLIPBOARD) {
                    byte[] data = new byte[length];
                    int dataRead = 0;
                    while (dataRead < length) {
                        int r = is.read(data, dataRead, length - dataRead);
                        if (r == -1) break;
                        dataRead += r;
                    }
                    if (dataRead == length) {
                        String text = new String(data, StandardCharsets.UTF_8);
                        device.setClipboardText(text);
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
