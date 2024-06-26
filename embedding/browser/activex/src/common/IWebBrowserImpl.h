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
 *   Adam Lock <adamlock@netscape.com>
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

#ifndef IWEBBROWSERIMPL_H
#define IWEBBROWSERIMPL_H

#include <mshtml.h>

#include "nsIWebNavigation.h"
#include "nsIPref.h"
#include "nsIDOMWindow.h"
#include "nsIBaseWindow.h"
#include "nsIWindowWatcher.h"
#include "nsIInputStream.h"
#include "nsIByteArrayInputStream.h"
#include "nsIURI.h"

#include "PropertyList.h"

// CPMozillaControl.h was autogenerated by the ATL proxy wizard so don't edit it!
#include "CPMozillaControl.h"

#define ENSURE_BROWSER_IS_VALID() \
    if (!BrowserIsValid()) \
    { \
        NS_ASSERTION(0, "Browser is not valid"); \
        return SetErrorInfo(E_UNEXPECTED, L"Browser is not in a valid state"); \
    }

#define ENSURE_GET_WEBNAV() \
    nsCOMPtr<nsIWebNavigation> webNav; \
    nsresult rv = GetWebNavigation(getter_AddRefs(webNav)); \
    if (NS_FAILED(rv)) \
    { \
        NS_ASSERTION(0, "Cannot get nsIWebNavigation"); \
        return SetErrorInfo(E_UNEXPECTED, L"Could not obtain nsIWebNavigation interface"); \
    }

