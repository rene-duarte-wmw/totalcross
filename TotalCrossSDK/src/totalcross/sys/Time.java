/*********************************************************************************
 *  TotalCross Software Development Kit                                          *
 *  Copyright (C) 1998, 1999 Wabasoft <www.wabasoft.com>                         *
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



package totalcross.sys;

/**
 * Time identifies a date and time.
 * <p>
 * Here is an example of Time being used to display the current date:
 *
 * <pre>
 * Time t = new Time();
 * ...
 * label.setText("Today is " + t.year + "/" + t.month + "/" + t.day);
 * </pre>
 * You can also call the update method to update the time with the current system values.
 */

public final class Time
{
   /** The year as its full set of digits (year 2010 is 2010). */
   public int year;

   /** The month in the range of 1 to 12. */
   public int month;

   /** The day in the range of 1 to the last day in the month. */
   public int day;

   /** The hour in the range of 0 to 23. */
   public int hour;

   /** The minute in the range of 0 to 59. */
   public int minute;

   /** The second in the range of 0 to 59. */
   public int second;

   /** Milliseconds in the range of 0 to 999. */
   public int millis;

   public static final int SECONDS_PER_DAY = 24 * 3600;
   
   /**
   * Constructs a time object set to the current date and time.
   */
   public Time()
   {
	   update();
   }

   /** Updates the internal fields with the current timestamp.
    * @since TotalCross 1.23
    */
   public void update() // guich@tc123_65: renamed timeCreate to update and made it public
   {
      java.util.GregorianCalendar d = new java.util.GregorianCalendar();
      year = d.get(java.util.GregorianCalendar.YEAR);
      month = d.get(java.util.GregorianCalendar.MONTH) + 1;
      day = d.get(java.util.GregorianCalendar.DATE);
      hour = d.get(java.util.GregorianCalendar.HOUR_OF_DAY);
      minute = d.get(java.util.GregorianCalendar.MINUTE);
      second = d.get(java.util.GregorianCalendar.SECOND);
      millis = d.get(java.util.GregorianCalendar.MILLISECOND);
   }

   public native void update4D();

   /** Constructs a time object from the given value. 
    * @see #getTimeLong
    * @since SuperWaba 4.0
    */
   public Time(long yyyymmddhhmmss)
   {
      second = (int)(yyyymmddhhmmss % 100); yyyymmddhhmmss /= 100;
      minute = (int)(yyyymmddhhmmss % 100); yyyymmddhhmmss /= 100;
      hour   = (int)(yyyymmddhhmmss % 100); yyyymmddhhmmss /= 100;
      day    = (int)(yyyymmddhhmmss % 100); yyyymmddhhmmss /= 100;
      month  = (int)(yyyymmddhhmmss % 100); yyyymmddhhmmss /= 100;
      year   = (int)yyyymmddhhmmss;
   }

   /** Constructs a time object from the given date and time values. 
    * @since TotalCross 1.0 beta 4
    */
   public Time(int yyyymmdd, int hhmmssmmm)
   {
      millis =  hhmmssmmm % 1000; hhmmssmmm /= 1000;
      second = (hhmmssmmm % 100); hhmmssmmm /= 100;
      minute = (hhmmssmmm % 100); hhmmssmmm /= 100;
      hour   = (hhmmssmmm % 100); 
      day    = (yyyymmdd % 100); yyyymmdd /= 100;
      month  = (yyyymmdd % 100); yyyymmdd /= 100;
      year   = yyyymmdd;
   }

   /** Creates a Time object with a String in the given Iso8601 format: <pre>YYYYMMDDTHH:MM:SS</pre>.
    * @since SuperWaba 5.61
    */
   public Time(String iso8601) throws InvalidNumberException // guich@561_4
   {
      char chars[] = iso8601.toCharArray();
      year = Convert.toInt(new String(chars,0,4));
      month = Convert.toInt(new String(chars, 4, 2));
      day = Convert.toInt(new String(chars, 6, 2));
      hour = Convert.toInt(new String(chars, 9, 2));
      minute = Convert.toInt(new String(chars, 12, 2));
      second = Convert.toInt(new String(chars, 15, 2));
   }
   
   /** Constructs a Time object, parsing the String and placing the fields depending on
    * the flags that were set, using the Settings.timeSeparator as spliter.
    * The number of parts must match the number of true flags, or an ArrayIndexOutOfBoundsException will be thrown.
    * 
    * AM/PM is supported.
    * @since TotalCross 1.3
    */
   public Time(String time, boolean hasYear, boolean hasMonth, boolean hasDay, boolean hasHour, boolean hasMinute, boolean hasSeconds) throws InvalidNumberException
   {
      String timeLow = time.toLowerCase();
      if (timeLow.endsWith("AM"))
         time = time.substring(0,time.length()-2).trim();
      else
      if (timeLow.endsWith("PM"))
      {
         time = time.substring(0,time.length()-2).trim();
         hour += 12;
      }
         
      String[] parts = Convert.tokenizeString(time, Settings.timeSeparator);
      int idx = 0;
      if (hasYear) year = Convert.toInt(parts[idx++]);
      if (hasMonth) month = Convert.toInt(parts[idx++]);
      if (hasDay) day = Convert.toInt(parts[idx++]);
      if (hasHour) hour += Convert.toInt(parts[idx++]);
      if (hasMinute) minute = Convert.toInt(parts[idx++]);
      if (hasSeconds) second = Convert.toInt(parts[idx++]);
   }

