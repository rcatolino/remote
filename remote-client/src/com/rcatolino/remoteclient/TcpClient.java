package com.rcatolino.remoteclient;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.IllegalArgumentException;
import java.lang.Thread;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.SynchronousQueue;

public class TcpClient {

  private static final String SHUFFLE = "SHUFFLE";
  private static final String LOOP = "LOOP";
  private static final String PLAYBACK = "PLAYBACK";
  private static final String TRACK = "TRACK";
  private static final String TITLE = "TITLE";
  private static final String ALBUM = "ALBUM";
  private static final String ARTIST = "ARTIST";
  private static final String COVERART= "COVERART";
  private static final String LENGTH = "LENGTH";
  private static final String PROFILES = "PROFILES";
  private static final String POSITION = "POSITION";
  private static final String TRACKID = "ID";
  private static final int POSITION_HEADER_SIZE = 9;
  private static final int TRACK_LENGHT_HEADER_SIZE = 13;

  private static final String LOGTAG = "RemoteClient/TcpClient";
  private static final int MAX_COMMAND_SIZE = 255;

  private class Receiver implements Runnable {

    private DataInputStream input;
    private boolean stopped = false;
    private Object stoppedLock = new Object();
    private Thread execThread;

    public Receiver(Socket sock) throws IOException {
      input = new DataInputStream(sock.getInputStream());

      // Start thread :
      execThread = new Thread(this);
      execThread.start();
    }

    public void stop() {
      Log.d(LOGTAG, "Stopping receiver");
      synchronized(stoppedLock) {
        stopped = true;
      }

      Log.d(LOGTAG, "Flag set");
      try {
        input.close();
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not close input stream: " + e.getMessage());
      }
      Log.d(LOGTAG, "Closed input stream");

      try {
        execThread.join();
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not join sending thread : " + e.getMessage());
      }

      Log.d(LOGTAG, "Joined thread");
    }

    private void writeFile(int fileSize) {
      final String filename = "cover";
      int written = 0;
      FileOutputStream out = null;
      try {
        out = new FileOutputStream(filename);
      } catch (Exception e) {
        Log.d(LOGTAG, "open file error : " + e.getMessage());
      }

      while (written < fileSize) {
      }
    }

    private boolean shouldStop() {
      boolean shouldStop = false;
      synchronized (stoppedLock) {
        shouldStop = stopped;
      }

      return shouldStop;
    }

    private byte[] readFile() {
      int fileSize = 0;
      byte[] buffer = null;
      try {
        fileSize = input.readInt();
        Log.d(LOGTAG, "Receiving file of " + fileSize + " bytes");
        if (fileSize > Integer.MAX_VALUE) {
          Log.d(LOGTAG, "File too big!");
          input.skip(fileSize);
          return null;
        }

        buffer = new byte[fileSize];
        input.readFully(buffer, 0, fileSize);
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not read file : " + e.getMessage());
        return null;
      }

      return buffer;
    }

    private int readFileSize() {
      int fileSize = 0;
      try {
        fileSize = input.readInt();
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not read file size : " + e.getMessage());
        return 0;
      }

      return fileSize;
    }

    private byte[] readMessage() {
      int messageSize = 0;
      byte[] buffer = null;
      try {
        //Log.d(LOGTAG, "Reading int");
        messageSize = input.readInt();
        //Log.d(LOGTAG, "Receiving " + messageSize + " bytes");
        if (messageSize < MAX_COMMAND_SIZE) {
          buffer = new byte[messageSize];
          input.readFully(buffer, 0, messageSize);
        } else {
          Log.d(LOGTAG, "Too much data in message!!");
          input.skip(messageSize);
          return new byte[1];
        }
      } catch (Exception e) {
        Log.d(LOGTAG, "Could not readInt message : " + e.getMessage());
        return null;
      }

      return buffer;
    }

