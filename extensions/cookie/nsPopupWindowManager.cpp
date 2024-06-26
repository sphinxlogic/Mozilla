/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPopupWindowManager.h"

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsPermission.h"
#include "nsIPermissionManager.h"

#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsIServiceManagerUtils.h"
#include "nsIURI.h"

/**
 * The Popup Window Manager maintains popup window permissions by website.
 */

static const char kPopupDisablePref[] = "dom.disable_open_during_load";

//*****************************************************************************
//*** nsPopupWindowManager object management and nsISupports
//*****************************************************************************

nsPopupWindowManager::nsPopupWindowManager() :
  mPolicy(ALLOW_POPUP)
{
}

nsPopupWindowManager::~nsPopupWindowManager(void)
{
}

NS_IMPL_ISUPPORTS3(nsPopupWindowManager, 
                   nsIPopupWindowManager,
                   nsIObserver,
                   nsSupportsWeakReference);

nsresult
nsPopupWindowManager::Init()
{
  nsresult rv;
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    PRBool permission;
    rv = mPrefBranch->GetBoolPref(kPopupDisablePref, &permission);
    if (NS_FAILED(rv)) {
      permission = PR_FALSE;
    }
    mPolicy = permission ? DENY_POPUP : ALLOW_POPUP;

    nsCOMPtr<nsIPrefBranchInternal> prefInternal = do_QueryInterface(mPrefBranch, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefInternal->AddObserver(kPopupDisablePref, this, PR_TRUE);
    }
  } 

  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIPopupWindowManager
//*****************************************************************************

NS_IMETHODIMP
nsPopupWindowManager::GetDefaultPermission(PRUint32 *aDefaultPermission)
{
  NS_ENSURE_ARG_POINTER(aDefaultPermission);
  *aDefaultPermission = mPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsPopupWindowManager::SetDefaultPermission(PRUint32 aDefaultPermission)
{
  mPolicy = aDefaultPermission;
  return NS_OK;
}


NS_IMETHODIMP
nsPopupWindowManager::TestPermission(nsIURI *aURI, PRUint32 *aPermission)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aPermission);

  nsresult rv;
  PRUint32 permit;

  if (mPermissionManager) {
    rv = mPermissionManager->TestPermission(aURI, "popup", &permit);

    // Share some constants between interfaces?
    if (permit == nsIPermissionManager::ALLOW_ACTION) {
      *aPermission = ALLOW_POPUP;
    } else if (permit == nsIPermissionManager::DENY_ACTION) {
      *aPermission = DENY_POPUP;
    } else {
      *aPermission = mPolicy;
    }
  } else {
    *aPermission = mPolicy;
  }
  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIObserver
//*****************************************************************************
NS_IMETHODIMP
nsPopupWindowManager::Observe(nsISupports *aSubject, 
                              const char *aTopic,
                              const PRUnichar *aData)
{
  NS_LossyConvertUCS2toASCII pref(aData);
  if (pref.Equals(kPopupDisablePref)) {
    // refresh our local copy of the "disable popups" pref
    PRBool permission = PR_FALSE;

    if (mPrefBranch) {
      mPrefBranch->GetBoolPref(kPopupDisablePref, &permission);
    }
    mPolicy = permission ? DENY_POPUP : ALLOW_POPUP;
  }
  return NS_OK;
}
