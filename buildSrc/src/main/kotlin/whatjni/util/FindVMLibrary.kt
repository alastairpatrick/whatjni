package whatjni.util

import com.sun.jna.Native
import com.sun.jna.Pointer
import com.sun.jna.Structure
import com.sun.jna.win32.StdCallLibrary
import java.nio.file.*
import java.nio.file.attribute.BasicFileAttributes

@Structure.FieldOrder("version", "nOptions", "options", "ignoreUnrecognized")
class JavaVMInitArgs: Structure() {
    @JvmField var version: Int = 0
    @JvmField var nOptions: Int = 0
    @JvmField var options: Pointer? = null
    @JvmField var ignoreUnrecognized: Int = 0
}

interface JVMLibrary : StdCallLibrary {
    fun JNI_GetDefaultJavaVMInitArgs(args: JavaVMInitArgs?): Int
}

fun isWorkingVMLibrary(file: Path): Boolean {
    try {
        val jvmLibrary = Native.load(file.toString(), JVMLibrary::class.java) as JVMLibrary

        val args = JavaVMInitArgs()
        args.version = 0x10002  // JNI version 1.2
        if (jvmLibrary.JNI_GetDefaultJavaVMInitArgs(args) != 0) {
            return false;
        }

        return true
    } catch (e: UnsatisfiedLinkError) {
        return false
    }
}

fun findVMLibrary(): String {
    val osName = System.getProperty("os.name").toLowerCase()
    val vmLibraryName = if (osName.startsWith("win"))
        "jvm.dll"
    else if (osName.startsWith("mac"))
        "libjvm.dylib"
    else
        "libjvm.so"

    var candidates = ArrayList<Path>()
    val javaHome = System.getProperty("java.home")
    Files.walkFileTree(Paths.get(javaHome), object: SimpleFileVisitor<Path>() {
        override fun visitFile(file: Path, attrs: BasicFileAttributes): FileVisitResult {
            if (file.fileName.toString().equals(vmLibraryName)) {
                candidates.add(file)
            }
            return FileVisitResult.CONTINUE;
        }
    })

    candidates.sortBy {
        when (it.parent.fileName.toString()) {
            "server" -> 0
            "client" -> 1
            "default" -> 2
            else -> 3
        }
    }

    println("Candidates $candidates")
    for (candidate in candidates) {
        if (isWorkingVMLibrary(candidate)) {
            return candidate.toString()
        }
    }

    throw RuntimeException("Could not find Java VM shared library.")
}

// Wrap in class so we can call from Groovy build script
class FindVMLibrary {
    companion object {
        @JvmStatic
        fun find(): String {
            return findVMLibrary()
        }
    }
}