plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.a51.android"
    compileSdk = 34
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.a51.android"
        minSdk = 21
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        // One .so per ABI, no fat binaries.  armeabi-v7a is still here on
        // purpose: a lot of the phones this has to run on are 32 bit.
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_ARM_NEON=ON"
                )
            }
        }

        resourceConfigurations += listOf("en", "ru")
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Split APKs per ABI: the user only downloads the ~2 MB of native code for
    // their own CPU instead of all three.
    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "armeabi-v7a", "x86_64")
            isUniversalApk = true
        }
    }

    // Optional release signing.  When A51_KEYSTORE is not set the release build
    // falls back to the debug key, so the APK that CI produces can be installed
    // straight away (see README for store publishing).
    val keystorePath = System.getenv("A51_KEYSTORE")
    if (keystorePath != null) {
        signingConfigs {
            create("releaseLocal") {
                storeFile = file(keystorePath)
                storePassword = System.getenv("A51_KEYSTORE_PASSWORD") ?: ""
                keyAlias = System.getenv("A51_KEY_ALIAS") ?: ""
                keyPassword = System.getenv("A51_KEY_PASSWORD") ?: ""
            }
        }
    }

    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
            isMinifyEnabled = false
            isJniDebuggable = true
            ndk {
                // Debug builds only carry the ABI of the connected device,
                // which keeps the incremental build fast.
                debugSymbolLevel = "full"
            }
        }

        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            // Falls back to the debug key when no keystore is configured.
            signingConfig = signingConfigs.findByName("releaseLocal")
                ?: signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        viewBinding = true
    }

    packaging {
        jniLibs {
            // Compressed native libs are extracted on install; uncompressed
            // (the default on API 23+) loads faster and uses less RAM.
            useLegacyPackaging = false
        }
        resources {
            excludes += listOf(
                "META-INF/*.kotlin_module",
                "META-INF/DEPENDENCIES",
                "META-INF/LICENSE*",
                "Debug META-INF/**",
                "kotlin/**"
            )
        }
    }

    lint {
        abortOnError = false
        checkReleaseBuilds = false
        warningsAsErrors = false
    }

    testOptions {
        unitTests.isReturnDefaultValues = true
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.documentfile:documentfile:1.0.1")

    testImplementation("junit:junit:4.13.2")
    // Real org.json for the JVM unit tests (android.jar only ships a stub).
    testImplementation("org.json:json:20240303")
}
