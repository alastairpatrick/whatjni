package whatjni

import ClassMap
import kotlinx.serialization.Serializable
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.ainslec.picocog.PicoWriter
import org.gradle.api.DefaultTask
import org.gradle.api.file.*
import org.gradle.api.provider.SetProperty
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
import java.io.FileWriter
import java.io.FileNotFoundException
import java.lang.IllegalArgumentException
import java.net.URLClassLoader
import javax.inject.Inject

@Serializable
data class Index(val units: HashMap<String, ArrayList<String>> = hashMapOf())

abstract class GenerateJNIBindingsTask : DefaultTask() {
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

    @get:Input
    abstract val nativePackages: SetProperty<String>

    init {
        source.from(projectLayout.projectDirectory.dir("src/main/cpp"), projectLayout.projectDirectory.dir("src/main/headers"))
        generatedDir.convention(projectLayout.buildDirectory.dir(GenerateJNIBindingsPlugin.GENERATED_DIR))
    }

    @TaskAction
    fun perform(changes: InputChanges) {
        val indexFile = generatedDir.file("index.json").get().asFile
        val index = readIndex(indexFile)
        val dependencies = updateIndexDependencies(changes, index)

        URLClassLoader((classpath.map { it.toURI().toURL() }).toTypedArray()).use { loader ->
            val classMap = ClassMap(generatedDir.get().asFile, loader, nativePackages.get())
            for (className in dependencies) {
                classMap.get(className)
            }

            writeImplementation(classMap)
        }

        writeIndex(indexFile, index)
    }

    private fun readIndex(indexFile: File): Index {
        try {
            val index = Json.decodeFromString<Index>(indexFile.readText(Charsets.UTF_8))
            indexFile.delete()
            return index
        } catch (e: FileNotFoundException) {
        }
        return Index()
    }

    private fun writeIndex(indexFile: File, index: Index) {
        indexFile.writeText(Json.encodeToString(index), Charsets.UTF_8)
    }

    private fun updateIndexDependencies(
        changes: InputChanges,
        index: Index
    ): HashSet<String> {
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
        return dependencies
    }

    private val importJavaRegex = Regex("""^\s*#include\s+"(.*)\.class\.h"\s*$""")

    private fun parseDependencies(sourceFile: File): ArrayList<String> {
        val dependencies = sortedSetOf<String>()
        BufferedReader(FileReader(sourceFile)).use { reader ->
            while (true) {
                val line = reader.readLine()
                if (line == null) {
                    break
                }

                val match = importJavaRegex.matchEntire(line)
                if (match != null) {
                    val className = match.groupValues[1]
                    dependencies.add(className)
                }
            }
        }
        return ArrayList(dependencies)
    }

    private fun writeImplementation(classMap: ClassMap) {
        val writer = PicoWriter()

        val sentry = "initialize_classes_SENTRY_"
        writer.writeln("#ifndef ${sentry}")
        writer.writeln("#define ${sentry}")
        writer.writeln()

        for ((_, classModel) in classMap.classes) {
            writer.writeln("#include \"${classModel.unescapedName}.class.h\"")
        }
        writer.writeln()

        for ((_, classModel) in classMap.classes) {
            writer.writeln("#include \"${classModel.unescapedName}.class.impl.h\"")
        }
        writer.writeln()

        writer.writeln_r("inline void initialize_classes() {")
        for ((_, classModel) in classMap.classes) {
            writer.writeln("${classModel.escapedName}::initialize_class();")
        }
        writer.writeln_l("}")
        writer.writeln()

        writer.writeln("#endif  // ${sentry}")

        val file = generatedDir.file("initialize_classes.h").get().asFile
        FileWriter(file).use {
            it.write(writer.toString())
        }
    }
}
