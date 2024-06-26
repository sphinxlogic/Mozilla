/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Daniel Witte.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCookie_h__
#define nsCookie_h__

#include "nsICookie.h"
#include "nsICookie2.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsInt64.h"

/** 
 * The nsCookie class is the main cookie storage medium for use within cookie
 * code. It implements nsICookie2, which extends nsICookie, a frozen interface
 * for xpcom access of cookie objects.
 */

/******************************************************************************
 * nsCookie:
 * implementation
 ******************************************************************************/

class nsCookie : public nsICookie2
{
  // this is required because we use a bitfield refcount member.
  public:
    NS_DECL_ISUPPORTS_INHERITED
  protected:
    NS_DECL_OWNINGTHREAD

  public:
    // nsISupports
    NS_DECL_NSICOOKIE
    NS_DECL_NSICOOKIE2

    nsCookie(const nsACString &aName,
             const nsACString &aValue,
             const nsACString &aHost,
             const nsACString &aPath,
             nsInt64          aExpiry,
             nsInt64          aLastAccessed,
             PRBool           aIsSession,
             PRBool           aIsDomain,
             PRBool           aIsSecure,
             nsCookieStatus   aStatus,
             nsCookiePolicy   aPolicy);

    virtual ~nsCookie();

    // fast (inline, non-xpcom) getters
    inline const nsDependentCString Name()  const { return nsDependentCString(mName, mValue - 1); }
    inline const nsDependentCString Value() const { return nsDependentCString(mValue, mHost - 1); }
    inline const nsDependentCString Host()  const { return nsDependentCString(mHost, mPath - 1); }
    inline const nsDependentCString Path()  const { return nsDependentCString(mPath, mEnd); }
    inline nsInt64 Expiry()                 const { NS_ASSERTION(!IsSession(), "can't get expiry time for a session cookie"); return mExpiry; }
    inline nsInt64 LastAccessed()           const { return mLastAccessed; }
    inline PRBool IsSession()               const { return mIsSession; }
    inline PRBool IsDomain()                const { return mIsDomain; }
    inline PRBool IsSecure()                const { return mIsSecure; }
    inline nsCookieStatus Status()          const { return mStatus; }
    inline nsCookiePolicy Policy()          const { return mPolicy; }

    // setters on nsCookie only exist for SetLastAccessed().
    // except for this method, an nsCookie is immutable
    // and must be deleted & recreated if it needs to be changed.
    inline void SetLastAccessed(nsInt64 aLastAccessed) { mLastAccessed = aLastAccessed; }

  protected:
    // member variables
    // we use char* ptrs to store the strings in a contiguous block,
    // so we save on the overhead of using nsCStrings. However, we
    // store a terminating null for each string, so we can hand them
    // out as nsAFlatCStrings.

    // sizeof(nsCookie) = 44 bytes + Length(strings)
    char     *mName;
    char     *mValue;
    char     *mHost;
    char     *mPath;
    char     *mEnd;
    nsInt64  mExpiry;
    nsInt64  mLastAccessed;
    PRUint32 mRefCnt    : 16;
    PRUint32 mIsSession : 1;
    PRUint32 mIsDomain  : 1;
    PRUint32 mIsSecure  : 1;
    PRUint32 mStatus    : 3;
    PRUint32 mPolicy    : 3;
};

#endif // nsCookie_h__
