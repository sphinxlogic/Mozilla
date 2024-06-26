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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Mike Shaver <shaver@mozilla.org>
 *   Ben Goodger <ben@netscape.com>
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

  The base XUL element class and associates.

*/

#ifndef nsXULElement_h__
#define nsXULElement_h__

// XXX because nsIEventListenerManager has broken includes
#include "nsIDOMEvent.h"
#include "nsIServiceManager.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIControllers.h"
#include "nsICSSParser.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIEventListenerManager.h"
#include "nsINameSpace.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyledContent.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsIURI.h"
#include "nsIXULContent.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIBoxObject.h"
#include "nsXULAttributes.h"
#include "nsIChromeEventHandler.h"
#include "nsXULAttributeValue.h"
#include "nsIXBLService.h"
#include "nsLayoutCID.h"

#include "nsGenericElement.h" // for nsCheapVoidArray

class nsIDocument;
class nsIRDFService;
class nsISupportsArray;
class nsIXULContentUtils;
class nsIXULPrototypeDocument;
class nsRDFDOMNodeList;
class nsString;
class nsXULAttributes;
class nsVoidArray;
class nsIDocShell;

class nsIObjectInputStream;
class nsIObjectOutputStream;

static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);

////////////////////////////////////////////////////////////////////////

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) (nsXULPrototypeAttribute::counter++)
#else
#define XUL_PROTOTYPE_ATTRIBUTE_METER(counter) ((void) 0)
#endif


/**

  A prototype attribute for an nsXULPrototypeElement.

 */

class nsXULPrototypeAttribute
{
public:
    nsXULPrototypeAttribute()
        : mEventHandler(nsnull)
    {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumAttributes);
    }

    ~nsXULPrototypeAttribute();

    nsCOMPtr<nsINodeInfo> mNodeInfo;
    nsXULAttributeValue   mValue;
    void*                 mEventHandler;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
    /**
      If enough attributes, on average, are event handlers, it pays to keep
      mEventHandler here, instead of maintaining a separate mapping in each
      nsXULElement associating those mName values with their mEventHandlers.
      Assume we don't need to keep mNameSpaceID along with mName in such an
      event-handler-only name-to-function-pointer mapping.

      Let
        minAttrSize  = sizeof(mNodeInof) + sizeof(mValue)
        mappingSize  = sizeof(mNodeInfo) + sizeof(mEventHandler)
        elemOverhead = nElems * sizeof(MappingPtr)

      Then
        nAttrs * minAttrSize + nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (minAttrSize + mappingSize - sizeof(mNodeInfo))
      which simplifies to
        nEventHandlers * mappingSize + elemOverhead
        > nAttrs * (mappingSize - sizeof(mNodeInfo))
      or
        nEventHandlers + (nElems * sizeof(MappingPtr)) / mappingSize
        > nAttrs * (1 - sizeof(mNodeInfo) / mappingSize)

      If nsCOMPtr and all other pointers are the same size, this reduces to
        nEventHandlers + nElems / 2 > nAttrs / 2

      To measure how many attributes are event handlers, compile XUL source
      with XUL_PROTOTYPE_ATTRIBUTE_METERING and watch the counters below.
      Plug into the above relation -- if true, it pays to put mEventHandler
      in nsXULPrototypeAttribute rather than to keep a separate mapping.

      Recent numbers after opening four browser windows:
        nElems 3537, nAttrs 2528, nEventHandlers 1042
      giving 1042 + 3537/2 > 2528/2 or 2810 > 1264.

      As it happens, mEventHandler also makes this struct power-of-2 sized,
      8 words on most architectures, which makes for strength-reduced array
      index-to-pointer calculations.
     */
    static PRUint32   gNumElements;
    static PRUint32   gNumAttributes;
    static PRUint32   gNumEventHandlers;
    static PRUint32   gNumCacheTests;
    static PRUint32   gNumCacheHits;
    static PRUint32   gNumCacheSets;
    static PRUint32   gNumCacheFills;
