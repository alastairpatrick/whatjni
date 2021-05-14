package whatjni

import org.ainslec.picocog.PicoWriter
import org.objectweb.asm.Opcodes

val simpleNameRegex = Regex("""[^/.]+""")

fun escapeQualifiedName(name: String): String {
    return name.replace(simpleNameRegex, {
        escapeSimpleName(it.value)
    })
}

val preserveRegex = Regex("^([A-Z][A-Z0-9_]*)|([A-Za-z][A-Za-z0-9]*)$")
val escapeRegex = Regex("[^A-Za-z0-9]")
val keywordRegex = Regex(
    """alignas|alignof|and|and_eq|asm|atomic_cancel|atomic_commit|atomic_noexcept|auto
|bitand|bitor|bool|break
|case|catch|char|char8_t|char16_t|char32_t|class|compl|concept|const|consteval|constexpr|constinit|const_cast|continue
|co_await""".replace("\n", ""))

fun escapeSimpleName(name: String): String {
    if (keywordRegex.matches(name)) {
        return "id_" + name
    }

    if (preserveRegex.matches(name)) {
        return name
    }

    return "esc_" + name.replace(escapeRegex, {
        "_" + it.value[0].toInt().toString(16)
    })
}

fun writeOpenNamespace(writer: PicoWriter, nameParts: List<String>) {
    for (i in 0 .. nameParts.size - 1) {
        writer.writeln("namespace ${nameParts[i]} {")
    }
    writer.writeln()
}

fun writeCloseNamespace(writer: PicoWriter, nameParts: List<String>) {
    for (i in nameParts.size - 1 downTo 0) {
        writer.writeln("}  // namespace ${nameParts[i]}")
    }
    writer.writeln()
}

fun rankAccess(access: Int): Int {
    return when (access and (Opcodes.ACC_PUBLIC or Opcodes.ACC_PROTECTED or Opcodes.ACC_PRIVATE)) {
        Opcodes.ACC_PUBLIC -> 0
        Opcodes.ACC_PROTECTED -> 2
        Opcodes.ACC_PRIVATE -> 3
        else -> 1  // default access
    }
}
