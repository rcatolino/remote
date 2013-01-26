package com.rcatolino.remoteclient;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

public class RCService extends Service {

  private final LocalBinder binder = new LocalBinder(this);
  private static final String LOGTAG = "RemoteClient/RCService";

  private TcpClient client;

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

}