#endif /* !XUL_PROTOTYPE_ATTRIBUTE_METERING */
};


/**

  A prototype content model element that holds the "primordial" values
  that have been parsed from the original XUL document. A
  'lightweight' nsXULElement may delegate its representation to this
  structure, which is shared.

 */

class nsXULPrototypeNode
{
public:
    enum Type { eType_Element, eType_Script, eType_Text };

    Type                     mType;

    PRInt32                  mRefCnt;

    virtual ~nsXULPrototypeNode() {}
    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               nsISupportsArray* aNodeInfos) = 0;
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 nsISupportsArray* aNodeInfos) = 0;

    void AddRef() { ++mRefCnt; };
    void Release()
    {
        --mRefCnt;
        if (mRefCnt == 0)
            delete this;
    };
    virtual void ReleaseSubtree() { Release(); };

protected:
    nsXULPrototypeNode(Type aType)
        : mType(aType), mRefCnt(1) {}
};

class nsXULPrototypeElement : public nsXULPrototypeNode
{
public:
    nsXULPrototypeElement()
        : nsXULPrototypeNode(eType_Element),
          mNumChildren(0),
          mChildren(nsnull),
          mNumAttributes(0),
          mAttributes(nsnull),
          mClassList(nsnull)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeElement);
    }

    virtual ~nsXULPrototypeElement()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeElement);

        delete[] mAttributes;
        delete mClassList;
        delete[] mChildren;
    }

    virtual void ReleaseSubtree()
    {
      if (mChildren) {
        for (PRInt32 i = mNumChildren-1; i >= 0; i--) {
          if (! mChildren[i])
            break;
          mChildren[i]->ReleaseSubtree();
        }
      }

      nsXULPrototypeNode::ReleaseSubtree();
    }

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               nsISupportsArray* aNodeInfos);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 nsISupportsArray* aNodeInfos);

    PRInt32                  mNumChildren;
    nsXULPrototypeNode**     mChildren;           // [OWNER]

    nsCOMPtr<nsINodeInfo>    mNodeInfo;           // [OWNER]

    PRInt32                  mNumAttributes;
    nsXULPrototypeAttribute* mAttributes;         // [OWNER]

    nsCOMPtr<nsIStyleRule>   mInlineStyleRule;    // [OWNER]
    nsClassList*             mClassList;

    nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsAString& aValue);


    static void ReleaseGlobals()
    {
        NS_IF_RELEASE(sCSSParser);
    }

protected:
    static nsICSSParser* GetCSSParser()
    {
        if (!sCSSParser)
            CallCreateInstance(kCSSParserCID, &sCSSParser);
        return sCSSParser;
    }
    static nsICSSParser* sCSSParser;
};

struct JSRuntime;
struct JSObject;
class nsXULDocument;

class nsXULPrototypeScript : public nsXULPrototypeNode
{
public:
    nsXULPrototypeScript(PRUint16 aLineNo, const char *aVersion);
    virtual ~nsXULPrototypeScript();

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               nsISupportsArray* aNodeInfos);
    nsresult SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                nsIScriptContext* aContext);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 nsISupportsArray* aNodeInfos);
    nsresult DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                  nsIScriptContext* aContext);

    nsresult Compile(const PRUnichar* aText, PRInt32 aTextLength,
                     nsIURI* aURI, PRUint16 aLineNo,
                     nsIDocument* aDocument,
                     nsIXULPrototypeDocument* aPrototypeDocument);

    nsCOMPtr<nsIURI>         mSrcURI;
    PRUint16                 mLineNo;
    PRPackedBool             mSrcLoading;
    PRPackedBool             mOutOfLine;
    nsXULDocument*           mSrcLoadWaiters;   // [OWNER] but not COMPtr
    JSObject*                mJSObject;
    const char*              mLangVersion;

    static void ReleaseGlobals()
    {
        NS_IF_RELEASE(sXULPrototypeCache);
    }

