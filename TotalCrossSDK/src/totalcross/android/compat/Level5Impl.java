/*********************************************************************************
 *  TotalCross Software Development Kit                                          *
 *  Copyright (C) 2000-2011 SuperWaba Ltda.                                      *
 *  All Rights Reserved                                                          *
 *                                                                               *
 *  This library and virtual machine is distributed in the hope that it will     *
 *  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                         *
 *                                                                               *
 *  This file is covered by the GNU LESSER GENERAL PUBLIC LICENSE VERSION 3.0    *
 *  A copy of this license is located in file license.txt at the root of this    *
 *  SDK or can be downloaded here:                                               *
 *  http://www.gnu.org/licenses/lgpl-3.0.txt                                     *
 *                                                                               *
 *********************************************************************************/

package totalcross.android.compat;

import totalcross.*;

import java.io.*;
import java.util.*;

import android.bluetooth.*;
import android.content.*;
import android.hardware.Camera.*;
import android.os.*;

public class Level5Impl extends Level5
{
   //private static final UUID MY_UUID  = UUID.fromString("e3b9f92c-3226-4ccb-9e0a-218695bd402c");
   private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
   
   BluetoothAdapter btAdapter;
   
   public Level5Impl()
   {
      btAdapter = BluetoothAdapter.getDefaultAdapter();
   }

   public void processMessage(Bundle b) 
   {
      int type = b.getInt("subtype");
      if (type == BT_IS_SUPPORTED && btAdapter == null)
         setResponse(ERROR, null);
      else
         try
         {
            switch (type)
            {
               case BT_IS_SUPPORTED: //
                  setResponse(btAdapter != null, null);
                  break;
               case BT_ACTIVATE: //
                  setResponse(btAdapter.isEnabled() || btAdapter.enable(),null);
                  break;
               case BT_DEACTIVATE: //
                  setResponse(!btAdapter.isEnabled() || btAdapter.disable(),null);
                  break;
               case BT_GET_UNPAIRED_DEVICES:
                  btGetUnpairedDevices();
                  break;
               case BT_GET_PAIRED_DEVICES:
                  btGetPairedDevices();
                  break;
               case BT_MAKE_DISCOVERABLE: //
                  btMakeDiscoverable();
                  break;
               case BT_CONNECT:
                  btConnect(b.getString("param"));
                  break;
               case BT_READ:
                  btReadWrite(true, b.getString("param"), b.getByteArray("bytes"), b.getInt("ofs"), b.getInt("len"));
                  break;
               case BT_WRITE:
                  btReadWrite(false, b.getString("param"), b.getByteArray("bytes"), b.getInt("ofs"), b.getInt("len"));
                  break;
               case BT_CLOSE:
                  btClose(b.getString("param"));
                  break;
               case BT_IS_RADIO_ON: //
                  setResponse(btAdapter.getState() == BluetoothAdapter.STATE_ON, null);
                  break;
               case BT_IS_DISCOVERABLE: //
                  setResponse(btAdapter.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE, null);
                  break;
            }
         }
         catch (Exception e)
         {
            AndroidUtils.handleException(e,false);
            setResponse(false,null);
         }
   }      
   
   private void btReadWrite(boolean isRead, String btc, byte[] byteArray, int ofs, int count) throws Exception
   {
      BluetoothSocket sock = htbt.get(btc);
      if (sock == null)
         throw new IOException("socket for device "+btc+" not found on hashtable");
      if (isRead)
      {
         InputStream is = sock.getInputStream();
         if (is == null)
            setResponse(ERROR,null);
         else
         {
            int n = is.read(byteArray, ofs, count);
            setResponse(n,null);
         }
      }
      else
      {
         OutputStream os = sock.getOutputStream();
         if (os == null)
            setResponse(ERROR,null);
         else
         {
            os.write(byteArray, ofs, count);
            setResponse(count,null);
         }
      }
   }

   private void btClose(String btc)
   {
      BluetoothSocket sock = htbt.get(btc);
      if (sock != null)
      {
         try {sock.close();} catch (Exception e) {AndroidUtils.handleException(e,false);}
         htbt.remove(btc);
      }
      setResponse(sock != null, null);
   }

