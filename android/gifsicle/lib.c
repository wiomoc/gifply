#include <android/log.h>
#include <jni.h>
#include <lcdfgif/gif.h>

extern int main(int argc, char *argv[]);
JNIEXPORT void JNICALL Java_de_wiomoc_giffy_GifProcessor_process(
    JNIEnv *env, jclass clazz, jstring input, jstring output) {
  const char *input_str = (*env)->GetStringUTFChars(env, input, 0);
  const char *output_str = (*env)->GetStringUTFChars(env, output, 0);
  char *argv[] = {"gifsicle",
                   "--resize",
                   "240x240",
                   "--colors",
                   "256",
                   "--lossy",
                   "-o",
                   output_str,
                   input_str};

   main(9, argv);
}
