
package com.rcatolino.remoteclient;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;

public class ConnectionDialog extends Dialog implements OnClickListener {

  private static final String LOGTAG = "RemoteClient/ConnectionDialog";
  Button connectB;
  Button cancelB;
  EditText hostET;
  EditText portET;
  boolean shouldConnect = false;

  public ConnectionDialog(Context context) {
    super(context);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    setContentView(R.layout.connection);
    setTitle(R.string.connect_dialog);
    connectB = (Button) findViewById(R.id.connect_dialog_ok_button);
    connectB.setOnClickListener(this);
    cancelB = (Button) findViewById(R.id.connect_dialog_cancel_button);
    cancelB.setOnClickListener(this);
    hostET = (EditText) findViewById(R.id.hostET);
    portET = (EditText) findViewById(R.id.portET);
  }

  public void setHost(String host) {
    if (hostET == null) {
      Log.d(LOGTAG, "Cannot set host before inflation");
      return;
    }
    hostET.setText(host);
  }

  public String getHost() {
    return hostET.getText().toString();
  }

  public void setPort(int port) {
    if (portET == null) {
      Log.d(LOGTAG, "Cannot set port before inflation");
      return;
    }
    portET.setText(String.valueOf(port));
  }

  public int getPort() {
    return Integer.parseInt(portET.getText().toString());
  }

  public boolean shouldConnect() {
    return shouldConnect;
  }

  @Override
  public void onClick(View v) {
    if(v == connectB){
      shouldConnect = true;
      this.dismiss();
    }

    if(v == cancelB){
      shouldConnect = false;
      this.cancel();
    }
  }

}

