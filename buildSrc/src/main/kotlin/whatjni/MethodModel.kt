import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type
import whatjni.escapeSimpleName

class MethodModel(val access: Int,
                  val unescapedName: String,
                  val descriptor: String,
                  val signature: String?): Comparable<MethodModel> {
    val escapedName = escapeSimpleName(unescapedName)
    val jniName = "jni_" + escapedName + "_" + escapeSimpleName(descriptor)
    val type = Type.getMethodType(descriptor)
    val isConstructor = unescapedName.equals("<init>")

    override fun compareTo(other: MethodModel): Int {
        fun rankAccess(access: Int): Int {
            return when (access and (Opcodes.ACC_PUBLIC or Opcodes.ACC_PROTECTED or Opcodes.ACC_PRIVATE)) {
                Opcodes.ACC_PUBLIC -> 0
                Opcodes.ACC_PROTECTED -> 2
                Opcodes.ACC_PRIVATE -> 3
                else -> 1  // default access
            }
        }

        val compareAccess = rankAccess(access) - rankAccess(other.access)
        if (compareAccess != 0) {
            return compareAccess
        }

        val compareName = unescapedName.compareTo(other.unescapedName)
        if (compareName != 0) {
            return compareName
        }

        return descriptor.compareTo(other.descriptor)
    }
}
