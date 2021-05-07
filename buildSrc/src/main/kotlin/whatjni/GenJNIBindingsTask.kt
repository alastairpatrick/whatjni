package whatjni

import com.google.gson.Gson
import org.gradle.api.DefaultTask
import org.gradle.api.file.*
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import org.objectweb.asm.ClassReader
import org.objectweb.asm.ClassReader.*
import java.io.*
import java.lang.Exception
import java.lang.IllegalArgumentException
import java.net.URLClassLoader
import javax.inject.Inject

data class Index(val units: HashMap<String, ArrayList<String>> = hashMapOf())

abstract class GenJNIBindingsTask : DefaultTask() {
    @get:Inject
    abstract val projectLayout: ProjectLayout

    @get:CompileClasspath
    @get:InputFiles
    abstract val classpath: ConfigurableFileCollection

    @get:PathSensitive(PathSensitivity.RELATIVE)
    @get:InputFiles
    @get:Incremental
    abstract val source: ConfigurableFileCollection

    @get:OutputDirectory
    abstract val generatedDir: DirectoryProperty

    init {
        source.from(projectLayout.projectDirectory.dir("src/main/cpp"), projectLayout.projectDirectory.dir("src/main/headers"))
        generatedDir.convention(projectLayout.buildDirectory.dir("generated/sources/jniBindings"))
    }

    @TaskAction
    fun perform(changes: InputChanges) {
        System.out.println("Classpath: ${classpath.asPath}")
        val gson = Gson()

        // TODO: use kotlin serialization
        var index = Index()
        val indexFile = generatedDir.file("index.json").get().asFile
        try {
            index = BufferedReader(FileReader(indexFile)).use {
                gson.fromJson(it, Index::class.java)
            }
            indexFile.delete()
        } catch (e: FileNotFoundException) {
        }

        val projectPath = projectLayout.projectDirectory.asFile.toPath()
        val dependencies = hashSetOf<String>()
        for (change in changes.getFileChanges(source)) {
            if (change.fileType == FileType.DIRECTORY) {
                continue
            }

            var unitPath = change.file.toPath()
            try {
                unitPath = projectPath.relativize(unitPath)
            } catch (e: IllegalArgumentException) {
                // Fall back to absolute path if source file is outside the project directory
            }

            val unitKey = unitPath.joinToString("/")
            if (change.changeType == ChangeType.REMOVED) {
                index.units.remove(unitKey)
            } else {
                val directDependencies = parseDependencies(change.file)
                if (directDependencies.isEmpty()) {
                    index.units.remove(unitKey)
                } else {
                    index.units[unitKey] = directDependencies
                    dependencies.addAll(directDependencies)
                }
            }
        }

        val loader = URLClassLoader((classpath.map { it.toURL() }).toTypedArray())

        val todo = ArrayList<String>(dependencies)
        while (!todo.isEmpty()) {
            val className = todo.removeLast()
            val baseVisitor = BaseVisitor()
            var classReader: ClassReader?

            val generatedFile = generatedDir.file(className + ".class.h").get().asFile;
            try {
                classReader = ClassReader(loader.getResourceAsStream(className + ".class"))
                classReader.accept(baseVisitor, SKIP_CODE or SKIP_DEBUG or SKIP_FRAMES)
            } catch (e: Exception) {
                // This is not necessarily an error; what looked like an #include directive might not be. It could be
                // part of a multiline string or skipped by an #if directive, for example. Instead, write the error to
                // the generated file so that, if it actually is #included, the C++ compiler will report the error
                // and fail.
                FileWriter(generatedFile).use {
                    it.write("#error ${e.message}\n")
                }
                continue
            }

            for (baseClassName in baseVisitor.baseClassNames) {
                if (dependencies.add(baseClassName)) {
                    todo.add(baseClassName)
                }
            }

            generateBindings(generatedFile, classReader, className)
        }

        BufferedWriter(FileWriter(indexFile)).use {
            gson.toJson(index, it)
        }
    }

    private val importJavaRegex = Regex("""^\s*#include\s+"(.*)\.class\.h"\s*$""")

    private fun parseDependencies(sourceFile: File): ArrayList<String> {
        val dependencies = sortedSetOf<String>()
        BufferedReader(FileReader(sourceFile)).use { reader ->
            while (true) {
                val line = reader.readLine();
                if (line == null) {
                    break;
                }

                val match = importJavaRegex.matchEntire(line)
                if (match != null) {
                    val className = match.groupValues[1];
                    dependencies.add(className)
                }
            }
        }
        return ArrayList(dependencies)
    }

    private fun generateBindings(generatedFile: File, classReader: ClassReader, className: String) {
        generatedFile.parentFile.mkdirs()

        BufferedWriter(FileWriter(generatedFile)).use { writer ->
            val nameParts = escapeQualifiedName(className).split("/")
            writeHeader(writer, nameParts)

            classReader.accept(IncludeDirectiveVisitor(writer), SKIP_CODE or SKIP_DEBUG or SKIP_FRAMES)
            writer.write("\n")
            classReader.accept(ForwardDeclarationVisitor(writer), SKIP_CODE or SKIP_DEBUG or SKIP_FRAMES)
            writer.write("\n")
            classReader.accept(ClassDefinitionVisitor(writer), SKIP_CODE or SKIP_DEBUG or SKIP_FRAMES)

            writeFooter(writer)
        }
    }

    private fun writeHeader(writer: BufferedWriter, nameParts: List<String>) {
        val sentryMacro = nameParts.joinToString(separator = "_")
        writer.write(
"""
// Automatically generated; don't edit.
            
#ifndef ${sentryMacro}_SENTRY_H_
#define ${sentryMacro}_SENTRY_H_

#include "whatjni/array.h"
#include "whatjni/ref.h"
""");
    }

    private fun writeFooter(writer: BufferedWriter) {
        writer.write("\n\n#endif\n")
    }
}
