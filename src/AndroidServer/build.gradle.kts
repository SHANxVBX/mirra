import com.android.build.api.variant.LibraryAndroidComponentsExtension

plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "com.mirra.server"
    compileSdk = 35

    defaultConfig {
        minSdk = 29
        targetSdk = 35
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

val buildServerJar by tasks.registering(Exec::class) {
    dependsOn("assembleRelease")
    
    // In a standard AGP build, classes are bundled into an AAR. 
    // We can run d8 on the intermediate classes.jar to generate classes.dex
    val buildToolsVersion = android.buildToolsVersion
    val sdkDir = android.sdkDirectory.absolutePath
    val d8Ext = if (org.gradle.internal.os.OperatingSystem.current().isWindows) "d8.bat" else "d8"
    val d8 = file("$sdkDir/build-tools/$buildToolsVersion/$d8Ext").absolutePath

    val intermediates = layout.buildDirectory.dir("intermediates/aar_main_jar/release/syncReleaseLibJars").get().asFile
    val classesJar = file("$intermediates/classes.jar").absolutePath
    val outputJar = layout.buildDirectory.file("outputs/jar/mirra-server.jar").get().asFile

    doFirst {
        outputJar.parentFile.mkdirs()
    }

    commandLine(d8, classesJar, "--output", outputJar.absolutePath, "--release")
}

dependencies {
}
