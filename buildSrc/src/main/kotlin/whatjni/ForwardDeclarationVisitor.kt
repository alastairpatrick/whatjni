package whatjni

import org.objectweb.asm.*
import java.io.BufferedWriter
import java.io.FileWriter

class ForwardDeclarationVisitor(val writer: BufferedWriter) : ClassVisitor(Opcodes.ASM7) {
    private val declared = hashSetOf<String>()

    private fun recurseType(type: Type) {
        when (type.sort) {
            Type.OBJECT -> {
                if (!declared.contains(type.className)) {
                    declared.add(type.className)
                    val name = escapeQualifiedName(type.className)

                    val parts = name.split(".")
                    for (i in 0..parts.size - 2) {
                        writer.write("namespace ${parts[i]} { ")
                    }
                    writer.write("class ${parts[parts.size - 1]}; ")
                    for (i in 0..parts.size - 2) {
                        writer.write("} ")
                    }
                    writer.write("\n");
                }
            }
            Type.ARRAY -> recurseType(type.elementType)
            Type.METHOD -> {
                recurseType(type.returnType);
                for (argType in type.argumentTypes) {
                    recurseType(argType)
                }
            }
        }
    }

    override fun visitField(
        access: Int,
        name: String?,
        descriptor: String?,
        signature: String?,
        value: Any?
    ): FieldVisitor? {
        if (descriptor != null) {
            val type = Type.getType(descriptor)
            recurseType(type)
        }
        return null
    }

    override fun visitMethod(
        access: Int,
        name: String?,
        descriptor: String?,
        signature: String?,
        exceptions: Array<out String>?
    ): MethodVisitor? {
        if (descriptor != null) {
            val type = Type.getMethodType(descriptor);
            recurseType(type.returnType);
            for (argType in type.argumentTypes) {
                recurseType(argType)
            }
        }
        return null
    }
}