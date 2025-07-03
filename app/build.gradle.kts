plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    alias(libs.plugins.kotlin.compose)
    id("kotlinx-serialization")
}

android {
    namespace = "com.juliejohnson.voicegenderpavlok"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.juliejohnson.voicegenderpavlok"
        minSdk = 28
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // NDK configuration
        ndk {
            // Specify the ABIs you want to build for
            abiFilters += listOf("arm64-v8a")
            // Add x86, x86_64 if you need emulator support:
            // abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )

            // NDK debug symbols for release builds (optional)
            ndk {
                debugSymbolLevel = "SYMBOL_TABLE"
            }

            isDebuggable = true
            signingConfig = signingConfigs.getByName("debug")
        }

        debug {
            // Enable NDK debugging
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    // NDK build configuration
    ndkVersion = "27.2.12479018"

    packaging {
        jniLibs {
            pickFirsts += listOf("**/libc++_shared.so", "**/libjsc.so")
        }
        resources {
            excludes += listOf(
                "META-INF/DEPENDENCIES",
                "META-INF/LICENSE",
                "META-INF/LICENSE.txt",
                "META-INF/NOTICE",
                "META-INF/NOTICE.txt"
            )
        }
    }

    buildFeatures {
        viewBinding = true
        compose = true
    }

    // This tells Gradle where your C++ build script is located
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1" // Or a recent version installed via SDK manager
        }
    }

    // Source sets configuration
    sourceSets {
        getByName("main") {
            // Ensure JNI libraries are included
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }
}

dependencies {
    // Voice Gender Assist (VGA) Audio Logic Library
//    implementation(project(":audiologiclib"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.ui)
    implementation(libs.androidx.material3.android)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.recyclerview) // or latest

    // Coroutines for background tasks
    implementation(libs.kotlinx.coroutines.android)

    // TensorFlow Lite for ML (we can later swap in PyTorch Mobile)
    implementation(libs.tensorflow.lite)
//    implementation(libs.tensorflow.lite.support)

    // Retrofit for API calls
    implementation(libs.retrofit)
    implementation(libs.converter.gson)

    // Biometric authentication
    implementation(libs.androidx.biometric)

    // gkonovalov/android-vad Library/Module
    implementation(linkedMapOf("name" to "android-vad-silero","ext" to "aar"))
    implementation(libs.onnxruntime.android)

    // Json Serialization
    implementation(libs.kotlinx.serialization.json)

    implementation(libs.jtransforms)

    implementation(libs.androidx.lifecycle.viewmodel.ktx)
    implementation(libs.androidx.activity.ktx)

    // Audio Feature processing
    // https://mvnrepository.com/artifact/be.tarsos.dsp/core
    implementation(libs.core)
    implementation(libs.jvm)
}

// Optional: Task to verify Essentia libraries exist
tasks.register("verifyEssentiaLibraries") {
    doLast {
        val abiList = listOf("arm64-v8a", "armeabi-v7a")
        val missingLibs = mutableListOf<String>()

        abiList.forEach { abi ->
            val libPath = file("src/main/jniLibs/$abi/libessentia.a")
            if (!libPath.exists()) {
                missingLibs.add("$abi/libessentia.a")
            } else {
                println("✓ Found libessentia.a for $abi")
            }
        }

        if (missingLibs.isNotEmpty()) {
            throw GradleException("Missing Essentia libraries: ${missingLibs.joinToString(", ")}")
        }

        // Check for headers
        val headersPath = file("src/main/cpp/essentia")
        if (!headersPath.exists()) {
            println("⚠ Warning: Essentia headers not found at src/main/cpp/essentia")
        } else {
            println("✓ Found Essentia headers")
        }
    }
}

// Run verification before building
tasks.named("preBuild").configure {
    dependsOn("verifyEssentiaLibraries")
}