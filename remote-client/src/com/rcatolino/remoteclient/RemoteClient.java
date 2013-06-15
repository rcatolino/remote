package com.rcatolino.remoteclient;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.IBinder;
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
import android.widget.ToggleButton;

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
                          implements ServiceConnection,
                                     PlaybackBinder.PipelineStateListener,
                                     ImageViewAdapter.OnPreviousNextListener {

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
  private static final String volumedownCmd = "VOLUMEDOWN";
  private static final String streamonCmd = "STREAM_ON";
  private static final String streamoffCmd = "STREAM_OFF";
  private static final String streamstopCmd = "STREAM_STOP";
  private boolean playing = false;
  private DialogListener dialogListener;
  private boolean connected = false;
  private TcpClient client;
  private PositionQuery pQuery;
  private long trackLength = 0;
  private PlaybackBinder binder = null;

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
    private RemoteClient parent;

    public DialogListener(RemoteClient parent) {
      this.parent = parent;
    }

    public void onCancel(DialogInterface dialog) {
      Log.d(LOGTAG, "Connection dialog canceled");
    }

    public void onDismiss(DialogInterface dialog) {
      ConnectionDialog d = (ConnectionDialog) dialog;
      if (d.shouldConnect() && !connected) {
        new Connector(parent).Connect(d.getHost(), d.getPort(), 500);
      }
    }

  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    dialogListener = new DialogListener(this);
    setContentView(R.layout.main);
    pQuery = new PositionQuery();

    ViewPager pager= (ViewPager) findViewById(R.id.pager);
    pager.setPageMargin(200);
    adapter = new ImageViewAdapter(this.getSupportFragmentManager(), pager);
    adapter.setOnPreviousNextListener(this);
  }

  @Override
  public void onStart() {
    super.onStart();
    if (binder != null) {
      binder.setPipelineStateListener(this);
    }

    new Connector(this).start();
    Log.d(LOGTAG, "Connector started");
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
    if (binder != null) {
      binder.removePipelineStateListener();
    }

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

  public void onServiceConnected(ComponentName name, IBinder playbackBinder) {
    Log.d(LOGTAG, "Service connected");
    binder = (PlaybackBinder) playbackBinder;
    binder.setPipelineStateListener(this);
    if (client == null) {
      Log.d(LOGTAG, "Can't start streaming without any connection");
      return;
    }

    binder.createStreamingPipeline(client.getPort()+1, playing);
    ToggleButton streamingB = (ToggleButton) findViewById(R.id.streaming_button);
    if (!streamingB.isChecked()) {
      streamingB.setChecked(true);
    }
  }

  public void onServiceDisconnected(ComponentName name) {
    Log.d(LOGTAG, "Service disconnected");
    binder.removePipelineStateListener();
    binder = null;
    ToggleButton streamingB= (ToggleButton) findViewById(R.id.streaming_button);
    if (streamingB.isChecked()) {
      streamingB.setChecked(false);
    }
  }

  public void onPipelineConnected() {
    Log.d(LOGTAG, "Pipeline connected");
    client.sendCommand(streamonCmd);
  }

  public void onPipelinePlaying() {
    Log.d(LOGTAG, "Pipeline playing");
    // TODO: it could be nice to have a visual cue
    // that the app is currently streaming, and playing, audio.
  }

  public void onPipelinePaused() {
    Log.d(LOGTAG, "Pipeline paused");
    client.sendCommand(streamoffCmd);
  }

  public void onDecodeError() {
    Log.d(LOGTAG, "Stream decode error");
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    int action = event.getAction();
    int keyCode = event.getKeyCode();

    if (!connected || client == null ) {
      Log.d(LOGTAG, "We're not connected, can't send Volume command");
      return super.dispatchKeyEvent(event);
    }

    if (binder != null && binder.isPlaybackOn()) {
      Log.d(LOGTAG, "Playback is on, change volume locally");
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

  private Button getConnectB() {
    return (Button) findViewById(R.id.connect_button);
  }

  private ImageButton getPlayPauseB() {
    return (ImageButton) findViewById(R.id.play_pause);
  }

  public void showConnectDialog(View connectButton) {
    if (client != null) {
      Log.d(LOGTAG, "Disconnecting");
      ((Button)connectButton).setText(R.string.disco);
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

  private void updateStreamingStatus() {
    if (binder == null) {
      Log.d(LOGTAG, "Don't update streaming status, binder is null");
      return;
    }

    binder.setPlaybackStatus(playing);
  }

  public void toggleStreaming(View streamingButton) {
    boolean on = ((ToggleButton) streamingButton).isChecked();
    if (on) {
      if(!connected || client == null) {
        Log.d(LOGTAG, "Can't start streaming without any connection");
        ((ToggleButton) streamingButton).setChecked(false);
        return;
      }

      if (binder == null) {
        Intent playback = new Intent(RemoteClient.this, PlaybackService.class);
        Log.d(LOGTAG, "Starting playback service");
        try {
          if (!bindService(playback, RemoteClient.this, BIND_AUTO_CREATE)) {
            Log.d(LOGTAG, "Couldn't bind to Playback service");
          }
        } catch (Exception e) {
          Log.d(LOGTAG, "BindService failed : " + e.getMessage());
        }
      } else {
        binder.createStreamingPipeline(client.getPort()+1, playing);
      }
    } else {
      if (binder == null) {
        Log.d(LOGTAG, "Toggle Streaming error! Tried to stop streaming without any binder");
        return;
      }

      Log.d(LOGTAG, "Toggling off playback");
      binder.stopStreamingPipeline();
      if (connected && client != null) {
        client.sendCommand(streamstopCmd);
      }
    }

  }

  public void connectionFailure(String message) {
    getConnectB().setText(R.string.unco);
    Toast.makeText(this, message, Toast.LENGTH_LONG).show();
  }

  public void setConnected(String host, int port, TcpClient newClient) {
    if (connected) {
      Log.d(LOGTAG, "Attempted to setConnected while already connected");
      return;
    }

    if (newClient == null) {
      Log.d(LOGTAG, "Attempted to setConnected whithout any client");
      return;
    }

    client = newClient;
    connected = true;
    getConnectB().setText("Connected to " + host + ":" + port);
    getPlayPauseB().setClickable(true);
    pQuery.start();
  }

  public void clearData() {
    setTitle("-");
    setArtist("-");
    setAlbum("-");
    adapter.setBackground(R.drawable.cover_unknown);
  }

  public void setDisconnected() {
    if (!connected) {
      return;
    }

    connected = false;
    getConnectB().setText(R.string.unco);
    getPlayPauseB().setClickable(false);
    pQuery.stop();
    client = null;
    clearData();
  }

  public void setPlaybackStatus(String status) {
    TextView playbackTV = (TextView) findViewById(R.id.playback_text);
    ImageButton playPauseB = getPlayPauseB();
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

    Log.d(LOGTAG, "Changing playback streaming according to playing status");
    updateStreamingStatus();
  }

  public void setTitle(String title) {
    TextView titleTV = (TextView) findViewById(R.id.title_text);
    titleTV.setText(title);
  }

  public void setArtist(String artist) {
    TextView artistTV = (TextView) findViewById(R.id.artist_text);
    artistTV.setText(artist);
  }

  public void setAlbum(String album) {
    TextView albumTV = (TextView) findViewById(R.id.album_text);
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
    ProgressBar positionPB = (ProgressBar) findViewById(R.id.progress_bar);
    positionPB.setProgress(progress);
  }

  public void setTrackLength(long tl) {
    trackLength = tl;
  }
}

