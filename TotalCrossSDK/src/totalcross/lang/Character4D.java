package totalcross.lang;

import totalcross.sys.*;

public class Character4D
{
   public static final Class TYPE = Character.class;
   char v;
   
   public Character4D(char v)
   {
      this.v = v;
   }
   public char charValue()
   {
      return v;
   }
   public boolean equals(Object o)
   {
      return o != null && o instanceof Character4D && ((Character4D)o).v == this.v; 
   }
   public int hashCode()
   {
      return v;
   }
   public String toString()
   {
      return super.toString()+" v="+v;
   }

   // functions used in regex
   /*
   0: 6359
   1: 707
   2: 886
   3: 31
   4: 114
   5: 44113
   6: 530
   7: 10
   8: 131
   9: 208
   10: 52
   11: 242
   12: 19
   13: 1
   14: 1
   15: 65
   16: 32
   17: 0
   18: 6400
   19: 2048
   20: 17
   21: 65
   22: 64
   23: 12
   24: 199
   25: 889
   26: 36
   27: 74
   28: 2221
   29: 6
   30: 4
   31: 0

         try
         {
            byte[] v = new byte[65536];
            for(int i=0; i <= 65535;i++)
               v[i] = (byte)Character.getType((char)i);
            File f = new File("G:\\TotalCross\\TotalCrossSDK\\src\\totalcross\\chartypes.bin", File.CREATE_EMPTY);
            f.writeBytes(v,0,v.length);
            f.close();
         }
         catch (IOException e)
         {
            e.printStackTrace();
         }
   */

   public static final byte UNASSIGNED = 0;
   public static final byte UPPERCASE_LETTER = 1;
   public static final byte LOWERCASE_LETTER = 2;
   public static final byte TITLECASE_LETTER = 3;
   public static final byte MODIFIER_LETTER = 4;
   public static final byte OTHER_LETTER = 5;
   public static final byte NON_SPACING_MARK = 6;
   public static final byte ENCLOSING_MARK = 7;
   public static final byte COMBINING_SPACING_MARK = 8;
   public static final byte DECIMAL_DIGIT_NUMBER = 9;
   public static final byte LETTER_NUMBER = 10;
   public static final byte OTHER_NUMBER = 11;
   public static final byte SPACE_SEPARATOR = 12;
   public static final byte LINE_SEPARATOR = 13;
   public static final byte PARAGRAPH_SEPARATOR = 14;
   public static final byte CONTROL = 15;
   public static final byte FORMAT = 16;
   public static final byte PRIVATE_USE = 18;
   public static final byte SURROGATE = 19;
   public static final byte DASH_PUNCTUATION = 20;
   public static final byte START_PUNCTUATION = 21;
   public static final byte END_PUNCTUATION = 22;
   public static final byte CONNECTOR_PUNCTUATION = 23;
   public static final byte OTHER_PUNCTUATION = 24;
   public static final byte MATH_SYMBOL = 25;
   public static final byte CURRENCY_SYMBOL = 26;
   public static final byte MODIFIER_SYMBOL = 27;
   public static final byte OTHER_SYMBOL = 28;
   public static final byte INITIAL_QUOTE_PUNCTUATION = 29;
   public static final byte FINAL_QUOTE_PUNCTUATION = 30;
   public static final int MAX_VALUE = 65535;
   private static final int TYPE_MASK = 0x1F;
   private static final int NO_BREAK_MASK = 0x20;

   static byte[] types = totalcross.sys.Vm.getFile("totalcross/chartypes.bin");
   
   public static int getType(char i)
   {
      return types[i];
   }
   
   public static boolean isDigit(char i)
   {
      return types[i] == DECIMAL_DIGIT_NUMBER;
   }
   
   public static char toLowerCase(char c)
   {
      return Convert.toLowerCase(c);
   }
   
   public static char toUpperCase(char c)
   {
      return Convert.toUpperCase(c);
   }

   public static char toTitleCase(char ch)
   {
      return Convert.toTitleCase(ch);
   }
   
   public static boolean isWhitespace(char ch)
   {
      int codePoint = ch;
      int attr = types[codePoint];
      return ((((1 << (attr & TYPE_MASK))
                & ((1 << SPACE_SEPARATOR)
                   | (1 << LINE_SEPARATOR)
                   | (1 << PARAGRAPH_SEPARATOR))) != 0)
              && (attr & NO_BREAK_MASK) == 0)
        || (codePoint <= '\u001F' && ((1 << codePoint)
                               & ((1 << '\t')
                                  | (1 << '\n')
                                  | (1 << '\u000B')
                                  | (1 << '\u000C')
                                  | (1 << '\r')
                                  | (1 << '\u001C')
                                  | (1 << '\u001D')
                                  | (1 << '\u001E')
                                  | (1 << '\u001F'))) != 0);
   }

   public static boolean isJavaIdentifierStart(char ch)
   {
      int codePoint = ch;
     return ((1 << types[codePoint])
             & ((1 << UPPERCASE_LETTER)
                | (1 << LOWERCASE_LETTER)
                | (1 << TITLECASE_LETTER)
                | (1 << MODIFIER_LETTER)
                | (1 << OTHER_LETTER)
                | (1 << LETTER_NUMBER)
                | (1 << CURRENCY_SYMBOL)
                | (1 << CONNECTOR_PUNCTUATION))) != 0;
   }

   public static boolean isJavaIdentifierPart(char ch)
   {
      int codePoint = ch;
     int category = types[codePoint];
     return ((1 << category)
             & ((1 << UPPERCASE_LETTER)
                | (1 << LOWERCASE_LETTER)
                | (1 << TITLECASE_LETTER)
                | (1 << MODIFIER_LETTER)
                | (1 << OTHER_LETTER)
                | (1 << NON_SPACING_MARK)
                | (1 << COMBINING_SPACING_MARK)
                | (1 << DECIMAL_DIGIT_NUMBER)
                | (1 << LETTER_NUMBER)
                | (1 << CURRENCY_SYMBOL)
                | (1 << CONNECTOR_PUNCTUATION)
                | (1 << FORMAT))) != 0
       || (category == CONTROL && isIdentifierIgnorable(ch));
   }
   public static boolean isIdentifierIgnorable(char ch)
   {
      int codePoint = ch;
     if ((codePoint >= 0 && codePoint <= 0x0008)
         || (codePoint >= 0x000E && codePoint <= 0x001B)
         || (codePoint >= 0x007F && codePoint <= 0x009F)
         || types[codePoint] == FORMAT)
       return true;
     return false;
   }
}