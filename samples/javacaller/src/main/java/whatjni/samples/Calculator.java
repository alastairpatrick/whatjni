package whatjni.samples;

public class Calculator {
    public int calculate() {
        return staticAdd(instanceMultiply(2, 3), 1);
    }

    public static native int staticAdd(int a, int b);
    public native int instanceMultiply(int a, int b);
}