template<class T, const CLSID *pclsid, const GUID* plibid = &LIBID_MSHTML>
class IWebBrowserImpl :
    public CStockPropImpl<T, IWebBrowser2, &IID_IWebBrowser2, plibid>,
    public CProxyDWebBrowserEvents<T>,
    public CProxyDWebBrowserEvents2<T>
{
public:
    IWebBrowserImpl()
    {
        // Ready state of control
        mBrowserReadyState = READYSTATE_UNINITIALIZED;
        // Flag indicates if the browser is busy
        mBusyFlag = PR_FALSE;
    }

public:
// Methods to be implemented by the derived class
    // Return the nsIWebNavigation object
    virtual nsresult GetWebNavigation(nsIWebNavigation **aWebNav) = 0;
    // Return the nsIDOMWindow object
    virtual nsresult GetDOMWindow(nsIDOMWindow **aDOMWindow) = 0;
    // Return the nsIPref object
    virtual nsresult GetPrefs(nsIPref **aPrefs) = 0;
    // Return the valid state of the browser
    virtual PRBool BrowserIsValid() = 0;

public:
// Properties related to this interface
    // Post data from last navigate operation
    CComVariant             mLastPostData;
    // Ready status of the browser
    READYSTATE              mBrowserReadyState;
    // Controls starts off unbusy
    PRBool                  mBusyFlag;
    // Property list
    PropertyList            mPropertyList;

// Helper methods

    //
    // Sets error information for VB programmers and the like who want to know why
    // some method call failed.
    //
    virtual HRESULT SetErrorInfo(HRESULT hr, LPCOLESTR lpszDesc = NULL)
    {
        if (lpszDesc == NULL)
        {
            // Fill in a few generic descriptions
            switch (hr)
            {
            case E_UNEXPECTED:
                lpszDesc = L"Method was called while control was uninitialized";
                break;
            case E_INVALIDARG:
                lpszDesc = L"Method was called with an invalid parameter";
                break;
            }
        }
        AtlSetErrorInfo(*pclsid, lpszDesc, 0, NULL, GUID_NULL, hr, NULL);
        return hr;
    }


// IWebBrowser implementation
    virtual HRESULT STDMETHODCALLTYPE GoBack(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::GoBack()\n"));
        ENSURE_BROWSER_IS_VALID();
        ENSURE_GET_WEBNAV();

        PRBool aCanGoBack = PR_FALSE;
        webNav->GetCanGoBack(&aCanGoBack);
        if (aCanGoBack == PR_TRUE)
        {
            webNav->GoBack();
        }
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GoForward(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::GoBack()\n"));
        ENSURE_BROWSER_IS_VALID();
        ENSURE_GET_WEBNAV();

        PRBool aCanGoForward = PR_FALSE;
        webNav->GetCanGoForward(&aCanGoForward);
        if (aCanGoForward == PR_TRUE)
        {
            webNav->GoForward();
        }
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE GoHome(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::GoHome()\n"));
        ENSURE_BROWSER_IS_VALID();

        CComBSTR bstrUrl(L"http://home.netscape.com/");

        // Find the home page stored in prefs
        nsCOMPtr<nsIPref> prefs;
        if (NS_SUCCEEDED(GetPrefs(getter_AddRefs(prefs))))
        {
            nsXPIDLString homePage;
            nsresult rv;
            rv = prefs->GetLocalizedUnicharPref("browser.startup.homepage", getter_Copies(homePage));
            if (rv == NS_OK)
            {
                bstrUrl = homePage.get();;
            }
        }

        // Navigate to the home page
        Navigate(bstrUrl, NULL, NULL, NULL, NULL);
    
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE GoSearch(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::GoSearch()\n"));
        ENSURE_BROWSER_IS_VALID();

        CComBSTR bstrUrl(L"http://search.netscape.com/");

        // Find the home page stored in prefs
        nsCOMPtr<nsIPref> prefs;
        if (NS_SUCCEEDED(GetPrefs(getter_AddRefs(prefs))))
        {
            // TODO find and navigate to the search page stored in prefs
            //      and not this hard coded address
        }

        // Navigate to the search page
        Navigate(bstrUrl, NULL, NULL, NULL, NULL);
    
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Navigate(BSTR URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
    {
        ATLTRACE(_T("IWebBrowserImpl::Navigate()\n"));
        ENSURE_BROWSER_IS_VALID();

        // Extract the URL parameter
        if (URL == NULL)
        {
            NS_ASSERTION(0, "No URL supplied");
            return SetErrorInfo(E_INVALIDARG);
        }

        PRBool openInNewWindow = PR_FALSE;
        PRUint32 loadFlags = nsIWebNavigation::LOAD_FLAGS_NONE;

        // Extract the navigate flags parameter
        LONG lFlags = 0;
        if (Flags &&
            Flags->vt != VT_ERROR &&
            Flags->vt != VT_EMPTY &&
            Flags->vt != VT_NULL)
        {
            CComVariant vFlags;
            if ( vFlags.ChangeType(VT_I4, Flags) != S_OK )
            {
                NS_ASSERTION(0, "Flags param is invalid");
                return SetErrorInfo(E_INVALIDARG);
            }
            lFlags = vFlags.lVal;
        }
        if (lFlags & navOpenInNewWindow) 
        {
            openInNewWindow = PR_TRUE;
        }
        if (lFlags & navNoHistory)
        {
            // Disable history
            loadFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY;
        }
        if (lFlags & navNoReadFromCache)
        {
            // Disable read from cache
            loadFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;

        }
        if (lFlags & navNoWriteToCache)
        {
            // Disable write to cache
            loadFlags |= nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
        }

        // Extract the target frame parameter
        nsCOMPtr<nsIWebNavigation> targetNav;
        if (TargetFrameName &&
            TargetFrameName->vt == VT_BSTR &&
            TargetFrameName->bstrVal)
        {
            // Search for the named frame
            nsCOMPtr<nsIDOMWindow> window;
            GetDOMWindow(getter_AddRefs(window));
            if (window)
            {
                nsCOMPtr<nsIWindowWatcher> windowWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
                if (windowWatcher)
                {
                    nsCOMPtr<nsIDOMWindow> targetWindow;
                    windowWatcher->GetWindowByName(TargetFrameName->bstrVal, window,
                        getter_AddRefs(targetWindow));
                    if (targetWindow)
                    {
                        targetNav = do_GetInterface(targetWindow);
                    }
                }
            }
            // No target nav? Open in new window
            if (!targetNav)
                openInNewWindow = PR_TRUE;
        }

        // Open in new window
        if (openInNewWindow)
        {
            CComQIPtr<IDispatch> spDispNew;
            VARIANT_BOOL bCancel = VARIANT_FALSE;
        
            // Test if the event sink can give us a new window to navigate into
            Fire_NewWindow2(&spDispNew, &bCancel);

            lFlags &= ~(navOpenInNewWindow);
            if ((bCancel == VARIANT_FALSE) && spDispNew)
            {
                CComQIPtr<IWebBrowser2> spOther = spDispNew;
                if (spOther)
                {
                    CComVariant vURL(URL);
                    CComVariant vFlags(lFlags);
                    return spOther->Navigate2(&vURL, &vFlags, TargetFrameName, PostData, Headers);
                }
            }

            // NOTE: The IE control will launch an instance of iexplore.exe and
            //       return an interface to that if the client does not respond to
            //       FireNewWindow2, but the Mozilla control will not. Potentially
            //       it could also open an instance of IE for such occasions.
            //

            // Can't open a new window without client support
            return S_OK;
        }

        // As documented in MSDN:
        //
        //   The post data specified by PostData is passed as a SAFEARRAY
        //   structure. The variant should be of type VT_ARRAY and point to
        //   a SAFEARRAY. The SAFEARRAY should be of element type VT_UI1,
        //   dimension one, and have an element count equal to the number of
        //   bytes of post data.

        // Extract the post data parameter
        nsCOMPtr<nsIInputStream> postDataStream;
        mLastPostData.Clear();
        if (PostData &&
            PostData->vt == (VT_ARRAY | VT_UI1) &&
            PostData->parray)
        {
            mLastPostData.Copy(PostData);
            
            unsigned long nSizeData = PostData->parray->rgsabound[0].cElements;
            if (nSizeData > 0)
            {
                char szCL[64];
                sprintf(szCL, "Content-Length: %lu\r\n\r\n", nSizeData);
                unsigned long nSizeCL = strlen(szCL);
                unsigned long nSize = nSizeCL + nSizeData;

                char *tmp = (char *) nsMemory::Alloc(nSize + 1); // byte stream owns this mem
                if (tmp)
                {

                    // Copy the array data into a buffer
                    SafeArrayLock(PostData->parray);
                    memcpy(tmp, szCL, nSizeCL);
                    memcpy(tmp + nSizeCL, PostData->parray->pvData, nSizeData);
                    tmp[nSize] = '\0';
                    SafeArrayUnlock(PostData->parray);

                    // Create a byte array input stream object.
                    nsCOMPtr<nsIByteArrayInputStream> stream;
                    nsresult rv = NS_NewByteArrayInputStream(
                        getter_AddRefs(stream), tmp, nSize);
                    if (NS_FAILED(rv) || !stream)
                    {
                        NS_ASSERTION(0, "cannot create byte stream");
                        nsMemory::Free(tmp);
                        return SetErrorInfo(E_UNEXPECTED);
                    }

                    postDataStream = stream;
                }
            }
        }

        // Extract the headers parameter
        nsCOMPtr<nsIByteArrayInputStream> headersStream;
        if (Headers &&
            Headers->vt == VT_BSTR &&
            Headers->bstrVal)
        {
            
            USES_CONVERSION;
            char *headers = OLE2A(Headers->bstrVal);
            if (headers)
            {
                size_t nSize = SysStringLen(Headers->bstrVal) + 1;
                char *tmp = (char *) nsMemory::Alloc(nSize); // byteArray stream owns this mem
                if (tmp)
                {
                    // Copy BSTR to buffer
                    WideCharToMultiByte(CP_ACP, 0, Headers->bstrVal, nSize - 1, tmp, nSize, NULL, NULL);
                    tmp[nSize - 1] = '\0';

                    // Create a byte array input stream object which will own the buffer
                    nsCOMPtr<nsIByteArrayInputStream> stream;
                    nsresult rv = 
                        NS_NewByteArrayInputStream(getter_AddRefs(stream), tmp, nSize);
                    if (NS_FAILED(rv) || !stream)
                    {
                        NS_ASSERTION(0, "cannot create byte stream");
                        nsMemory::Free(tmp);
                    }
                    headersStream = do_QueryInterface(stream);
                }
            }
        }

        // Use the specified target or the top level web navigation
        nsCOMPtr<nsIWebNavigation> webNavToUse;
        if (targetNav)
        {
            webNavToUse = targetNav;
        }
        else
        {
            GetWebNavigation(getter_AddRefs(webNavToUse));
        }
    
        // Load the URL    
        nsresult rv = NS_ERROR_FAILURE;
        if (webNavToUse)
        {
            rv = webNavToUse->LoadURI(URL,
                    loadFlags, nsnull, postDataStream, headersStream);
        }

        return NS_SUCCEEDED(rv) ? S_OK : E_FAIL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Refresh(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::Refresh()\n"));
        // Reload the page
        CComVariant vRefreshType(REFRESH_NORMAL);
        return Refresh2(&vRefreshType);
    }

    virtual HRESULT STDMETHODCALLTYPE Refresh2(VARIANT __RPC_FAR *Level)
    {
        ATLTRACE(_T("IWebBrowserImpl::Refresh2()\n"));

        ENSURE_BROWSER_IS_VALID();
        ENSURE_GET_WEBNAV();
        if (Level == NULL)
            return E_INVALIDARG;

        // Check the requested refresh type
        OLECMDID_REFRESHFLAG iRefreshLevel = OLECMDIDF_REFRESH_NORMAL;
        CComVariant vLevelAsInt;
        if ( vLevelAsInt.ChangeType(VT_I4, Level) != S_OK )
        {
            NS_ASSERTION(0, "Cannot change refresh type to int");
            return SetErrorInfo(E_UNEXPECTED);
        }
        iRefreshLevel = (OLECMDID_REFRESHFLAG) vLevelAsInt.iVal;

        // Turn the IE refresh type into the nearest NG equivalent
        PRUint32 flags = nsIWebNavigation::LOAD_FLAGS_NONE;
        switch (iRefreshLevel & OLECMDIDF_REFRESH_LEVELMASK)
        {
        case OLECMDIDF_REFRESH_NORMAL:
        case OLECMDIDF_REFRESH_IFEXPIRED:
        case OLECMDIDF_REFRESH_CONTINUE:
        case OLECMDIDF_REFRESH_NO_CACHE:
        case OLECMDIDF_REFRESH_RELOAD:
            flags = nsIWebNavigation::LOAD_FLAGS_NONE;
            break;
        case OLECMDIDF_REFRESH_COMPLETELY:
            flags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
            break;
        default:
            // No idea what refresh type this is supposed to be
            NS_ASSERTION(0, "Unknown refresh type");
            return SetErrorInfo(E_UNEXPECTED);
        }

        webNav->Reload(flags);

        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Stop(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::Stop()\n"));
        ENSURE_BROWSER_IS_VALID();
        ENSURE_GET_WEBNAV();
        webNav->Stop(nsIWebNavigation::STOP_ALL);
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Application(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Application()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (!ppDisp)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        // Return a pointer to this controls dispatch interface
        *ppDisp = (IDispatch *) this;
        (*ppDisp)->AddRef();
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
    {
        // TODO
        return E_NOTIMPL;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Container(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Container()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (!ppDisp)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO: Implement get_Container: Retrieve a pointer to the IDispatch interface of the container.
        *ppDisp = NULL;
        return SetErrorInfo(E_UNEXPECTED);
    }
    virtual HRESULT STDMETHODCALLTYPE get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Document()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (!ppDisp)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        *ppDisp = NULL;
        return E_NOTIMPL;
    }
    virtual HRESULT STDMETHODCALLTYPE get_TopLevelContainer(VARIANT_BOOL __RPC_FAR *pBool)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_TopLevelContainer()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (!pBool)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO: Implement get_TopLevelContainer
        *pBool = VARIANT_TRUE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Type(BSTR __RPC_FAR *Type)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Type()\n"));
        ENSURE_BROWSER_IS_VALID();

        //NOTE:    This code should work in theory, but can't be verified because GetDoctype
        //        has not been implemented yet.
#if 0
        nsIDOMDocument *pIDOMDocument = nsnull;
        if ( SUCCEEDED(GetDOMDocument(&pIDOMDocument)) )
        {
            nsIDOMDocumentType *pIDOMDocumentType = nsnull;
            if ( SUCCEEDED(pIDOMDocument->GetDoctype(&pIDOMDocumentType)) )
            {
                nsAutoString docName;
                pIDOMDocumentType->GetName(docName);
                //NG_TRACE("pIDOMDocumentType returns: %s", docName);
                //Still need to manipulate docName so that it goes into *Type param of this function.
            }
        }
#endif
        //TODO: Implement get_Type
        return SetErrorInfo(E_FAIL, L"get_Type: failed");
    }

    virtual HRESULT STDMETHODCALLTYPE get_Left(long __RPC_FAR *pl)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Left()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pl == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //TODO: Implement get_Left - Should return the left position of this control.
        *pl = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Left(long Left)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Left()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement put_Left - Should set the left position of this control.
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Top(long __RPC_FAR *pl)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Top()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pl == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO: Implement get_Top - Should return the top position of this control.
        *pl = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Top(long Top)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Top()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement set_Top - Should set the top position of this control.
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Width(long __RPC_FAR *pl)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Width()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement get_Width- Should return the width of this control.
        if (pl == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        *pl = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Width(long Width)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Width()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement put_Width - Should set the width of this control.
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE get_Height(long __RPC_FAR *pl)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Height()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pl == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //TODO: Implement get_Height - Should return the hieght of this control.
        *pl = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Height(long Height)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Height()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement put_Height - Should set the height of this control.
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE get_LocationName(BSTR __RPC_FAR *LocationName)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_LocationName()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (LocationName == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        // Get the url from the web shell
        nsXPIDLString szLocationName;
        ENSURE_GET_WEBNAV();
        nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webNav);
        baseWindow->GetTitle(getter_Copies(szLocationName));
        if (nsnull == (const PRUnichar *) szLocationName)
        {
            return SetErrorInfo(E_UNEXPECTED);
        }

        // Convert the string to a BSTR
        USES_CONVERSION;
        LPCOLESTR pszConvertedLocationName = W2COLE((const PRUnichar *) szLocationName);
        *LocationName = SysAllocString(pszConvertedLocationName);

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_LocationURL(BSTR __RPC_FAR *LocationURL)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_LocationURL()\n"));
        ENSURE_BROWSER_IS_VALID();

        if (LocationURL == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        nsCOMPtr<nsIURI> uri;
        // Get the current url from the browser
        nsCOMPtr<nsIWebNavigation> webNav;
        GetWebNavigation(getter_AddRefs(webNav));
        if (webNav)
        {
            webNav->GetCurrentURI(getter_AddRefs(uri));
        }
        if (uri)
        {
            USES_CONVERSION;
            nsCAutoString aURI;
            uri->GetAsciiSpec(aURI);
            *LocationURL = SysAllocString(A2OLE(aURI.get()));
        }
        else
        {
            *LocationURL = NULL;
        }

    return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Busy(VARIANT_BOOL __RPC_FAR *pBool)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Busy()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (!pBool)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        *pBool = (mBusyFlag) ? VARIANT_TRUE : VARIANT_FALSE;
        return S_OK;
    }

// IWebBrowserApp implementation
    virtual HRESULT STDMETHODCALLTYPE Quit(void)
    {
        ATLTRACE(_T("IWebBrowserImpl::Quit()\n"));
        ENSURE_BROWSER_IS_VALID();

        //This generates an exception in the IE control.
        // TODO fire quit event
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE ClientToWindow(int __RPC_FAR *pcx, int __RPC_FAR *pcy)
    {
        ATLTRACE(_T("IWebBrowserImpl::ClientToWindow()\n"));
        ENSURE_BROWSER_IS_VALID();

        //This generates an exception in the IE control.
        // TODO convert points to be relative to browser
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE PutProperty(BSTR Property, VARIANT vtValue)
    {
        ATLTRACE(_T("IWebBrowserImpl::PutProperty()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Property == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        mPropertyList.AddOrReplaceNamedProperty(Property, vtValue);
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE GetProperty(BSTR Property, VARIANT __RPC_FAR *pvtValue)
    {
        ATLTRACE(_T("IWebBrowserImpl::GetProperty()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Property == NULL || pvtValue == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        VariantInit(pvtValue);
        for (unsigned long i = 0; i < mPropertyList.GetSize(); i++)
        {
            if (wcsicmp(mPropertyList.GetNameOf(i), Property) == 0)
            {
                // Copy the new value
                VariantCopy(pvtValue, const_cast<VARIANT *>(mPropertyList.GetValueOf(i)));
                return S_OK;
            }
        }
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Name(BSTR __RPC_FAR *Name)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Name()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Name == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        // TODO: Implement get_Name (get Mozilla's executable name)
        *Name = SysAllocString(L"Mozilla Web Browser Control");
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_HWND(long __RPC_FAR *pHWND)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_HWND()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pHWND == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //This is supposed to return a handle to the IE main window.  Since that doesn't exist
        //in the control's case, this shouldn't do anything.
        *pHWND = NULL;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_FullName(BSTR __RPC_FAR *FullName)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_FullName()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (FullName == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        // TODO: Implement get_FullName (Return the full path of the executable containing this control)
        *FullName = SysAllocString(L""); // TODO get Mozilla's executable name
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Path(BSTR __RPC_FAR *Path)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Path()\n"));
        ENSURE_BROWSER_IS_VALID();

        if (Path == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        // TODO: Implement get_Path (get Mozilla's path)
        *Path = SysAllocString(L"");
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Visible(VARIANT_BOOL __RPC_FAR *pBool)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Visible()\n"));
        ENSURE_BROWSER_IS_VALID();

        if (pBool == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //TODO: Implement get_Visible?
        *pBool = VARIANT_TRUE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_Visible(VARIANT_BOOL Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Visible()\n"));
        ENSURE_BROWSER_IS_VALID();

        //TODO: Implement put_Visible?
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_StatusBar(VARIANT_BOOL __RPC_FAR *pBool)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_StatusBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pBool == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //There is no StatusBar in this control.
        *pBool = VARIANT_FALSE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_StatusBar(VARIANT_BOOL Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_StatusBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        //There is no StatusBar in this control.
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_StatusText(BSTR __RPC_FAR *StatusText)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_StatusText()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (StatusText == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //TODO: Implement get_StatusText
        //NOTE: This function is related to the MS status bar which doesn't exist in this control.  Needs more
        //        investigation, but probably doesn't apply.
        *StatusText = SysAllocString(L"");
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_StatusText(BSTR StatusText)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_StatusText()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement put_StatusText
        //NOTE: This function is related to the MS status bar which doesn't exist in this control.  Needs more
        //        investigation, but probably doesn't apply.
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_ToolBar(int __RPC_FAR *Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_ToolBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Value == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //There is no ToolBar in this control.
        *Value = FALSE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_ToolBar(int Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_ToolBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        //There is no ToolBar in this control.
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_MenuBar(VARIANT_BOOL __RPC_FAR *Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_MenuBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Value == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //There is no MenuBar in this control.
        *Value = VARIANT_FALSE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_MenuBar(VARIANT_BOOL Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_MenuBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        //There is no MenuBar in this control.
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_FullScreen(VARIANT_BOOL __RPC_FAR *pbFullScreen)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_FullScreen()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pbFullScreen == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }

        //FullScreen mode doesn't really apply to this control.
        *pbFullScreen = VARIANT_FALSE;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE put_FullScreen(VARIANT_BOOL bFullScreen)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_FullScreen()\n"));
        ENSURE_BROWSER_IS_VALID();
        //FullScreen mode doesn't really apply to this control.
        return S_OK;
    }

// IWebBrowser2 implementation
    virtual HRESULT STDMETHODCALLTYPE Navigate2(VARIANT __RPC_FAR *URL, VARIANT __RPC_FAR *Flags, VARIANT __RPC_FAR *TargetFrameName, VARIANT __RPC_FAR *PostData, VARIANT __RPC_FAR *Headers)
    {
        ATLTRACE(_T("IWebBrowserImpl::Navigate2()\n"));
        CComVariant vURLAsString;
        if (vURLAsString.ChangeType(VT_BSTR, URL) != S_OK)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        return Navigate(vURLAsString.bstrVal, Flags, TargetFrameName, PostData, Headers);
    }
    
    virtual HRESULT STDMETHODCALLTYPE QueryStatusWB(OLECMDID cmdID, OLECMDF __RPC_FAR *pcmdf)
    {
        ATLTRACE(_T("IWebBrowserImpl::QueryStatusWB()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pcmdf == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        // Call through to IOleCommandTarget::QueryStatus
        CComQIPtr<IOleCommandTarget> cmdTarget = this;
        if (cmdTarget)
        {
            OLECMD cmd;
            HRESULT hr;
    
            cmd.cmdID = cmdID;
            cmd.cmdf = 0;
            hr = cmdTarget->QueryStatus(NULL, 1, &cmd, NULL);
            if (SUCCEEDED(hr))
            {
                *pcmdf = (OLECMDF) cmd.cmdf;
            }
            return hr;
        }
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE ExecWB(OLECMDID cmdID, OLECMDEXECOPT cmdexecopt, VARIANT __RPC_FAR *pvaIn, VARIANT __RPC_FAR *pvaOut)
    {
        ATLTRACE(_T("IWebBrowserImpl::ExecWB()\n"));
        ENSURE_BROWSER_IS_VALID();
        // Call through to IOleCommandTarget::Exec
        CComQIPtr<IOleCommandTarget> cmdTarget = this;
        if (cmdTarget)
        {
            return cmdTarget->Exec(NULL, cmdID, cmdexecopt, pvaIn, pvaOut);
        }
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE ShowBrowserBar(VARIANT __RPC_FAR *pvaClsid, VARIANT __RPC_FAR *pvarShow, VARIANT __RPC_FAR *pvarSize)
    {
        ATLTRACE(_T("IWebBrowserImpl::ShowBrowserBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_ReadyState(READYSTATE __RPC_FAR *plReadyState)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_ReadyState()\n"));
        // Note: may be called when browser is not yet initialized so there
        // is no validity check here.
        if (plReadyState == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        *plReadyState = mBrowserReadyState;
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Offline(VARIANT_BOOL __RPC_FAR *pbOffline)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Offline()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pbOffline == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO: Implement get_Offline
        *pbOffline = VARIANT_FALSE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Offline(VARIANT_BOOL bOffline)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Offline()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement get_Offline
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Silent(VARIANT_BOOL __RPC_FAR *pbSilent)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Silent()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pbSilent == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //Only really applies to the IE app, not a control
        *pbSilent = VARIANT_FALSE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Silent(VARIANT_BOOL bSilent)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Silent()\n"));
        ENSURE_BROWSER_IS_VALID();
        //Only really applies to the IE app, not a control
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_RegisterAsBrowser(VARIANT_BOOL __RPC_FAR *pbRegister)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_RegisterAsBrowser()\n"));
        ENSURE_BROWSER_IS_VALID();

        if (pbRegister == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO: Implement get_RegisterAsBrowser
        *pbRegister = VARIANT_FALSE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_RegisterAsBrowser(VARIANT_BOOL bRegister)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_RegisterAsBrowser()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO: Implement put_RegisterAsBrowser
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister)
    {
        // TODO
        return E_NOTIMPL;
    }
    virtual HRESULT STDMETHODCALLTYPE put_RegisterAsDropTarget(VARIANT_BOOL bRegister)
    {
        // TODO
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_TheaterMode(VARIANT_BOOL __RPC_FAR *pbRegister)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_TheaterMode()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (pbRegister == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        // TheaterMode doesn't apply to this control.
        *pbRegister = VARIANT_FALSE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_TheaterMode(VARIANT_BOOL bRegister)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_TheaterMode()\n"));
        ENSURE_BROWSER_IS_VALID();
        //There is no TheaterMode in this control.
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE get_AddressBar(VARIANT_BOOL __RPC_FAR *Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_AddressBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Value == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //There is no AddressBar in this control.
        *Value = VARIANT_FALSE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_AddressBar(VARIANT_BOOL Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_AddressBar()\n"));
        ENSURE_BROWSER_IS_VALID();
        //There is no AddressBar in this control.
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE get_Resizable(VARIANT_BOOL __RPC_FAR *Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::get_Resizable()\n"));
        ENSURE_BROWSER_IS_VALID();
        if (Value == NULL)
        {
            return SetErrorInfo(E_INVALIDARG);
        }
        //TODO:  Not sure if this should actually be implemented or not.
        *Value = VARIANT_TRUE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE put_Resizable(VARIANT_BOOL Value)
    {
        ATLTRACE(_T("IWebBrowserImpl::put_Resizable()\n"));
        ENSURE_BROWSER_IS_VALID();
        //TODO:  Not sure if this should actually be implemented or not.
        return S_OK;
    }
};

#endif
