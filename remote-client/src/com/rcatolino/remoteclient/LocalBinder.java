package com.rcatolino.remoteclient;

import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import java.io.IOException;
import java.lang.IllegalArgumentException;

public class LocalBinder extends Binder {

  private static final String LOGTAG = "RemoteClient/LocalBinder";

  private RemoteClient context;
  private RCService parentService;
  private TcpClient client;
  private String serverHost;
  private int serverPort;

  public LocalBinder(RCService service) {
    parentService = service;
    client = null;
    context = null;
    serverHost = null;
    serverPort = 0;
  }

  public void setActivity(RemoteClient activity) {
    if (context != null) {
      Log.d(LOGTAG, "Already connected");
      return;
    }

    context = activity;
  }

  public boolean isConnected() {
    if (client == null) {
      return false;
    }

    return true;
  }

  public String getHost() {
    return serverHost;
  }

  public int getPort() {
    return serverPort;
  }

  public TcpClient connect(String host, int port)
                          throws IOException, IllegalArgumentException {
    if (context == null) {
      Log.d(LOGTAG, "Warning, no reference to context thread. You should call setActivity()");
    }

    Log.d(LOGTAG, "Connecting to " + host + ":" + port);
    client = new TcpClient(host, port, this);
    parentService.start();
    serverHost = host;
    serverPort = port;
    return client;
  }

  public void unbind() {
    context = null;
  }

  public void setDisconnected() {
    parentService.stop();
    serverPort = 0;
    serverHost = null;
    client = null;
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setDisconnected();
      }
    });
  }

  public void setProfilesIfBound(final String arg) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setProfiles(arg);
      }
    });
  }

  public void setAlbumIfBound(final String arg) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setAlbum(arg);
      }
    });
  }

  public void setArtistIfBound(final String arg) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setArtist(arg);
      }
    });
  }

  public void setTitleIfBound(final String arg) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setTitle(arg);
      }
    });
  }

  public void setCoverArtIfBound(final byte[] file) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setCoverArt(file);
      }
    });
  }

  public void setPlaybackIfBound(final String arg) {
    if (context == null) {
      return;
    }

    context.runOnUiThread(new Runnable() {
      public void run() {
        context.setPlaybackStatus(arg);
      }
    });
  }

}
