#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>
#include <pthread.h>

#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/GstreamerMod", __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, "RemoteClient/GstreamerMod", __VA_ARGS__)
GST_DEBUG_CATEGORY_STATIC(debug_category);
#define GST_CAT_DEFAULT debug_category

/*
 * These macros provide a way to store the native pointer to CustomData, which might be 32 or 64 bits, into
 * a jlong, which is always 64 bits, without warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField(env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField(env, thiz, fieldID, (jlong)data)
#else
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField(env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField(env, thiz, fieldID, (jlong)(jint)data)
#endif

// Structure to contain all our information, so we can pass it to callbacks
typedef struct _CustomData {
  jobject app;           // Application instance, used to call its methods. A global reference is kept. */
  GstElement *pipeline;  // The running pipeline */
  GMainContext *context; // GLib context used to run the main loop */
  GMainLoop *main_loop;  // GLib main loop */
  gboolean initialized;  // To avoid informing the UI multiple times about the initialization */
  jstring host;
  int port;
} CustomData;

// These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_gstreamer_initialized_method_id;

// Register this thread with the VM */
static JNIEnv *attach_current_thread() {
  JNIEnv *env;
  JavaVMAttachArgs args;

  LOG_DEBUG("Attaching thread %p", g_thread_self());
  args.version = JNI_VERSION_1_4;
  args.name = NULL;
  args.group = NULL;

  if ((*java_vm)->AttachCurrentThread(java_vm, &env, &args) < 0) {
    LOG_ERROR("Failed to attach current thread");
    return NULL;
  }

  return env;
}

// Unregister this thread from the VM */
static void detach_current_thread (void *env) {
  LOG_DEBUG("Detaching thread %p", g_thread_self());
  (*java_vm)->DetachCurrentThread (java_vm);
}

// Retrieve the JNI environment for this thread */
static JNIEnv *get_jni_env() {
  JNIEnv *env;

  if ((env = pthread_getspecific(current_jni_env)) == NULL) {
    env = attach_current_thread();
    pthread_setspecific(current_jni_env, env);
  }

  return env;
}

// Check if all conditions are met to report GStreamer as initialized.
// These conditions will change depending on the application */
static void check_initialization_complete (CustomData *data) {
  JNIEnv *env = get_jni_env();
  if (!data->initialized && data->main_loop) {
    LOG_DEBUG ("Initialization complete, notifying application. main_loop:%p", data->main_loop);
    (*env)->CallVoidMethod(env, data->app, on_gstreamer_initialized_method_id);
    if ((*env)->ExceptionCheck(env)) {
      LOG_ERROR ("Failed to call Java method");
      (*env)->ExceptionClear(env);
    }
    data->initialized = TRUE;
  }
}

// Main method
static void * create_pipeline(void * in_data) {
  int argc = 0;
  char ** argv = NULL;

  CustomData * data = (CustomData *) in_data;
  GstBus * bus;
  GstElement * source;
  GstElement * oggdemux;
  GstElement * vorbisdecoder;
  GstElement * audioconvert;
  GstElement * sink;
  GError * error = NULL;

  /*
  if (!gst_init_check(&argc, &argv, &error)) {
    LOG_ERROR("Error initializing gstreamer: %s\n", error->message);
    g_error_free(error);
    return NULL;
  }
  */

  // Create our own GLib Main Context to run the main loop
  data->context = g_main_context_new();
  g_main_context_push_thread_default(data->context);

  data->pipeline = gst_parse_launch("udpsrc port=52001 ! \
                                     vorbisdec ! audioconvert ! audioresample ! openslessink",
                                     &error);
  /*
  data->pipeline = gst_parse_launch("audiotestsrc ! audioconvert ! audioresample ! autoaudiosink",
                                     &error);
                                     */
  if (error) {
    LOG_ERROR("Unable to build pipeline: %s", error->message);
    g_error_free(error);
    return NULL;
  }

  /*
  data->pipeline = gst_pipeline_new("streaming-client");
  source = gst_element_factory_make("tcpclientsrc", "tcp source");
  oggdemux = gst_element_factory_make("oggdemux", "ogg multiplexer");
  vorbisdecoder = gst_element_factory_make("vorbisdec", "vorbis decoder");
  audioconvert = gst_element_factory_make("audioconvert", "converter");
  sink = gst_element_factory_make("autoaudiosink", "sink");
  if (!data->pipeline || !source || !oggdemux || !vorbisdecoder || !audioconvert || !sink) {
    LOG_ERROR("Error creating gstreamer pipeline.\n");
    LOG_DEBUG("pipeline : %p, source : %p, oggdemux : %p, vorbisdec : %p,\
              audioconvert : %p, sink : %p\n",
              data->pipeline, source, oggdemux, vorbisdecoder, audioconvert, sink);
    return NULL;
  }

  // TODO: Dynamically find out the host:port
  g_object_set(G_OBJECT(source), "host", "192.168.0.11", "port", 52001, NULL);

  // Add the elements to the pipeline
  gst_bin_add_many(GST_BIN(data->pipeline), source, oggdemux, vorbisdecoder,
                   audioconvert, sink, NULL);

  // Link them
  gst_element_link_many(source, oggdemux, vorbisdecoder, audioconvert, sink,
                        NULL);

  */
  // Create a GLib Main Loop and set it to run
  LOG_DEBUG("Entering main loop... (CustomData:%p)", data);
  data->main_loop = g_main_loop_new(data->context, FALSE);
  check_initialization_complete(data);
  g_main_loop_run(data->main_loop);
  LOG_DEBUG("Exited main loop");
  g_main_loop_unref(data->main_loop);
  data->main_loop = NULL;

  // Free resources
  g_main_context_pop_thread_default(data->context);
  g_main_context_unref(data->context);
  gst_element_set_state(data->pipeline, GST_STATE_NULL);
  gst_object_unref(data->pipeline);

  return NULL;
}
/*
 * Java Bindings
 */
