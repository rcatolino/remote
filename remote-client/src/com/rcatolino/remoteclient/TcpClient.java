package com.rcatolino.remoteclient;

import android.util.Log;

import java.io.OutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.IllegalArgumentException;
import java.lang.Thread;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.util.concurrent.SynchronousQueue;

public class TcpClient {

  private static final String LOGTAG = "RemoteClient/TcpClient";
  private static final int MAX_COMMAND_SIZE = 64;

  private class Receiver implements Runnable {

    private InputStream input;
    private boolean stopped = false;
    private Thread execThread;

    public Receiver(Socket sock) throws IOException {
      input = sock.getInputStream();

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
          messageSize = input.read();
          Log.d(LOGTAG, "Receiving " + messageSize + " bytes");
          if (messageSize < MAX_COMMAND_SIZE) {
            ret = input.read(buffer);
            if (ret == -1) {
              break;
            }
          }
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not read message from queue : " + e.getMessage());
          break;
        }

        try {
          message = new String(buffer, 0, messageSize, "US-ASCII").split(" ");
        } catch (Exception e) {
          Log.d(LOGTAG, "Could not split message " + e.getMessage());
          continue;
        }

        Log.d(LOGTAG, "Received message : " + message[0]);
        if (message.length == 0) {
          ui.runOnUiThread(new Runnable() {
            public void run() {
              ui.setPaused();
            }
          });
        } else {
          if (message[0].equals("PLAYING")) {
            final String arg = message[1];
            ui.runOnUiThread(new Runnable() {
              public void run() {
                ui.setPlaying(arg);
              }
            });
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

  }

  public void stop() {
    sender.stop();
  }

  public void sendCommand(String command) {
    sender.sendCommand(command);
  }

}
