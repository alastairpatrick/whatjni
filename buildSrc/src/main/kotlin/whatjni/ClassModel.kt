import org.objectweb.asm.Opcodes
import whatjni.PropertyModel
import whatjni.escapeQualifiedName
import java.beans.Introspector

val getAccessorRegex = Regex("^(?:get|is)(.+)$")
val setAccessorRegex = Regex("^set(.+)$")

class ClassModel(val access: Int,
                 val unescapedName: String,
                 val signature: String?,
                 val superClass: ClassModel?,
                 val interfaces: List<ClassModel>) {
    val escapedName = escapeQualifiedName(unescapedName).replace("/", "::")
    val nameParts = escapedName.split("::")
    val escapedClassName = nameParts[nameParts.size - 1]
    val headerSentryMacro = nameParts.joinToString(separator = "_") + "_SENTRY_"
    val implSentryMacro = nameParts.joinToString(separator = "_") + "_IMPL_SENTRY_"
    val isInterface = (access and Opcodes.ACC_INTERFACE) != 0
    val baseClassNames = sortedSetOf<String>()

    val fields = sortedSetOf<FieldModel>()
    val methods = sortedSetOf<MethodModel>()
    val methodsByName = hashMapOf<String, MethodModel>()
    val propertiesByName = sortedMapOf<String, PropertyModel>()

    var hasNativeMethods = false

    init {
        if (!isInterface) {
            if (superClass != null) {
                baseClassNames.add(superClass.escapedName)
            }

            val baseInterfaces = sortedSetOf<String>()
            var baseClass = superClass
            while (baseClass != null) {
                baseInterfaces.addAll(baseClass.baseClassNames)
                baseClass = baseClass.superClass
            }

            val todo = arrayListOf(this)
            while (!todo.isEmpty()) {
                val classModel = todo.removeLast()
                for (it in classModel.interfaces) {
                    if (baseInterfaces.contains(it.escapedName)) {
                        continue
                    }
                    if (baseClassNames.add(it.escapedName)) {
                        todo.add(it)
                    }
                }
            }
        }
    }

    fun addMethod(method: MethodModel) {
        // Skip bridge methods generated by compiler
        if ((method.access and Opcodes.ACC_BRIDGE) != 0) {
            return
        }

        // Skip static constructors.
        if (method.unescapedName.equals("<clinit>")) {
            return
        }

        if ((method.access and Opcodes.ACC_NATIVE) != 0) {
            hasNativeMethods = true
        } else if (!method.isConstructor) {
            // Skip method overrides unless native
            var model = superClass
            while (model != null) {
                if (model.methods.contains(method)) {
                    return
                }
                model = model.superClass
            }
        }

        methods.add(method)
        methodsByName[unescapedName] = method

        if ((method.access and Opcodes.ACC_STATIC) == 0) {
            when (method.type.argumentTypes.size) {
                0 -> {
                    val match = getAccessorRegex.matchEntire(method.unescapedName)
                    if (match != null) {
                        val name = Introspector.decapitalize(match.groupValues[1])
                        addProperty(name, method.type.returnType.descriptor, method, null)
                    }
                }
                1 -> {
                    val match = setAccessorRegex.matchEntire(method.unescapedName)
                    if (match != null) {
                        val name = Introspector.decapitalize(match.groupValues[1])
                        addProperty(name, method.type.argumentTypes[0].descriptor, null, method)
                    }
                }
            }
        }
    }

    fun addProperty(unescapedName: String, descriptor: String, getMethod: MethodModel?, setMethod: MethodModel?) {
        if (methodsByName.containsKey(unescapedName)) {
            return
        }

        var property = propertiesByName[unescapedName]
        if (property == null) {
            property = PropertyModel(unescapedName, descriptor, getMethod, setMethod)
            propertiesByName[unescapedName] = property
        } else {
            property.merge(descriptor, getMethod, setMethod)
        }

        if (property.descriptor == null) {
            return
        }

        var model = superClass
        while (model != null) {
            val baseProperty = model.propertiesByName.get(unescapedName)
            if (baseProperty != null && property.descriptor.equals(baseProperty.descriptor)) {
                property.merge(property.descriptor!!, baseProperty.getMethod, baseProperty.setMethod)
            }
            model = model.superClass
        }

        propertiesByName[unescapedName] = property
    }
}
