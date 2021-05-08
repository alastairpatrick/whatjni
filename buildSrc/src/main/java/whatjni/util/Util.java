package whatjni.util;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.win32.StdCallLibrary;
import com.sun.jna.win32.W32APIOptions;

// Even if one knows the pach to a JDK, it is difficult to locate the path to the virtual machine shared library, the
// one that implements the Invocation API. Different JDKs put it in all kinds of different directories, just to name a
// few:
//
//   $JAVA_HOME/bin/client
//   $JAVA_HOME/bin/server
//   $JAVA_HOME/jre/bin/server
//   $JAVA_HOME/jre/bin/server
//   $JAVA_HOME/lib/server
//   $JAVA_HOME/jre/lib/amd64/server
//
// To make matters worse, JDKs invent knew schemes for hiding the shared library over time, making it is especially
// difficult to write software against the Invocation API that is forward compatible.
//
// This is not a problem if one redistributes the Java runtime, since it is easy to figure out where the shared
// library is in _that particular_ Java runtime and write the dependent code accordingly.
//
// It _is_ a problem if one authors a library and cannot know which JDK other devs might use.

public class Util {
    public interface Kernel32Library extends StdCallLibrary {
        Kernel32Library INSTANCE = (Kernel32Library) Native.load("kernel32", Kernel32Library.class, W32APIOptions.DEFAULT_OPTIONS);

        Pointer LoadLibrary(String filename);
        int GetModuleFileName(Pointer module, char[] path, IntByReference size);
        void FreeLibrary(Pointer module);
    }

    @Structure.FieldOrder({ "fname", "fbase", "sname", "saddr" })
    public static class DL_info extends Structure {
        public String fname;
        public Pointer fbase;
        public String sname;
        public Pointer saddr;
    }

    public interface DLLibrary extends Library {
        DLLibrary INSTANCE = (DLLibrary) Native.load("dl", DLLibrary.class);

        Pointer dlopen(String path, int flag);
        Pointer dlsym(Pointer module, String symbol);
        int dladdr(Pointer addr, DL_info info);
        void dlclose(Pointer module);
    }

    public static void main(String[] args) {
        System.out.println("JVM path is " + getJVMLibraryPath());
    }

    // By the time this function is called, the Java virtual machine shared library must have been loaded, since we're
    // running Java code. This function figures out what its path is.
    public static String getJVMLibraryPath() {
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.startsWith("win")) {
            Pointer module = Kernel32Library.INSTANCE.LoadLibrary("jvm.dll");
            if (module == null) {
                throw new RuntimeException("LoadLibrary failed.");
            }

            try {
                char[] pathChars = new char[256];
                IntByReference pathSize = new IntByReference(256);
                if (Kernel32Library.INSTANCE.GetModuleFileName(module, pathChars, pathSize) == 0) {
                    throw new RuntimeException("GetModuleFileName failed.");
                }

                return new String(pathChars, 0, pathSize.getValue());
            } finally {
                Kernel32Library.INSTANCE.FreeLibrary(module);
            }
        } else {
            String moduleName = osName.startsWith("mac") ? "libjvm.dylib" : "libjvm.so";

            Pointer module = DLLibrary.INSTANCE.dlopen(moduleName, 2 /* RTTLD_NOW */);
            if (module == null) {
                throw new RuntimeException("dlopen failed.");
            }

            try {
                Pointer addr = DLLibrary.INSTANCE.dlsym(module, "JNI_CreateJavaVM");
                if (addr == null) {
                    throw new RuntimeException("dlsym failed.");
                }

                DL_info info = new DL_info();
                if (DLLibrary.INSTANCE.dladdr(addr, info) == 0) {
                    throw new RuntimeException("dladdr failed.");
                }

                return info.fname;
            } finally {
                DLLibrary.INSTANCE.dlclose(module);
            }
        }
    }
}
