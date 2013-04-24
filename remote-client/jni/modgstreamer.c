#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>
#include <pthread.h>

#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/GStreamerMod", __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, "RemoteClient/GStreamerMod", __VA_ARGS__)
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
  GstElement *udpsource;
  GstElement *vorbisdecoder;
  GMainContext *context; // GLib context used to run the main loop */
  GMainLoop *main_loop;  // GLib main loop */
  gboolean initialized;  // To avoid informing the UI multiple times about the initialization */
  jboolean initial_state;
  int port;
} CustomData;

// These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_gstreamer_initialized_method_id;
static jmethodID on_pipeline_playing_id;
static jmethodID on_pipeline_paused_id;
static jmethodID on_pipeline_connected_id;
static jmethodID on_decode_error_id;

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

static gboolean busMsgHandler(GstBus * bus, GstMessage * msg, void * custom_data) {
  JNIEnv * env;
  GError *err;
  gchar *debug_info;
  CustomData * data = (CustomData *) custom_data;
  env=get_jni_env();

  LOG_DEBUG("Received message from bus");
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      LOG_ERROR("Error received from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
      LOG_ERROR("Debugging information: %s", debug_info ? debug_info : "none");
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->vorbisdecoder) &&
          err->code == GST_STREAM_ERROR_DECODE) {
        LOG_ERROR("Need to resend vorbis headers");
        (*env)->CallVoidMethod(env, data->app, on_decode_error_id);
      }

      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      LOG_DEBUG("End-Of-Stream reached.\n");
      break;
    case GST_MESSAGE_STATE_CHANGED: {
      LOG_DEBUG("Stage changed");
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
      LOG_DEBUG("Element %s changed state from %s to %s.\n",
                GST_OBJECT_NAME(msg->src),
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state));
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->udpsource)) {
        if (new_state == GST_STATE_PLAYING) {
          (*env)->CallVoidMethod(env, data->app, on_pipeline_connected_id);
        }
      }

      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
        if (new_state == GST_STATE_PLAYING) {
          (*env)->CallVoidMethod(env, data->app, on_pipeline_playing_id);
        }
        if (new_state == GST_STATE_PAUSED) {
          (*env)->CallVoidMethod(env, data->app, on_pipeline_paused_id);
        }
      }
    }
      break;
    default:
      LOG_DEBUG("Message type : %s", gst_message_type_get_name(GST_MESSAGE_TYPE(msg)));
      break;
  }

  return TRUE;
}

// Main method
static void * create_pipeline(void * in_data) {
  unsigned int bus_watch_id;
  int argc = 0;
  char ** argv = NULL;

  CustomData * data = (CustomData *) in_data;
  GstBus * bus;
  GstElement * source;
  GstElement * vorbisdecoder;
  GstElement * audioconvert;
  GstElement * audioresample;
  GstElement * sink;
  GError * error = NULL;

  // Create our own GLib Main Context to run the main loop
  data->context = g_main_context_new();
  g_main_context_push_thread_default(data->context);

  /*
  data->pipeline = gst_parse_launch("udpsrc port=52001 ! \
                                     vorbisdec ! audioconvert ! audioresample ! openslessink",
                                     &error);
  if (error) {
    LOG_ERROR("Unable to build pipeline: %s", error->message);
    g_error_free(error);
    return NULL;
  }
  */

  data->pipeline = gst_pipeline_new("streaming-client");
  source = gst_element_factory_make("udpsrc", "udpsource");
  vorbisdecoder = gst_element_factory_make("vorbisdec", "vorbisdecoder");
  audioconvert = gst_element_factory_make("audioconvert", "converter");
  audioresample = gst_element_factory_make("audioresample", "sampler");
  sink = gst_element_factory_make("openslessink", "sink");
  if (!data->pipeline || !source || !vorbisdecoder || !audioconvert ||
      !audioresample || !sink) {
    LOG_ERROR("Error creating gstreamer pipeline.\n");
    LOG_DEBUG("pipeline : %p, source : %p, vorbisdec : %p, audioconvert : %p,\
               audioresample : %p, sink : %p\n",
              data->pipeline, source, vorbisdecoder, audioconvert, audioresample, sink);
    return NULL;
  }

  g_object_set(G_OBJECT(source), "do-timestamp", 1, "port", data->port, NULL);
  g_object_set(G_OBJECT(sink), "slave-method", 2, NULL);
  data->udpsource = source;
  data->vorbisdecoder = vorbisdecoder;

  // Add the elements to the pipeline
  gst_bin_add_many(GST_BIN(data->pipeline), source, vorbisdecoder,
                   audioconvert, audioresample, sink, NULL);

  // Link them
  gst_element_link_many(source, vorbisdecoder, audioconvert, audioresample, sink,
                        NULL);


  // Create a bus to receive the messages from the pipeline.
  bus = gst_pipeline_get_bus(GST_PIPELINE(data->pipeline));
  bus_watch_id = gst_bus_add_watch(bus, busMsgHandler, data);
  gst_object_unref (bus);

  if (data->initial_state == JNI_TRUE) {
    gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
  }

  // Create a GLib Main Loop and set it to run
  LOG_DEBUG("Entering main loop... (CustomData:%p)", data);
  data->main_loop = g_main_loop_new(data->context, FALSE);
  check_initialization_complete(data);
  g_main_loop_run(data->main_loop);
  LOG_DEBUG("Exited main loop");
  g_main_loop_unref(data->main_loop);
  data->main_loop = NULL;

  // Free resources
  g_source_remove(bus_watch_id);
  g_main_context_pop_thread_default(data->context);
  g_main_context_unref(data->context);
  gst_element_set_state(data->pipeline, GST_STATE_NULL);
  gst_object_unref(data->pipeline);

  return NULL;
}
/*
 * Java Bindings
 */
