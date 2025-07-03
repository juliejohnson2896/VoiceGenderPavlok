pluginManagement {
    repositories {
        google {
            content {
                includeGroupByRegex("com\\.android.*")
                includeGroupByRegex("com\\.google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS) // This enforces repository definition in settings.gradle
    repositories {
        google()
        mavenCentral()
        maven {
            name = "be.0110.repoReleases"
            url = uri("https://mvn.0110.be/releases")
        }
        // Add any other repositories you were using in build.gradle.kts here
        flatDir {
            dirs("libs")
        }
    }
}

rootProject.name = "VoiceGenderPavlok"
include(":app")

// Include the KMP library build
includeBuild("../audiologiclib") {
    dependencySubstitution {
        // Substitute the Maven dependency with the local project
        substitute(module("com.juliejohnson.voicegenderassist:audiologiclib")).using(project(":audiologiclib"))
    }
}