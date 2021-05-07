package whatjni

import org.objectweb.asm.*
import java.io.BufferedWriter

class ClassDefinitionVisitor(val writer: BufferedWriter) : ClassVisitor(Opcodes.ASM7) {
    var nameParts: List<String> = listOf()
    var className: String = "";

    override fun visit(
        version: Int,
        access: Int,
        name: String?,
        signature: String?,
        superName: String?,
        interfaces: Array<out String>?
    ) {
        name!!
        interfaces!!

        className = name
        nameParts = escapeQualifiedName(name).split("/")

        for (i in 0 .. nameParts.size - 2) {
            writer.write("namespace ${nameParts[i]} {\n")
        }
        writer.write("\nclass ${nameParts[nameParts.size - 1]}")

        var baseSeparator = ":"

        if (superName != null) {
            writer.write(baseSeparator)
            writer.write(" public ::")
            writer.write(escapeQualifiedName(superName).replace("/", "::"))
            baseSeparator = ","
        }

        for (interfaceName in interfaces) {
            writer.write(baseSeparator)
            writer.write(" public ::")
            writer.write(escapeQualifiedName(interfaceName).replace("/", "::"))
            baseSeparator = ","
        }

        writer.write(" {\n")
    }

    override fun visitField(
        access: Int,
        unescapedName: String?,
        descriptor: String?,
        signature: String?,
        value: Any?
    ): FieldVisitor? {
        unescapedName!!

        val type = Type.getType(descriptor)
        val cppTypeName = makeCPPTypeName(type, false)
        val paramCPPTypeName = makeCPPTypeName(type, true)
        val escapedName = escapeSimpleName(unescapedName) + "_"
        val modifiers = getModifiers(access)

        var getFieldID = "whatjni::get_field_id"
        var getField = "whatjni::get_field"
        var setField = "whatjni::set_field"
        var target = "(jobject) this"
        if ((access and Opcodes.ACC_STATIC) != 0) {
            getFieldID = "whatjni::get_static_field_id"
            getField = "whatjni::get_static_field"
            setField = "whatjni::set_static_field"
            target = "clazz"
        }

        writeAccess(access)

        if (value != null) {
            writer.write("""
                const static $cppTypeName $escapedName = $value;
                """.replaceIndent("    "))
        } else {
            writer.write("""
                $modifiers$cppTypeName get_$escapedName() {
                    static jclass clazz = whatjni::find_class("$className");
                    static jfieldID field = $getFieldID(clazz, "$unescapedName", "$descriptor");
                
                """.replaceIndent("    "))

            when (type.sort) {
                Type.OBJECT, Type.ARRAY -> writer.write("        return $cppTypeName($getField<jobject>($target, field), whatjni::own_ref);\n")
                else ->                    writer.write("        return $getField<$cppTypeName>($target, field);\n")
            }

            writer.write("    }\n")

            if ((access and Opcodes.ACC_FINAL) == 0) {
                writer.write("""
                        ${modifiers}void set_$escapedName($paramCPPTypeName value) {
                            static jclass clazz = whatjni::find_class("$className");
                            static jfieldID field = $getFieldID(clazz, "$unescapedName", "$descriptor");
                        
                        """.replaceIndent("    "))

                when (type.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.write("        $setField($target, field, (jobject) value.operator->());\n")
                    else ->                    writer.write("        $setField($target, field, value);\n")
                }

                writer.write("    }\n")

                if ((access and Opcodes.ACC_STATIC) == 0) {
                    writer.write("    WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName, put=set_$escapedName)) $modifiers$cppTypeName $escapedName;)\n")
                }
            } else {
                if ((access and Opcodes.ACC_STATIC) == 0) {
                    writer.write("    WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName)) $modifiers$cppTypeName $escapedName;)\n")
                }
            }
        }

        writer.write("\n")

        return null;
    }

