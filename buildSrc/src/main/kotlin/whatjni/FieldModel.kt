import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type
import whatjni.escapeSimpleName

class FieldModel(val access: Int,
                 val unescapedName: String,
                 val descriptor: String,
                 val signature: String?,
                 val value: Any?) : Comparable<FieldModel> {
    val escapedName = escapeSimpleName(unescapedName)
    val type = Type.getType(descriptor)

    override fun compareTo(other: FieldModel): Int {
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

        return unescapedName.compareTo(other.unescapedName)
    }
}
