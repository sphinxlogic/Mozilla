/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsXULWindow.h"

// Helper Classes
#include "nsString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIDocShellTreeItem.h"

// Interfaces needed to include
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIWebProgress.h"
#include "nsIWindowMediator.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULElement.h"
#include "nsIXULBrowserWindow.h"
#include "nsPIDOMWindow.h"

// CIDs
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
//***    nsChromeTreeOwner: Object Management
//*****************************************************************************

nsChromeTreeOwner::nsChromeTreeOwner() : mXULWindow(nsnull)
{
}

nsChromeTreeOwner::~nsChromeTreeOwner()
{
}

//*****************************************************************************
// nsChromeTreeOwner::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsChromeTreeOwner)
NS_IMPL_RELEASE(nsChromeTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsChromeTreeOwner)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsChromeTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);

  if(aIID.Equals(NS_GET_IID(nsIPrompt)))
    return mXULWindow->GetInterface(aIID, aSink);
  if(aIID.Equals(NS_GET_IID(nsIAuthPrompt)))
    return mXULWindow->GetInterface(aIID, aSink);
  if(aIID.Equals(NS_GET_IID(nsIWebBrowserChrome)))
    return mXULWindow->GetInterface(aIID, aSink);
  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow)))
    return mXULWindow->GetInterface(aIID, aSink);
  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow2)))
    return mXULWindow->GetInterface(aIID, aSink);
  if (aIID.Equals(NS_GET_IID(nsIXULWindow)))
    return mXULWindow->QueryInterface(aIID, aSink);

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);

   *aFoundItem = nsnull;

   nsAutoString   name(aName);

   PRBool fIs_Content = PR_FALSE;

   /* Special Cases */
   if(name.IsEmpty())
      return NS_OK;
   if(name.EqualsIgnoreCase("_blank"))
      return NS_OK;
   if(name.EqualsIgnoreCase("_content") || name.EqualsIgnoreCase("_main"))
      {
      fIs_Content = PR_TRUE;
      mXULWindow->GetPrimaryContentShell(aFoundItem);
      if(*aFoundItem)
         return NS_OK;
      // Otherwise fall through and ask the other windows for a content area.
      }

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

   nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
   NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nsnull, 
      getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);
   
   PRBool more;
   
   windowEnumerator->HasMoreElements(&more);
   while(more)
      {
      nsCOMPtr<nsISupports> nextWindow = nsnull;
      windowEnumerator->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
      NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem;
     
      if(fIs_Content)
         {
         xulWindow->GetPrimaryContentShell(getter_AddRefs(shellAsTreeItem));
         if(shellAsTreeItem)
            *aFoundItem = shellAsTreeItem;
         }
      else
         {
         nsCOMPtr<nsIDocShell> shell;
         xulWindow->GetDocShell(getter_AddRefs(shell));
         shellAsTreeItem = do_QueryInterface(shell);
         if(shellAsTreeItem && (aRequestor != shellAsTreeItem.get()))
            {
            // Do this so we can pass in the tree owner as the requestor so the child knows not
            // to call back up.
            nsCOMPtr<nsIDocShellTreeOwner> shellOwner;
            shellAsTreeItem->GetTreeOwner(getter_AddRefs(shellOwner));
            nsCOMPtr<nsISupports> shellOwnerSupports(do_QueryInterface(shellOwner));

            shellAsTreeItem->FindItemWithName(aName, shellOwnerSupports, aFoundItem);
            }
         }
      if(*aFoundItem)
         return NS_OK;   
      windowEnumerator->HasMoreElements(&more);
      }
   return NS_OK;      
}

