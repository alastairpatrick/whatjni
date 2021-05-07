#include "foo/main.h"
#include "java/io/PrintStream.class.h"
#include "java/lang/String.class.h"
#include "java/lang/System.class.h"
#include "mymodule/Vector2.class.h"

#if false
#include "mymodule/NonExist.class.h"
#endif

#include <stdio.h>

#include <algorithm>
#include <string>

using java::lang::String;
using java::lang::System;
using mymodule::Vector2;

using whatjni::ref;

int main(int argc, char **argv) {
    std::string exePath = argv[0];
    std::replace(exePath.begin(), exePath.end(), '\\', '/');
    size_t idx = exePath.rfind("/build/");

    std::string classPathArg = "-Djava.class.path=" + exePath.substr(0, idx) + "/../mymodule/build/classes/java/main";

    static JavaVMOption vmOptions[] = {
        { (char*) classPathArg.c_str() },
        { (char*) "-Xcheck:jni" },
    };

    static JavaVMInitArgs vmArgs = {
        JNI_VERSION_1_8,
        (int) std::size(vmOptions),
        vmOptions,
        false,
    };

    whatjni::create_vm(vmArgs);

    ref<String> hi = u"Hello, world";
    hi = hi->toUpperCase();

    System::get_out()->println(hi);
    System::var::get_out()->flush();

    ref<Vector2> v;
    v->var::x;
}