    public void run() {
      String[] message;
      int ret;
      while (!shouldStop()) {
        byte[] buffer = readMessage();
        if(buffer == null) {
          break;
        } else if (buffer.length == 1) {
          continue;
        }

        try {
          //Log.d(LOGTAG, "received " + new String(buffer, 0, buffer.length, "US-ASCII") +
          //      ", available : " + input.available());
          message = new String(buffer, 0, buffer.length, "US-ASCII").split(" ", 2);
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not split message " + e.getMessage());
          continue;
        }

        //Log.d(LOGTAG, "Received message : " + message[0]);
        if (message.length <= 1) {
          Log.d(LOGTAG, "Invalid message, no argument.");
        } else {
          final String arg = message[1];
          if (message[0].equals(PLAYBACK)) {
            ui.runOnUiThread(new Runnable() {
              public void run() {
                ui.setPlaybackStatus(arg);
              }
            });
          } else if (message[0].equals(TRACK)) {
            message = arg.split(" ", 2);
            if (message.length <= 1) {
              if (message[0].equals(COVERART)) {
                final byte[] file = readFile();
                if (file != null) {
                  Log.d(LOGTAG, "Received " + file.length);
                  ui.runOnUiThread(new Runnable() {
                    public void run() {
                      ui.setCoverArt(file);
                    }
                  });
                }
              } else {
                Log.d(LOGTAG, "Invalid track data!, no argument.");
              }
              continue;
            }
            final String track_arg = message[1];
            if (message[0].equals(TITLE)) {
              Log.d(LOGTAG, "Track title changed to " + track_arg);
              ui.runOnUiThread(new Runnable() {
                public void run() {
                  ui.setTitle(track_arg);
                }
              });
            } else if (message[0].equals(ARTIST)) {
              Log.d(LOGTAG, "Track artist changed to " + track_arg);
              ui.runOnUiThread(new Runnable() {
                public void run() {
                  ui.setArtist(track_arg);
                }
              });
            } else if (message[0].equals(ALBUM)) {
              Log.d(LOGTAG, "Track album changed to " + track_arg);
              ui.runOnUiThread(new Runnable() {
                public void run() {
                  ui.setAlbum(track_arg);
                }
              });
            } else if (message[0].equals(TRACKID)) {
              Log.d(LOGTAG, "TrackId changed to " + arg);
            } else if (message[0].equals(LENGTH)) {
              ByteBuffer bb = ByteBuffer.wrap(buffer, TRACK_LENGHT_HEADER_SIZE, 8);
              bb.order(ByteOrder.BIG_ENDIAN);
              final long tl = bb.getLong();
              Log.d(LOGTAG, "Track length changed to " + tl + "ms");
              ui.runOnUiThread(new Runnable() {
                public void run() {
                  ui.setTrackLength(tl);
                }
              });
            }
          } else if (message[0].equals(LOOP)) {
            Log.d(LOGTAG, "Loop status changed to " + arg);
          } else if (message[0].equals(SHUFFLE)) {
            Log.d(LOGTAG, "Shuffle status changed to " + arg);
          } else if (message[0].equals(POSITION)) {
            ByteBuffer bb = ByteBuffer.wrap(buffer, POSITION_HEADER_SIZE, 8);
            bb.order(ByteOrder.BIG_ENDIAN);
            final long pos = bb.getLong();
            ui.runOnUiThread(new Runnable() {
              public void run() {
                ui.setPosition(pos);
              }
            });
          } else if (message[0].equals(PROFILES)) {
            Log.d(LOGTAG, "Profiles available : " + arg);
            ui.runOnUiThread(new Runnable() {
              public void run() {
                ui.setProfiles(arg);
              }
            });
          } else {
            Log.e(LOGTAG, "Unkown message type : " + message[0]);
          }
        }

      }

      Log.d(LOGTAG, "Running finished");
      try {
        sock.close();
      } catch (Exception e) {
        Log.d(LOGTAG, "Error on sock.close : " + e.getMessage());
      }

      ui.runOnUiThread(new Runnable() {
        public void run() {
          ui.setDisconnected();
        }
      });
      Log.d(LOGTAG, "Set as disconnected");
    }

  }
  private class Sender implements Runnable {

    private SynchronousQueue<String> toSend;
    private DataOutputStream output;
    private boolean stopped = false;
    private Object stoppedLock = new Object();
    private Thread execThread;

    public Sender(Socket sock) throws IOException {
      toSend = new SynchronousQueue<String>();
      output = new DataOutputStream(sock.getOutputStream());

      // Start thread :
      execThread = new Thread(this);
      execThread.start();
    }

    public void stop() {
      Log.d(LOGTAG, "Stopping sender");
      synchronized(stoppedLock) {
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
        Log.d(LOGTAG, "Sendindg command in queue failed : " + e.getMessage());
      }
    }

    private boolean shouldStop() {
      boolean shouldStop = false;
      synchronized(stoppedLock) {
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
          output.writeLong(buffer.length);
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

      ui.runOnUiThread(new Runnable() {
        public void run() {
          ui.setDisconnected();
        }
      });
      Log.d(LOGTAG, "Set as disconnected");
    }

  }

  private Sender sender;
  private Receiver receiver;
  private Socket sock;
  private RemoteClient ui;
  private int serverPort;
  private String serverUrl;

  public TcpClient(String host, int port, int timeout, RemoteClient parent)
                   throws IOException, IllegalArgumentException {
    ui = parent;
    if (timeout <= 0) timeout = 1000;

    // Open connection :
    sock = new Socket();
    InetSocketAddress adress = new InetSocketAddress(host, port);
    if (adress.isUnresolved()) {
      Log.d(LOGTAG, "Adress is unresolved");
      throw new IllegalArgumentException("Bad adress");
    }

    sock.connect(adress, timeout);
    sender = new Sender(sock);
    receiver = new Receiver(sock);
    serverPort = port;
    serverUrl = host;
  }

  public int getPort() {
    return serverPort;
  }

  public String getHost() {
    return serverUrl;
  }

  public void stop() {
    sender.stop();
    receiver.stop();
  }

  public void sendCommand(String command) {
    sender.sendCommand(command);
  }

}
