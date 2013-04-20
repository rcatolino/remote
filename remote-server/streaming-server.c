#include <gst/gst.h>
#include <glib.h>

#include "tcpserver.h"
#include "utils.h"

static GstElement * pipeline = NULL;
static unsigned int bus_watch_id;
static gboolean busMsgHandler(GstBus * bus, GstMessage * msg, void * data) {
  return TRUE;
}

int createStreamingServer(GMainLoop * loop) {
  int argc = 0;
  char ** argv = NULL;

  GstBus * bus;
  GstElement * source;
  GstElement * audioconvert;
  GstElement * vorbisencoder;
  GstElement * oggmux;
  GstElement * sink;

  GError * error = NULL;

  if (!gst_init_check(&argc, &argv, &error)) {
    g_printerr("Error initializing gstreamer: %s\n", error->message);
    g_error_free(error);
    return -1;
  }

  pipeline = gst_pipeline_new("streaming-server");
  source = gst_element_factory_make("pulsesrc", "pulseaudio source");
  audioconvert = gst_element_factory_make("audioconvert", "converter");
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

  return 0;
}

int startStreaming() {
  if (pipeline == NULL) {
    debug("startStreaming: error, uninitialized pipeline");
    return -1;
  }

  debug("Start gstreamer pipeline");
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  return 0;
}

int deleteStreamingServer() {
  if (pipeline == NULL) {
    debug("stopStreaming: error, uninitialized pipeline");
    return -1;
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
  debug("Deleting pipeline\n");
  gst_object_unref(GST_OBJECT(pipeline));
  g_source_remove(bus_watch_id);
  return 0;
}