protected:
    static nsIXULPrototypeCache* GetXULCache()
    {
        if (!sXULPrototypeCache)
            CallGetService("@mozilla.org/xul/xul-prototype-cache;1", &sXULPrototypeCache);

        return sXULPrototypeCache;
    }
    static nsIXULPrototypeCache* sXULPrototypeCache;
};

class nsXULPrototypeText : public nsXULPrototypeNode
{
public:
    nsXULPrototypeText()
        : nsXULPrototypeNode(eType_Text)
    {
        MOZ_COUNT_CTOR(nsXULPrototypeText);
    }

    virtual ~nsXULPrototypeText()
    {
        MOZ_COUNT_DTOR(nsXULPrototypeText);
    }

    virtual nsresult Serialize(nsIObjectOutputStream* aStream,
                               nsIScriptContext* aContext,
                               nsISupportsArray* aNodeInfos);
    virtual nsresult Deserialize(nsIObjectInputStream* aStream,
                                 nsIScriptContext* aContext,
                                 nsIURI* aDocumentURI,
                                 nsISupportsArray* aNodeInfos);

    nsString                 mValue;
};

////////////////////////////////////////////////////////////////////////

/**

  The XUL element.

 */

class nsXULElement : public nsIXULContent,
                     public nsIDOMXULElement,
                     public nsIScriptEventHandlerOwner,
                     public nsIChromeEventHandler
{
public:
    static nsIXBLService* GetXBLService() {
        if (!gXBLService)
            CallGetService("@mozilla.org/xbl;1", &gXBLService);
        return gXBLService;
    }
    static void ReleaseGlobals() { NS_IF_RELEASE(gXBLService); }

protected:
    static nsrefcnt             gRefCnt;
    // pseudo-constants
    static nsIRDFService*       gRDFService;

public:
    static nsresult
    Create(nsXULPrototypeElement* aPrototype, nsIDocument* aDocument,
           PRBool aIsScriptable, nsIContent** aResult);

    static nsresult
    Create(nsINodeInfo* aNodeInfo, nsIContent** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContent (from nsIStyledContent)
    NS_IMETHOD_(nsIDocument*) GetDocument() const;
    NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
    NS_IMETHOD_(nsIContent*) GetParent() const;
    NS_IMETHOD SetParent(nsIContent* aParent);
    NS_IMETHOD_(PRBool) IsNativeAnonymous() const;
    NS_IMETHOD_(void) SetNativeAnonymous(PRBool aAnonymous);
    NS_IMETHOD CanContainChildren(PRBool& aResult) const;
    NS_IMETHOD ChildCount(PRInt32& aResult) const;
    NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent** aResult) const;
    NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const;
    NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                             PRBool aDeepSetDocument);
    NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                              PRBool aNotify, PRBool aDeepSetDocument);
    NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                             PRBool aDeepSetDocument);
    NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
    NS_IMETHOD GetNameSpaceID(PRInt32* aNameSpeceID) const;
    NS_IMETHOD GetTag(nsIAtom** aResult) const;
    NS_IMETHOD GetNodeInfo(nsINodeInfo** aResult) const;
    NS_IMETHOD NormalizeAttrString(const nsAString& aStr, nsINodeInfo** aNodeInfo);
    NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, const nsAString& aValue, PRBool aNotify);
    NS_IMETHOD SetAttr(nsINodeInfo *aNodeInfo, const nsAString& aValue, PRBool aNotify);
    NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsAString& aResult) const;
    NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom** aPrefix, nsAString& aResult) const;
    NS_IMETHOD_(PRBool) HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
    NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify);
    NS_IMETHOD GetAttrNameAt(PRInt32 aIndex, PRInt32* aNameSpaceID,
                             nsIAtom** aName, nsIAtom** aPrefix) const;
    NS_IMETHOD GetAttrCount(PRInt32& aResult) const;
