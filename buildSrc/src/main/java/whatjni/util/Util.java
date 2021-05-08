package whatjni.util;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
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

    public static void main(String[] args) {
        System.out.println("JVM path is " + getJVMLibraryPath());
    }

    // By the time this function is called, the Java virtual machine shared library must have been loaded, since we're
    // running Java code. This function figures out what its path is.
    public static String getJVMLibraryPath() {
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
    }
}
