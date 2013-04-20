package com.rcatolino.remoteclient;

import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

public class PlaybackBinder extends Binder {

  private static final String LOGTAG = "RemoteClient/PlaybackBinder";

  private PlaybackService parentService;

  public PlaybackBinder(PlaybackService service) {
    Log.d(LOGTAG, "Creating PlaybackBinder");
    parentService = service;
  }

  public boolean getPlaybackStatus() {
    return parentService.getPlaybackStatus();
  }

  public void setPlaybackStatus(boolean status) {
    parentService.setPlaybackStatus(status);
  }

}

