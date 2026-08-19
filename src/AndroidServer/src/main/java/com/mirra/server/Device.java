package com.mirra.server;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Looper;
import java.lang.reflect.Method;

public class Device {
    private ClipboardManager clipboardManager;
    private String lastClipboard = "";

    public Device() {
        try {
            if (Looper.myLooper() == null) {
                Looper.prepare();
            }
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Method systemMainMethod = activityThreadClass.getMethod("systemMain");
            Object activityThread = systemMainMethod.invoke(null);
            
            Method getSystemContextMethod = activityThreadClass.getMethod("getSystemContext");
            Context context = (Context) getSystemContextMethod.invoke(activityThread);
            
            clipboardManager = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        } catch (Exception e) {
            e.printStackTrace();
            System.err.println("Failed to initialize ClipboardManager via Context");
        }
    }

    public void addClipboardListener(Runnable listener) {
        if (clipboardManager != null) {
            clipboardManager.addPrimaryClipChangedListener(() -> listener.run());
        }
    }

    public String getClipboardText() {
        if (clipboardManager != null && clipboardManager.hasPrimaryClip()) {
            ClipData clip = clipboardManager.getPrimaryClip();
            if (clip != null && clip.getItemCount() > 0) {
                CharSequence text = clip.getItemAt(0).getText();
                if (text != null) return text.toString();
            }
        }
        return null;
    }

    public void setClipboardText(String text) {
        if (text == null || text.equals(lastClipboard)) return;
        lastClipboard = text;
        if (clipboardManager != null) {
            clipboardManager.setPrimaryClip(ClipData.newPlainText("text", text));
        }
    }

    public Size getScreenSize() {
        // Screen coordinate resolution, orientation handling
        return new Size(1080, 1920);
    }
    
    public Object getInputManager() {
        // InputManager reflection
        try {
            return Workarounds.getServiceManager();
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }
}
