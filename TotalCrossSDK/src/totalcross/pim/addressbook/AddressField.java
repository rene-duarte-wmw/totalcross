/*********************************************************************************
 *  TotalCross Software Development Kit                                          *
 *  Copyright (C) 2003 Gilbert Fridgen                                           *
 *  Copyright (C) 2003-2012 SuperWaba Ltda.                                      *
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



package totalcross.pim.addressbook;

import totalcross.pim.*;

/**
 * Provides a data structure to save address data similarly to the vCard3.0 specification.
 * @author Gilbert Fridgen
 */
public class AddressField extends VCardField
{
   /**
    * @param key this address' key (one of the static keys contained in VCardField)
    * @param options an array of Strings of the form "option=value"
    * @param values an array of values, corresponding to the vCard3.0 specification of the chosen key
    */
   public AddressField(int key, String[] options, String[] values)
   {
      super(key, options, values);
   }
   /**
    * Clone's the <code>AddressField</clone>
    * @return a clone of this <code>AddressField</code>
    * @author Fabian Kroeher
    */
   public Object clone()
   {
      return new AddressField(getKey(), cloneOptions(), cloneValues());
   }
}
