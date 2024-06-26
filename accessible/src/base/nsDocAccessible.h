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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#ifndef _nsDocAccessible_H_
#define _nsDocAccessible_H_

#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsPIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIDOMMutationListener.h"
#include "nsIEditor.h"
#include "nsIObserver.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"

class nsIScrollableView;

const PRUint32 kDefaultCacheSize = 256;

class nsDocAccessible : public nsBlockAccessible,
                        public nsIAccessibleDocument,
                        public nsPIAccessibleDocument,
                        public nsIWebProgressListener,
                        public nsIObserver,
                        public nsIDOMMutationListener,
                        public nsIScrollPositionListener,
                        public nsSupportsWeakReference
{
  enum EBusyState {eBusyStateUninitialized, eBusyStateLoading, eBusyStateDone};
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEDOCUMENT
  NS_DECL_NSPIACCESSIBLEDOCUMENT
  NS_DECL_NSIOBSERVER

  public:
    nsDocAccessible(nsIDOMNode *aNode, nsIWeakReference* aShell);
    virtual ~nsDocAccessible();

    NS_IMETHOD GetRole(PRUint32 *aRole);
    NS_IMETHOD GetName(nsAString& aName);
    NS_IMETHOD GetValue(nsAString& aValue);
    NS_IMETHOD GetState(PRUint32 *aState);

    // ----- nsIScrollPositionListener ---------------------------
    NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
    NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

    // ----- nsIDOMMutationListener -------------------------
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
    NS_IMETHOD SubtreeModified(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD NodeInserted(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD NodeRemoved(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD NodeRemovedFromDocument(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD NodeInsertedIntoDocument(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD AttrModified(nsIDOMEvent* aMutationEvent);
    NS_IMETHOD CharacterDataModified(nsIDOMEvent* aMutationEvent);

    NS_DECL_NSIWEBPROGRESSLISTENER

    NS_IMETHOD FireToolkitEvent(PRUint32 aEvent, nsIAccessible* aAccessible, void* aData);

    // nsIAccessNode
    NS_IMETHOD Shutdown();
    NS_IMETHOD Init();

  protected:
    virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsIFrame* GetFrame();
    virtual nsresult AddEventListeners();
    virtual nsresult RemoveEventListeners();
    void AddScrollListener(nsIPresShell *aPresShell);
    void RemoveScrollListener(nsIPresShell *aPresShell);
    void FireDocLoadFinished();
    void HandleMutationEvent(nsIDOMEvent *aEvent, PRUint32 aEventType);
    static void DocLoadCallback(nsITimer *aTimer, void *aClosure);
    static void ScrollTimerCallback(nsITimer *aTimer, void *aClosure);
    void GetEventShell(nsIDOMNode *aNode, nsIPresShell **aEventShell);
    void GetEventDocAccessible(nsIDOMNode *aNode, 
                               nsIAccessibleDocument **aAccessibleDoc);
    void CheckForEditor();

    nsInterfaceHashtable<nsVoidHashKey, nsIAccessNode> *mAccessNodeCache;
    void *mWnd;
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    nsCOMPtr<nsITimer> mDocLoadTimer;
    nsCOMPtr<nsIWebProgress> mWebProgress;
    nsCOMPtr<nsIEditor> mEditor; // Editor, if there is one
    EBusyState mBusy;
    PRUint16 mScrollPositionChangedTicks; // Used for tracking scroll events
    PRPackedBool mIsNewDocument;
};

#endif  