   Hashtable<String,BluetoothSocket> htbt = new Hashtable<String,BluetoothSocket>(5);
   
/*   private boolean isPaired(String addr)
   {
      println("listing paired devices and searching for "+addr);
      Set<BluetoothDevice> pairedDevices = btAdapter.getBondedDevices();
      // If there are paired devices
      for (BluetoothDevice dev : pairedDevices)
      {
         String a = dev.getAddress().replace(":","");
         println("Found "+a);
         if (a.equals(addr))
            return true;
      }
      return false;
   }
   
   private void restartBT()
   {
      println("Disabling...");
      btAdapter.disable();
      while (btAdapter.getState() != BluetoothAdapter.STATE_OFF)
         try {Thread.sleep(100);} catch (Exception eee) {}
      println("Enabling...");
      btAdapter.enable();
      while (btAdapter.getState() != BluetoothAdapter.STATE_ON)
      {
         println("Waiting enable: "+btAdapter.getState());
         try {Thread.sleep(500);} catch (Exception eee) {}
      }
   }
*/   
   private void btConnect(String addr) throws Exception
   {
      BluetoothSocket sock = htbt.get(addr);
      if (sock == null)
      {
         if (btAdapter.isDiscovering()) // unlikely to occur but a good practice 
            btAdapter.cancelDiscovery();
         String formattedAddr = formatAddress(addr);
         BluetoothDevice device = btAdapter.getRemoteDevice(formattedAddr);
         sock = device.createRfcommSocketToServiceRecord(SPP_UUID);
         while (true)
            try
            {
               sock.connect();
               break;
            }
            catch (java.io.IOException e)
            {
               AndroidUtils.handleException(e,false);
               String msg = e.getMessage();
               if (msg != null && msg.indexOf("discovery failed") >= 0) // invalid password
               {
                  if (device.getBondState() == BluetoothDevice.BOND_BONDING)
                  {
                     println("STILL BONDING");
                     for (int i = 0; i < 20 && device.getBondState() == BluetoothDevice.BOND_BONDING; i++)
                     {
                        println("Waiting "+(20-i)+" seconds before giving up.");
                        try {Thread.sleep(1000);} catch (Exception ee) {}
                     }
                     if (device.getBondState() == BluetoothDevice.BOND_BONDED)
                     {
                        println("BONDED! TRYING AGAIN");
                        continue;
                     }
                  }
                  setResponse(INVALID_PASSWORD,null);
                  return;
               }
               else
               {
                  e.fillInStackTrace();
                  throw e;
               }
            }
         println("sock "+sock+" connected on device "+addr);
         htbt.put(addr,sock);
      }
      setResponse(sock != null,null);
   }

   private static String formatAddress(String s)
   {
      char[] out = new char[17];
      for (int j = 0, i = 0, k = 0; j < 6; j++)
      {
         out[k++] = s.charAt(i++); 
         out[k++] = s.charAt(i++); 
         if (j < 5) out[k++] = ':';
      }
      return new String(out);
   }

   private void btMakeDiscoverable()
   {
      if (btAdapter.getScanMode() != BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) 
      {
         Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
         discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
         Launcher4A.loader.startActivityForResult(discoverableIntent, BT_MAKE_DISCOVERABLE);
      }
      else setResponse(true,null);
   }

   Vector<String> devices = new Vector<String>(10);
   
   private void btGetPairedDevices()
   {
      devices.removeAllElements();
      Set<BluetoothDevice> pairedDevices = btAdapter.getBondedDevices();
      // If there are paired devices
      if (pairedDevices.size() > 0) 
          for (BluetoothDevice device : pairedDevices)
             devices.add(device.getAddress().replace(":","")+"|"+device.getName());
      String[] ret = null;
      if (devices.size() > 0) devices.copyInto(ret = new String[devices.size()]);
      setResponse(true,ret);
   }
   
   private static void println(String s)
   {
      AndroidUtils.debug("*** "+s);
   }
   
   private final BroadcastReceiver mReceiver = new BroadcastReceiver() 
   {
      public void onReceive(Context context, Intent intent) 
      {
         String action = intent.getAction();
         // When discovery finds a device
         if (BluetoothDevice.ACTION_FOUND.equals(action)) 
         {
            // Get the BluetoothDevice object from the Intent
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            String name = device.getName();
            String btd = device.getAddress().replace(":","")+"|"+name;
            if (name != null && !devices.contains(btd))
               devices.add(btd);
         }
         else
         if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action))
         {
            Launcher4A.loader.unregisterReceiver(this);
            String[] ret = null;
            if (devices.size() > 0) devices.copyInto(ret = new String[devices.size()]);
            setResponse(true,ret);
         }
      }
   };

   private void btGetUnpairedDevices()
   {
      devices.removeAllElements();
      Launcher4A.loader.registerReceiver(mReceiver, new IntentFilter(BluetoothDevice.ACTION_FOUND));
      Launcher4A.loader.registerReceiver(mReceiver, new IntentFilter(BluetoothAdapter.ACTION_DISCOVERY_FINISHED));
      btAdapter.startDiscovery();
   }
   
   ///////////////////  CAMERA METHODS ////////////////////
   public void setPictureParameters(Parameters parameters, int stillQuality, int ww, int hh)
   {
      parameters.setPreviewSize(ww,hh);
      parameters.setJpegQuality(stillQuality == 1 ? 75 : stillQuality == 2 ? 85 : 100);
   }
   public List<Size> getSupportedPictureSizes(Parameters parameters)
   {
      return parameters.getSupportedPictureSizes();
   }
}
