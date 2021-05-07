package whatjni

import org.objectweb.asm.*

class BaseVisitor() : ClassVisitor(Opcodes.ASM7) {
    val baseClassNames: ArrayList<String> = arrayListOf()

    override fun visit(
        version: Int,
        access: Int,
        name: String?,
        signature: String?,
        superName: String?,
        interfaces: Array<out String>?
    ) {
        interfaces!!

        if (superName != null) {
            baseClassNames.add(superName)
        }
        for (interfaceName in interfaces) {
            baseClassNames.add(interfaceName)
        }
    }
}
