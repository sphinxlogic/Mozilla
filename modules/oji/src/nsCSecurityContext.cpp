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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 *
 * It contains the implementation providing nsISecurityCOntext XP-COM interface.
 * This file snapshots a JS frame before entering into java.
 *
 */


#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "nsCSecurityContext.h"
#include "nsIScriptContext.h"
#include "jvmmgr.h"

// For GetOrigin()

#include "nsIScriptSecurityManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsCRT.h"

#include "nsTraceRefcnt.h"

static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


////////////////////////////////////////////////////////////////////////////
// from nsISupports 

// Thes macro expands to the aggregated query interface scheme.

NS_IMPL_ADDREF(nsCSecurityContext);
NS_IMPL_RELEASE(nsCSecurityContext);

NS_METHOD
nsCSecurityContext::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }   
    *aInstancePtr = NULL;
    if (aIID.Equals(kISecurityContextIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (nsISecurityContext*) this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////
// from nsISecurityContext:

NS_METHOD 
nsCSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
    if(!bAllowedAccess) {
        return NS_ERROR_FAILURE;
    }
  
    if(!nsCRT::strcmp(target,"UniversalBrowserRead")) {
        *bAllowedAccess = m_HasUniversalBrowserReadCapability;
        return NS_OK;
    } else if(!nsCRT::strcmp(target,"UniversalJavaPermission")) {
        *bAllowedAccess = m_HasUniversalJavaCapability;
        return NS_OK;
    } else {
        *bAllowedAccess = PR_FALSE;
    }
    return NS_OK;
}


NS_METHOD 
nsCSecurityContext::GetOrigin(char* buf, int buflen)
{
    if (!m_pPrincipal) {
        // Get the Script Security Manager.
        nsresult rv = NS_OK;
        nsCOMPtr<nsIScriptSecurityManager> secMan =
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv) || !secMan) {
            return NS_ERROR_FAILURE;
        }

        secMan->GetSubjectPrincipal(getter_AddRefs(m_pPrincipal));
        if (!m_pPrincipal) {
            return NS_ERROR_FAILURE;
        }
    }

    nsXPIDLCString origin;
    m_pPrincipal->GetOrigin(getter_Copies(origin));

    PRInt32 originlen = origin.Length();
    if (origin.IsEmpty() || originlen > buflen - 1) {
        return NS_ERROR_FAILURE;
    }

    // Copy the string into to user supplied buffer. Is there a better
    // way to do this?

    memcpy(buf, origin, originlen);
    buf[originlen] = nsnull; // Gotta terminate it.

    return NS_OK;
}

NS_METHOD 
nsCSecurityContext::GetCertificateID(char* buf, int buflen)
{
    nsCOMPtr<nsIPrincipal> principal;
  
    // Get the Script Security Manager.

    nsresult rv      = NS_OK;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secMan) return NS_ERROR_FAILURE;

    secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    if (!principal) {
        return NS_ERROR_FAILURE;
    }

    nsXPIDLCString certificate;
    principal->GetCertificateID(getter_Copies(certificate));

    PRInt32 certlen = certificate.Length();
    if (buflen <= certlen) {
        return NS_ERROR_FAILURE;
    }

    memcpy(buf, certificate.get(), certlen);
    buf[certlen] = nsnull;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// from nsCSecurityContext:
extern PRUintn tlsIndex3_g;

MOZ_DECL_CTOR_COUNTER(nsCSecurityContext)

nsCSecurityContext::nsCSecurityContext(JSContext* cx)
                   : m_pJStoJavaFrame(NULL), m_pJSCX(cx),
                     m_pPrincipal(NULL),
                     m_HasUniversalJavaCapability(PR_FALSE),
                     m_HasUniversalBrowserReadCapability(PR_FALSE)
{
    MOZ_COUNT_CTOR(nsCSecurityContext);

      // Get the Script Security Manager.

    nsresult rv = NS_OK;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secMan) return;

    
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
        // return;
        ; // Don't return here because the security manager returns 
          // NS_ERROR_FAILURE when there is no subject principal. In
          // that case we are not done.

    nsCOMPtr<nsIPrincipal> sysprincipal;
    if (NS_FAILED(secMan->GetSystemPrincipal(getter_AddRefs(sysprincipal))))
        return;

    // Do early evaluation of "UniversalJavaPermission" capability.

    PRBool equals;
    if (!principal || 
        NS_SUCCEEDED(principal->Equals(sysprincipal, &equals)) && equals) {
        // We have native code or the system principal: just allow general access
        m_HasUniversalBrowserReadCapability = PR_TRUE;
        m_HasUniversalJavaCapability = PR_TRUE;
    }
    else {
        // Otherwise, check with the js security manager.
        secMan->IsCapabilityEnabled("UniversalBrowserRead",&m_HasUniversalBrowserReadCapability);
        secMan->IsCapabilityEnabled("UniversalJavaPermission",&m_HasUniversalJavaCapability);
    }
}

nsCSecurityContext::nsCSecurityContext(nsIPrincipal *principal)
                   : m_pJStoJavaFrame(NULL), m_pJSCX(NULL),
                     m_pPrincipal(principal),
                     m_HasUniversalJavaCapability(PR_FALSE),
                     m_HasUniversalBrowserReadCapability(PR_FALSE)
{
    MOZ_COUNT_CTOR(nsCSecurityContext);

      // Get the Script Security Manager.

    nsresult rv = NS_OK;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secMan) return;

    nsCOMPtr<nsIPrincipal> sysprincipal;
    if (NS_FAILED(secMan->GetSystemPrincipal(getter_AddRefs(sysprincipal))))
        return;

    // Do early evaluation of "UniversalJavaPermission" capability.

    if (!m_pPrincipal || m_pPrincipal == sysprincipal) {
        // We have native code or the system principal: just allow general access
        m_HasUniversalBrowserReadCapability = PR_TRUE;
        m_HasUniversalJavaCapability = PR_TRUE;
    }
    else {
        // Otherwise, check with the js security manager.
        secMan->IsCapabilityEnabled("UniversalBrowserRead",&m_HasUniversalBrowserReadCapability);
        secMan->IsCapabilityEnabled("UniversalJavaPermission",&m_HasUniversalJavaCapability);
    }
}

nsCSecurityContext::~nsCSecurityContext()
{
  MOZ_COUNT_DTOR(nsCSecurityContext);
}

