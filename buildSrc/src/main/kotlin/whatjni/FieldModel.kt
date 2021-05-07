import org.objectweb.asm.Type
import whatjni.escapeSimpleName

class FieldModel(val access: Int,
                 val unescapedName: String,
                 val descriptor: String,
                 val signature: String?,
                 val value: Any?) {
    val escapedName = escapeSimpleName(unescapedName)
    val type = Type.getType(descriptor)
}
