package com.mirra.server;

public class ControlMessage {
    public static final int TYPE_INJECT_TOUCH = 0;
    public static final int TYPE_INJECT_KEYCODE = 1;
    public static final int TYPE_INJECT_TEXT = 2;

    private int type;
    private String text;
    private Position position;

    public ControlMessage(int type) { this.type = type; }

    public int getType() { return type; }
    public String getText() { return text; }
    public Position getPosition() { return position; }
}
