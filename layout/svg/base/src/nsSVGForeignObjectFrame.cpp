/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is 
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsBlockFrame.h"
#include "nsIDOMSVGGElement.h"
#include "nsIPresContext.h"
#include "nsISVGFrame.h"
#include "nsSVGRenderingContext.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGPoint.h"
#include "libart-incs.h"
#include "nsSpaceManager.h"

typedef nsBlockFrame nsSVGForeignObjectFrameBase;

class nsSVGForeignObjectFrame : public nsSVGForeignObjectFrameBase,
                                public nsISVGFrame,
                                public nsISVGValueObserver,
                                public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGForeignObjectFrame();
  virtual ~nsSVGForeignObjectFrame();
  nsresult Init();
  
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:  
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  
  NS_IMETHOD  ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGFrame interface:
  NS_IMETHOD Paint(nsSVGRenderingContext* renderingContext);
  NS_IMETHOD InvalidateRegion(ArtUta* uta, PRBool bRedraw);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);  
  NS_IMETHOD NotifyCTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended);
  
protected:
  // implementation helpers:
  void Update();
  ArtUta* DoReflow();
  ArtUta* GetUta();
  float GetPxPerTwips();
  float GetTwipsPerPx();
  void TransformPoint(float& x, float& y);
  void TransformVector(float& x, float& y);
  void GetCTM(nsIDOMSVGMatrix** ctm);
  static void AccumulateUta(ArtUta** accu, ArtUta* uta);

  PRBool mIsDirty;
  nsIPresShell* mPresShell; // XXX is a non-owning ref ok?
  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
  nsCOMPtr<nsIDOMSVGLength> mWidth;
  nsCOMPtr<nsIDOMSVGLength> mHeight;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(aContent);
  if (!foreignObject) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGForeignObjectFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGForeignObjectFrame* it = new (aPresShell) nsSVGForeignObjectFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  // XXX is this ok?
  it->mPresShell = aPresShell;

  return NS_OK;
}

nsSVGForeignObjectFrame::nsSVGForeignObjectFrame()
    : mIsDirty(PR_TRUE)
{
}

nsSVGForeignObjectFrame::~nsSVGForeignObjectFrame()
{
//   nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
//   NS_ASSERTION(transformable, "wrong content element");
//   nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
//   transformable->GetTransform(getter_AddRefs(transforms));
//   nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
//   NS_ASSERTION(value, "interface not found");
//   if (value)
//     value->RemoveObserver(this);
  nsCOMPtr<nsISVGValue> value;
  if (mX && (value = do_QueryInterface(mX)))
      value->RemoveObserver(this);
  if (mY && (value = do_QueryInterface(mY)))
      value->RemoveObserver(this);
  if (mWidth && (value = do_QueryInterface(mWidth)))
      value->RemoveObserver(this);
  if (mHeight && (value = do_QueryInterface(mHeight)))
      value->RemoveObserver(this);
}

nsresult nsSVGForeignObjectFrame::Init()
{
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(mContent);
  NS_ASSERTION(foreignObject, "wrong content element");
  
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetWidth(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no width");
    if (!mWidth) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mWidth);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetHeight(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no height");
    if (!mHeight) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mHeight);
    if (value)
      value->AddObserver(this);
  }
  
// XXX 
//   nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
//   NS_ASSERTION(transformable, "wrong content element");
//   nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
//   transformable->GetTransform(getter_AddRefs(transforms));
//   nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
//   NS_ASSERTION(value, "interface not found");
//   if (value)
//     value->AddObserver(this);

  // XXX for some reason updating fails when done here. Why is this too early?
  // anyway - we use a less desirable mechanism now of updating in paint().
