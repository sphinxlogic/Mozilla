/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsWindow.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"
#include <windows.h>
#include <SHLOBJ.H>

// commdlg.h and cderr.h are needed to build with WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <cderr.h>

#include "nsString.h"
#include "nsToolkit.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

nsString nsFilePicker::mLastUsedUnicodeDirectory;
char nsFilePicker::mLastUsedDirectory[MAX_PATH+1] = { 0 };

#define MAX_EXTENSION_LENGTH 10

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
{
  mParentWidget = nsnull;
  mUnicodeEncoder = nsnull;
  mUnicodeDecoder = nsnull;
  mSelectedType   = 1;
  mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  NS_IF_RELEASE(mParentWidget);
  NS_IF_RELEASE(mUnicodeEncoder);
  NS_IF_RELEASE(mUnicodeDecoder);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::ShowW(PRInt16 *aReturnVal)
{
  NS_ENSURE_ARG_POINTER(aReturnVal);

  // suppress blur events
  if (mParentWidget) {
    nsWindow *parent = NS_STATIC_CAST(nsWindow *, mParentWidget);
    parent->SuppressBlurEvents(PR_TRUE);
  }

  PRBool result = PR_FALSE;
  PRUnichar fileBuffer[MAX_PATH+1];
  wcsncpy(fileBuffer,  mDefault.get(), MAX_PATH);

  nsAutoString htmExt(NS_LITERAL_STRING("html"));
  PRUnichar *title = ToNewUnicode(mTitle);
  nsAutoString initialDir;
  mDisplayDirectory->GetPath(initialDir);

  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty()) {
    // Allocate copy of last used dir.
    initialDir = mLastUsedUnicodeDirectory;
  }

  mUnicodeFile.SetLength(0);

  if (mMode == modeGetFolder) {
    PRUnichar dirBuffer[MAX_PATH+1];
    wcsncpy(dirBuffer, initialDir.get(), MAX_PATH);

    BROWSEINFOW browserInfo;
    browserInfo.hwndOwner      = (HWND)
      (mParentWidget ? mParentWidget->GetNativeData(NS_NATIVE_WINDOW) : 0); 
    browserInfo.pidlRoot       = nsnull;
    browserInfo.pszDisplayName = (LPWSTR)dirBuffer;
    browserInfo.lpszTitle      = title;
    browserInfo.ulFlags        = BIF_RETURNONLYFSDIRS;//BIF_STATUSTEXT | BIF_RETURNONLYFSDIRS;
    browserInfo.lpfn           = nsnull;
    browserInfo.lParam         = nsnull;
    browserInfo.iImage         = nsnull;

    // XXX UNICODE support is needed here --> DONE
    LPITEMIDLIST list = nsToolkit::mSHBrowseForFolder(&browserInfo);
    if (list != NULL) {
      result = nsToolkit::mSHGetPathFromIDList(list, (LPWSTR)fileBuffer);
      if (result) {
          mUnicodeFile.Append(fileBuffer);
      }
  
      // free PIDL
      LPMALLOC pMalloc = NULL;
      ::SHGetMalloc(&pMalloc);
      if(pMalloc) {
         pMalloc->Free(list);
         pMalloc->Release();
      }
    }
  }
  else {

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);

    nsString filterBuffer = mFilterList;
                                  
    if (!initialDir.IsEmpty()) {
      ofn.lpstrInitialDir = initialDir.get();
    }
    
    ofn.lpstrTitle   = (LPCWSTR)title;
    ofn.lpstrFilter  = (LPCWSTR)filterBuffer.get();
    ofn.nFilterIndex = mSelectedType;
    ofn.hwndOwner    = (HWND)
      (mParentWidget ? mParentWidget->GetNativeData(NS_NATIVE_WINDOW) : 0); 
    ofn.lpstrFile    = fileBuffer;
    ofn.nMaxFile     = MAX_PATH;

    ofn.Flags = OFN_NOCHANGEDIR | OFN_SHAREAWARE | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    if (!mDefaultExtension.IsEmpty()) {
      ofn.lpstrDefExt = mDefaultExtension.get();
    }
    else {
      // Get file extension from suggested filename
      //  to detect if we are saving an html file
      //XXX: nsIFile SHOULD HAVE A GetExtension() METHOD!
      PRInt32 extIndex = mDefault.RFind(".");
      if ( extIndex >= 0) {
        nsAutoString ext;
        mDefault.Right(ext, mDefault.Length() - extIndex);
        // Should we test for ".cgi", ".asp", ".jsp" and other
        // "generated" html pages?

        if ( ext.EqualsIgnoreCase(".htm")  ||
             ext.EqualsIgnoreCase(".html") ||
             ext.EqualsIgnoreCase(".shtml") ) {
          // This is supposed to append ".htm" if user doesn't supply an extension
          //XXX Actually, behavior is sort of weird:
          //    often appends ".html" even if you have an extension
          //    It obeys your extension if you put quotes around name
          ofn.lpstrDefExt = htmExt.get();
        }
      }
    }

    if (mMode == modeOpen) {
      // FILE MUST EXIST!
      ofn.Flags |= OFN_FILEMUSTEXIST;
      result = nsToolkit::mGetOpenFileName(&ofn);
    }
    else if (mMode == modeOpenMultiple) {
      ofn.Flags |= OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
      result = nsToolkit::mGetOpenFileName(&ofn);
    }
    else if (mMode == modeSave) {
      ofn.Flags |= OFN_NOREADONLYRETURN;
      result = nsToolkit::mGetSaveFileName(&ofn);
      if (!result) {
        // Error, find out what kind.
        if (::GetLastError() == ERROR_INVALID_PARAMETER ||
            ::CommDlgExtendedError() == FNERR_INVALIDFILENAME) {
          // probably the default file name is too long or contains illegal characters!
          // Try again, without a starting file name.
          ofn.lpstrFile[0] = 0;
          result = nsToolkit::mGetSaveFileName(&ofn);
        }
      }
    }
    else {
      NS_ASSERTION(0, "unsupported mode"); 
    }
  
    // Remember what filter type the user selected
    mSelectedType = (PRInt16)ofn.nFilterIndex;

    // Set user-selected location of file or directory
    if (result == PR_TRUE) {
      if (mMode == modeOpenMultiple) {
        nsresult rv = NS_NewISupportsArray(getter_AddRefs(mFiles));
        NS_ENSURE_SUCCESS(rv,rv);
        
        // from msdn.microsoft.com, "Open and Save As Dialog Boxes" section:
        // If you specify OFN_EXPLORER,
        // The directory and file name strings are NULL separated, 
        // with an extra NULL character after the last file name. 
        // This format enables the Explorer-style dialog boxes
        // to return long file names that include spaces. 
        PRUnichar *current = fileBuffer;
        
        nsAutoString dirName(current);
        // sometimes dirName contains a trailing slash
        // and sometimes it doesn't.
        if (current[dirName.Length() - 1] != '\\')
          dirName.Append((PRUnichar)'\\');
        
        while (current && *current && *(current + nsCRT::strlen(current) + 1)) {
          current = current + nsCRT::strlen(current) + 1;
          
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = file->InitWithPath(dirName + nsDependentString(current));
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = mFiles->AppendElement(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
        
        // handle the case where the user selected just one
        // file.  according to msdn.microsoft.com:
        // If you specify OFN_ALLOWMULTISELECT and the user selects 
        // only one file, the lpstrFile string does not have 
        // a separator between the path and file name.
        if (current && *current && (current == fileBuffer)) {
          nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = file->InitWithPath(nsDependentString(current));
          NS_ENSURE_SUCCESS(rv,rv);
          
          rv = mFiles->AppendElement(file);
          NS_ENSURE_SUCCESS(rv,rv);
        }
      }
      else {
        // I think it also needs a conversion here (to unicode since appending to nsString) 
        // but doing that generates garbage file name, weird.
        mUnicodeFile.Append(fileBuffer);
      }
    }

  }

  if (title)
    nsMemory::Free( title );

  if (result) {
    PRInt16 returnOKorReplace = returnOK;

    // Remember last used directory.
    nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

    // work around.  InitWithPath() will convert UCS2 to FS path !!!  corrupts unicode 
    file->InitWithPath(mUnicodeFile);
    nsCOMPtr<nsIFile> dir;
    if (NS_SUCCEEDED(file->GetParent(getter_AddRefs(dir)))) {
      nsCOMPtr<nsILocalFile> localDir(do_QueryInterface(dir));
      if (localDir) {
        nsAutoString newDir;
        localDir->GetPath(newDir);
        if(!newDir.IsEmpty())
          mLastUsedUnicodeDirectory.Assign(newDir);
        // Update mDisplayDirectory with this directory, also.
        // Some callers rely on this.
        mDisplayDirectory->InitWithPath(mLastUsedUnicodeDirectory);
      }
    }

    if (mMode == modeSave) {
      // Windows does not return resultReplace,
      //   we must check if file already exists
      PRBool exists = PR_FALSE;
      file->Exists(&exists);
      if (exists)
        returnOKorReplace = returnReplace;
    }
    *aReturnVal = returnOKorReplace;
  }
  else {
    *aReturnVal = returnCancel;
  }
  if (mParentWidget) {
    nsWindow *parent = NS_STATIC_CAST(nsWindow *, mParentWidget);
    parent->SuppressBlurEvents(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::Show(PRInt16 *aReturnVal)
{
  return ShowW(aReturnVal);
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mUnicodeFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(mUnicodeFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithPath(mUnicodeFile);

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);

  NS_ADDREF(*aFileURL = fileURL);

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);
  return NS_NewArrayEnumerator(aFiles, mFiles);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const PRUnichar *aString)
{
  mDefault = aString;

  //First, make sure the file name is not too long!
  PRInt32 nameLength;
  PRInt32 nameIndex = mDefault.RFind("\\");
  if (nameIndex == kNotFound)
    nameIndex = 0;
  else
    nameIndex ++;
  nameLength = mDefault.Length() - nameIndex;
  
  if (nameLength > _MAX_FNAME) {
    PRInt32 extIndex = mDefault.RFind(".");
    if (extIndex == kNotFound)
      extIndex = mDefault.Length();

    //Let's try to shave the needed characters from the name part
    PRInt32 charsToRemove = nameLength - _MAX_FNAME;
    if (extIndex - nameIndex >= charsToRemove) {
      mDefault.Cut(extIndex - charsToRemove, charsToRemove);
    }
  }

  //Then, we need to replace illegal characters.
  //At this stage, we cannot replace the backslash as the string might represent a file path.
  mDefault.ReplaceChar(FILE_ILLEGAL_CHARACTERS, '-');

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(PRUnichar **aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(PRUnichar **aExtension)
{
  *aExtension = ToNewUnicode(mDefaultExtension);
  if (!*aExtension)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const PRUnichar *aExtension)
{
  mDefaultExtension = aExtension;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the filter index
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  *aFilterIndex = mSelectedType - 1;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  // Windows' filter index is 1-based, we use a 0-based system.
  mSelectedType = aFilterIndex + 1;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  *aDirectory = mDisplayDirectory;
  NS_IF_ADDREF(*aDirectory);
  return NS_OK;
}



//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::InitNative(nsIWidget *aParent,
                                       const PRUnichar *aTitle,
                                       PRInt16 aMode)
{
  mParentWidget = aParent;
  NS_IF_ADDREF(mParentWidget);
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
  return NS_OK;
}


//-------------------------------------------------------------------------
void nsFilePicker::GetFileSystemCharset(nsCString & fileSystemCharset)
{
  static nsCAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.Assign(NS_LITERAL_CSTRING("windows-1252"));
  }
  fileSystemCharset = aCharset;
}

//-------------------------------------------------------------------------
char * nsFilePicker::ConvertToFileSystemCharset(const PRUnichar *inString)
{
  char *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeEncoder) {
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeEncoderRaw(fileSystemCharset.get(), &mUnicodeEncoder);
    }
  }

  // converts from unicode to the file system charset
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeEncoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( char*, nsMemory::Alloc( outLength+1 ) );
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeEncoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = '\0';
      }
    }
  }
  
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}

//-------------------------------------------------------------------------
PRUnichar * nsFilePicker::ConvertFromFileSystemCharset(const char *inString)
{
  PRUnichar *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeDecoder) {
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeDecoderRaw(fileSystemCharset.get(), &mUnicodeDecoder);
    }
  }

  // converts from the file system charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeDecoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( PRUnichar*, nsMemory::Alloc( (outLength+1) * sizeof( PRUnichar ) ) );
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeDecoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = 0;
      }
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "error charset conversion");
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}


NS_IMETHODIMP
nsFilePicker::AppendFilter(const PRUnichar *aTitle, const PRUnichar *aFilter)
{
  mFilterList.Append(aTitle);
  mFilterList.Append(PRUnichar('\0'));

  if (!nsCRT::strcmp(aFilter, NS_LITERAL_STRING("..apps").get()))
    mFilterList.Append(NS_LITERAL_STRING("*.exe;*.com"));
  else
  {
    nsString filter;
    filter.Assign(aFilter);
    filter.StripWhitespace();
    if (filter.Equals(NS_LITERAL_STRING("*")))
      filter.Append(NS_LITERAL_STRING(".*"));
    mFilterList.Append(filter);
  }

  mFilterList.Append(PRUnichar('\0'));

  return NS_OK;
}
