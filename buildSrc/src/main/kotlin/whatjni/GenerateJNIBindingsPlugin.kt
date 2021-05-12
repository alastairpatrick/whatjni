package whatjni

import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.attributes.LibraryElements
import org.gradle.api.attributes.Usage
import org.gradle.api.file.ProjectLayout
import org.gradle.api.model.ObjectFactory
import org.gradle.language.cpp.tasks.CppCompile
import org.gradle.nativeplatform.tasks.InstallExecutable
import org.gradle.nativeplatform.test.tasks.RunTestExecutable
import whatjni.util.FindVMLibrary
import javax.inject.Inject

abstract class GenerateJNIBindingsPlugin: Plugin<Project> {
    companion object {
        val BINDING_CONFIGURATION = "jniBinding"
        val GENERATED_DIR = "generated/sources/jniBindings"
    }

    @get:Inject
    abstract val objectFactory: ObjectFactory

    @get:Inject
    abstract val projectLayout: ProjectLayout

    override fun apply(project: Project) {
        val extension = project.extensions.create("whatjni", GenerateJNIBindingsExtension::class.java)

        val jniBinding = project.configurations.create(BINDING_CONFIGURATION).apply {
            isCanBeConsumed = false
            isCanBeResolved = true

            attributes.attribute(Usage.USAGE_ATTRIBUTE, objectFactory.named(Usage::class.java, Usage.JAVA_API))
            attributes.attribute(LibraryElements.LIBRARY_ELEMENTS_ATTRIBUTE, objectFactory.named(LibraryElements::class.java, LibraryElements.CLASSES))
        }

        val generateTask = project.tasks.register("generateJNIBindings", GenerateJNIBindingsTask::class.java)
        generateTask.configure {
            it.dependsOn(jniBinding)
            it.classpath.from(jniBinding.files)
            it.nativePackages.addAll(extension.nativePackages)
        }

        project.tasks.withType(CppCompile::class.java).configureEach {
            it.dependsOn(generateTask)
            it.includes.from(projectLayout.buildDirectory.dir(GENERATED_DIR))
        }

        project.tasks.withType(InstallExecutable::class.java).all { installTask ->
            val task = project.tasks.register("whatjni\$${installTask.name}", GenerateRunScriptTask::class.java) {
                it.installDirectory.set(installTask.installDirectory.get())
                it.executableFile.set(installTask.executableFile.get())
            }

            installTask.dependsOn(task)
        }

        project.tasks.withType(RunTestExecutable::class.java).configureEach {
            it.environment.put("WHATJNI_CLASSPATH", jniBinding.asPath)
            it.environment.put("WHATJNI_VM_PATH", FindVMLibrary.find())
        }
    }
}