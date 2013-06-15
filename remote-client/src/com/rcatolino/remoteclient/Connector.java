package com.rcatolino.remoteclient;

import android.content.SharedPreferences;
import android.util.Log;

import com.rcatolino.smdp.Smdp;

import java.lang.Thread;

public class Connector extends Thread {
  // This class provides non blocking ways to connect to the server.
  // The connect method can be invoked directly (eg when the connect
  // dialog is spawned by the user), or it can be called after the
  // coordiante of the server has been found either using the last
  // working coordinate, or the smdp service.
  private static final String LOGTAG = "RemoteClient/Connector";
  private boolean connected;
  private Object connected_sync;
  private RemoteClient parent;

  public Connector(RemoteClient parent) {
    connected = false;
    this.parent = parent;
  }

  public void run() {
    // Try to get the coordinates from the cache.
    // All in all, if these succeed at all, this shouldn't
    // take more than a sec. If it needs more to connect,
    // the service will be crappy anyway. And well this is
    // supposed to work on local networks only.
    Log.d(LOGTAG, "Connector running");
    Smdp smdp;
    try {
      smdp = new Smdp();
      Log.d(LOGTAG, "Created smdp");
    } catch (ExceptionInInitializerError e) {
      Log.d(LOGTAG, "Error, couldn't load smdp library : " + e.getMessage());
      smdp = null;
    }

    if (smdp != null) {
      smdp.createService("remote-server", null, null, null);
      Log.d(LOGTAG, "Created service");
      smdp.startBroadcastServer();
      Log.d(LOGTAG, "server started");
    }

    final RemoteClient parent = this.parent;
    SharedPreferences serverParams = parent.getPreferences(parent.MODE_PRIVATE);
    final String serverUrl = serverParams.getString("url", null);
    final int serverPort = serverParams.getInt("port", 0);

    if (serverUrl != null && serverPort != 0) {
      if (RawConnect(serverUrl, serverPort, 300, parent)) {
        return;
      }
    }

    // Try to get the coordinates from the smdp service.
    Log.d(LOGTAG, "Trying smdp service");
    smdp.sendQuery();
    Log.d(LOGTAG, "Sent service broadcast");
    if (smdp != null && smdp.waitForAnswer(3000)) {
      Log.d(LOGTAG, "Received service broadcast : " + smdp.getServiceField(2)
            + ":" + smdp.getServiceField(3));
      final String url = smdp.getServiceField(2);
      final int port = Integer.parseInt(smdp.getServiceField(3));
      RawConnect(url, port, 300, parent);
      return;
    }


  }

  private boolean RawConnect(final String host, final int port,
                             int timeout, final RemoteClient parent) {
    Log.d(LOGTAG, "Connecting to " + host + ":" + port);
    TcpClient client = null;
    try {
      client = new TcpClient(host, port, timeout, parent);
      final TcpClient connectedClient = client;
      parent.runOnUiThread(new Runnable() {
        public void run() {
          parent.setConnected(host, port, connectedClient);
        }
      });
    } catch (final Exception ex) {
      Log.d(LOGTAG, "Error on TcpClient() : " + ex.getMessage());
      parent.runOnUiThread(new Runnable() {
        public void run() {
          parent.connectionFailure(ex.getMessage());
        }
      });
      if (client != null) {
        client.stop();
        client = null;
      }

      return false;
    }

    return true;
  }

  public void Connect(final String host, final int port, final int timeout) {
    final RemoteClient parent = this.parent;
    Thread networkThread = new Thread(new Runnable() {
      public void run() {
        RawConnect(host, port, timeout, parent);
      }
    });
    networkThread.start();
  }
}
