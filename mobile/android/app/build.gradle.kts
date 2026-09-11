plugins { id("com.android.application"); id("org.jetbrains.kotlin.android") }
android {
    namespace = "dev.fofo.maglava"
    compileSdk = 36
    ndkVersion = "27.0.12077973"
    defaultConfig {
        applicationId = "dev.fofo.maglava"
        minSdk = 26
        targetSdk = 36
        testInstrumentationRunner = "dev.fofo.maglava.SmokeRunner"
        versionCode = 2
        versionName = "1.0"
        externalNativeBuild { cmake { arguments += "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON" } }
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    sourceSets["main"].assets.srcDirs("../../../v4/assets/fonts", "../../assets")
    signingConfigs {
        create("uat") {
            val keystore = System.getenv("MAGLAVA_KEYSTORE")
            if (keystore != null) {
                storeFile = file(keystore)
                storePassword = System.getenv("MAGLAVA_STORE_PASSWORD")
                keyAlias = System.getenv("MAGLAVA_KEY_ALIAS") ?: "spt"
                keyPassword = System.getenv("MAGLAVA_KEY_PASSWORD")
            }
        }
    }
    buildTypes { release {
        isMinifyEnabled = false
        if (System.getenv("MAGLAVA_KEYSTORE") != null) signingConfig = signingConfigs.getByName("uat")
    } }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}
