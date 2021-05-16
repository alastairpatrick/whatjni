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
    val headerWriter = PicoWriter()
    val implWriter = PicoWriter()

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

        val generatedHeaderFile = generatedDir.resolve(classModel.unescapedName + ".class.h")
        generatedHeaderFile.parentFile.mkdirs()
        FileWriter(generatedHeaderFile).use {
            it.write((headerWriter.toString()))
        }

        val generatedCPPFile = generatedDir.resolve(classModel.unescapedName + ".class.impl.h")
        FileWriter(generatedCPPFile).use {
            it.write((implWriter.toString()))
        }
    }

    fun generate() {
        writeHeader()
        writeForwardDeclarations()
        val namespaceParts = classModel.nameParts.take(classModel.nameParts.size - 1)
        writeOpenNamespace(headerWriter, namespaceParts)
        writeOpenNamespace(implWriter, namespaceParts)
        writeFieldClass()
        writeMethodClass()
        writeCloseNamespace(headerWriter, namespaceParts)
        writeCloseNamespace(implWriter, namespaceParts)
        writeFooter()
    }

    fun writeHeader() {
        headerWriter.writeln("// Don't edit; automatically generated.")
        headerWriter.writeln("#ifndef ${classModel.headerSentryMacro}")
        headerWriter.writeln("#define ${classModel.headerSentryMacro}")
        headerWriter.writeln()
        headerWriter.writeln("#include \"whatjni/generated.h\"")

        implWriter.writeln("// Don't edit; automatically generated.")
        implWriter.writeln("#ifndef ${classModel.implSentryMacro}")
        implWriter.writeln("#define ${classModel.implSentryMacro}")
        implWriter.writeln()

        val superClass = classModel.superClass
        if (superClass != null) {
            headerWriter.writeln("#include \"${superClass.unescapedName}.class.h\"")
        }
        for (interfaceModel in classModel.interfaces) {
            headerWriter.writeln("#include \"${interfaceModel.unescapedName}.class.h\"")
        }

        headerWriter.writeln()
        implWriter.writeln()
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

            headerWriter.writeln(line)
        }

        headerWriter.writeln()
    }

    fun writeFieldClass() {
        val nameParts = classModel.nameParts
        val fieldClassName = "var_${nameParts[nameParts.size - 1]}"
        headerWriter.write("class WHATJNI_EMPTY_BASES $fieldClassName")

        if (!classModel.baseClassNames.isEmpty()) {
            val baseClasses = classModel.baseClassNames.map { "public " + it }.joinToString(", ")
            headerWriter.write(": $baseClasses")
        }
        headerWriter.writeln_r(" {")

        val accessMask = Opcodes.ACC_PUBLIC or Opcodes.ACC_PROTECTED or Opcodes.ACC_PRIVATE
        var lastAccess = -1
        for (field in classModel.fields) {
            if (field.access and accessMask != lastAccess) {
                lastAccess = field.access and accessMask
                writeAccess(lastAccess)
            }
            writeField(field)
        }

        headerWriter.writeln_l("};")
        headerWriter.writeln()
    }

    fun writeField(field: FieldModel) {
        field.apply {
            if ((access and Opcodes.ACC_PRIVATE) != 0) {
                if (!implementsNative || !classModel.hasNativeMethods) {
                    return
                }
            }

            val cppType = makeCPPType(type)
            val paramCPPType = makeCPPType(type)
            val escapedName = escapeSimpleName(unescapedName)
            val modifiers = getModifiers(access)

            var getFieldID = "whatjni::get_field_id"
            var getField = "whatjni::get_field"
            var setField = "whatjni::set_field"
            var target = "reinterpret_cast<jobject>(this)"
            if ((access and Opcodes.ACC_STATIC) != 0) {
                getFieldID = "whatjni::get_static_field_id"
                getField = "whatjni::get_static_field"
                setField = "whatjni::set_static_field"
                target = "clazz"
            }

            if (value != null) {
                headerWriter.writeln("static constexpr $cppType $escapedName = ${literalValue(value)};")
            } else if (((access and Opcodes.ACC_STATIC) != 0) and ((access and Opcodes.ACC_FINAL) != 0)) {
                headerWriter.writeln("static $cppType get_$escapedName();")

                implWriter.writeln_r("$cppType var_${classModel.escapedClassName}::get_$escapedName() {")
                implWriter.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                implWriter.writeln("static jfieldID field = whatjni::get_static_field_id(clazz, \"$unescapedName\", \"$descriptor\");")

                when (type.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.writeln("static $cppType value = ($cppType) whatjni::new_global_ref($getField<jobject>($target, field));")
                    else ->                    implWriter.writeln("static $cppType value = $getField<$cppType>($target, field);")
                }

                implWriter.writeln("return value;")
                implWriter.writeln_l("}")

                headerWriter.writeln_r("struct access_$escapedName {")
                headerWriter.writeln("$cppType operator->() const { return get_$escapedName(); }")
                headerWriter.writeln("operator $cppType() const { return get_$escapedName(); }")
                headerWriter.writeln_l("};")
                headerWriter.writeln("static const access_$escapedName $escapedName;")

                implWriter.writeln("const var_${classModel.escapedClassName}::access_$escapedName var_${classModel.escapedClassName}::$escapedName;")
            } else {
                headerWriter.writeln("$modifiers$cppType get_$escapedName();")

                implWriter.writeln_r("$cppType var_${classModel.escapedClassName}::get_$escapedName() {")
                implWriter.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                implWriter.writeln("static jfieldID field = $getFieldID(clazz, \"$unescapedName\", \"$descriptor\");")

                when (type.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.writeln("return ($cppType) $getField<jobject>($target, field);")
                    else ->                    implWriter.writeln("return $getField<$cppType>($target, field);")
                }

                implWriter.writeln_l("}")

                if ((access and Opcodes.ACC_FINAL) == 0) {
                    headerWriter.writeln("${modifiers}void set_$escapedName($paramCPPType value);")

                    implWriter.writeln_r("void var_${classModel.escapedClassName}::set_$escapedName($paramCPPType value) {")
                    implWriter.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
                    implWriter.writeln("static jfieldID field = $getFieldID(clazz, \"$unescapedName\", \"$descriptor\");")

                    when (type.sort) {
                        Type.OBJECT, Type.ARRAY -> implWriter.writeln("$setField($target, field, reinterpret_cast<jobject>(value));")
                        else ->                    implWriter.writeln("$setField($target, field, value);")
                    }

                    implWriter.writeln_l("}")

                    if ((access and Opcodes.ACC_STATIC) == 0) {
                        headerWriter.writeln("WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName, put=set_$escapedName)) $modifiers$cppType $escapedName;)")
                    }
                } else {
                    if ((access and Opcodes.ACC_STATIC) == 0) {
                        headerWriter.writeln("WHATJNI_IF_PROPERTY(__declspec(property(get=get_$escapedName)) $modifiers$cppType $escapedName;)")
                    }
                }
            }

            headerWriter.writeln()
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
        val superClassName = "var_$unqualifiedClassName"
        headerWriter.writeln_r("class $unqualifiedClassName: public $superClassName {")

        headerWriter.writeln_lr("public:")

        headerWriter.writeln("typedef var_$unqualifiedClassName var;")

        val accessMask = Opcodes.ACC_PUBLIC or Opcodes.ACC_PROTECTED or Opcodes.ACC_PRIVATE
        var lastAccess = -1
        for (method in classModel.methods) {
            if (method.access and accessMask != lastAccess) {
                lastAccess = method.access and accessMask
                writeAccess(lastAccess)
            }
            writeMethod(method)
        }

        headerWriter.writeln_lr("public:")
        for ((_, property) in classModel.propertiesByName) {
            writeProperty(property)
        }

        writeMethodRegistration()
        writeDescriptor()

        headerWriter.writeln_l("};")
        headerWriter.writeln("static_assert(std::is_standard_layout<$unqualifiedClassName>::value, \"Must be standard layout.\");")
        headerWriter.writeln()
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
            val cppReturnType = makeCPPType(type.returnType)
            val escapedName = escapeSimpleName(unescapedName)
            val modifiers = getModifiers(access)

            var getMethodID = "whatjni::get_method_id"
            var callMethod = "whatjni::call_method"
            var target = "reinterpret_cast<jobject>(this)"
            if ((access and Opcodes.ACC_STATIC) != 0) {
                getMethodID = "whatjni::get_static_method_id"
                callMethod = "whatjni::call_static_method"
                target = "clazz"
            }

            if (isConstructor) {
                headerWriter.write("static ${classModel.escapedName}* new_object")
            } else {
                headerWriter.write("${modifiers}$cppReturnType ${escapedName}")
            }
            writeParameters(headerWriter, type)
            headerWriter.writeln(";")

            if (isConstructor) {
                implWriter.write("${classModel.escapedName}* ${classModel.escapedClassName}::new_object")
            } else {
                implWriter.write("$cppReturnType ${classModel.escapedClassName}::${escapedName}")
            }
            writeParameters(implWriter, type)
            implWriter.writeln_r(" {")

            implWriter.writeln("static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
            implWriter.writeln("static jmethodID method = $getMethodID(clazz, \"$unescapedName\", \"$descriptor\");")

            implWriter.write("return ")
            if (isConstructor) {
                implWriter.write("(${classModel.escapedName}*) whatjni::new_object(clazz, method")
            } else {
                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.write("($cppReturnType) $callMethod<jobject>($target, method")
                    else ->                    implWriter.write("$callMethod<$cppReturnType>($target, method")
                }
            }

            var i = 0
            for (argumentType in type.argumentTypes) {
                implWriter.write(", ")
                when (argumentType.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.write("reinterpret_cast<jobject>(arg_$i)")
                    else -> implWriter.write("arg_$i")
                }
                ++i
            }
            implWriter.writeln(");")

            implWriter.writeln_l("}")
            implWriter.writeln()

            if (implementsNative && (access and Opcodes.ACC_NATIVE) != 0) {
                headerWriter.writeln_lr("private:")
                headerWriter.write("${modifiers}$cppReturnType native_${escapedName}")
                writeParameters(headerWriter, type)
                headerWriter.writeln(";")

                var jniParameters = "JNIEnv* env"
                if ((access and Opcodes.ACC_STATIC) != 0) {
                    jniParameters += ", jclass"
                } else {
                    jniParameters += ", jobject arg_thiz"
                }

                i = 0
                for (argumentType in type.argumentTypes) {
                    jniParameters += ", ${makeJNIType(argumentType)} arg_${i}"
                    ++i
                }

                val jniReturnType = makeJNIType(type.returnType)
                headerWriter.writeln("static $jniReturnType $jniName($jniParameters);")

                implWriter.writeln_r("$jniReturnType ${classModel.escapedClassName}::$jniName($jniParameters) {")
                implWriter.writeln("whatjni::initialize_thread(env);")
                implWriter.writeln_r("try {")

                implWriter.write("return ")
                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.write("reinteret_cast<jobject>(")
                }

                if ((access and Opcodes.ACC_STATIC) != 0) {
                    implWriter.write("native_$escapedName(")
                } else {
                    implWriter.write("((${classModel.escapedClassName}*) arg_thiz)->native_$escapedName(")
                }

                i = 0
                for (argumentType in type.argumentTypes) {
                    if (i > 0) {
                        implWriter.write(", ")
                    }
                    when (type.returnType.sort) {
                        Type.OBJECT, Type.ARRAY -> implWriter.write("(${makeCPPType(argumentType)}) ")
                    }
                    implWriter.write("arg_$i")
                    ++i
                }

                when (type.returnType.sort) {
                    Type.OBJECT, Type.ARRAY -> implWriter.write(")")
                }
                implWriter.writeln(");")

                implWriter.writeln_lr("} catch (const whatjni::jvm_exception& e) {")
                implWriter.writeln("e.schedule();")
                implWriter.writeln_lr("} catch (const std::exception& e) {")
                implWriter.writeln("schedule_new_runtime_exception(e.what());")
                implWriter.writeln_lr("} catch (...) {")
                implWriter.writeln("schedule_new_runtime_exception(\"Unknown exception type\");")
                implWriter.writeln_l("}")

                implWriter.writeln_l("}")
                implWriter.writeln()
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

        val cppType = makeCPPType(property.type)
        headerWriter.writeln("WHATJNI_IF_PROPERTY(__declspec(property($accessorsString)) $cppType ${property.escapedName};)")
    }

    private fun writeMethodRegistration() {
        headerWriter.writeln_lr("public:")
        headerWriter.writeln("static void initialize_class();")

        implWriter.writeln_r("void ${classModel.escapedClassName}::initialize_class() {")

        val nativeMethods = classModel.methods.filter { implementsNative && (it.access and Opcodes.ACC_NATIVE) != 0 }
        if (!nativeMethods.isEmpty()) {
            implWriter.writeln_r("const static JNINativeMethod methods[] = {")

            for (method in classModel.methods) {
                method.apply {
                    if (implementsNative && (access and Opcodes.ACC_NATIVE) != 0) {
                        implWriter.writeln("{(char*) \"$unescapedName\", (char*) \"$descriptor\", (void*) &${classModel.escapedName}::$jniName },")
                    }
                }
            }

            implWriter.writeln_l("};")
            implWriter.writeln("const static jclass clazz = whatjni::find_class(\"${classModel.unescapedName}\");")
            implWriter.writeln("whatjni::register_natives(clazz, methods, ${nativeMethods.size});")
        }

        implWriter.writeln_l("}")
    }

    private fun writeDescriptor() {
        headerWriter.writeln_lr("public:")
        headerWriter.writeln("static std::string get_signature();")
        implWriter.writeln_r("std::string ${classModel.escapedClassName}::get_signature() {")
        implWriter.writeln("return \"L${classModel.unescapedName};\";")
        implWriter.writeln_l("}")
    }

    private fun writeParameters(writer: PicoWriter, type: Type) {
        writer.write("(")
        var i = 0
        for (argumentType in type.argumentTypes) {
            val cppArgumentTypeName = makeCPPType(argumentType)
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
            headerWriter.writeln_lr("public:")
        } else if ((access and Opcodes.ACC_PROTECTED) != 0) {
            headerWriter.writeln_lr("protected:")
        } else if ((access and Opcodes.ACC_PRIVATE) != 0) {
            headerWriter.writeln_lr("private:")
        } else {
            headerWriter.writeln_lr("public:")
        }
    }

    fun writeFooter() {
        headerWriter.writeln("#endif  // ${classModel.headerSentryMacro}")
        implWriter.writeln("#endif  // ${classModel.implSentryMacro}")
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

    private fun makeCPPType(type: Type): String {
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
            Type.OBJECT -> "${escapeQualifiedName(type.className).replace(".", "::")}*"
            Type.ARRAY -> "whatjni::array< " + makeCPPType(type.elementType) + " >*"
            else -> "void"
        }
    }
}