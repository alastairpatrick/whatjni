package whatjni

import ClassMap
import ClassModel
import FieldModel
import MethodModel
import org.ainslec.picocog.PicoWriter
import org.objectweb.asm.*
import java.io.File
import java.io.FileWriter

class Generator(val generatedDir: File, val classMap: ClassMap, val implementsNative: Boolean): ClassVisitor(Opcodes.ASM7) {
    lateinit var classModel: ClassModel
    val writer = PicoWriter()

    override fun visit(
        version: Int,
        access: Int,
        unescapedName: String,
        signature: String?,
        superName: String?,
        interfaceNames: Array<out String>
    ) {
        val superClass = if (superName != null) classMap.get(superName) else null
        val interfaces = interfaceNames.map { classMap.get(it) }

        classModel = ClassModel(access, unescapedName, signature, superClass, interfaces)
    }

    override fun visitField(
        access: Int,
        unescapedName: String,
        descriptor: String,
        signature: String?,
        value: Any?
    ): FieldVisitor? {
        val field = FieldModel(access, unescapedName, descriptor, signature, value)
        classModel.fields.add(field)
        return null
    }

    override fun visitMethod(
        access: Int,
        unescapedName: String,
        descriptor: String,
        signature: String?,
        exceptions: Array<out String>?
    ): MethodVisitor? {
        val method = MethodModel(access, unescapedName, descriptor, signature)
        classModel.addMethod(method)
        return null
    }

    override fun visitEnd() {
        generate()

        val generatedFile = generatedDir.resolve(classModel.unescapedName + ".class.h")
        generatedFile.parentFile.mkdirs()

        FileWriter(generatedFile).use {
            it.write((writer.toString()))
        }
    }

    fun generate() {
        writeHeader()
        writeForwardDeclarations()
        val namespaceParts = classModel.nameParts.take(classModel.nameParts.size - 1)
        writeOpenNamespace(writer, namespaceParts)
        writeFieldClass()
        writeMethodClass()
        writeCloseNamespace(writer, namespaceParts)
        writeStaticAsserts()
        writeFooter()
    }

    fun writeHeader() {
        writer.writeln("// Don't edit; automatically generated.")
        writer.writeln("#ifndef ${classModel.sentryMacro}")
        writer.writeln("#define ${classModel.sentryMacro}")
        writer.writeln()
        writer.writeln("#include \"whatjni/generated.h\"")

        val superClass = classModel.superClass
        if (superClass != null) {
            writer.writeln("#include \"${superClass.unescapedName}.class.h\"")
        }

        writer.writeln()
    }

    fun writeForwardDeclarations() {
        val declared = sortedSetOf<String>()

        fun recurseType(type: Type) {
            when (type.sort) {
                Type.OBJECT -> declared.add(type.className)
                Type.ARRAY -> recurseType(type.elementType)
                Type.METHOD -> {
                    recurseType(type.returnType)
                    for (argType in type.argumentTypes) {
                        recurseType(argType)
                    }
                }
            }
        }

        val todo = arrayListOf(classModel)
        while (!todo.isEmpty()) {
            val model = todo.removeLast()
            if (declared.add(model.unescapedName.replace("/", "."))) {
                for (iface in model.interfaces) {
                    todo.add(iface)
                }
            }
        }

        for (field in classModel.fields) {
            recurseType(field.type)
        }

        for (method in classModel.methods) {
            recurseType(method.type.returnType)
            for (argType in method.type.argumentTypes) {
                recurseType(argType)
            }
        }

        for (unescapedName in declared) {
            val name = escapeQualifiedName(unescapedName)

            var line = ""
            val parts = name.split(".")
            for (i in 0..parts.size - 2) {
                line += "namespace ${parts[i]} { "
            }
            line += "class ${parts[parts.size - 1]}; "
            for (i in 0..parts.size - 2) {
                line += "} "
            }

            writer.writeln(line)
        }

        writer.writeln()
    }

    fun writeFieldClass() {
        val nameParts = classModel.nameParts
        writer.write("class var_${nameParts[nameParts.size - 1]}")

        val superClass = classModel.superClass
        if (superClass != null) {
            writer.write(": public " + superClass.escapedName)
        }

        writer.writeln_r(" {")

        for (field in classModel.fields) {
            writeField(field)
        }

        writer.writeln_l("};")
        writer.writeln()
    }

