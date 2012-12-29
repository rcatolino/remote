package com.rcatolino.remoteclient;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnDismissListener;
import android.os.Bundle;
import android.R.id;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import java.io.OutputStream;
import java.io.IOException;
import java.lang.IllegalArgumentException;
import java.lang.Thread;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.util.concurrent.SynchronousQueue;

public class RemoteClient extends Activity {

  private static final String LOGTAG = "RemoteClient";
  private Sender sender = null;
  private DialogListener dialogListener;
  private boolean connected = false;

  private Button connectB;
  private ImageButton playPauseB;

  private class Sender implements Runnable {

    private SynchronousQueue<String> toSend;
    private OutputStream output;
    private boolean stopped = false;
    private Thread execThread;

    public Sender(String host, int port) throws IOException, IllegalArgumentException {
      toSend = new SynchronousQueue<String>();
      // Open connection :
      Socket sock = new Socket();
      InetSocketAddress adress = new InetSocketAddress(host, port);
      if (adress.isUnresolved()) {
        Log.d(LOGTAG, "Adress is unresolved");
        throw new IllegalArgumentException("Bad adress");
      }

      connectB.setText("Connecting...");
      sock.connect(adress, 1000);
      output = sock.getOutputStream();

      // Start thread :
      execThread = new Thread(this);
      execThread.start();
    }

    public void stop() {
      Log.d(LOGTAG, "Stopping sender");
      synchronized(this) {
        stopped = true;
      }

      Log.d(LOGTAG, "Flag set");
      try {
        toSend.put("");
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not send empty terminating message in queue : " + e.getMessage());
        toSend = null;
      }
      Log.d(LOGTAG, "Sent empty terminating command");

      try {
        execThread.join();
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not join sending thread : " + e.getMessage());
      }

      Log.d(LOGTAG, "Joined thread");
    }

    public void sendCommand(String command) {
      try {
        toSend.put(command);
      } catch (Exception e) {
        Log.d(LOGTAG, "Sendind command in queue failed : " + e.getMessage());
      }
    }

    public boolean shouldStop() {
      boolean shouldStop = false;
      synchronized(this) {
        shouldStop = stopped;
      }

      return shouldStop;
    }

    public void run() {
      String message = null;
      byte[] buffer = null;
      while (!shouldStop()) {
        try {
          message = toSend.take();
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not read message from queue : " + e.getMessage());
          return;
        }

        if (message.equals("")) {
          Log.d(LOGTAG, "Received empty terminating command");
          break;
        }

        try {
          buffer = message.getBytes("US-ASCII");
        } catch (Exception e) {
          Log.d(LOGTAG, e.getMessage());
          break;
        }

        //bufferSize = new Integer(buffer.length);
        try {
          output.write(buffer.length);
          output.write(buffer);
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not send data : " + e.getMessage());
          break;
        }
      }

      Log.d(LOGTAG, "Running finished");
      setDisconnected();
      Log.d(LOGTAG, "Set as disconnected");
    }

  }

  private class DialogListener implements OnCancelListener, OnDismissListener {
    private ConnectionDialog d;
    public void onCancel(DialogInterface dialog) {
      d = (ConnectionDialog) dialog;
      Log.d(LOGTAG, "Connection dialog canceled");
    }

    public void onDismiss(DialogInterface dialog) {
      d = (ConnectionDialog) dialog;
      if (d.shouldConnect()) {
        Log.d(LOGTAG, "Connecting to " + d.getHost() + ":" + d.getPort());
        try {
          sender = new Sender(d.getHost(), d.getPort());
          setConnected(d.getHost(), d.getPort());
        } catch (Exception ex) {
          Log.d(LOGTAG, "Error on connectToHost() : " + ex.getMessage());
          connectB.setText(R.string.unco);
          sender.stop();
          sender = null;
          return;
        }
      }
    }

  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    dialogListener = new DialogListener();
    setContentView(R.layout.main);

    playPauseB = (ImageButton) findViewById(R.id.play_pause);
    connectB = (Button) findViewById(R.id.connect_button);
  }

  public void showConnectDialog(View connectButton) {
    if (sender != null) {
      Log.d(LOGTAG, "Disconnecting");
      connectB.setText(R.string.disco);
      Log.d(LOGTAG, "disconnect set text");
      sender.stop();
      Log.d(LOGTAG, "sender stopped");
      sender = null;
      return;
    }

    ConnectionDialog connectD = new ConnectionDialog(this);
    connectD.setOnDismissListener(dialogListener);
    connectD.setOnCancelListener(dialogListener);

    connectD.show();
  }

  public Socket connectToHost(String host, int port) throws IllegalArgumentException, IOException {
    Socket sock = new Socket();
    InetSocketAddress adress = new InetSocketAddress(host, port);
    if (adress.isUnresolved()) {
      Log.d(LOGTAG, "Adress is unresolved");
      throw new IllegalArgumentException("Bad adress");
    }

    connectB.setText("Connecting...");
    sock.connect(adress, 1000);

    return sock;

  }

  public void playPause(View playPauseButton) {
    if (!connected) {
      Log.d(LOGTAG, "Error aksed for play/pause while unconnected!");
      return;
    }

    if (sender == null) {
      // We've been disconnected
      Log.d(LOGTAG, "The remote has been disconnected");
      return;
    }

    sender.sendCommand("play");
  }

  private void setConnected(String host, int port) {
    if (connected) {
      Log.d(LOGTAG, "Attempted to setConnected while already connected");
      return;
    }

    if (sender == null) {
      Log.d(LOGTAG, "Attempted to setConnected whithout any sender");
      return;
    }

    connected = true;
    connectB.setText("Connected to " + host + ":" + port);
    playPauseB.setClickable(true);
  }

  private void setDisconnected() {
    if (!connected) {
      return;
    }

    connected = false;
    connectB.setText(R.string.unco);
    playPauseB.setClickable(false);
  }

}

