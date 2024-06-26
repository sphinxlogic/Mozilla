
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsCollationWin_h__
#define nsCollationWin_h__


#include "nsICollation.h"
#include "nsCollation.h"  // static library



class nsCollationWin : public nsICollation {

protected:
  nsCollation   *mCollation;  // XP collation class
  PRBool        mW_API;       // If Windows95 or 98, we cannot use W version of API
  PRUint32      mLCID;        // Windows platform locale ID

public: 
  NS_DECL_ISUPPORTS
    
  // compare two strings
  // result is same as strcmp
  NS_IMETHOD CompareString(const nsCollationStrength strength, 
                           const nsAString& string1, const nsAString& string2, PRInt32* result);

  // allocate sort key from input string
  // returns newly allocated key and its byte length
  NS_IMETHOD AllocateRawSortKey(const nsCollationStrength strength, 
                                const nsAString& stringIn, PRUint8** key, PRUint32 *outLen);

  // compare two sort keys
  // length is a byte length, result is same as strcmp
  NS_IMETHOD CompareRawSortKey(const PRUint8* key1, const PRUint32 len1, 
                               const PRUint8* key2, const PRUint32 len2, 
                               PRInt32* result)
                               {*result = strcmp((const char *)key1, (const char *)key2); return NS_OK;}

  // init this interface to a specified locale (should only be called by collation factory)
  //
  NS_IMETHOD Initialize(nsILocale* locale);

  nsCollationWin();

  virtual ~nsCollationWin(); 

};

#endif  /* nsCollationWin_h__ */
