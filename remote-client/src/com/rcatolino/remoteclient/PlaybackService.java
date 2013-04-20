package com.rcatolino.remoteclient;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;
import android.widget.Toast;

import com.gstreamer.GStreamer;

import java.net.Socket;
import java.net.InetSocketAddress;

public class PlaybackService extends Service {

  private native void nativeInit();
  private native void nativeFinalize();
  private native void nativePlay();
  private native void nativePause();
  private static native boolean nativeClassInit();
  private long native_custom_data;
  private boolean running = false;
  private boolean gstInitialized = false;
  private boolean shouldPlay = false;
  private boolean started = false;

  //private final LocalBinder binder = new LocalBinder(this);
  private static final String LOGTAG = "RemoteClient/PlaybackService";

  private MediaPlayer mp = null;
  private Socket sock = null;
  private PlaybackBinder binder = null;

  @Override
  public void onCreate() {
    super.onCreate();
    binder = new PlaybackBinder(this);
    Log.d(LOGTAG, "Gstreamer initialization");
    try {
      GStreamer.init(this);
    } catch (Exception e) {
      Log.d(LOGTAG, "Gstreamer.init() failed : " + e.getMessage());
      return;
    }

    return;
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startid) {
    Log.d(LOGTAG, "Starting service");
    started = true;
    return START_NOT_STICKY;
  }

  @Override
  public IBinder onBind(Intent intent) {
    if (!running) {
      Bundle extras = intent.getExtras();
      if (extras == null) {
        Log.d(LOGTAG, "Error, no host/port specified in intent");
        return null;
      }

      String host = extras.getString("Host");
      int port = extras.getInt("Port");

      Log.d(LOGTAG, "Calling nativeInit()");
      nativeInit();
    }

    return binder;
  }

  private void start() {
    startService(new Intent(this, PlaybackService.class));
  }

  private void stop() {
    started = false;
    Log.d(LOGTAG, "Stopping service");
    stopSelf();
  }

  @Override
  public boolean onUnbind(Intent intent) {
    Log.d(LOGTAG, "Service unbinding");
    if (started && !shouldPlay) {
      stop();
    }

    return false;
  }

  public boolean getPlaybackStatus() {
    return shouldPlay;
  }

  public void startStreaming() {
    setPlaybackStatus(true);
    if (!gstInitialized) {
      Log.d(LOGTAG, "startStreaming failed : GStreamer not initialized");
    }
  }

  public void stopStreaming() {
    setPlaybackStatus(false);
    if (!gstInitialized) {
      Log.d(LOGTAG, "stopStreaming failed : GStreamer not initialized");
    }
  }

  public void setPlaybackStatus(boolean status) {
    Log.d(LOGTAG, "Changed playback status to " + status);
    shouldPlay = status;
    tryChangePlaybackState();
  }

  private void tryChangePlaybackState() {
    if (!gstInitialized) {
      return;
    }

    if (shouldPlay) {
      nativePlay();
      if (!started) {
        start();
      }
    } else {
      nativePause();
    }
  }

  private void setMessage(final String message) {
    Log.d(LOGTAG, "setMessage : " + message);
    return;
  }

  // Called from native code. Native code calls this once it has created its pipeline and
  // the main loop is running, so it is ready to accept commands.
  private void onGStreamerInitialized() {
    Log.d("GStreamer", "Gst initialized.");
    //notify();
    gstInitialized = true;
    running = true;
    tryChangePlaybackState();
  }

  /*

  @Override
  public void onDestroy() {
    binder.stopServer();
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    Log.d(LOGTAG, "Starting service!");
    return START_NOT_STICKY;
  }

  @Override
  public IBinder onBind(Intent intent) {
    Log.d(LOGTAG, "Binding service!");
    return binder;
  }

  public void start() {
    startService(new Intent(this, RCService.class));
  }

  public void stop() {
    Log.d(LOGTAG, "Stopping service");
    stopSelf();
  }
  */
  static {
    System.loadLibrary("gstreamer_android");
    System.loadLibrary("modgstreamer");
    nativeClassInit();
  }
}
