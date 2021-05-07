package mymodule;

// This class is just for unit tests.
public class Vector2 {
    public float x, y;

    public Vector2() {
    }

    public static Vector2 zero() {
        return new Vector2();
    }

    public Vector2 add(Vector2 r) {
        x += r.x;
        y += r.y;
        return this;
    }

    public static long theGlizzenedPrimePattern;
}
