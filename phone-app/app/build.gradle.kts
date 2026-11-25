plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.bagclip.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.bagclip.app"
        minSdk = 26          // Android 8.0
        targetSdk = 35
        // Each GitHub build gets a higher number, so a new APK installs over the old one.
        versionCode = System.getenv("GITHUB_RUN_NUMBER")?.toIntOrNull() ?: 1
        versionName = "1.0"
    }

    // Every build is signed with the same key (bagclip-debug.keystore, committed to the
    // repo). Android only installs an update over the old app if the key matches; with a
    // fresh key per build you'd have to uninstall first and redo notification access,
    // "Allow restricted settings" and the battery setting every time.
    // This is a debug key for a personal, sideloaded app, not for the Play Store.
    signingConfigs {
        getByName("debug") {
            storeFile = file("bagclip-debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildTypes {
        debug {
            signingConfig = signingConfigs.getByName("debug")
        }
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

// No libraries beyond the Kotlin standard library: the app only uses the Android framework.
