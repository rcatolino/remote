package com.rcatolino.remoteclient;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioManager;
import android.os.Bundle;
import android.R.id;
import android.support.v4.app.FragmentActivity;
import android.support.v4.view.ViewPager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.OutputStream;
import java.io.IOException;
import java.lang.IllegalArgumentException;
import java.lang.Thread;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class RemoteClient extends FragmentActivity
                          implements ImageViewAdapter.OnPreviousNextListener {

  private static final String LOGTAG = "RemoteClient";
  private static final String playCmd = "PLAY";
  private static final String pauseCmd = "PAUSE";
  private static final String stopCmd = "STOP";
  private static final String nextCmd = "NEXT";
  private static final String positionCmd = "POSITION";
  private static final String prevCmd = "PREV";
  private static final String profileCmd = "PROFILE ";
  private static final String sleepCmd = "SLEEP";
  private static final String volumeupCmd = "VOLUMEUP";
  private static final String volumedownCmd= "VOLUMEDOWN";

  private boolean playing = false;
  private DialogListener dialogListener;
  private boolean connected = false;
  private RemoteClient main;
  private TcpClient client;
  private PositionQuery pQuery;
  private long trackLength = 0;

  private Button connectB;
  private ImageButton playPauseB;
  private ProgressBar positionPB;
  private TextView titleTV;
  private TextView artistTV;
  private TextView albumTV;
  private TextView playbackTV;
  private ViewPager pager;
  private ImageViewAdapter adapter;
  private String[] profiles = null;

  private class PositionQuery implements Runnable {
    private ScheduledThreadPoolExecutor scheduler;
    private static final int period = 3;

    public void start() {
      if (!connected || client == null || scheduler != null) {
        return;
      }

      Log.d(LOGTAG, "position query start");
      scheduler = new ScheduledThreadPoolExecutor(1);
      try {
        scheduler.scheduleWithFixedDelay(this, 0, period, TimeUnit.SECONDS);
      } catch (Exception e) {
        Log.d(LOGTAG, "scheduler error : " + e.getMessage());
      }
    }

    public void stop() {
      Log.d(LOGTAG, "position query stop");
      if (scheduler != null) {
        scheduler.shutdownNow();
        scheduler = null;
      }
    }

    public void run() {
      Log.d(LOGTAG, "Sending position query");
      if (!connected || client == null) {
        stop();
        return;
      }
      if (scheduler != null) {
        client.sendCommand(positionCmd);
      }
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
        connect(d.getHost(), d.getPort());
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
    pQuery = new PositionQuery();

    playPauseB = (ImageButton) findViewById(R.id.play_pause);
    connectB = (Button) findViewById(R.id.connect_button);
    titleTV = (TextView) findViewById(R.id.title_text);
    artistTV = (TextView) findViewById(R.id.artist_text);
    albumTV = (TextView) findViewById(R.id.album_text);
    playbackTV = (TextView) findViewById(R.id.playback_text);
    positionPB = (ProgressBar) findViewById(R.id.progress_bar);
    pager = (ViewPager) findViewById(R.id.pager);
    pager.setPageMargin(200);
    adapter = new ImageViewAdapter(this.getSupportFragmentManager(), pager);
    adapter.setOnPreviousNextListener(this);
  }

  @Override
  public void onStart() {
    super.onStart();
    SharedPreferences serverParams = getPreferences(MODE_PRIVATE);
    String serverUrl = serverParams.getString("url", null);
    int serverPort = serverParams.getInt("port", 0);
    if (serverUrl != null && serverPort != 0) {
      connect(serverUrl, serverPort);
    }

  }

  @Override
  public void onPause() {
    super.onPause();
    pQuery.stop();
  }

  @Override
  public void onResume() {
    super.onResume();
    pQuery.start();
  }

  @Override
  public void onStop() {
    super.onStop();
    SharedPreferences serverParams = getPreferences(MODE_PRIVATE);
    SharedPreferences.Editor editor = serverParams.edit();
    if (connected) {
      editor.putString("url", client.getHost());
      editor.putInt("port", client.getPort());
      client.stop();
      client = null;
    }

    editor.commit();
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.menu, menu);
    return true;
  }

  @Override
  public boolean onPrepareOptionsMenu (Menu menu) {
    if (profiles == null) {
      return true;
    }

    Menu profileMenu = menu.findItem(R.id.profile_menu).getSubMenu();
    if (profileMenu == null) {
      Log.d(LOGTAG, "The profile menu item had no submenu!");
      return true;
    }

    for (int i=0; i<profiles.length; i++) {
      profileMenu.add(profiles[i]);
    }

    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    if (!connected || client == null) {
      Log.d(LOGTAG, "We're not connected, can't send profile change command!");
      return super.onOptionsItemSelected(item);
    }

    switch (item.getItemId()) {
      case R.id.sleep:
        client.sendCommand(sleepCmd);
        return true;
      case R.id.profile_menu:
        return super.onOptionsItemSelected(item);
      default:
        if (profiles != null) {
          client.sendCommand(profileCmd + item.getTitle());
          clearData();
          return true;
        }
        return super.onOptionsItemSelected(item);
  }
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

  public void connect(final String host, final int port) {
    final RemoteClient context = this;
    Log.d(LOGTAG, "Connecting to " + host + ":" + port);
    Thread networkThread = new Thread(new Runnable() {
      public void run() {
        try {
          client = new TcpClient(host, port, context);
          context.runOnUiThread(new Runnable() {
            public void run() {
              setConnected(host, port);
            }
          });
        } catch (final Exception ex) {
          Log.d(LOGTAG, "Error on TcpClient() : " + ex.getMessage());
          context.runOnUiThread(new Runnable() {
            public void run() {
              connectB.setText(R.string.unco);
              Toast.makeText(context, ex.getMessage(), Toast.LENGTH_LONG).show();
            }
          });
          if (client != null) {
            client.stop();
            client = null;
          }
        }
      }
    });
    networkThread.start();
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
    Log.d(LOGTAG, "Dialog listener creation");
    SharedPreferences serverParams = getPreferences(MODE_PRIVATE);
    String serverUrl = serverParams.getString("url", null);
    int serverPort = serverParams.getInt("port", 0);
    Log.d(LOGTAG, "Last server : " + serverUrl + " Port : " + serverPort);
    connectD.show();
    if (serverUrl != null && serverPort != 0) {
      connectD.setHost(serverUrl);
      connectD.setPort(serverPort);
    }
  }

  public void onPrevious() {
    previous(null);
  }

  public void onNext() {
    next(null);
  }

  public void previous(View previousButton) {
    if (!connected) {
      Log.d(LOGTAG, "Error aksed for previous while unconnected!");
      return;
    }

    if (client == null) {
      // We've been disconnected
      Log.d(LOGTAG, "The remote has been disconnected");
      return;
    }

    client.sendCommand(prevCmd);
  }

  public void next(View nextButton) {
    if (!connected) {
      Log.d(LOGTAG, "Error aksed for next while unconnected!");
      return;
    }

    if (client == null) {
      // We've been disconnected
      Log.d(LOGTAG, "The remote has been disconnected");
      return;
    }

    client.sendCommand(nextCmd);
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

    if (playing) {
      client.sendCommand(pauseCmd);
    } else {
      client.sendCommand(playCmd);
    }
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
    pQuery.start();
    Intent playback = new Intent(this, PlaybackService.class);
    playback.putExtra("Host", host);
    playback.putExtra("Port", port);
    startService(playback);
  }

  public void clearData() {
    setTitle("-");
    setArtist("-");
    setAlbum("-");
    adapter.setBackground(R.drawable.cover_unknown);
    setPlaybackStatus("Stopped");
  }

  public void setDisconnected() {
    if (!connected) {
      return;
    }

    connected = false;
    connectB.setText(R.string.unco);
    playPauseB.setClickable(false);
    pQuery.stop();
    client = null;
    clearData();
  }

  public void setPlaybackStatus(String status) {
    if (status.equals("Paused")) {
      playPauseB.setImageResource(R.drawable.remote_music_play);
      playbackTV.setText("Paused");
      playing = false;
      pQuery.stop();
    } else if (status.equals("Stopped")) {
      playPauseB.setImageResource(R.drawable.remote_music_play);
      playbackTV.setText("Stopped");
      playing = false;
      pQuery.stop();
    } else {
      playPauseB.setImageResource(R.drawable.remote_music_pause);
      playbackTV.setText("Playing");
      playing = true;
      pQuery.start();
    }
  }

  public void setTitle(String title) {
    titleTV.setText(title);
  }

  public void setArtist(String artist) {
    artistTV.setText(artist);
  }

  public void setAlbum(String album) {
    albumTV.setText(album);
  }

  public void setCoverArt(byte[] file) {
    if (file == null) {
      return;
    }

    Bitmap cover = BitmapFactory.decodeByteArray(file, 0, file.length);
    if (cover == null) {
      Log.d(LOGTAG, "Could not decode bitmap");
      return;
    }

    adapter.setBackground(cover);
  }

  public void setProfiles(String arg) {
    profiles = arg.split(" ");
    invalidateOptionsMenu();
  }

  public void setPosition(long position) {
    if (trackLength <= 0 || position > trackLength) {
      Log.d(LOGTAG, "Tried to set position superior than trackLength");
      return;
    }

    int progress = (int)((100*position)/trackLength);
    Log.d(LOGTAG, "Setting progress to : " + progress);
    positionPB.setProgress(progress);
  }

  public void setTrackLength(long tl) {
    trackLength = tl;
  }
}

