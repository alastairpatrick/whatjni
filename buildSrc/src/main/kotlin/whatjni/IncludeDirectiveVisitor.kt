package whatjni

import org.objectweb.asm.*
import java.io.BufferedWriter
import java.io.FileWriter

class IncludeDirectiveVisitor(val writer: BufferedWriter) : ClassVisitor(Opcodes.ASM7) {
    private fun writeInclude(className: String) {
        writer.write("#include \"")
        writer.write(className)
        writer.write(".class.h\"\n")
    }

    override fun visit(
        version: Int,
        access: Int,
        name: String?,
        signature: String?,
        superName: String?,
        interfaces: Array<out String>?
    ) {
        if (superName != null) {
            writeInclude(superName)
        }
        if (interfaces != null) {
            for (name in interfaces) {
                writeInclude(name)
            }
        }
    }
}