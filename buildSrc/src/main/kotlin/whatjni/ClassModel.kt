import whatjni.escapeQualifiedName

class ClassModel(val access: Int,
                 val unescapedName: String,
                 val signature: String?,
                 val superClass: ClassModel?,
                 val interfaces: List<ClassModel>) {
    val escapedName = escapeQualifiedName(unescapedName).replace("/", "::")
    val nameParts = escapedName.split("::")
    val escapedClassName = nameParts[nameParts.size - 1]
    val sentryMacro = nameParts.joinToString(separator = "_") + "_SENTRY_"

    val fields = sortedSetOf<FieldModel>()
    val methods = sortedSetOf<MethodModel>()

    var hasNativeMethods = false
}