    override fun visitMethod(
        access: Int,
        unescapedName: String?,
        descriptor: String?,
        signature: String?,
        exceptions: Array<out String>?
    ): MethodVisitor? {
        unescapedName!!

        if ((access and (Opcodes.ACC_BRIDGE or Opcodes.ACC_SYNTHETIC)) != 0) {
            return null
        }

        val type = Type.getMethodType(descriptor)
        val cppReturnTypeName = makeCPPTypeName(type.returnType, false)
        val escapedName = escapeSimpleName(unescapedName)
        val modifiers = getModifiers(access)

        var getMethodID = "whatjni::get_method_id"
        var callMethod = "whatjni::call_method"
        var target = "(jobject) this"
        if ((access and Opcodes.ACC_STATIC) != 0) {
            getMethodID = "whatjni::get_static_method_id"
            callMethod = "whatjni::call_static_method"
            target = "clazz"
        }
        writeAccess(access)

        writer.write("    ${modifiers}$cppReturnTypeName ${escapedName}(")
        var i = 0
        for (argumentType in type.argumentTypes) {
            val cppArgumentTypeName = makeCPPTypeName(argumentType, true)
            if (i > 0) {
                writer.write(", ")
            }
            writer.write("$cppArgumentTypeName a$i")
            ++i
        }
        writer.write(") {\n")

        writer.write("""
            static jclass clazz = whatjni::find_class("$className");
            static jmethodID method = $getMethodID(clazz, "$unescapedName", "$descriptor");
            return """.replaceIndent("        "))

        when (type.returnType.sort) {
            Type.OBJECT, Type.ARRAY -> writer.write("$cppReturnTypeName($callMethod<jobject>($target, method")
            else ->                    writer.write("$callMethod<$cppReturnTypeName>($target, method")
        }

        i = 0
        for (argumentType in type.argumentTypes) {
            writer.write(", ")
            when (argumentType.sort) {
                Type.OBJECT, Type.ARRAY -> writer.write("(jobject) a$i.operator->()")
                else -> writer.write("a$i")
            }
            ++i
        }

        when (type.returnType.sort) {
            Type.OBJECT, Type.ARRAY -> writer.write("), whatjni::own_ref);\n")
            else ->                    writer.write(");\n")
        }

        writer.write("    }\n\n")

        return null
    }

    override fun visitEnd() {
        writer.write("};\n\n")

        for (i in 0 .. nameParts.size - 2) {
            writer.write("}\n")
        }
    }

    private fun writeAccess(access: Int) {
        if ((access and Opcodes.ACC_PUBLIC) != 0) {
            writer.write("public:\n")
        } else if ((access and Opcodes.ACC_PROTECTED) != 0) {
            writer.write("protected:\n")
        } else if ((access and Opcodes.ACC_PRIVATE) != 0) {
            writer.write("private:\n")
        } else {
            writer.write("public:\n")
        }
    }

    private fun getModifiers(access: Int): String {
        var result = ""
        if ((access and Opcodes.ACC_STATIC) != 0) {
            result += "static "
        }
        return result;
    }

    private fun makeCPPTypeName(type: Type, param: Boolean): String {
         var result = when (type.sort) {
            Type.VOID -> "void"
            Type.BOOLEAN -> "jboolean"
            Type.BYTE -> "jbyte"
            Type.SHORT -> "jshort"
            Type.INT -> "jint"
            Type.LONG -> "jlong"
            Type.CHAR -> "jchar"
            Type.FLOAT -> "jfloat"
            Type.DOUBLE -> "jdouble"
            Type.OBJECT -> "whatjni::ref<${escapeQualifiedName(type.className).replace(".", "::")}>"
            Type.ARRAY -> "whatjni::ref<whatjni::array< " + makeCPPTypeName(type.elementType, false) + " >>"
            else -> "void"
        }

        if (param && (type.sort == Type.OBJECT || type.sort == Type.ARRAY)) {
            result = "const ${result}&";
        }

        return result;
    }
}