   /** Returns the time in the format YYYYMMDDHHMMSS as a long value. It does
     * not include the millis.
     * @since SuperWaba 4.0
     */
   public long getTimeLong()
   {
      int i = hour * 10000 + minute * 100 + second;
      return year * 10000000000L + month * 100000000L + day * 1000000 + i;
   }

   /** Constructs a new time with the given values. The values are not checked.
     * @since SuperWaba 3.5
     */
   public Time(int year, int month, int day, int hour, int minute, int second, int millis)
   {
      this.year = year;
      this.month = month;
      this.day = day;
      this.hour = hour;
      this.minute = minute;
      this.second = second;
      this.millis = millis;
   }

   /** Returns the time in format specified in totalcross.sys.Settings (does NOT include the date neither the millis).
     * To return the date, use class totalcross.util.Date. So, to get a String with the date and time, use:
     * <pre>
     * Time t = new Time();
     * String dateAndTime = new Date(t) + " " + t;
     * </pre>
     */
   public String toString() // guich@300_56
   {
      return toString(Convert.toString(Settings.timeSeparator));
   }
   
   /** Returns the time in format specified in totalcross.sys.Settings (does NOT include the date neither the millis).
    * To return the date, use class totalcross.util.Date. So, to get a String with the date and time, use:
    * <pre>
    * Time t = new Time();
    * String dateAndTime = new Date(t) + " " + t;
    * </pre>
    * @since TotalCross 1.15
    */
   public String toString(String timeSeparator) // guich@tc115_22
   {
      return dump(new StringBuffer(20), timeSeparator, false).toString();
   }
   
   /** Dumps the time into the given StringBuffer.
    * @since TotalCross 1.24
    */
   public StringBuffer dump(StringBuffer sb, String timeSeparator, boolean includeMillis) // guich@tc123_62
   {
      boolean useAmPm = !Settings.is24Hour;
      if (useAmPm) // guich@566_40
      {
         if (hour == 0 || hour == 12)
            sb.append("12");
         else
         {
            int h = hour < 12 ? hour : (hour-12);
            if (h < 10) sb.append('0');
            sb.append(h);
         }
      }
      else
      {
         if (hour < 10) sb.append('0');
         sb.append(hour);
      }
      sb.append(timeSeparator);
      if (minute < 10) sb.append('0');
      sb.append(minute);
      sb.append(timeSeparator);
      if (second < 10) sb.append('0');
      sb.append(second);
      if (includeMillis)
      {
         if (millis < 10)
            sb.append("00");
         else
         if (millis < 100)
            sb.append("0");
         sb.append(millis);
      }
      if (useAmPm)
         sb.append(hour >= 12 ? " PM":" AM"); // guich@566_40: 12 is already pm
      return sb;
   }

   public boolean equals(Object o)
   {
      if (o == this) // flsobral@tc100b4: If both are the same object, avoids wasting time comparing their fields.
         return true;
      if (o instanceof Time)
      {
         Time t = (Time) o;
         return year == t.year && month == t.month && day == t.day &&
                hour == t.hour && minute == t.minute && second == t.second && millis == t.millis;
      }
      return false;
   }

   /** Converts this time object to a string in the Iso8601 format: <pre>YYYYMMDDTHH:MM:SS</pre>.
    * @since SuperWaba 5.61
    */
   public String toIso8601() // guich@561_4
   {
      StringBuffer sb = new StringBuffer(20);
      sb.setLength(0);
      // flsobral@tc100b4: calculate first part, instead of creating a Date object to call getDateInt.
      sb.append((year * 10000)+ (month*100) + day);
      sb.append('T');
      if (hour < 10) sb.append('0');
      sb.append(hour);
      sb.append(':');
      if (minute < 10) sb.append('0');
      sb.append(minute);
      sb.append(':');
      if (second < 10) sb.append('0');
      sb.append(second);
      return sb.toString();
   }
   
   /** Returns true if the time is valid. Note that the date part is NOT checked, only hour, minute, second and millis are checked against valid ranges.
    * @since TotalCross 1.22
    */
   public boolean isValid()
   {
      return 0 <= hour && hour <= 23 && 0 <= minute && minute <= 59 && 0 <= second && second <= 59 && 0 <= millis && millis <= 999;
   }
   
   /** Increments or decrements the fields below. Note that this method does NOT update the 
    * day/month/year fields. 
    * <br><br>Parameters can be positive (to increment), zero (to keep it), or negative (to decrement).
    * @since TotalCross 1.24
    */
   public void inc(int hours, int minutes, int seconds) // guich@tc124_5
   {
      // convert everyone to seconds
      int is = seconds + minutes * 60 + hours * 3600;
      int ts = this.second + this.minute * 60 + this.hour * 3600;
      int s = ts + is;
      if (s > SECONDS_PER_DAY) // above a single day?
         s %= SECONDS_PER_DAY;
      else
      if (s < 0) // previous day?
         s = SECONDS_PER_DAY - (-s % SECONDS_PER_DAY);
      this.second = s % 60; s = s / 60;
      this.minute = s % 60; s = s / 60;
      this.hour   = s;
   }
   
}