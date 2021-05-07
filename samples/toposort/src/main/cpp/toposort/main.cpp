#include "toposort/main.h"
#include "java/awt/Point.class.h"
#include "java/io/BufferedReader.class.h"
#include "java/io/InputStream.class.h"
#include "java/io/InputStreamReader.class.h"
#include "java/io/PrintStream.class.h"
#include "java/lang/String.class.h"
#include "java/lang/System.class.h"

#if false
#include "mymodule/NonExistent.class.h"
#endif

#include <stdio.h>

#include <algorithm>
#include <string>

using java::awt::Point;
using java::lang::String;
using java::lang::System;

using namespace whatjni;

int main(int argc, char **argv) {
    std::string exePath = argv[0];
    std::replace(exePath.begin(), exePath.end(), '\\', '/');
    size_t idx = exePath.rfind("/build/");

    std::string classPathArg = "-Djava.class.path=" + exePath.substr(0, idx) + "/../mymodule/build/classes/java/main";

    const int numVMOptions = 2;
    static JavaVMOption vmOptions[numVMOptions] = {
        { (char*) classPathArg.c_str() },
        { (char*) "-Xcheck:jni" },
    };

    static JavaVMInitArgs vmArgs = {
        JNI_VERSION_1_8,
        numVMOptions,
        vmOptions,
        false,
    };

    whatjni::create_vm(vmArgs);

    System::get_out()->println(u"Enter some integers then 'done' when finished:"_j);
}
