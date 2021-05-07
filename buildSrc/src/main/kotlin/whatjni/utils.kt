package whatjni

val simpleNameRegex = Regex("""[^/\.]+""")

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
        return "id_" + name;
    }

    if (preserveRegex.matches(name)) {
        return name;
    }

    return "esc_" + name.replace(escapeRegex, {
        "_" + it.value[0].toInt().toString(16)
    })
}