static void gst_native_init(JNIEnv * env, jobject thiz, jstring host, jint port) {
  __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/Gstreamer", "native_init");
  CustomData * data = g_new0(CustomData, 1);
  SET_CUSTOM_DATA(env, thiz, custom_data_field_id, data);
  GST_DEBUG_CATEGORY_INIT(debug_category, "RemoteClient/Gstreamer-pipeline",
                          0, "Android Gstreamer pipeline");
  gst_debug_set_threshold_for_name("Gstreamer-pipeline", GST_LEVEL_DEBUG);
  //gst_debug_set_default_threshold(GST_LEVEL_DEBUG);
  LOG_DEBUG("Created CustomData at %p", data);
  data->app = (*env)->NewGlobalRef(env, thiz);
  data->host = host;
  data->port = port;
  LOG_DEBUG("Created GlobalRef for app object at %p", data->app);
  pthread_create(&gst_app_thread, NULL, &create_pipeline, data);
}

static void gst_native_finalize(JNIEnv * env, jobject thiz) {
  CustomData * data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
  if (!data) return;
  LOG_DEBUG("Quitting main loop...");
  g_main_loop_quit(data->main_loop);
  LOG_DEBUG("Waiting for thread to finish...");
  pthread_join(gst_app_thread, NULL);
  LOG_DEBUG("Deleting GlobalRef for app object at %p", data->app);
  (*env)->DeleteGlobalRef(env, data->app);
  LOG_DEBUG("Freeing CustomData at %p", data);
  g_free(data);
  SET_CUSTOM_DATA(env, thiz, custom_data_field_id, NULL);
  LOG_DEBUG("Done finalizing");
}

// Set pipeline to PLAYING state */
static void gst_native_play(JNIEnv* env, jobject thiz) {
  CustomData * data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
  if (!data) return;
  LOG_DEBUG("Setting state to PLAYING");
  gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}

// Set pipeline to PAUSED state */
static void gst_native_pause(JNIEnv* env, jobject thiz) {
  CustomData * data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
  if (!data) return;
  LOG_DEBUG("Setting state to PAUSED");
  gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
}

// Static initializer
static jboolean gst_native_class_init(JNIEnv* env, jclass class) {
  __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/Gstreamer", "gst_native_class_init");
  custom_data_field_id = (*env)->GetFieldID(env, class, "native_custom_data", "J");
  set_message_method_id = (*env)->GetMethodID(env, class, "setMessage", "(Ljava/lang/String;)V");
  on_gstreamer_initialized_method_id = (*env)->GetMethodID(env, class, "onGStreamerInitialized", "()V");

  if (!custom_data_field_id || !set_message_method_id || !on_gstreamer_initialized_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, "RemoteClient/Gstreamer",
                        "The calling class does not implement all necessary interface methods");
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

static JNINativeMethod native_methods[] = {
  { "nativeInit", "(Ljava/lang/String;I)V", (void *) gst_native_init},
  { "nativeFinalize", "()V", (void *) gst_native_finalize},
  { "nativePlay", "()V", (void *) gst_native_play},
  { "nativePause", "()V", (void *) gst_native_pause},
  { "nativeClassInit", "()Z", (void *) gst_native_class_init}
};

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env = NULL;

  java_vm = vm;

  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "RemoteClient/Gstreamer", "Could not retrieve JNIEnv");
    return 0;
  }
  jclass class = (*env)->FindClass(env, "com/rcatolino/remoteclient/PlaybackService");
  (*env)->RegisterNatives(env, class, native_methods, G_N_ELEMENTS(native_methods));

  pthread_key_create(&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