#ifdef DEBUG
    NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
    NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const { return NS_OK; }
#endif
    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);
    NS_IMETHOD DoneCreatingElement();

    NS_IMETHOD GetContentID(PRUint32* aID);
    NS_IMETHOD SetContentID(PRUint32 aID);

    NS_IMETHOD RangeAdd(nsIDOMRange* aRange);
    NS_IMETHOD RangeRemove(nsIDOMRange* aRange);
    NS_IMETHOD GetRangeList(nsVoidArray** aResult) const;
    NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
    NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

    NS_IMETHOD_(nsIContent*) GetBindingParent() const;
    NS_IMETHOD SetBindingParent(nsIContent* aParent);
    NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);
    NS_IMETHOD GetBaseURL(nsIURI** aURI) const;
    NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult);

    // nsIXMLContent
    NS_IMETHOD MaybeTriggerAutoLink(nsIDocShell *aShell);

    // nsIStyledContent
    NS_IMETHOD GetID(nsIAtom** aResult) const;
    NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
    NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;

    NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
    NS_IMETHOD GetInlineStyleRule(nsIStyleRule** aStyleRule);
    NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      PRInt32 aModType,
                                      nsChangeHint& aHint) const;
    NS_IMETHOD_(PRBool) HasAttributeDependentStyle(const nsIAtom* aAttribute) const;

    // nsIXULContent
    NS_IMETHOD PeekChildCount(PRInt32& aCount) const;
    NS_IMETHOD SetLazyState(LazyState aFlags);
    NS_IMETHOD ClearLazyState(LazyState aFlags);
    NS_IMETHOD GetLazyState(LazyState aFlag, PRBool& aValue);
    NS_IMETHOD AddScriptEventListener(nsIAtom* aName, const nsAString& aValue);

    // nsIDOMNode (from nsIDOMElement)
    NS_DECL_NSIDOMNODE

    // nsIDOMElement
    NS_DECL_NSIDOMELEMENT

    // nsIDOMXULElement
    NS_DECL_NSIDOMXULELEMENT

    // nsIScriptEventHandlerOwner
    NS_IMETHOD CompileEventHandler(nsIScriptContext* aContext,
                                   void* aTarget,
                                   nsIAtom *aName,
                                   const nsAString& aBody,
                                   const char* aURL,
                                   PRUint32 aLineNo,
                                   void** aHandler);
    NS_IMETHOD GetCompiledEventHandler(nsIAtom *aName, void** aHandler);

    // nsIChromeEventHandler
    NS_DECL_NSICHROMEEVENTHANDLER


protected:
    nsXULElement();
    nsresult Init();
    virtual ~nsXULElement(void);


    // Implementation methods
    nsresult EnsureContentsGenerated(void) const;

    nsresult ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsAString& attrName);

    static nsresult
    ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent);

    // Static helpers
    static nsresult
    GetElementsByAttribute(nsIDOMNode* aNode,
                           const nsAString& aAttributeName,
                           const nsAString& aAttributeValue,
                           nsRDFDOMNodeList* aElements);

    static PRBool IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode);

    // Helper routine that crawls a parent chain looking for a tree element.
    NS_IMETHOD GetParentTree(nsIDOMXULMultiSelectControlElement** aTreeElement);

    nsresult AddPopupListener(nsIAtom* aName);

