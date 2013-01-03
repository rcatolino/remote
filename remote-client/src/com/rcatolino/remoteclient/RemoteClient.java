package com.rcatolino.remoteclient;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnDismissListener;
import android.os.Bundle;
import android.R.id;
import android.util.Log;
import android.view.KeyEvent;
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

  private static final String playCmd = "PLAY";
  private static final String pauseCmd = "PAUSE";
  private static final String stopCmd = "STOP";
  private static final String nextCmd = "NEXT";
  private static final String prevCmd = "PREV";
  private static final String volumeupCmd = "VOLUMEUP";
  private static final String volumedownCmd= "VOLUMEDOWN";

  private Sender sender = null;
  private DialogListener dialogListener;
  private boolean connected = false;
  private Activity mainActivity;

  private Button connectB;
  private ImageButton playPauseB;

  private class Sender implements Runnable {

    private SynchronousQueue<String> toSend;
    private OutputStream output;
    private boolean stopped = false;
    private Thread execThread;
    private Socket sock;

    public Sender(String host, int port) throws IOException, IllegalArgumentException {
      toSend = new SynchronousQueue<String>();
      // Open connection :
      sock = new Socket();
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
      try {
        sock.close();
      } catch (Exception e) {
        Log.d(LOGTAG, "Error on sock.close : " + e.getMessage());
      }

      mainActivity.runOnUiThread(new Runnable() {
        public void run() {
          setDisconnected();
        }
      });
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
      if (d.shouldConnect() && !connected) {
        Log.d(LOGTAG, "Connecting to " + d.getHost() + ":" + d.getPort());
        try {
          sender = new Sender(d.getHost(), d.getPort());
          setConnected(d.getHost(), d.getPort());
        } catch (Exception ex) {
          Log.d(LOGTAG, "Error on sender() : " + ex.getMessage());
          connectB.setText(R.string.unco);
          if (sender != null) {
            sender.stop();
            sender = null;
          }
          return;
        }
      }
    }

  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    mainActivity = this;
    dialogListener = new DialogListener();
    setContentView(R.layout.main);

    playPauseB = (ImageButton) findViewById(R.id.play_pause);
    connectB = (Button) findViewById(R.id.connect_button);
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    int action = event.getAction();
    int keyCode = event.getKeyCode();

    if (!connected || sender == null) {
      Log.d(LOGTAG, "We're not connected, can't send Volume command");
      return super.dispatchKeyEvent(event);
    }

    switch (keyCode) {
    case KeyEvent.KEYCODE_VOLUME_UP:
      if (action == KeyEvent.ACTION_UP) {
        sender.sendCommand(volumeupCmd);
      }
      return true;
    case KeyEvent.KEYCODE_VOLUME_DOWN:
      if (action == KeyEvent.ACTION_DOWN) {
        sender.sendCommand(volumedownCmd);
      }
      return true;
    default:
      return super.dispatchKeyEvent(event);
    }
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

    sender.sendCommand(pauseCmd);
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
    sender = null;
  }

}

