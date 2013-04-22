package com.rcatolino.remoteclient;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.Bundle;
import android.os.Binder;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import android.widget.Toast;

import com.gstreamer.GStreamer;

import java.net.Socket;
import java.net.InetSocketAddress;

public class PlaybackService extends Service {

  private native void nativeInit(String host, int port);
  private native void nativeFinalize();
  private native void nativePlay();
  private native void nativePause();
  private static native boolean nativeClassInit();
  private long native_custom_data;
  private boolean running = false; // Tracks the sate of the gstreamer pipeline
  private boolean gstInitialized = false; // Tracks the state of the gstreamer lib
  private boolean shouldPlay = false; // Tracks wether or not we should be playing
  private boolean started = false; // Tracks the state of the service

  private static final String LOGTAG = "RemoteClient/PlaybackService";

  private MediaPlayer mp = null;
  private Socket sock = null;
  private PlaybackBinder binder = null;
  private WifiLock wifiLock;
  private WakeLock wakeLock;

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

    WifiManager wm = (WifiManager) getSystemService(Context.WIFI_SERVICE);
    wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL, "streamingWifiLock");
    PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
    wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "streamingCpuLock");
    wifiLock.setReferenceCounted(false);
    wakeLock.setReferenceCounted(false);
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
    return binder;
  }

  private void start() {
    startService(new Intent(this, PlaybackService.class));
  }

  public void createStreamingPipeline(String host, int port) {
    if (!running) {
      Log.d(LOGTAG, "Calling nativeInit()");
      nativeInit(host, port);
    }
  }

  public void stopStreamingPipeline() {
    nativeFinalize();
    running=false;
    gstInitialized=false;
    Log.d(LOGTAG, "Stopping service");
    stopSelf();
    started = false;
  }

  @Override
  public boolean onUnbind(Intent intent) {
    Log.d(LOGTAG, "Service unbinding");
    if (started && !shouldPlay) {
      stopStreamingPipeline();
    }

    return false;
  }

  public boolean getPlaybackStatus() {
    return (shouldPlay && running);
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
    /*
    if (!shouldPlay) {
      Log.d(LOGTAG, "Releasing locks");
      wifiLock.release();
      wakeLock.release();
    }
    */

    if (!gstInitialized) {
      Log.d(LOGTAG, "Didn't change the pipeline stat because gst isn't initialized yet");
      return;
    }

    if (shouldPlay) {
      nativePlay();
      /*
      if (!wifiLock.isHeld()) {
        Log.d(LOGTAG, "Acquiring wifiLock");
        wifiLock.acquire();
      }
      if (!wakeLock.isHeld()) {
        Log.d(LOGTAG, "Acquiring wakeLock");
        wakeLock.acquire();
      }
      */
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
    gstInitialized = true;
    running = true;
    tryChangePlaybackState();
  }

  /*

  @Override
  public void onDestroy() {
    binder.stopServer();
  }

  */
  static {
    System.loadLibrary("gstreamer_android");
    System.loadLibrary("modgstreamer");
    nativeClassInit();
  }
}
