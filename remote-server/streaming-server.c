#include <gst/gst.h>
#include <glib.h>

#include "tcpserver.h"
#include "utils.h"

#ifdef AUDIO_CODEC_FLAC
  #define CODEC "flacenc"
#else
  #define CODEC "vorbisenc"
#endif

static GstElement * pipeline = NULL;
static unsigned int bus_watch_id;
static gboolean busMsgHandler(GstBus * bus, GstMessage * msg, void * data) {
  GError *err;
  gchar *debug_info;

  debug("Received message from bus\n");
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      debug("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
      debug("Debugging information: %s\n", debug_info ? debug_info : "none");
      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      debug("End-Of-Stream reached.\n");
      break;
    case GST_MESSAGE_STATE_CHANGED: {
      debug("Stage changed\n");
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
      debug("Element %s changed state from %s to %s.\n",
                GST_OBJECT_NAME(msg->src),
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state));
    }
      break;
    default:
      debug("Message type : %s\n", gst_message_type_get_name(GST_MESSAGE_TYPE(msg)));
      break;
  }

  return TRUE;
}

static int createStreamingServer(GMainLoop * loop, const char * address) {
  int argc = 0;
  char ** argv = NULL;

  GstBus * bus;
  GstElement * source;
  GstElement * audioresample;
  GstElement * audioconvert;
  GstElement * encoder;
  GstElement * sink;

  GError * error = NULL;

  if (!gst_init_check(&argc, &argv, &error)) {
    g_printerr("Error initializing gstreamer: %s\n", error->message);
    g_error_free(error);
    return -1;
  }

  pipeline = gst_pipeline_new("streaming-server");
  source = gst_element_factory_make("pulsesrc", "pulseaudio source");
  audioresample = gst_element_factory_make("audioresample", "sampler");
  audioconvert = gst_element_factory_make("audioconvert", "converter");
  encoder = gst_element_factory_make(CODEC, "encoder");
  sink = gst_element_factory_make("udpsink", "network sink");
  if (!pipeline || !source || !audioresample || !audioconvert|| !encoder || !sink) {
    g_printerr ("Error creating gstreamer pipeline.\n");
    return -1;
  }

  // TODO: Dynamically find out the name of the input device
  g_object_set(G_OBJECT(source), "device",
               "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor", NULL);
  g_object_set(G_OBJECT(sink), "host", address, "port", 52001, "force-ipv4", 1, NULL);

  // Add a message handler
  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  bus_watch_id = gst_bus_add_watch(bus, busMsgHandler, loop);
  gst_object_unref (bus);

  // Add the elements to the pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, audioresample, audioconvert,
                   encoder, sink, NULL);

  // Link them
  gst_element_link_many(source, audioresample, audioconvert, encoder, sink,
                        NULL);
  /*
  vorbisencoder = gst_element_factory_make("vorbisenc", "vorbis encoder");
  oggmux = gst_element_factory_make("oggmux", "ogg multiplexer");
  sink = gst_element_factory_make("tcpserversink", "network sink");
  if (!pipeline || !source || !audioconvert|| !vorbisencoder|| !oggmux|| !sink) {
    g_printerr ("Error creating gstreamer pipeline.\n");
    return -1;
  }

  // TODO: Dynamically find out the name of the input device
  g_object_set(G_OBJECT(source), "device",
               "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor", NULL);
  g_object_set(G_OBJECT(sink), "port", 52001, "host", "0.0.0.0", NULL);

  // Add a message handler
  bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch(bus, busMsgHandler, loop);
  gst_object_unref (bus);

  // Add the elements to the pipeline
  gst_bin_add_many(GST_BIN(pipeline), source, audioconvert, vorbisencoder,
                   oggmux, sink, NULL);

  // Link them
  gst_element_link_many(source, audioconvert, vorbisencoder, oggmux, sink,
                        NULL);
  */

  return 0;
}

int startStreaming(GMainLoop * loop, const char * address) {
  if (pipeline == NULL) {
    createStreamingServer(loop, address);
    if (pipeline == NULL) {
      debug("startStreaming: error, uninitialized pipeline\n");
      return -1;
    }
  }

  debug("Start gstreamer pipeline\n");
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  return 0;
}

int pauseStreaming() {
  if (pipeline == NULL) {
    debug("stopStreaming: error, uninitialized pipeline\n");
    return -1;
  }

  debug("Stop gstreamer pipeline\n");
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
  return 0;
}

int deleteStreamingServer() {
  if (pipeline == NULL) {
    debug("stopStreaming: uninitialized pipeline\n");
    return -1;
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
  debug("Deleting pipeline\n");
  gst_object_unref(GST_OBJECT(pipeline));
  pipeline = NULL;
  g_source_remove(bus_watch_id);
  return 0;
}
