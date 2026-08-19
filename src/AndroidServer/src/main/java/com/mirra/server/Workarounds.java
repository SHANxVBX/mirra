package com.mirra.server;

public class Workarounds {
    public static void prepare() {
        // Hidden API reflection for SurfaceControl and ServiceManager across Android 10-15
        try {
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            // Workaround initialization logic here
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    public static Object getServiceManager() throws Exception {
        Class<?> serviceManagerClass = Class.forName("android.os.ServiceManager");
        return serviceManagerClass.getMethod("getService", String.class).invoke(null, "input");
    }
}
