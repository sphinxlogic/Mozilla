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
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen (mvl@exedo.nl).
 * Portions created by the Initial Developer are Copyright (C) 2003
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


#include "nsCookiePermission.h"
#include "nsICookie.h"
#include "nsIPermissionManager.h"
#include "nsIServiceManager.h"
#include "nsICookiePromptService.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

/****************************************************************
 ************************ nsCookiePermission ********************
 ****************************************************************/

static const PRBool kDefaultPolicy = PR_TRUE;

NS_IMPL_ISUPPORTS1(nsCookiePermission, nsICookiePermission)

nsCookiePermission::nsCookiePermission()
{
}

nsCookiePermission::~nsCookiePermission()
{
}

NS_IMETHODIMP 
nsCookiePermission::TestPermission(nsIURI *aURI,
                                   nsICookie *aCookie,
                                   nsIDOMWindow *aParent,
                                   PRInt32 aCookiesFromHost,
                                   PRBool aChangingCookie,
                                   PRBool aShowDialog,
                                   PRUint32 aListPermission,
                                   PRBool *aPermission) 
{
  NS_ASSERTION(aURI, "could not get uri");

  nsresult rv;
  *aPermission = kDefaultPolicy;

  // check whether the user wants to be prompted. we only do this if a prior
  // permissionlist lookup (performed by the caller) returned UNKNOWN_ACTION
  // (i.e., no permissionlist entry exists for this host).
  if (aShowDialog && aListPermission == nsIPermissionManager::UNKNOWN_ACTION) {
    // default to rejecting, in case the prompting process fails
    *aPermission = PR_FALSE;

    nsCAutoString hostPort;
    aURI->GetHostPort(hostPort);

    if (!aCookie || hostPort.IsEmpty()) {
      return NS_ERROR_UNEXPECTED;
    }

    // we don't cache the cookiePromptService - it's not used often, so not worth the memory
    nsCOMPtr<nsICookiePromptService> cookiePromptService = do_GetService(NS_COOKIEPROMPTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    PRBool rememberDecision = PR_FALSE;

    rv = cookiePromptService->
           CookieDialog(nsnull, aCookie, hostPort, 
                        aCookiesFromHost, aChangingCookie, &rememberDecision, 
                        aPermission);
    if (NS_FAILED(rv)) return rv;

    if (rememberDecision) {
      nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      permissionManager->Add(aURI, "cookie", 
                              *aPermission ? nsIPermissionManager::ALLOW_ACTION : nsIPermissionManager::DENY_ACTION);
    }
  }

  return NS_OK;
}