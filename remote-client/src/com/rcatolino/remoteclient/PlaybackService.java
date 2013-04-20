package com.rcatolino.remoteclient;

import android.app.IntentService;
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

public class PlaybackService extends IntentService {

  private native void nativeInit();
  private native void nativeFinalize();
  private native void nativePlay();
  private native void nativePause();
  private static native boolean nativeClassInit();
  private long native_custom_data;

  //private final LocalBinder binder = new LocalBinder(this);
  private static final String LOGTAG = "RemoteClient/PlaybackService";

  private MediaPlayer mp = null;
  private Socket sock = null;

  @Override
  public void onCreate() {
    super.onCreate();
    Log.d(LOGTAG, "Gstreamer initialization");
    try {
      GStreamer.init(this);
    } catch (Exception e) {
      Log.d(LOGTAG, "Gstreamer.init() failed : " + e.getMessage());
      return;
    }

    Log.d(LOGTAG, "Calling nativeInit()");
    nativeInit();
    return;
  }

  public PlaybackService() {
    super("PlaybackService");
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    Bundle extras = intent.getExtras();
    if (extras == null) {
      Log.d(LOGTAG, "Error, no host/port specified in intent");
      return;
    }

    String host = extras.getString("Host");
    int port = extras.getInt("Port");
    Log.d(LOGTAG, "Waiting for notification");
    synchronized(this) {
      try {
        wait();
      } catch (Exception e) {
        Log.d(LOGTAG, "Failed on wait() : " + e.getMessage());
      }
    }
    Log.d(LOGTAG, "Terminating service");
    /*
    InetSocketAddress adress = new InetSocketAddress(host, port);
    if (adress.isUnresolved()) {
      Log.d(LOGTAG, "Adress is unresolved");
      Toast.makeText(context, "Bad address : " + host + ":" + port, Toast.LENGTH_SHORT).show();
    }

    try {
      sock.connect(adress, 1000);
    } catch (IOException ex) {
      Toast.makeText(context, ex.getMessage(), Toast.LENGTH_SHORT).show();
    }
    */
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
    nativePlay();
  }

  /*
  @Override
  public void onCreate() {
    Log.d(LOGTAG, "Creating service");
    client = null;
  }

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

  @Override
  public boolean onUnbind(Intent intent) {
    Log.d(LOGTAG, "Service unbinding");
    binder.unbind();
    return false;
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
