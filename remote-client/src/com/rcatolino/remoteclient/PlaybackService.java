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

import java.net.Socket;
import java.net.InetSocketAddress;

public class PlaybackService extends IntentService {

  //private final LocalBinder binder = new LocalBinder(this);
  private static final String LOGTAG = "RemoteClient/PlaybackService";

  private MediaPlayer mp = null;
  private Socket sock = null;

  public PlaybackService() {
    super("PlaybackService");
    //sock = new Socket();
    mp = new MediaPlayer();
    mp.setAudioStreamType(AudioManager.STREAM_MUSIC);
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
    Log.d(LOGTAG, "Showing toast");
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

}