protected:
    // Required fields
    nsXULPrototypeElement*              mPrototype;
    nsIDocument*                        mDocument;           // [WEAK]
    nsIContent*                         mParent;             // [WEAK]
    nsSmallVoidArray                    mChildren;           // [OWNER]
    nsCOMPtr<nsIEventListenerManager>   mListenerManager;    // [OWNER]

    /**
     * The nearest enclosing content node with a binding
     * that created us. [Weak]
     */
    nsIContent*                         mBindingParent;

    /**
     * Lazily instantiated if/when object is mutated. mAttributes are
     * lazily copied from the prototype when changed.
     */
    struct Slots {
        Slots(nsXULElement* mElement);
        ~Slots();

        nsCOMPtr<nsINodeInfo>               mNodeInfo;           // [OWNER]
        nsCOMPtr<nsIControllers>            mControllers;        // [OWNER]

        /**
         * Contains the mLazyState in the low two bits, and a pointer
         * to the nsXULAttributes structure in the high bits.
         */
        PRWord                              mBits;

#define LAZYSTATE_MASK  ((PRWord(1) << LAZYSTATE_BITS) - 1)
#define ATTRIBUTES_MASK (~LAZYSTATE_MASK)

        nsXULAttributes *
        GetAttributes() const {
            return NS_REINTERPRET_CAST(nsXULAttributes *, mBits & ATTRIBUTES_MASK);
        }

        void
        SetAttributes(nsXULAttributes *aAttributes) {
            NS_ASSERTION((NS_REINTERPRET_CAST(PRWord, aAttributes) & ~ATTRIBUTES_MASK) == 0,
                         "nsXULAttributes pointer is unaligned");

            mBits &= ~ATTRIBUTES_MASK;
            mBits |= NS_REINTERPRET_CAST(PRWord, aAttributes);
        }

        LazyState
        GetLazyState() const {
            return LazyState(mBits & LAZYSTATE_MASK);
        }

        void
        SetLazyState(LazyState aLazyState) {
            NS_ASSERTION((aLazyState & ~LAZYSTATE_MASK) == 0,
                         "lazy state includes high bits");

            mBits &= ~LAZYSTATE_MASK;
            mBits |= PRWord(aLazyState);
        }
    };

    friend struct Slots;
    Slots* mSlots;

    /**
     * Ensure that we've got an mSlots object, creating a Slots object
     * if necessary.
     */
    nsresult EnsureSlots();

    /**
     * Ensure that our mSlots has an mAttributes, creating an
     * nsXULAttributes object if necessary.
     */
    nsresult EnsureAttributes();

    /**
     * Abandon our prototype linkage, and copy all attributes locally
     */
    nsresult MakeHeavyweight();

    /**
     * Return our private copy of the attribute, if one exists.
     */
    nsXULAttribute *FindLocalAttribute(nsINodeInfo *info) const;

    /**
     * Return our private copy of the attribute, if one exists.
     */
    nsXULAttribute *FindLocalAttribute(PRInt32 aNameSpaceID,
                                       nsIAtom *aName,
                                       PRInt32 *aIndex = nsnull) const;

    /**
     * Return our prototype's attribute, if one exists.
     */
    nsXULPrototypeAttribute *FindPrototypeAttribute(nsINodeInfo *info) const;

    /**
     * Return our prototype's attribute, if one exists.
     */
    nsXULPrototypeAttribute *FindPrototypeAttribute(PRInt32 aNameSpaceID,
                                                    nsIAtom *aName) const;
    /**
     * Add a listener for the specified attribute, if appropriate.
     */
    nsresult AddListenerFor(nsINodeInfo *aNodeInfo,
                            PRBool aCompileEventHandlers);


    nsresult HideWindowChrome(PRBool aShouldHide);

protected:
    // Internal accessors. These shadow the 'Slots', and return
    // appropriate default values if there are no slots defined in the
    // delegate.
    nsINodeInfo     *NodeInfo() const    { return mSlots ? mSlots->mNodeInfo          : mPrototype->mNodeInfo; }
    nsIControllers  *Controllers() const { return mSlots ? mSlots->mControllers.get() : nsnull; }
    nsXULAttributes *Attributes() const  { return mSlots ? mSlots->GetAttributes()    : nsnull; }

    void UnregisterAccessKey(const nsAString& aOldValue);

    static nsIXBLService *gXBLService;
};


#endif // nsXULElement_h__
