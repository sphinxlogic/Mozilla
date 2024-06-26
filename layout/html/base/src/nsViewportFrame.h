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
#ifndef nsViewportFrame_h___
#define nsViewportFrame_h___

#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIPresContext.h"
#include "nsReflowPath.h"
#include "nsIPresShell.h"
#include "nsAbsoluteContainingBlock.h"

class nsFixedContainingBlock : public nsAbsoluteContainingBlock {
public:
  nsFixedContainingBlock() { }          // useful for debugging

  virtual ~nsFixedContainingBlock() { } // useful for debugging

  virtual nsIAtom* GetChildListName() const { return nsLayoutAtoms::fixedList; }
};

/**
  * ViewportFrame is the parent of a single child - the doc root frame or a scroll frame 
  * containing the doc root frame. ViewportFrame stores this child in its primary child 
  * list. It stores fixed positioned items in a secondary child list and its mFixedContainer 
  * delegate handles them. 
  */
class ViewportFrame : public nsContainerFrame {
public:
  ViewportFrame() { }          // useful for debugging

  virtual ~ViewportFrame() { } // useful for debugging

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD GetFrameForPoint(nsIPresContext*   aPresContext,
                              const nsPoint&    aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**        aFrame);

  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);

  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual PRBool CanPaintBackground() { return PR_FALSE; }
  
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::viewportFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

protected:
  void AdjustReflowStateForScrollbars(nsIPresContext*    aPresContext,
                                      nsHTMLReflowState& aReflowState) const;

protected:
  nsFixedContainingBlock mFixedContainer;
};


#endif // nsViewportFrame_h___
