package com.mirra.server;

import java.net.ServerSocket;
import java.net.Socket;

public class Server {
    private static final int PORT_VIDEO = 27183;
    private static final int PORT_CONTROL = 27184;
    private static final int PORT_AUDIO = 27185;

    public static void main(String[] args) {
        // CLI parser
        System.out.println("Starting Android Server Engine...");
        
        // Thread management & Port binding
        new Thread(Server::startVideoServer).start();
        new Thread(Server::startControlServer).start();
        new Thread(Server::startAudioServer).start();
    }

    private static void startVideoServer() {
        try (ServerSocket serverSocket = new ServerSocket(PORT_VIDEO)) {
            System.out.println("Video server bound to " + PORT_VIDEO);
            Socket socket = serverSocket.accept();
            new ScreenEncoder(socket).streamScreen();
        } catch (Exception e) { e.printStackTrace(); }
    }

    private static void startControlServer() {
        try (ServerSocket serverSocket = new ServerSocket(PORT_CONTROL)) {
            System.out.println("Control server bound to " + PORT_CONTROL);
            Socket socket = serverSocket.accept();
            Device device = new Device();
            new Controller(socket, device).controlLoop();
        } catch (Exception e) { e.printStackTrace(); }
    }

    private static void startAudioServer() {
        try (ServerSocket serverSocket = new ServerSocket(PORT_AUDIO)) {
            System.out.println("Audio server bound to " + PORT_AUDIO);
            Socket socket = serverSocket.accept();
            new AudioCapture(socket).streamAudio();
        } catch (Exception e) { e.printStackTrace(); }
    }
}