    fun writeField(field: FieldModel) {
        field.apply {
            if ((access and Opcodes.ACC_PRIVATE) != 0) {
                if (!implementsNative || !classModel.hasNativeMethods) {
                    return
                }
            }

            val cppType = makeCPPType(type, false)
            val paramCPPType = makeCPPType(type, true)
            val escapedName = escapeSimpleName(unescapedName)
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
                writer.writeln("static constexpr $cppType $escapedName = ${literalValue(value)};")
            } else if (((access and Opcodes.ACC_STATIC) != 0) and ((access and Opcodes.ACC_FINAL) != 0)) {
                writer.writeln_r("static const $cppType& get_$escapedName() {")
                writer.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                writer.writeln("static jfieldID field = whatjni::get_static_field_id(clazz, \"$unescapedName\", \"$descriptor\");")

                when (type.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.writeln("static whatjni::no_destroy<$cppType> value($getField<jobject>($target, field), whatjni::own_ref);")
                    else ->                    writer.writeln("static whatjni::no_destroy<$cppType> value($getField<$cppType>($target, field));")
                }

                writer.writeln("return value.get();")
                writer.writeln_l("}")

                writer.writeln_lr("#if WHATJNI_LANG >= 201703L")
                writer.writeln_r("inline static struct {")
                writer.writeln("const $cppType& operator->() const { return get_$escapedName(); }")
                writer.writeln("operator const $cppType&() const { return get_$escapedName(); }")
                writer.writeln_l("} $escapedName;")
                writer.writeln_lr("#endif")
            } else {
                writer.writeln_r("$modifiers$cppType get_$escapedName() {")
                writer.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                writer.writeln("static jfieldID field = $getFieldID(clazz, \"$unescapedName\", \"$descriptor\");")

                when (type.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.writeln("return $cppType($getField<jobject>($target, field), whatjni::own_ref);")
                    else ->                    writer.writeln("return $getField<$cppType>($target, field);")
                }

                writer.writeln_l("}")

                if ((access and Opcodes.ACC_FINAL) == 0) {
                    writer.writeln_r("${modifiers}void set_$escapedName($paramCPPType value) {")
                    writer.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                    writer.writeln("static jfieldID field = $getFieldID(clazz, \"$unescapedName\", \"$descriptor\");")

                    when (type.sort) {
                        Type.OBJECT, Type.ARRAY -> writer.writeln("$setField($target, field, (jobject) value.operator->());")
                        else ->                    writer.writeln("$setField($target, field, value);")
                    }

                    writer.writeln_l("}")

                    if ((access and Opcodes.ACC_STATIC) == 0) {
                        writer.writeln("WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName, put=set_$escapedName)) $modifiers$cppType $escapedName;)")
                    }
                } else {
                    if ((access and Opcodes.ACC_STATIC) == 0) {
                        writer.writeln("WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName)) $modifiers$cppType $escapedName;)")
                    }
                }
            }

            writer.writeln()
        }
    }

    fun literalValue(value: Any?): String {
        if (value == null) {
            return "nullptr"
        }

        return when (value) {
            is Boolean -> if (value) "true" else "false"
            is Byte, is Short, is Int, is Long -> value.toString()
            is Float -> if (value.isInfinite())
                            if (value > 0) "std::numeric_limits<jfloat>::infinity()" else "-std::numeric_limits<jfloat>::infinity()"
                        else if (value.isNaN()) "std::numeric_limits<jfloat>::quiet_NaN()"
                        else value.toString()
            is Double -> if (value.isInfinite())
                             if (value > 0) "std::numeric_limits<jdouble>::infinity()" else "-std::numeric_limits<jdouble>::infinity()"
                         else if (value.isNaN()) "std::numeric_limits<jdouble>::quiet_NaN()"
                         else value.toString()
            is Char -> "'$value'"
            is String -> """u"$value""""
            else -> "?"
        }
    }

    fun writeMethodClass() {
        val nameParts = classModel.nameParts
        val unqualifiedClassName = nameParts[nameParts.size - 1]
        writer.writeln_r("class $unqualifiedClassName: public var_$unqualifiedClassName {")
        writer.writeln_lr("public:")
        writer.writeln("typedef var_$unqualifiedClassName var;")

        for (method in classModel.methods) {
            writeMethod(method)
        }

        writer.writeln_lr("public:")
        for ((_, property) in classModel.propertiesByName) {
            writeProperty(property)
        }

        writeMethodRegistration()

        writer.writeln_l("};")
        writer.writeln()
    }

    fun writeMethod(method: MethodModel) {
        method.apply {
            if ((access and Opcodes.ACC_PRIVATE) != 0) {
                if (!implementsNative || !classModel.hasNativeMethods) {
                    return
                }
            }

            if (isConstructor && (classModel.access and Opcodes.ACC_ABSTRACT) != 0) {
                return
            }

            val type = Type.getMethodType(descriptor)
            val cppReturnType = makeCPPType(type.returnType, false)
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

            if (isConstructor) {
                writer.write("static whatjni::ref<${classModel.escapedName}> new_object")
            } else {
                writer.write("${modifiers}$cppReturnType ${escapedName}")
            }
            writeParameters(type)
            writer.writeln_r(" {")

            writer.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
            writer.writeln("static jmethodID method = $getMethodID(clazz, \"$unescapedName\", \"$descriptor\");")

            writer.write("return ")
            if (isConstructor) {
                writer.write("whatjni::ref<${classModel.escapedName}>(whatjni::new_object(clazz, method")
            } else {
                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.write("$cppReturnType($callMethod<jobject>($target, method")
                    else -> writer.write("$callMethod<$cppReturnType>($target, method")
                }
            }

            var i = 0
            for (argumentType in type.argumentTypes) {
                writer.write(", ")
                when (argumentType.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.write("(jobject) arg_$i.operator->()")
                    else -> writer.write("arg_$i")
                }
                ++i
            }

            if (isConstructor) {
                writer.writeln("), whatjni::own_ref);")
            } else {
                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.writeln("), whatjni::own_ref);")
                    else -> writer.writeln(");")
                }
            }

            writer.writeln_l("}")
            writer.writeln()

            if (implementsNative && (access and Opcodes.ACC_NATIVE) != 0) {
                writer.writeln_lr("private:")
                writer.write("${modifiers}$cppReturnType native_${escapedName}")
                writeParameters(type)
                writer.writeln(";")

                writer.write("static ${makeJNIType(type.returnType)} ${jniName}(JNIEnv* env")
                if ((access and Opcodes.ACC_STATIC) != 0) {
                    writer.write(", jclass")
                } else {
                    writer.write(", jobject ja_thiz")
                }

                i = 0
                for (argumentType in type.argumentTypes) {
                    writer.write(", ${makeJNIType(argumentType)} ja_${i}")
                    ++i
                }

                writer.writeln_r(") {")
                writer.writeln("whatjni::initialize_thread(env);")

                if ((access and Opcodes.ACC_STATIC) == 0) {
                    writer.writeln("whatjni::ref<${classModel.escapedName}> arg_thiz(ja_thiz, whatjni::own_ref);")
                }

                i = 0
                for (argumentType in type.argumentTypes) {
                    val cppArgumentType = makeCPPType(argumentType, false)
                    when (argumentType.sort) {
                        Type.OBJECT, Type.ARRAY -> writer.writeln("$cppArgumentType arg_$i(ja_${i}, whatjni::own_ref);")
                        else ->                    writer.writeln("$cppArgumentType arg_$i = ja_${i};")
                    }
                    ++i
                }

                writer.write("return ")
                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.write("(jobject) ")
                }
                if ((access and Opcodes.ACC_STATIC) != 0) {
                    writer.write("native_$escapedName(")
                } else {
                    writer.write("arg_thiz->native_$escapedName(")
                }

                i = 0
                for (argumentType in type.argumentTypes) {
                    if (i > 0) {
                        writer.write(", ")
                    }
                    writer.write("arg_$i")
                    ++i
                }
                writer.write(")")

                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> writer.write(".operator->()")
                }

                writer.writeln(";")
                writer.writeln_l("}")
            }
        }
    }

    private fun writeProperty(property: PropertyModel) {
        val getMethod = property.getMethod
        val setMethod = property.setMethod
        val accessors = arrayListOf<String>()
        if (getMethod != null) {
            accessors.add("get=${getMethod.escapedName}")
        }
        if (setMethod != null) {
            accessors.add("put=${setMethod.escapedName}")
        }
        val accessorsString = accessors.joinToString(",")

        val cppType = makeCPPType(property.type, false)
        writer.writeln("WHATJNI_IF_PROPERTY(__declspec(property($accessorsString)) $cppType ${property.escapedName};)")
    }

    private fun writeMethodRegistration() {
        val nativeMethods = classModel.methods.filter { implementsNative && (it.access and Opcodes.ACC_NATIVE) != 0 }
        if (nativeMethods.isEmpty()) {
            return
        }

        writer.writeln_lr("public:")
        writer.writeln_r("static void register_natives() {")

        writer.writeln_r("const static JNINativeMethod methods[] = {")

        for (method in classModel.methods) {
            method.apply {
                if (implementsNative && (access and Opcodes.ACC_NATIVE) != 0) {
                    writer.writeln("{(char*) \"$unescapedName\", (char*) \"$descriptor\", (void*) &${classModel.escapedName}::$jniName },")
                }
            }
        }

        writer.writeln_l("};")
        writer.writeln("const static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
        writer.writeln("whatjni::register_natives(clazz, methods, ${nativeMethods.size});")

        writer.writeln_l("}")
    }

    private fun writeParameters(type: Type) {
        writer.write("(")
        var i = 0
        for (argumentType in type.argumentTypes) {
            val cppArgumentTypeName = makeCPPType(argumentType, true)
            if (i > 0) {
                writer.write(", ")
            }
            writer.write("$cppArgumentTypeName arg_$i")
            ++i
        }
        writer.write(")")
    }

    private fun writeAccess(access: Int) {
        if ((access and Opcodes.ACC_PUBLIC) != 0) {
            writer.writeln_lr("public:")
        } else if ((access and Opcodes.ACC_PROTECTED) != 0) {
            writer.writeln_lr("protected:")
        } else if ((access and Opcodes.ACC_PRIVATE) != 0) {
            writer.writeln_lr("private:")
        } else {
            writer.writeln_lr("public:")
        }
    }

    fun writeStaticAsserts() {
        val staticAsserts = sortedSetOf<String>()
        val todo = arrayListOf(classModel)
        while (!todo.isEmpty()) {
            val classModel = todo.removeLast()
            if (classModel.superClass != null && classModel.superClass.superClass != null) {
                if (staticAsserts.add(classModel.superClass.escapedName)) {
                    todo.add(classModel.superClass)
                }
            }
            for (iface in classModel.interfaces) {
                if (staticAsserts.add(iface.escapedName)) {
                    todo.add(iface)
                }
            }
        }

        writer.writeln_r("namespace whatjni {")
        for (escapedName in staticAsserts) {
            writer.writeln("inline void static_assert_instanceof(${classModel.escapedName}*, $escapedName*) {}")
        }
        writer.writeln_l("}  // namespace whatjni")
        writer.writeln()
    }

    fun writeFooter() {
        writer.writeln("#endif  // ${classModel.sentryMacro}")
    }

    private fun getModifiers(access: Int): String {
        var result = ""
        if ((access and Opcodes.ACC_STATIC) != 0) {
            result += "static "
        }
        return result
    }

    private fun makeJNIType(type: Type): String {
        return when (type.sort) {
            Type.VOID -> "void"
            Type.BOOLEAN -> "jboolean"
            Type.BYTE -> "jbyte"
            Type.SHORT -> "jshort"
            Type.INT -> "jint"
            Type.LONG -> "jlong"
            Type.CHAR -> "jchar"
            Type.FLOAT -> "jfloat"
            Type.DOUBLE -> "jdouble"
            Type.OBJECT -> "jobject"
            Type.ARRAY -> "jobject"
            else -> "void"
        }
    }

    private fun makeCPPType(type: Type, param: Boolean): String {
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
            Type.ARRAY -> "whatjni::ref<whatjni::array< " + makeCPPType(type.elementType, false) + " >>"
            else -> "void"
        }

        if (param && (type.sort == Type.OBJECT || type.sort == Type.ARRAY)) {
            result = "const ${result}&"
        }

        return result
    }
}