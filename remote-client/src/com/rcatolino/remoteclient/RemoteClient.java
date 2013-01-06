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
import android.widget.TextView;

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

  private DialogListener dialogListener;
  private boolean connected = false;
  private RemoteClient main;
  private TcpClient client;

  private Button connectB;
  private ImageButton playPauseB;
  private TextView playingTV;

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
          client = new TcpClient(d.getHost(), d.getPort(), main);
          setConnected(d.getHost(), d.getPort());
        } catch (Exception ex) {
          Log.d(LOGTAG, "Error on TcpClient() : " + ex.getMessage());
          connectB.setText(R.string.unco);
          if (client != null) {
            client.stop();
            client = null;
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
    main = this;
    dialogListener = new DialogListener();
    setContentView(R.layout.main);

    playPauseB = (ImageButton) findViewById(R.id.play_pause);
    connectB = (Button) findViewById(R.id.connect_button);
    playingTV = (TextView) findViewById(R.id.playing_text);
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    int action = event.getAction();
    int keyCode = event.getKeyCode();

    if (!connected || client == null) {
      Log.d(LOGTAG, "We're not connected, can't send Volume command");
      return super.dispatchKeyEvent(event);
    }

    switch (keyCode) {
    case KeyEvent.KEYCODE_VOLUME_UP:
      if (action == KeyEvent.ACTION_UP) {
        client.sendCommand(volumeupCmd);
      }
      return true;
    case KeyEvent.KEYCODE_VOLUME_DOWN:
      if (action == KeyEvent.ACTION_DOWN) {
        client.sendCommand(volumedownCmd);
      }
      return true;
    default:
      return super.dispatchKeyEvent(event);
    }
  }

  public void showConnectDialog(View connectButton) {
    if (client != null) {
      Log.d(LOGTAG, "Disconnecting");
      connectB.setText(R.string.disco);
      Log.d(LOGTAG, "disconnect set text");
      client.stop();
      Log.d(LOGTAG, "client stopped");
      client = null;
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

    if (client == null) {
      // We've been disconnected
      Log.d(LOGTAG, "The remote has been disconnected");
      return;
    }

    client.sendCommand(pauseCmd);
  }

  private void setConnected(String host, int port) {
    if (connected) {
      Log.d(LOGTAG, "Attempted to setConnected while already connected");
      return;
    }

    if (client == null) {
      Log.d(LOGTAG, "Attempted to setConnected whithout any client");
      return;
    }

    connected = true;
    connectB.setText("Connected to " + host + ":" + port);
    playPauseB.setClickable(true);
  }

  public void setDisconnected() {
    if (!connected) {
      return;
    }

    connected = false;
    connectB.setText(R.string.unco);
    playPauseB.setClickable(false);
    client = null;
  }

  public void setPlaybackStatus(String status) {
    if (status.equals("Paused")) {
      playPauseB.setImageResource(android.R.drawable.ic_media_pause);
      playingTV.setText("Paused");
    } else if (status.equals("Stopped")) {
      playingTV.setText("Stopped");
    } else {
      playPauseB.setImageResource(android.R.drawable.ic_media_play);
      playingTV.setText("Playing");
    }
  }

  public void setPlaying(String playing) {
    playPauseB.setImageResource(android.R.drawable.ic_media_pause);
    playingTV.setText(playing);
  }

}

