/*********************************************************************************
 *  TotalCross Software Development Kit                                          *
 *  Copyright (C) 2000-2012 SuperWaba Ltda.                                      *
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



package totalcross.android;

import totalcross.*;

import java.lang.reflect.*;

import android.content.*;
import android.content.res.*;
import android.net.wifi.*;
import android.os.*;
import android.provider.*;
import android.telephony.*;

public final class Settings4A
{
   public static final byte DATE_MDY = 1;
   public static final byte DATE_DMY = 2;
   public static final byte DATE_YMD = 3;   
   
   // date format
   public static byte dateFormat;
   public static char dateSeparator;
   public static byte weekStart;

   // time format
   public static boolean is24Hour;
   public static char timeSeparator;

   // number format
   public static char thousandsSeparator;
   public static char decimalSeparator;

   // graphics
   public static int screenWidth;
   public static int screenHeight;
   public static int screenBPP;
   
   // platform
   public static String deviceId;
   public static int romVersion;
   
   // locale
   public static boolean daylightSavings;
   public static int timeZone;
   public static String timeZoneStr;
   
   // identification
   public static String userName;
   public static String imei;
   public static String esn;   
   public static String iccid;
   public static String serialNumber;

   // device capabilities
   public static boolean virtualKeyboard;
   public static boolean keypadOnly;

   public static void refresh()
   {
      settingsRefresh();
   }
   
	// this class can't be instantiated
	private Settings4A()
	{
	   
	}
	
	static void fillSettings(boolean isActivationVM)
	{
	   Context ctx = Launcher4A.instance.getContext();
	   String id1,id2;
      // platform
      romVersion = Build.VERSION.SDK_INT;    
      deviceId = Build.MANUFACTURER + " " + Build.MODEL;
      
      // userName
      userName = null; // still looking for a way to retrieve this on droid.	   
	   
      // imei
      TelephonyManager telephonyMgr = (TelephonyManager) Launcher4A.loader.getSystemService(Context.TELEPHONY_SERVICE);
      // handle dual-sim phones. Usually, they overload the method with a
      Class<? extends TelephonyManager> cc = telephonyMgr.getClass();
      Method[] mtds = cc.getDeclaredMethods();
      int toFind = 2;
      for (int i = mtds.length; --i >= 0;)
      {
         Method m = mtds[i];
         String signat = m.toString();
         String name = m.getName();
         if (name.startsWith("getDeviceId") && signat.endsWith("(int)"))
         {
            try
            {
               id1 = (String)m.invoke(telephonyMgr, new Integer(0));
               id2 = (String)m.invoke(telephonyMgr, new Integer(0));
               if (id1 != null && id1.equals(id2))  // some devices return a dumb imei each time getDeviceId is called
                  imei = id1;
               if (--toFind == 0) break;
            }
            catch (Exception ee)
            {
               AndroidUtils.handleException(ee,false);
            }
         }
         else
         if (name.startsWith("getSimSerialNumber") && signat.endsWith("(int)"))
         {
            try
            {
               id1 = (String)m.invoke(telephonyMgr, new Integer(0));
               id2 = (String)m.invoke(telephonyMgr, new Integer(0));
               if (id1 != null && id1.equals(id2))  // some devices return a dumb imei each time getDeviceId is called
                  iccid = id1;
               if (--toFind == 0) break;
            }
            catch (Exception ee)
            {
               AndroidUtils.handleException(ee,false);
            }
         }
      }
         
      if (imei == null)
      {
         id1 = telephonyMgr.getDeviceId(); // try to get the imei
         id2 = telephonyMgr.getDeviceId();
         if (id1 != null && id1.equals(id2))  // some devices return a dumb imei each time getDeviceId is called
            imei = id1;
      }

      // iccid
      if (iccid == null)
      {
         id1 = telephonyMgr.getSimSerialNumber();
         id2 = telephonyMgr.getSimSerialNumber();
         if (id1 != null && id1.equals(id2))
            iccid = id1;
      }

      // if using a new device, get its serial number. otherwise, create one from the mac-address
      if (romVersion >= 9) // gingerbread
         try
         {
            Class<Build> c = android.os.Build.class;
            Field f = c.getField("SERIAL");
            serialNumber = (String)f.get(null);
         }
         catch (NoSuchFieldError nsfe) {}
         catch (Throwable t) {}
      
      if (serialNumber == null && !Loader.IS_EMULATOR) // no else here!
      {
         WifiManager wifiMan = (WifiManager) ctx.getSystemService(Context.WIFI_SERVICE);
         if (wifiMan != null) // not sure what happens when device has no connectivity at all
         {
            String macAddr = wifiMan.getConnectionInfo().getMacAddress();
            if (macAddr == null) // if wifi never turned on since last boot, turn it on and off to be able to get the mac (on android, the mac is cached by the O.S.)
            {
               wifiMan.setWifiEnabled(true);
               while (!wifiMan.isWifiEnabled()) // wait until its active
                  try {Thread.sleep(100);} catch (Exception e) {}
               wifiMan.setWifiEnabled(false);
               macAddr = wifiMan.getConnectionInfo().getMacAddress();
            }
            if (macAddr != null)
               serialNumber = String.valueOf(((long)macAddr.replace(":","").hashCode() & 0xFFFFFFFFFFFFFFL));
         }
      }
      
      // virtualKeyboard
      virtualKeyboard = true; // always available on droid?
      
      // number representation
      java.text.DecimalFormatSymbols dfs = new java.text.DecimalFormatSymbols();
      thousandsSeparator = dfs.getGroupingSeparator(); if (thousandsSeparator <= ' ') thousandsSeparator = ',';
      decimalSeparator = dfs.getDecimalSeparator();	 if (decimalSeparator <= ' ') decimalSeparator = '.';
	   
      // date representation
      java.util.Calendar calendar = java.util.Calendar.getInstance();
      calendar.set(2002,11,25,20,0,0);
      java.text.DateFormat df = java.text.DateFormat.getDateInstance(java.text.DateFormat.SHORT);
      String d = df.format(calendar.getTime());
      dateFormat = d.startsWith("25") ? DATE_DMY
                                   : d.startsWith("12") ? DATE_MDY
                                   : DATE_YMD;

      dateSeparator = getFirstSymbol(d);
      
      weekStart = (byte) (calendar.getFirstDayOfWeek() - 1);
      
      // time representation
      df = java.text.DateFormat.getTimeInstance(java.text.DateFormat.SHORT); // guich@401_32
      d = df.format(calendar.getTime());
      timeSeparator = getFirstSymbol(d);
      is24Hour = d.toLowerCase().indexOf("am") == -1 && d.toLowerCase().indexOf("pm") == -1;
      
      settingsRefresh();
	   
	   if (!Loader.IS_EMULATOR) // running on emulator, right now there's no way to retrieve more settings from it.
	   {
	      ContentResolver cr = Launcher4A.loader.getContentResolver();
	      
	      Configuration config = new Configuration();
	      Settings.System.getConfiguration(cr, config);
	      if (config.keyboard == Configuration.KEYBOARD_12KEY)
	         keypadOnly = true;
	      
	      // is24Hour
	      is24Hour = Settings.System.getInt(cr, Settings.System.TIME_12_24, is24Hour ? 24 : 12) == 24;
	      
	      // date format
	      String format = Settings.System.getString(cr, Settings.System.DATE_FORMAT);
	      if (format != null && format.length() > 0)
	      {
	         char firstChar = format.charAt(0);
	         dateFormat = firstChar == 'd' ? DATE_DMY
                  : firstChar == 'M' ? DATE_MDY
                  : DATE_YMD;
	      }
	   }
	}
	
   public static void settingsRefresh()
   {
      java.util.Calendar cal = java.util.Calendar.getInstance();
      daylightSavings = cal.get(java.util.Calendar.DST_OFFSET) != 0;
      java.util.TimeZone tz = java.util.TimeZone.getDefault();
      timeZone = tz.getRawOffset() / (60*60*1000);
      timeZoneStr = java.util.TimeZone.getDefault().getID();
   }
   
   private static char getFirstSymbol(String s)
   {
      char []c = s.toCharArray();
      for (int i =0; i < c.length; i++)
         if (c[i] != ' ' && !('0' <= c[i] && c[i] <= '9'))
            return c[i];
      return ' ';
   }
}