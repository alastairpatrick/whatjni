import org.objectweb.asm.Type
import whatjni.escapeSimpleName

class MethodModel(val access: Int,
                  val unescapedName: String,
                  val descriptor: String,
                  val signature: String?) {
    val escapedName = escapeSimpleName(unescapedName)
    val type = Type.getMethodType(descriptor)
}
