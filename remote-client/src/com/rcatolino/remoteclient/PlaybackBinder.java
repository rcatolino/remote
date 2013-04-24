package com.rcatolino.remoteclient;

import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

public class PlaybackBinder extends Binder {

  public interface PipelineStateListener {
    public abstract void onPipelinePlaying();
    public abstract void onPipelinePaused();
    public abstract void onPipelineConnected();
    public abstract void onDecodeError();
  }

  private static final String LOGTAG = "RemoteClient/PlaybackBinder";

  private PlaybackService parentService;
  private PipelineStateListener pl = null;

  public void setPipelineStateListener(PipelineStateListener listener) {
    synchronized(this) {
      pl = listener;
    }
  }

  public void removePipelineStateListener() {
    synchronized(this) {
      pl = null;
    }
  }

  public void onPipelineConnected() {
    synchronized(this) {
      if (pl != null) pl.onPipelineConnected();
    }
  }

  public void onPipelinePlaying() {
    synchronized(this) {
      if (pl != null) pl.onPipelinePlaying();
    }
  }

  public void onPipelinePaused() {
    synchronized(this) {
      if (pl != null) pl.onPipelinePaused();
    }
  }

  public void onDecodeError() {
    synchronized(this) {
      if (pl != null) pl.onDecodeError();
    }
  }

  public PlaybackBinder(PlaybackService service) {
    Log.d(LOGTAG, "Creating PlaybackBinder");
    parentService = service;
  }

  public boolean isPlaybackOn() {
    return parentService.getPlaybackStatus();
  }

  public void setPlaybackStatus(boolean status) {
    parentService.setPlaybackStatus(status);
  }

  public void stopStreamingPipeline() {
    parentService.stopStreamingPipeline();
  }

  public void createStreamingPipeline(int port, boolean playing) {
    parentService.createStreamingPipeline(port, playing);
  }

}

