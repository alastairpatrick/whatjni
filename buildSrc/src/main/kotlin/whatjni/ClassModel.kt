import whatjni.escapeQualifiedName

class ClassModel(val access: Int,
                 val unescapedName: String,
                 val signature: String?,
                 val superClass: ClassModel?,
                 val interfaces: List<ClassModel>) {
    val escapedName = escapeQualifiedName(unescapedName).replace("/", "::")
    val nameParts = escapedName.split("::")
    val escapedClassName = nameParts[nameParts.size - 1]
    val sentryMacro = nameParts.joinToString(separator = "_") + "_SENTRY_H_"

    val fields = ArrayList<FieldModel>()
    val methods = HashMap<Pair<String, String>, MethodModel>()

    var hasNativeMethods = false
}
