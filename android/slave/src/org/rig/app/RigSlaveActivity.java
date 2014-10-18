package org.rig.app;

public class RigSlaveActivity extends android.app.NativeActivity {

    static {
        System.loadLibrary("png16");
        System.loadLibrary("xml2");
        System.loadLibrary("freetype");
        System.loadLibrary("fontconfig");
        System.loadLibrary("icudata");
        System.loadLibrary("icuuc");
        System.loadLibrary("harfbuzz");
        System.loadLibrary("harfbuzz-icu");
        System.loadLibrary("protobuf-c");
        System.loadLibrary("uv");
    }
}
