/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsImageBoxFrame_h___
#define nsImageBoxFrame_h___

#include "nsLeafBoxFrame.h"

#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"

class nsImageBoxFrame;

class nsImageBoxListener : imgIDecoderObserver
{
public:
  nsImageBoxListener();
  virtual ~nsImageBoxListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  void SetFrame(nsImageBoxFrame *frame) { mFrame = frame; }

private:
  nsImageBoxFrame *mFrame;
};

class nsImageBoxFrame : public nsLeafBoxFrame
{
public:

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD NeedsRecalc();

  friend nsresult NS_NewImageBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIBox frame interface

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsStyleContext*  aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  NS_IMETHOD  DidSetStyleContext (nsIPresContext* aPresContext);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  void UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw);
  void UpdateImage(nsIPresContext*  aPresContext, PRBool& aResize);
  void UpdateLoadFlags();

  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0);

  NS_IMETHOD OnStartDecode(imgIRequest *request);
  NS_IMETHOD OnStartContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStartFrame(imgIRequest *request, gfxIImageFrame *frame);
  NS_IMETHOD OnDataAvailable(imgIRequest *request,
                             gfxIImageFrame *frame,
                             const nsRect * rect);
  NS_IMETHOD OnStopFrame(imgIRequest *request, gfxIImageFrame *frame);
  NS_IMETHOD OnStopContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopDecode(imgIRequest *request,
                          nsresult status,
                          const PRUnichar *statusArg);
  NS_IMETHOD FrameChanged(imgIContainer *container,
                          gfxIImageFrame *newframe,
                          nsRect * dirtyRect);

  virtual ~nsImageBoxFrame();
protected:

  void CacheImageSize(nsBoxLayoutState& aBoxLayoutState);


  NS_IMETHOD  PaintImage(nsIPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect,
                         nsFramePaintLayer aWhichLayer);

  nsImageBoxFrame(nsIPresShell* aShell);

  void GetImageSource();

  void GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **group);

  virtual void GetImageSize(nsIPresContext* aPresContext);

private:

  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIDecoderObserver> mListener;

  nsString mSrc; // The raw image source.

  PRPackedBool mUseSrcAttr; // Whether or not the image src comes from an attribute.
  PRPackedBool mSizeFrozen;
  PRPackedBool mHasImage;
  PRPackedBool mSuppressStyleCheck;
  
  nsRect mSubRect; // If set, indicates that only the portion of the image specified by the rect should be used.

  nsSize mIntrinsicSize;
  PRInt32 mLoadFlags;

  nsSize mImageSize;

  nsIPresContext* mPresContext; // weak ptr
}; // class nsImageBoxFrame

#endif /* nsImageBoxFrame_h___ */