NS_IMETHODIMP nsChromeTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
   mXULWindow->ContentShellAdded(aContentShell, aPrimary, aID);
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   return mXULWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP nsChromeTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   PRInt32 aCX, PRInt32 aCY)
{
   return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_NAMED_LITERAL_STRING(gLiteralPersist,"persist");
NS_NAMED_LITERAL_STRING(gLiteralScreenX,"screenX");
NS_NAMED_LITERAL_STRING(gLiteralScreenY,"screenY");
NS_NAMED_LITERAL_STRING(gLiteralWidth,"width");
NS_NAMED_LITERAL_STRING(gLiteralHeight,"height");
NS_NAMED_LITERAL_STRING(gLiteralSizemode,"sizemode");
NS_NAMED_LITERAL_STRING(gLiteralSpace," ");

NS_IMETHODIMP
nsChromeTreeOwner::SetPersistence(PRBool aPersistPosition,
                                  PRBool aPersistSize,
                                  PRBool aPersistSizeMode)
{
  nsCOMPtr<nsIDOMElement> docShellElement;
  mXULWindow->GetWindowDOMElement(getter_AddRefs(docShellElement));
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(gLiteralPersist, persistString);

  PRBool saveString = PR_FALSE;
  PRInt32 index;

#define FIND_PERSIST_STRING(aString, aCond)      \
  index = persistString.Find(aString);           \
  if (!aCond && index > kNotFound) {             \
    persistString.Cut(index, aString.Length());  \
    saveString = PR_TRUE;                        \
  } else if (aCond && index == kNotFound) {      \
    persistString.Append(gLiteralSpace+aString); \
    saveString = PR_TRUE;                        \
  }
  FIND_PERSIST_STRING(gLiteralScreenX,  aPersistPosition);
  FIND_PERSIST_STRING(gLiteralScreenY,  aPersistPosition);
  FIND_PERSIST_STRING(gLiteralWidth,    aPersistSize);
  FIND_PERSIST_STRING(gLiteralHeight,   aPersistSize);
  FIND_PERSIST_STRING(gLiteralSizemode, aPersistSizeMode);

  if (saveString) 
    docShellElement->SetAttribute(gLiteralPersist, persistString);

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPersistence(PRBool* aPersistPosition,
                                  PRBool* aPersistSize,
                                  PRBool* aPersistSizeMode)
{
  nsCOMPtr<nsIDOMElement> docShellElement;
  mXULWindow->GetWindowDOMElement(getter_AddRefs(docShellElement));
  if (!docShellElement) 
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(gLiteralPersist, persistString);

  // data structure doesn't quite match the question, but it's close enough
  // for what we want (since this method is never actually called...)
  if (aPersistPosition)
    *aPersistPosition = persistString.Find(gLiteralScreenX) > kNotFound ||
                        persistString.Find(gLiteralScreenY) > kNotFound;
  if (aPersistSize)
    *aPersistSize = persistString.Find(gLiteralWidth) > kNotFound ||
                    persistString.Find(gLiteralHeight) > kNotFound;
  if (aPersistSizeMode)
    *aPersistSizeMode = persistString.Find(gLiteralSizemode) > kNotFound;

  return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsChromeTreeOwner::Destroy()
{
   return mXULWindow->Destroy();
}

NS_IMETHODIMP nsChromeTreeOwner::SetPosition(PRInt32 x, PRInt32 y)
{
   return mXULWindow->SetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPosition(PRInt32* x, PRInt32* y)
{
   return mXULWindow->GetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   return mXULWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetSize(PRInt32* cx, PRInt32* cy)
{
   return mXULWindow->GetSize(cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   return mXULWindow->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   return mXULWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::Repaint(PRBool aForce)
{
   return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   return mXULWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   return mXULWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetVisibility(PRBool* aVisibility)
{
   return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::SetVisibility(PRBool aVisibility)
{
   return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::GetEnabled(PRBool *aEnabled)
{
   return mXULWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsChromeTreeOwner::SetEnabled(PRBool aEnable)
{
   return mXULWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsChromeTreeOwner::GetBlurSuppression(PRBool *aBlurSuppression)
{
  return mXULWindow->GetBlurSuppression(aBlurSuppression);
}

NS_IMETHODIMP nsChromeTreeOwner::SetBlurSuppression(PRBool aBlurSuppression)
{
  return mXULWindow->SetBlurSuppression(aBlurSuppression);
}

NS_IMETHODIMP nsChromeTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   *aMainWidget = mXULWindow->mWindow;
   NS_IF_ADDREF(*aMainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetFocus()
{
   return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsChromeTreeOwner::GetTitle(PRUnichar** aTitle)
{
   return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsChromeTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   return mXULWindow->SetTitle(aTitle);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP
nsChromeTreeOwner::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress, 
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
   return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRUint32 aProgressStateFlags,
                                 nsresult aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  nsIURI* aLocation)
{
  PRBool itsForYou = PR_TRUE;

  if (aWebProgress) {
    nsCOMPtr<nsIDOMWindow> progressWin;
    aWebProgress->GetDOMWindow(getter_AddRefs(progressWin));

    nsCOMPtr<nsIDocShell> docshell;
    mXULWindow->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDOMWindow> ourWin(do_QueryInterface(docshell));

    if (ourWin != progressWin)
      itsForYou = PR_FALSE;
  }

   // If loading a new root .xul document, then redo chrome.
  if (itsForYou)
    mXULWindow->mChromeLoaded = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsChromeTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP 
nsChromeTreeOwner::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsChromeTreeOwner: Accessors
//*****************************************************************************   

void nsChromeTreeOwner::XULWindow(nsXULWindow* aXULWindow)
{
   mXULWindow = aXULWindow;
}

nsXULWindow* nsChromeTreeOwner::XULWindow()
{
   return mXULWindow;
}
