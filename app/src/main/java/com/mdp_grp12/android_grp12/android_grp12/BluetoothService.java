package com.mdp_grp12.android_grp12.android_grp12;


import android.annotation.SuppressLint;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.widget.Toast;
import android.util.Log;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.UUID;

public class BluetoothService {
    public static final UUID myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private static final String TAG = "BluetoothServ";
    public static boolean BluetoothConnectionStatus = false;
    private static ConnectedThread myConnectedThread;
    private final BluetoothAdapter myBluetoothAdapter;
    public static BluetoothDevice myBluetoothDevice;
    private ConnectThread myConnectThread;
    private BluetoothDevice myDevice;
    private UUID deviceUUID;

    Context myContext;
    ProgressDialog myProgressDialog;
    Intent connectionStatus;

    public BluetoothService(Context context) {
        this.myBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        this.myContext = context;
    }

    /*
     * Class that initiates the bluetooth socket connection
     */
    private class ConnectThread extends Thread {
        private BluetoothSocket mySocket;

        public ConnectThread(BluetoothDevice device, UUID u) {
            myDevice = device;
            deviceUUID = u;
        }

        @SuppressLint("MissingPermission")
        public void run() {
            BluetoothSocket tmp = null;

            try {
                tmp = myDevice.createRfcommSocketToServiceRecord(deviceUUID);
            } catch (IOException ignored) {
            }

            mySocket = tmp;
            myBluetoothAdapter.cancelDiscovery();

            try {
                mySocket.connect();
                connected(mySocket, myDevice);
            } catch (IOException e) {
                try {
                    mySocket.close();
                } catch (IOException e1) {
                    e1.printStackTrace();
                }

                try {
                    Bluetooth mBluetoothActivity = (Bluetooth) myContext;
                    mBluetoothActivity.runOnUiThread(() -> Toast
                            .makeText(myContext, "Failed to connect to the device.", Toast.LENGTH_SHORT).show());
                } catch (Exception z) {
                    z.printStackTrace();
                }
            }

            try {
                myProgressDialog.dismiss();
            } catch (NullPointerException e) {
                e.printStackTrace();
            }
        }

        public void cancel() {
            try {
                mySocket.close();
            } catch (IOException ignored) {
            }
        }
    }

    /*
     * Starts the bluetooth connection with client
     */
    public void startClientThread(BluetoothDevice device, UUID uuid) {
        try {
            myBluetoothDevice = device;
            myProgressDialog = ProgressDialog.show(myContext, "Connecting Bluetooth", "Please Wait...", true);
        } catch (Exception e) {
            Log.d(TAG, "Failed to connect!");
            e.printStackTrace();
        }

        myConnectThread = new ConnectThread(device, uuid);
        myConnectThread.start();
    }

    /*
     * @SuppressLint("MissingPermission")
     * public void fastConnect() {
     * Log.d(TAG, myBluetoothDevice.getName());
     * myConnectThread = new ConnectThread(myBluetoothDevice, myUUID);
     * myConnectThread.start();
     * }
     */

    private class ConnectedThread extends Thread {
        private final InputStream inStream;
        private final OutputStream outStream;

        public ConnectedThread(BluetoothSocket socket) {
            connectionStatus = new Intent("ConnectionStatus");
            connectionStatus.putExtra("Status", "connected");
            connectionStatus.putExtra("Device", myDevice);
            LocalBroadcastManager.getInstance(myContext).sendBroadcast(connectionStatus);
            BluetoothConnectionStatus = true;

            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            try {
                tmpIn = socket.getInputStream();
                tmpOut = socket.getOutputStream();
            } catch (IOException e) {
                e.printStackTrace();
            }

            inStream = tmpIn;
            outStream = tmpOut;
        }