static void gst_native_init(JNIEnv * env, jobject thiz, jint port, jboolean initial_state) {
  __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/Gstreamer", "native_init");
  CustomData * data = g_new0(CustomData, 1);
  data->initial_state = initial_state;
  SET_CUSTOM_DATA(env, thiz, custom_data_field_id, data);
  GST_DEBUG_CATEGORY_INIT(debug_category, "RemoteClient/Gstreamer-pipeline",
                          0, "Android Gstreamer pipeline");
  gst_debug_set_threshold_for_name("Gstreamer-pipeline", GST_LEVEL_DEBUG);
  //gst_debug_set_default_threshold(GST_LEVEL_DEBUG);
  LOG_DEBUG("Created CustomData at %p", data);
  data->app = (*env)->NewGlobalRef(env, thiz);
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
  GstStateChangeReturn ret;
  CustomData * data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
  if (!data || !data->pipeline) return;
  LOG_DEBUG("Setting state to PLAYING");
  ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
  if (ret = GST_STATE_CHANGE_ASYNC) {
    LOG_DEBUG("Changing pipeline state asynchronously");
  }
}

// Set pipeline to PAUSED state */
static void gst_native_pause(JNIEnv* env, jobject thiz) {
  GstStateChangeReturn ret;
  CustomData * data = GET_CUSTOM_DATA(env, thiz, custom_data_field_id);
  if (!data || !data->pipeline) return;
  LOG_DEBUG("Setting state to PAUSED");
  gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
  if (ret = GST_STATE_CHANGE_ASYNC) {
    LOG_DEBUG("Changing pipeline state asynchronously");
  }
}

// Static initializer
static jboolean gst_native_class_init(JNIEnv* env, jclass class) {
  __android_log_print(ANDROID_LOG_DEBUG, "RemoteClient/Gstreamer", "gst_native_class_init");
  custom_data_field_id = (*env)->GetFieldID(env, class, "native_custom_data", "J");
  set_message_method_id = (*env)->GetMethodID(env, class, "setMessage", "(Ljava/lang/String;)V");
  on_gstreamer_initialized_method_id = (*env)->GetMethodID(env, class, "onGStreamerInitialized", "()V");
  on_pipeline_playing_id = (*env)->GetMethodID(env, class, "onPipelinePlaying", "()V");
  on_pipeline_paused_id = (*env)->GetMethodID(env, class, "onPipelinePaused", "()V");
  on_pipeline_connected_id = (*env)->GetMethodID(env, class, "onPipelineConnected", "()V");
  on_decode_error_id = (*env)->GetMethodID(env, class, "onDecodeError", "()V");

  if (!custom_data_field_id || !set_message_method_id || !on_gstreamer_initialized_method_id ||
      !on_pipeline_playing_id || !on_pipeline_paused_id || !on_pipeline_connected_id) {
    __android_log_print(ANDROID_LOG_ERROR, "RemoteClient/Gstreamer",
                        "The calling class does not implement all necessary interface methods");
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

static JNINativeMethod native_methods[] = {
  { "nativeInit", "(IZ)V", (void *) gst_native_init},
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
