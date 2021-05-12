package whatjni

import org.gradle.api.DefaultTask
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.TaskAction
import whatjni.util.FindVMLibrary
import java.io.BufferedWriter
import java.io.FileWriter

abstract class GenerateRunScriptTask: DefaultTask() {
    @get:Input
    abstract val installDirectory: DirectoryProperty

    @get:Input
    abstract val executableFile: RegularFileProperty

    @TaskAction
    fun perform() {
        val classpath = project.configurations.findByName(GenerateJNIBindingsPlugin.BINDING_CONFIGURATION)?.asPath
        val vmPath = FindVMLibrary.find()
        val executableFileName = executableFile.get().asFile.name

        val scriptPath = installDirectory.file("whatjni.bat").get().asFile
        scriptPath.parentFile.mkdirs()

        BufferedWriter(FileWriter(scriptPath)).use {
            it.write(
"""@echo off
set "WHATJNI_CLASSPATH=$classpath"
set "WHATJNI_VM_PATH=$vmPath"
call "%~dp0lib\\$executableFileName" %*
exit /B %ERRORLEVEL%
""")
        }
    }
}
