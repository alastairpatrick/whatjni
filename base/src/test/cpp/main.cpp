#include "gtest/gtest.h"
#include "whatjni/base.h"
#include <direct.h>

#include <algorithm>
#include <string>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

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

    try {
        return RUN_ALL_TESTS();
    } catch (whatjni::JVMException) {
        whatjni::print_exception();
        throw;
    }
}
