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
#include "nsCOMPtr.h"
#include "nsViewportFrame.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsReflowPath.h"
#include "nsIPresShell.h"

nsresult
NS_NewViewportFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  ViewportFrame* it = new (aPresShell) ViewportFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Destroy(nsIPresContext* aPresContext)
{
  mFixedContainer.DestroyFrames(this, aPresContext);
  return nsContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;

  // See which child list to add the frames to
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  } 
  else {
    rv = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, 
                               (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

NS_IMETHODIMP
ViewportFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.AppendFrames(this, aPresContext, aPresShell, 
                                      aListName, aFrameList);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.InsertFrames(this, aPresContext, aPresShell, aListName, 
                                      aPrevFrame, aFrameList);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                          nsIAtom** aListName) const
{
  NS_PRECONDITION(aListName, "null OUT parameter pointer");
  NS_PRECONDITION(aIndex >= 0, "illegal index"); 

  *aListName = nsnull;

  nsresult rv = NS_OK;

  if (0 == aIndex) {
    *aListName = mFixedContainer.GetChildListName();
    NS_ADDREF(*aListName);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::FirstChild(nsIPresContext* aPresContext,
                          nsIAtom*        aListName,
                          nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(aFirstChild, "null OUT parameter pointer");
  if (mFixedContainer.GetChildListName() == aListName) {
    mFixedContainer.FirstChild(this, aListName, aFirstChild);
    return NS_OK;
  }

  return nsContainerFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

void
ViewportFrame::AdjustReflowStateForScrollbars(nsIPresContext*    aPresContext,
                                              nsHTMLReflowState& aReflowState) const
{
  // Calculate how much room is available for fixed frames. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible

  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsCOMPtr<nsIScrollableFrame> scrollingFrame(do_QueryInterface(kidFrame));

  if (scrollingFrame) {
    nscoord sbWidth = 0, sbHeight = 0;
    PRBool sbHVisible = PR_FALSE, sbVVisible = PR_FALSE;
    scrollingFrame->GetScrollbarSizes(aPresContext, &sbWidth, &sbHeight);
    scrollingFrame->GetScrollbarVisibility(aPresContext, &sbVVisible, &sbHVisible);
    if (sbVVisible) {
      aReflowState.mComputedWidth -= sbWidth;
      aReflowState.availableWidth -= sbWidth;
    }
    if (sbHVisible) {
      aReflowState.mComputedHeight -= sbHeight;
    }
  }
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");
  NS_PRECONDITION(!aDesiredSize.mComputeMEW, "unexpected request");

  // Initialize OUT parameters
  aStatus = NS_FRAME_COMPLETE;

  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nsRect kidRect(0,0,aReflowState.availableWidth,aReflowState.availableHeight);

  nsresult rv = NS_OK;
  
  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (eReflowReason_Incremental != aReflowState.reason ||
        aReflowState.path->HasChild(mFrames.FirstChild())) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      nsHTMLReflowMetrics kidDesiredSize(nsnull);
      nsSize              availableSpace(aReflowState.availableWidth,
                                         aReflowState.availableHeight);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowState.mComputedHeight = aReflowState.availableHeight;
      rv = ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                       0, 0, 0, aStatus);
      kidRect.width = kidDesiredSize.width;
      kidRect.height = kidDesiredSize.height;

      FinishReflowChild(kidFrame, aPresContext, nsnull, kidDesiredSize, 0, 0, 0);
    }
  }

  // If we were flowed initially at both an unconstrained width and height, 
  // this is a hint that we should return our child's intrinsic size.
  if ((eReflowReason_Initial == aReflowState.reason ||
       eReflowReason_Resize == aReflowState.reason) &&
      aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE &&
      aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.width = kidRect.width;
    aDesiredSize.height = kidRect.height;
    aDesiredSize.ascent = kidRect.height;
    aDesiredSize.descent = 0;
  }
  else {
    // Return the max size as our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = aReflowState.availableHeight;
    aDesiredSize.ascent = aReflowState.availableHeight;
    aDesiredSize.descent = 0;
  }

  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  nsHTMLReflowState reflowState(aReflowState);
  AdjustReflowStateForScrollbars(aPresContext, reflowState);
  
  nsReflowType reflowType = eReflowType_ContentChanged;
  if (aReflowState.path) {
    // XXXwaterson this is more restrictive than the previous code
    // was: it insists that the UserDefined reflow be targeted at
    // _this_ frame.
    nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;
    if (command)
      command->GetType(reflowType);
  }

  PRBool wasHandled = PR_FALSE;
  if (reflowType != eReflowType_UserDefined &&
      aReflowState.reason == eReflowReason_Incremental) {
    // Incremental reflow
    rv = mFixedContainer.IncrementalReflow(this, aPresContext, reflowState,
                                           reflowState.mComputedWidth,
                                           reflowState.mComputedHeight,
                                           wasHandled);
  }

  if (!wasHandled) {
    // It's the initial reflow or some other non-incremental reflow or
    // IncrementalReflow() didn't handle it.  Just reflow all the
    // fixed-pos frames.
    rv = mFixedContainer.Reflow(this, aPresContext, reflowState,
                                reflowState.mComputedWidth, 
                                reflowState.mComputedHeight);
  }

  // If this is an initial reflow, resize reflow, or style change reflow
  // then do a repaint
  if ((eReflowReason_Initial == aReflowState.reason) ||
      (eReflowReason_Resize == aReflowState.reason) ||
      (eReflowReason_StyleChange == aReflowState.reason)) {
    nsRect damageRect(0, 0, aDesiredSize.width, aDesiredSize.height);
    if (!damageRect.IsEmpty()) {
      Invalidate(aPresContext, damageRect, PR_FALSE);
    }
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv; 
}

NS_IMETHODIMP
ViewportFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::viewportFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}
#endif
