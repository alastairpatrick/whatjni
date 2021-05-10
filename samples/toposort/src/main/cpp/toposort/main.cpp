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

using java::awt::Point;
using java::lang::String;
using java::lang::System;

using namespace whatjni;

int main(int argc, const char **argv) {
    whatjni::initialize_vm(JNI_VERSION_1_8, argc, argv);

    System::get_out()->println(u"Enter some integers then 'done' when finished:"_j);
}