        public void run() {
            byte[] buffer = new byte[1024];
            int bytes;

            while (true) {
                try {
                    bytes = inStream.read(buffer);
                    String incomingmessage = new String(buffer, 0, bytes);

                    Intent incomingMessageIntent = new Intent("incomingMessage");
                    incomingMessageIntent.putExtra("receivedMessage", incomingmessage);

                    LocalBroadcastManager.getInstance(myContext).sendBroadcast(incomingMessageIntent);
                } catch (IOException e) {
                    connectionStatus = new Intent("ConnectionStatus");
                    connectionStatus.putExtra("Status", "disconnected");
                    connectionStatus.putExtra("Device", myDevice);
                    LocalBroadcastManager.getInstance(myContext).sendBroadcast(connectionStatus);
                    BluetoothConnectionStatus = false;
                    break;
                }
            }
        }

        public void write(byte[] bytes) {
            try {
                outStream.write(bytes);
                Log.d(TAG, "I'm sending out messages");
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private void connected(BluetoothSocket mySocket, BluetoothDevice device) {
        myDevice = device;
        myConnectedThread = new ConnectedThread(mySocket);
        myConnectedThread.start();
    }

    public static void write(byte[] out) {
        myConnectedThread.write(out);
    }

    public static boolean sendMessage(String message) {
        if (BluetoothConnectionStatus == true) {
            byte[] bytes = message.getBytes(Charset.defaultCharset());
            BluetoothService.write(bytes);
            return true;
        }
        return false;
    }
}


//
//public class BluetoothService {
//    public static final UUID myUUID = UUID.fromString("01010111-1111-0000-8000-12203E4C31DA");
//    private static final String TAG = "BluetoothServ";
//    public static boolean BluetoothConnectionStatus = false;
//    private static ConnectedThread myConnectedThread;
//    private final BluetoothAdapter myBluetoothAdapter;
//    public static BluetoothDevice myBluetoothDevice;
//    private ConnectThread myConnectThread;
//    private BluetoothDevice myDevice;
//    private UUID deviceUUID;
//
//    Context myContext;
//    ProgressDialog myProgressDialog;
//    Intent connectionStatus;
//
//    public BluetoothService(Context context) {
//        this.myBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
//        this.myContext = context;
//    }
//
//    /**
//     * Class that is responsible for creating and managing the Bluetooth socket connection
//     * Includes methods for initializing the connection and handling exceptions
//     */
//    private class ConnectThread extends Thread {
//        private BluetoothSocket mySocket;
//
//        public ConnectThread(BluetoothDevice device, UUID u) {
//            myDevice = device;
//            deviceUUID = u;
//        }
//
//        @SuppressLint("MissingPermission")
//        public void run() {
//            BluetoothSocket tmp = null;
//            // Using the BluetoothDevice, get a BluetoothSocket by calling createRfcommSocketToServiceRecord(UUID)
//            // this method inits a BluetoothSocket object that allows client to connect to a BluetoothDevice.
//            // The UUID passed here must used by the server device when it called listenUsingRfcommWithServiceRecord(String, UUID) to open its BluetoothServerSocket
//            // To use a matching UUID, hard-code the UUID string into your app, and then reference it from both the server and client code
//            try {
//                tmp = myDevice.createRfcommSocketToServiceRecord(deviceUUID);
//            } catch (IOException e) {
//                Log.e(TAG, "Socket's create() method failed", e);
//            }
//
//            mySocket = tmp;
//            // Cancel discovery because it otherwise slows down the connection.
//            myBluetoothAdapter.cancelDiscovery();
//
//            try {
//                // Connect to the remote device through the socket. This call blocks
//                // until it succeeds or throws an exception.
//                mySocket.connect();
//                connected(mySocket, myDevice);
//            } catch (IOException connectException) {
//                // Unable to connect; close the socket and return
//                try {
//                    mySocket.close();
//                } catch (IOException closeException) {
//                    Log.e(TAG, "Could not close the client socket", closeException);
//                }
//
//                // TODO: find out what this does
//                try {
//                    Bluetooth mBluetoothActivity = (Bluetooth) myContext;
//                    mBluetoothActivity.runOnUiThread(() -> Toast
//                            .makeText(myContext, "Failed to connect to the device.", Toast.LENGTH_SHORT).show());
//                } catch (Exception z) {
//                    z.printStackTrace();
//                }
//            }
//
//            try {
//                myProgressDialog.dismiss();
//            } catch (NullPointerException e) {
//                e.printStackTrace();
//            }
//        }
//
//        // Closes the client socket and causes the thread to finish.
//        public void cancel() {
//            try {
//                mySocket.close();
//            } catch (IOException ignored) {
//            }
//        }
//    }
//
//
//    /*
//     * Method for starting the Bluetooth connection with a remote device. initialises a 'connectThread' and starts it
//     */
//    public void startClientThread(BluetoothDevice device, UUID uuid) {
//        try {
//            myBluetoothDevice = device;
//            myProgressDialog = ProgressDialog.show(myContext, "Connecting Bluetooth", "Please Wait...", true);
//        } catch (Exception e) {
//            Log.d(TAG, "Failed to connect!");
//            e.printStackTrace();
//        }
//
//        myConnectThread = new ConnectThread(device, uuid);
//        myConnectThread.start();
//    }
//
//    /**
//     * Class that is responsible for handling the connected Bluetooth socket.
//     * Includes methods for reading incoming data and writing data to the connected device.
//     */
//    private class ConnectedThread extends Thread {
//        private final InputStream inStream;
//        private final OutputStream outStream;
//
//        public ConnectedThread(BluetoothSocket socket) {
//            connectionStatus = new Intent("ConnectionStatus");
//            connectionStatus.putExtra("Status", "connected");
//            connectionStatus.putExtra("Device", myDevice);
//            LocalBroadcastManager.getInstance(myContext).sendBroadcast(connectionStatus);
//            BluetoothConnectionStatus = true;
//
//            InputStream tmpIn = null;
//            OutputStream tmpOut = null;
//            // Get the input and output streams; using temp objects because
//            // member streams are final.
//
//            try {
//                tmpIn = socket.getInputStream();
//                tmpOut = socket.getOutputStream();
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//
//            inStream = tmpIn;
//            outStream = tmpOut;
//        }
//
//        public void run() {
//            byte[] buffer = new byte[1024];
//            int bytes;
//
//            while (true) {
//                try {
//                    // Read from the input stream
//                    bytes = inStream.read(buffer);
//                    String incomingMsg = new String(buffer, 0, bytes);
//
//                    Intent incomingMessageIntent = new Intent("incomingMessage");
//                    incomingMessageIntent.putExtra("receivedMessage", incomingMsg);
//
//                    // LocalBroadcastManager helps to send broadcast intents. These intents are
//                    // used to notify other parts of the app about the connection status and incoming messages
//                    LocalBroadcastManager.getInstance(myContext).sendBroadcast(incomingMessageIntent);
//                } catch (IOException e) {
//                    connectionStatus = new Intent("ConnectionStatus");
//
//                    // use putExtra() to store data into the IntentObject
//                    connectionStatus.putExtra("Status", "disconnected");
//                    connectionStatus.putExtra("Device", myDevice);
//
//                    LocalBroadcastManager.getInstance(myContext).sendBroadcast(connectionStatus);
//                    BluetoothConnectionStatus = false;
//                    break;
//                }
//            }
//        }
//
//        public void write(byte[] bytes) {
//            try {
//                outStream.write(bytes);
//                Log.d(TAG, "I'm sending out messages");
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//        }
//    }
//
//    private void connected(BluetoothSocket mySocket, BluetoothDevice device) {
//        myDevice = device;
//        myConnectedThread = new ConnectedThread(mySocket);
//        myConnectedThread.start();
//    }
//
//    // Method to send data (byte array) over the bluetooth connection
//    public static void write(byte[] out) {
//        myConnectedThread.write(out);
//    }
//
//    // Method to send a text message over the bluetooth connection. It converts the
//    // message to bytes and uses the 'write' method to send it.
//    public static boolean sendMessage(String message) {
//        if (BluetoothConnectionStatus == true) {
//            byte[] bytes = message.getBytes(Charset.defaultCharset());
//            BluetoothService.write(bytes);
//            return true;
//        }
//        return false;
//    }
//
//}
