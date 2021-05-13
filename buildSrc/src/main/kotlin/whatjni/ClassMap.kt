import org.objectweb.asm.*
import whatjni.Generator
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter

class ClassMap(val generatedDir: File, val loader: ClassLoader, val nativePackages: Set<String>) {
    val classes = sortedMapOf<String, ClassModel>()

    fun get(className: String): ClassModel {
        val classModel = classes[className]
        if (classModel != null) {
            return classModel
        }

        val classStream = loader.getResourceAsStream(className + ".class")
        if (classStream == null) {
            // This is not necessarily an error; what looked like an #include directive might not be. It could be
            // part of a multiline string or skipped by an #if directive, for example. Instead, write the error to
            // the generated file so that, if it actually is #included, the C++ compiler will report the error
            // and fail.
            val generatedFile = generatedDir.resolve(className + ".class.h")
            generatedFile.parentFile.mkdirs()
            FileWriter(generatedFile).use {
                it.write("#error Could not find class '${className}'.\n")
            }
            return ClassModel(0, className, "", null, ArrayList<ClassModel>())
        }

        var implementsNative = false
        for (pkg in nativePackages) {
            if (className.replace("/", ".").startsWith(pkg + ".")) {
                implementsNative = true
            }
        }

        val generator = Generator(generatedDir,this, implementsNative)
        val classReader = ClassReader(classStream)
        classReader.accept(generator, ClassReader.SKIP_CODE or ClassReader.SKIP_DEBUG or ClassReader.SKIP_FRAMES)
        classes[className] = generator.classModel

        System.out.println("Generated ${className}")
        return generator.classModel
    }

}
