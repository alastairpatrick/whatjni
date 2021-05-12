package whatjni

import org.gradle.api.provider.SetProperty

interface GenerateJNIBindingsExtension {
    val nativePackages: SetProperty<String>
}