package whatjni

import ClassMap
import com.google.gson.Gson
import org.ainslec.picocog.PicoWriter
import org.gradle.api.DefaultTask
import org.gradle.api.file.*
import org.gradle.api.provider.SetProperty
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import org.objectweb.asm.Opcodes
import java.io.*
import java.lang.IllegalArgumentException
import java.net.URLClassLoader
import javax.inject.Inject

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
        val gson = Gson()

        val indexFile = generatedDir.file("index.json").get().asFile
        val index = readIndex(gson, indexFile)
        val dependencies = updateIndexDependencies(changes, index)

        URLClassLoader((classpath.map { it.toURI().toURL() }).toTypedArray()).use { loader ->
            val classMap = ClassMap(generatedDir.get().asFile, loader, nativePackages.get())
            for (className in dependencies) {
                classMap.get(className)
            }

            writeRegisterNatives(classMap)
        }

        writeIndex(gson, indexFile, index)
    }

    private fun readIndex(gson: Gson, indexFile: File): Index {
        // TODO: use kotlin serialization
        var index = Index()
        try {
            index = BufferedReader(FileReader(indexFile)).use {
                gson.fromJson(it, Index::class.java)
            }
            indexFile.delete()
        } catch (e: FileNotFoundException) {
        }
        return index
    }

    private fun writeIndex(gson: Gson, indexFile: File, index: Index) {
        BufferedWriter(FileWriter(indexFile)).use {
            gson.toJson(index, it)
        }
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

    private fun writeRegisterNatives(classMap: ClassMap) {
        for (nativePackage in nativePackages.get()) {
            val writer = PicoWriter()

            val sentry = nativePackage.replace(".", "_") + "register_natives_SENTRY_"
            writer.writeln("#ifndef ${sentry}")
            writer.writeln("#define ${sentry}")
            writer.writeln()

            val includeWriter = writer.createDeferredWriter()
            writer.writeln()

            val nameParts = nativePackage.split(".")
            writeOpenNamespace(writer, nameParts)

            writer.writeln_r("inline void register_natives() {")

            val prefix = nativePackage.replace(".", "/") + "/"
            for ((className, classModel) in classMap.classes.tailMap(nativePackage)) {
                if (!className.startsWith(prefix)) {
                    break
                }

                val nativeMethods = classModel.methods.filter { (it.access and Opcodes.ACC_NATIVE) != 0 }
                if (nativeMethods.isEmpty()) {
                    continue
                }

                includeWriter.writeln("#include \"${classModel.unescapedName}.class.h\"")
                writer.writeln("${classModel.escapedName}::register_natives();")
            }

            writer.writeln_l("}")
            writer.writeln()

            writeCloseNamespace(writer, nameParts)

            writer.writeln("#endif  // ${sentry}")

            val registerNativesFile =
                File(generatedDir.file(nativePackage.replace(".", "/")).get().asFile, "register_natives.h")
            FileWriter(registerNativesFile).use {
                it.write(writer.toString())
            }
        }
    }
}