//  Update(); 
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGForeignObjectFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGForeignObjectFrame::Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGForeignObjectFrameBase::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);

  Init();

  return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::Reflow(nsIPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
#ifdef DEBUG
//  printf("nsSVGForeignObjectFrame(%p)::Reflow\n", this);
#endif
  
  float twipsPerPx = GetTwipsPerPx();
  
  NS_ENSURE_TRUE(mX && mY && mWidth && mHeight, NS_ERROR_FAILURE);

  float x, y, width, height;
  mX->GetValue(&x);
  mY->GetValue(&y);
  mWidth->GetValue(&width);
  mHeight->GetValue(&height);

  // transform x,y,width,height according to the current ctm:
  // XXX we're ignoring rotation at the moment

  // (x, y): (left, top) -> (center_x, center_y)
  x+=width/2.0f;
  y+=height/2.0f;

  // (x, y): (cx, cy) -> (cx', cy')
  TransformPoint(x, y);
  
  // find new ex & ey unit vectors
  float e1x = 1.0f, e1y = 0.0f, e2x = 0.0f, e2y = 1.0f;
  TransformVector(e1x, e1y);
  TransformVector(e2x, e2y);
  // adopt the scale of them for (w,h)
  width  *= (float)sqrt(e1x*e1x + e1y*e1y);
  height *= (float)sqrt(e2x*e2x + e2y*e2y);
  
  // (x, y): (cx', cy') -> (left', top')
  x -= width/2.0f;
  y -= height/2.0f;

  // move ourselves to (x,y):
  SetPosition(nsPoint((nscoord) (x*twipsPerPx), (nscoord) (y*twipsPerPx)));
  // Xxx: if zewe have a view, move that 
  
  // create a new reflow state, setting our max size to (width,height):
  nsSize availableSpace((nscoord)(width*twipsPerPx), (nscoord)(height*twipsPerPx));
  nsHTMLReflowState sizedReflowState(aPresContext,
                                     aReflowState,
                                     this,
                                     availableSpace);

  // XXX Not sure the dirty-state should be cleared here. We should
  // re-examine how the reflow is driven from nsSVGOuterSVGFrame. I
  // think we're missing a call to DidReflow somewhere ?
  mState &= ~NS_FRAME_IS_DIRTY;
  
  // leverage our base class' reflow function to do all the work:
  return nsSVGForeignObjectFrameBase::Reflow(aPresContext, aDesiredSize,
                                             sizedReflowState, aStatus);
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::AppendFrames(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::AppendFrames(aPresContext, aPresShell,
                                                 aListName, aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InsertFrames(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aPrevFrame,
                                     nsIFrame*       aFrameList)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::InsertFrames(aPresContext, aPresShell,
                                                 aListName, aPrevFrame, aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::RemoveFrame(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::RemoveFrame(aPresContext, aPresShell,
                                                aListName, aOldFrame);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::ReplaceFrame(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aOldFrame,
                                      nsIFrame*       aNewFrame)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::ReplaceFrame(aPresContext, aPresShell,
                                                 aListName, aOldFrame, aNewFrame);
	Update();
	return rv;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGForeignObjectFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGForeignObjectFrame::DidModifySVGObservable (nsISVGValue* observable)
{
  Update();
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGFrame methods

NS_IMETHODIMP
nsSVGForeignObjectFrame::Paint(nsSVGRenderingContext* renderingContext)
{
  if (mIsDirty) {
    ArtUta* dirtyRegion = DoReflow();
    if (dirtyRegion) {
      art_uta_free(dirtyRegion);
    }
  }
  
  nsIRenderingContext* ctx = renderingContext->LockMozRenderingContext();
  nsRect dirtyRect = renderingContext->GetDirtyRectTwips();

  ctx->Translate(mRect.x, mRect.y);
  dirtyRect.x -= mRect.x;
  dirtyRect.y -= mRect.y;

  nsSVGForeignObjectFrameBase::Paint(renderingContext->GetPresContext(),
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_BACKGROUND,
                                     0);

  nsSVGForeignObjectFrameBase::Paint(renderingContext->GetPresContext(),
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_FLOATERS,
                                     0);

  nsSVGForeignObjectFrameBase::Paint(renderingContext->GetPresContext(),
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_FOREGROUND,
                                     0);

  renderingContext->UnlockMozRenderingContext();
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InvalidateRegion(ArtUta* uta, PRBool bRedraw)
{
  if (!uta && !bRedraw) return NS_OK;
  
  if (!mParent) {
    NS_ERROR("invalidating region without parent");
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  if (!SVGFrame) {
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  return SVGFrame->InvalidateRegion(uta, bRedraw);
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
  *hit = nsnull;

  NS_ASSERTION(mPresShell, "need presshell");
  if (!mPresShell) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));

  nsPoint p( (nscoord)(x*GetTwipsPerPx()),
             (nscoord)(y*GetTwipsPerPx()));

  nsresult rv;

  rv = nsSVGForeignObjectFrameBase::GetFrameForPoint(presContext, p,
                                                     NS_FRAME_PAINT_LAYER_FOREGROUND, hit);
  if (NS_SUCCEEDED(rv) && *hit) return rv;

  rv = nsSVGForeignObjectFrameBase::GetFrameForPoint(presContext, p,
                                                     NS_FRAME_PAINT_LAYER_FLOATERS, hit);
  if (NS_SUCCEEDED(rv) && *hit) return rv;

  return nsSVGForeignObjectFrameBase::GetFrameForPoint(presContext, p,
                                                       NS_FRAME_PAINT_LAYER_BACKGROUND, hit);
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyCTMChanged()
{
  Update();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawSuspended()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawUnsuspended()
{
  if (mIsDirty) {
    ArtUta* dirtyRegion = DoReflow();
    if (dirtyRegion) {
      InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  if (!SVGFrame) {
    *isSuspended = PR_FALSE;
    return NS_OK;
  }

  return SVGFrame->IsRedrawSuspended(isSuspended);  
}

//----------------------------------------------------------------------
// Implementation helpers

void nsSVGForeignObjectFrame::Update()
{
  mIsDirty = PR_TRUE;
  
  PRBool suspended;
  IsRedrawSuspended(&suspended);
  if (!suspended) {
    ArtUta* dirtyRegion = DoReflow();
    if (dirtyRegion) {
      InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }  
}

ArtUta* nsSVGForeignObjectFrame::DoReflow()
{
  NS_ASSERTION(mPresShell, "need a presshell");

  nsCOMPtr<nsIPresContext> presContext;
  mPresShell->GetPresContext(getter_AddRefs(presContext));  

  // remember the area we have to invalidate after this reflow:
  ArtUta* dirtyRegion = nsnull;
  AccumulateUta(&dirtyRegion, GetUta());
  
  // initiate a synchronous reflow here and now:  
  nsSize availableSpace(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsCOMPtr<nsIRenderingContext> renderingContext;
  mPresShell->CreateRenderingContext(this,getter_AddRefs(renderingContext));

  // XXX we always pass this off as an initial reflow. is that a problem?
  nsHTMLReflowState reflowState(presContext, this, eReflowReason_Initial,
                                renderingContext, availableSpace);

  nsSpaceManager* spaceManager = new nsSpaceManager(mPresShell, this);
  if (!spaceManager) {
    return dirtyRegion;
  }
  reflowState.mSpaceManager = spaceManager;
   
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsReflowStatus status;
  
  WillReflow(presContext);
  Reflow(presContext, desiredSize, reflowState, status);
  SetSize(nsSize(desiredSize.width, desiredSize.height));
  DidReflow(presContext, &reflowState, NS_FRAME_REFLOW_FINISHED);

  AccumulateUta(&dirtyRegion, GetUta());

  mIsDirty = PR_FALSE;

  return dirtyRegion;
}

ArtUta* nsSVGForeignObjectFrame::GetUta()
{
  // get a uta from our mRect
  
  if (mRect.width==0 || mRect.height==0) return nsnull;
  float pxPerTwips = GetPxPerTwips();
  ArtIRect irect;
  irect.x0 = (int)((mRect.x-1) * pxPerTwips);
  irect.y0 = (int)((mRect.y-1) * pxPerTwips);
  irect.x1 = (int)((mRect.x+mRect.width+2) * pxPerTwips);
  irect.y1 = (int)((mRect.y+mRect.height+2) * pxPerTwips);

  return art_uta_from_irect(&irect);
}

float nsSVGForeignObjectFrame::GetPxPerTwips()
{
  float val = GetTwipsPerPx();
  
  NS_ASSERTION(val!=0.0f, "invalid px/twips");  
  if (val == 0.0) val = 1e-20f;
  
  return 1.0f/val;
}

float nsSVGForeignObjectFrame::GetTwipsPerPx()
{
  float twipsPerPx=16.0f;
  NS_ASSERTION(mPresShell, "need presshell");
  if (mPresShell) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    presContext->GetScaledPixelsToTwips(&twipsPerPx);
  }
  return twipsPerPx;
}

void nsSVGForeignObjectFrame::TransformPoint(float& x, float& y)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm;
  GetCTM(getter_AddRefs(ctm));
  if (!ctm) return;

  // XXX this is absurd! we need to add another method (interface
  // even?) to nsIDOMSVGMatrix to make this easier. (something like
  // nsIDOMSVGMatrix::TransformPoint(float*x,float*y))
  
  nsCOMPtr<nsIDOMSVGElement> el = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMSVGSVGElement> svg_el;
  el->GetOwnerSVGElement(getter_AddRefs(svg_el));
  if (!svg_el) return;
  nsCOMPtr<nsIDOMSVGPoint> point;
  svg_el->CreateSVGPoint(getter_AddRefs(point));
  NS_ASSERTION(point, "couldn't create point!");
  if (!point) return;
  
  point->SetX(x);
  point->SetY(y);
  nsCOMPtr<nsIDOMSVGPoint> xfpoint;
  point->MatrixTransform(ctm, getter_AddRefs(xfpoint));
  xfpoint->GetX(&x);
  xfpoint->GetY(&y);
}

void nsSVGForeignObjectFrame::TransformVector(float& x, float& y)
{
  // XXX This is crazy. What we really want is
  // nsIDOMSVGMatrix::TransformVector(x,y);
  
  float x0=0.0f, y0=0.0f;
  TransformPoint(x0, y0);
  TransformPoint(x,y);
  x -= x0;
  y -= y0;
}

void nsSVGForeignObjectFrame::GetCTM(nsIDOMSVGMatrix** ctm)
{
  *ctm = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content type");
  
  transformable->GetCTM(ctm);  
}

void nsSVGForeignObjectFrame::AccumulateUta(ArtUta** accu, ArtUta* uta)
{
  if (uta == nsnull) return;
  
  if (*accu == nsnull) {
    *accu = uta;
    return ;
  }
  
  ArtUta* temp = *accu;
  *accu = art_uta_union(*accu, uta);
  art_uta_free(temp);
  art_uta_free(uta);  
}

