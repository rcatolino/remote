package com.rcatolino.remoteclient;

import android.util.Log;
import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.InputStream;
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
  private static final String LENGTH = "LENGTH";
  private static final int TRACK_LENGHT_HEADER_SIZE = 13;

  private static final String LOGTAG = "RemoteClient/TcpClient";
  private static final int MAX_COMMAND_SIZE = 255;

  private class Receiver implements Runnable {

    private DataInputStream input;
    private boolean stopped = false;
    private Thread execThread;

    public Receiver(Socket sock) throws IOException {
      input = new DataInputStream(sock.getInputStream());

      // Start thread :
      execThread = new Thread(this);
      execThread.start();
    }

    public void stop() {
      Log.d(LOGTAG, "Stopping receiver");
      synchronized(this) {
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

    private boolean shouldStop() {
      boolean shouldStop = false;
      synchronized(this) {
        shouldStop = stopped;
      }

      return shouldStop;
    }

    public void run() {
      String[] message;
      byte[] buffer = new byte[MAX_COMMAND_SIZE];
      int messageSize = 0;
      int ret;
      while (!shouldStop()) {
        try {
          Log.d(LOGTAG, "Waiting for data, available : " + input.available());
          messageSize = input.readInt();
          Log.d(LOGTAG, "Receiving " + messageSize + " bytes");
          if (messageSize < MAX_COMMAND_SIZE) {
            input.readFully(buffer, 0, messageSize);
          } else {
            Log.d(LOGTAG, "Out of band data!!");
            input.skip(messageSize);
          }
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not read message from queue : " + e.getMessage());
          break;
        }

        try {
          Log.d(LOGTAG, "received " + new String(buffer, 0, messageSize, "US-ASCII") +
                ", available : " + input.available());
          message = new String(buffer, 0, messageSize, "US-ASCII").split(" ", 2);
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not split message " + e.getMessage());
          continue;
        }

        Log.d(LOGTAG, "Received message : " + message[0]);
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
              Log.d(LOGTAG, "Invalid track data!, no argument.");
              continue;
            }
            final String track_arg = message[1];
            if (message[0].equals(TITLE)) {
              Log.d(LOGTAG, "Track title changed to " + track_arg);
            } else if (message[0].equals(ARTIST)) {
              Log.d(LOGTAG, "Track artist changed to " + track_arg);
            } else if (message[0].equals(ALBUM)) {
              Log.d(LOGTAG, "Track album changed to " + track_arg);
            } else if (message[0].equals(LENGTH)) {
              ByteBuffer bb = ByteBuffer.wrap(buffer, TRACK_LENGHT_HEADER_SIZE, 4);
              bb.order(ByteOrder.BIG_ENDIAN);
              Log.d(LOGTAG, "Track length changed to " + bb.getInt() + "ms");
            }
          } else if (message[0].equals(LOOP)) {
            Log.d(LOGTAG, "Loop status changed to " + arg);
          } else if (message[0].equals(SHUFFLE)) {
            Log.d(LOGTAG, "Shuffle status changed to " + arg);
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
    private OutputStream output;
    private boolean stopped = false;
    private Thread execThread;

    public Sender(Socket sock) throws IOException {
      toSend = new SynchronousQueue<String>();
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

    private boolean shouldStop() {
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

  public TcpClient(String host, int port, RemoteClient parent) throws IOException, IllegalArgumentException {
    ui = parent;
    // Open connection :
    sock = new Socket();
    InetSocketAddress adress = new InetSocketAddress(host, port);
    if (adress.isUnresolved()) {
      Log.d(LOGTAG, "Adress is unresolved");
      throw new IllegalArgumentException("Bad adress");
    }

    sock.connect(adress, 1000);
    sender = new Sender(sock);
    receiver = new Receiver(sock);

  }

  public void stop() {
    sender.stop();
    receiver.stop();
  }

  public void sendCommand(String command) {
    sender.sendCommand(command);
  }

}
