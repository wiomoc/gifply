package de.wiomoc.gifply;

public class GifProcessor {
    static {
        System.loadLibrary("gifsicle");
    }

    static native void process(String input, String output);
}
