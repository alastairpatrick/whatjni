package whatjni

import MethodModel
import org.objectweb.asm.Opcodes
import org.objectweb.asm.Type
import java.beans.Introspector

class PropertyModel(val unescapedName: String,
                    var descriptor: String?,
                    var getMethod: MethodModel?,
                    var setMethod: MethodModel?) {
    val escapedName = escapeSimpleName(unescapedName)

    val type: Type
        get() = Type.getType(descriptor)

    fun merge(otherDescriptor: String, otherGetMethod: MethodModel?, otherSetMethod: MethodModel?) {
        if (!descriptor.equals(otherDescriptor)) {
            descriptor = null
            return
        }

        if (getMethod == null) {
            getMethod = otherGetMethod
        }
        if (setMethod == null) {
            setMethod = otherSetMethod
        }
    }
}
