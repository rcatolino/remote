package com.rcatolino.remoteclient;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.MulticastLock;
import android.util.Log;

import com.rcatolino.smdp.Smdp;

import java.lang.Boolean;
import java.lang.InterruptedException;
import java.lang.NullPointerException;
import java.lang.Thread;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;

public class Connector extends Thread {
  // This class provides non blocking ways to connect to the server.
  // The connect method can be invoked directly (eg when the connect
  // dialog is spawned by the user), or it can be called after the
  // address of the server has been found either using the last
  // working adress, or the smdp service.
  private static final String LOGTAG = "RemoteClient/Connector";
  private boolean connected;
  private Object connected_sync;
  private RemoteClient parent;
  private SynchronousQueue<Boolean> networkStateQueue;
  private ConnectivityManager connectionManager;
  private MulticastLock multicastLock;

  public Connector(RemoteClient parent) {
    connected = false;
    this.parent = parent;
    connectionManager = (ConnectivityManager) parent.getSystemService(Context.CONNECTIVITY_SERVICE);
    WifiManager wm = (WifiManager) parent.getSystemService(Context.WIFI_SERVICE);
    multicastLock = wm.createMulticastLock("multicastLock");
  }

  public void run() {
    // Try to get the coordinates from the cache.
    // All in all, if these succeed at all, this shouldn't
    // take more than a sec. If it needs more to connect,
    // the service will be crappy anyway. And well this is
    // supposed to work on local networks only.

    // If we're not connected to the network, wait until we are.
    if (!isWifiConnected()) {
      Log.d(LOGTAG, "Wifi disconnected, registering broadcast receiver");
      final IntentFilter intentFilter = new IntentFilter();
      BroadcastReceiver receiver = new networkStatusReceiver();
      intentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
      parent.registerReceiver(receiver, intentFilter);
      // We only want to wait for the wifi to reconnect after the device was
      // woken up from sleep. This shouldn't take more than 3 secs. Beyond
      // that, the user will probably leave the app to see what's wrong
      // with the wireless.
      try {
        Boolean status = networkStateQueue.poll(7000, TimeUnit.MILLISECONDS);
        parent.unregisterReceiver(receiver);
        if (status != null && status.booleanValue()) {
          findAddressAndConnect();
        } else if (status == null) {
          Log.d(LOGTAG, "Timed out waiting for wifi connection");
          parent.runOnUiThread(new Runnable() {
            public void run() {
              parent.connectionFailure("Error : wifi is disabled");
            }
          });
        }
      } catch (InterruptedException e) {
        Log.d(LOGTAG, "Couldn't wait for network to connect : " + e.getMessage());
      }

    } else {
      findAddressAndConnect();
    }
  }

  public void findAddressAndConnect() {
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
      multicastLock.acquire();
    }

    final RemoteClient parent = this.parent;
    SharedPreferences serverParams = parent.getPreferences(parent.MODE_PRIVATE);
    final String serverUrl = serverParams.getString("url", null);
    final int serverPort = serverParams.getInt("port", 0);

    if (serverUrl != null && serverPort != 0) {
      if (rawConnect(serverUrl, serverPort, 300, parent)) {
        return;
      }
    }

    // The last known working address isn't working anymore.
    // Try to get the coordinates from the smdp service.
    if (smdp == null) {
      Log.d(LOGTAG, "Smdp client unavailable, skipping broadcast query method");
      return;
    }

    Log.d(LOGTAG, "Trying smdp service");
    smdp.sendQuery();
    Log.d(LOGTAG, "Sent service broadcast");
    if (smdp.waitForAnswer(3000)) {
      Log.d(LOGTAG, "Received service broadcast : " + smdp.getServiceField(2)
            + ":" + smdp.getServiceField(3));
      final String url = smdp.getServiceField(2);
      final int port = Integer.parseInt(smdp.getServiceField(3));
      rawConnect(url, port, 300, parent);
      return;
    } else {
      Log.d(LOGTAG, "Timed out waiting for an answer");
    }

    if (smdp != null) {
      smdp.stopBroadcastServer();
      if (multicastLock.isHeld()) {
        multicastLock.release();
      }
    }
  }

  private boolean rawConnect(final String host, final int port,
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

  public boolean isWifiConnected() {
    NetworkInfo ni = connectionManager.getActiveNetworkInfo();
    if (ni != null && ni.isConnected() && ni.getType() == ConnectivityManager.TYPE_WIFI) {
      return true;
    }
    if (ni != null) {
      Log.d(LOGTAG, "network state : " + ni.getExtraInfo() + ", " + ni.getState());
    }

    return false;
  }

  public void connect(final String host, final int port, final int timeout) {
    // Tries to connect to the specified host:port.
    final RemoteClient parent = this.parent;
    // If we're not connected to the network, no need trying.
    if (!isWifiConnected()) {
      parent.connectionFailure("You need to connect to the local wireless network.");
      return;
    }

    Thread networkThread = new Thread(new Runnable() {
      public void run() {
        rawConnect(host, port, timeout, parent);
      }
    });
    networkThread.start();
  }

  public void connect() {
    // Tries to find out the server's address then to connect to it.
    networkStateQueue = new SynchronousQueue<Boolean>();
    this.start();
  }

  private class networkStatusReceiver extends BroadcastReceiver {

    public void onReceive(Context context, Intent intent) {
      if (isWifiConnected()) {
        Log.d(LOGTAG, "Wifi connected");
        networkStateQueue.offer(new Boolean(true));
        Log.d(LOGTAG, "Connector thread notified");
      }
    }
  }
}
