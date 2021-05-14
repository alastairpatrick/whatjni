import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type
import whatjni.escapeSimpleName
import whatjni.rankAccess

class FieldModel(val access: Int,
                 val unescapedName: String,
                 val descriptor: String,
                 val signature: String?,
                 val value: Any?) : Comparable<FieldModel> {
    val escapedName = escapeSimpleName(unescapedName)
    val type = Type.getType(descriptor)

    override fun compareTo(other: FieldModel): Int {
        val compareAccess = rankAccess(access) - rankAccess(other.access)
        if (compareAccess != 0) {
            return compareAccess
        }

        return unescapedName.compareTo(other.unescapedName)
    }
}
