package whatjni

import ClassMap
import com.google.gson.Gson
import org.gradle.api.DefaultTask
import org.gradle.api.file.*
import org.gradle.api.tasks.*
import org.gradle.work.ChangeType
import org.gradle.work.Incremental
import org.gradle.work.InputChanges
import java.io.*
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

        URLClassLoader((classpath.map { it.toURI().toURL() }).toTypedArray()).use { loader ->
            val classMap = ClassMap(generatedDir.get().asFile, loader)
            for (className in dependencies) {
                classMap.get(className)
            }
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
}
