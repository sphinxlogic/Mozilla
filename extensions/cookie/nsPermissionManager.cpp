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
 *   Michiel van Leeuwen (mvl@exedo.nl)
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

#include "nsPermissionManager.h"
#include "nsPermission.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prprf.h"

////////////////////////////////////////////////////////////////////////////////

// XXX how do we choose this number?
#define PL_ARENA_CONST_ALIGN_MASK 3
#include "plarena.h"

static PLArenaPool *gHostArena = nsnull;

// making sHostArena 512b for nice allocation
// growing is quite cheap
#define HOST_ARENA_SIZE 512

// equivalent to strdup() - does no error checking,
// we're assuming we're only called with a valid pointer
static char *
ArenaStrDup(const char* str, PLArenaPool* aArena)
{
  void* mem;
  const PRUint32 size = strlen(str) + 1;
  PL_ARENA_ALLOCATE(mem, aArena, size);
  if (mem)
    memcpy(mem, str, size);
  return NS_STATIC_CAST(char*, mem);
}

nsHostEntry::nsHostEntry(const char* aHost)
{
  mHost = ArenaStrDup(aHost, gHostArena);
}

nsHostEntry::nsHostEntry(const nsHostEntry& toCopy)
{
  mHost = ArenaStrDup(toCopy.mHost, gHostArena);
}

////////////////////////////////////////////////////////////////////////////////

class nsPermissionEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS
    
    nsPermissionEnumerator(const nsTHashtable<nsHostEntry> *aHostTable,
                           const char*   *aHostList,
                           const PRUint32 aHostCount,
                           const char*   *aTypeArray)
      : mHostCount(aHostCount),
        mHostIndex(0),
        mTypeIndex(0),
        mHostTable(aHostTable),
        mHostList(aHostList),
        mTypeArray(aTypeArray)
    {
      Prefetch();
    }
    
    NS_IMETHOD HasMoreElements(PRBool *aResult) 
    {
      *aResult = (mNextPermission != nsnull);
      return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **aResult) 
    {
      *aResult = mNextPermission;
      if (!mNextPermission)
        return NS_ERROR_FAILURE;

      NS_ADDREF(*aResult);
      
      Prefetch();

      return NS_OK;
    }

    virtual ~nsPermissionEnumerator() 
    {
      delete[] mHostList;
    }

  protected:
    void Prefetch();

    PRInt32 mHostCount;
    PRInt32 mHostIndex;
    PRInt32 mTypeIndex;
    
    const nsTHashtable<nsHostEntry> *mHostTable;
    const char*                     *mHostList;
    nsCOMPtr<nsIPermission>          mNextPermission;
    const char*                     *mTypeArray;
};

NS_IMPL_ISUPPORTS1(nsPermissionEnumerator, nsISimpleEnumerator)

// Sets mNextPermission to a new nsIPermission on success,
// to nsnull when no new permissions are present.
void
nsPermissionEnumerator::Prefetch() 
{
  // init to null, so we know when we've prefetched something
  mNextPermission = nsnull;

  // check we have something more to get
  PRUint32 permission;
  while (mHostIndex < mHostCount && !mNextPermission) {
    // loop over the types to find it
    nsHostEntry *entry = mHostTable->GetEntry(mHostList[mHostIndex]);
    if (entry) {
      // see if we've found it
      permission = entry->GetPermission(mTypeIndex);
      if (permission != nsIPermissionManager::UNKNOWN_ACTION) {
        mNextPermission = new nsPermission(entry->GetHost(), 
                                           nsDependentCString(mTypeArray[mTypeIndex]),
                                           permission);
      }
    }

    // increment mTypeIndex/mHostIndex as required
    ++mTypeIndex;
    if (mTypeIndex == NUMBER_OF_TYPES) {
      mTypeIndex = 0;
      ++mHostIndex;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

static const char kPermissionsFileName[] = "cookperm.txt";
static const char kPermissionChangeNotification[] = PERM_CHANGE_NOTIFICATION;

NS_IMPL_ISUPPORTS3(nsPermissionManager, nsIPermissionManager, nsIObserver, nsISupportsWeakReference)

nsPermissionManager::nsPermissionManager()
 : mHostCount(0)
{
}

nsPermissionManager::~nsPermissionManager()
{
  RemoveTypeStrings();
  RemoveAllFromMemory();
}

nsresult nsPermissionManager::Init()
{
  nsresult rv;

  if (!mHostTable.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Cache the permissions file
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mPermissionsFile));
  if (NS_SUCCEEDED(rv)) {
    rv = mPermissionsFile->AppendNative(NS_LITERAL_CSTRING(kPermissionsFileName));
  }

  // Clear the array of type strings
  memset(mTypeArray, nsnull, sizeof(mTypeArray));

  // Ignore an error. That is not a problem. No cookperm.txt usually.
  Read();

  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::Add(nsIURI     *aURI,
                         const char *aType,
                         PRUint32    aPermission)
{
  NS_ASSERTION(aURI, "could not get uri");
  NS_ENSURE_ARG_POINTER(aType);
  nsresult rv;

  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  if (hostPort.IsEmpty()) {
    // Nothing to add
    return NS_OK;
  }

  PRInt32 typeIndex = GetTypeIndex(aType, PR_TRUE);
  if (typeIndex == -1 || aPermission >= NUMBER_OF_PERMISSIONS)
    return NS_ERROR_FAILURE;

  rv = AddInternal(hostPort, typeIndex, aPermission);
  if (NS_FAILED(rv)) return rv;

  // Notify permission manager dialog to update its display
  //
  // This used to be conditional, but now we use AddInternal 
  // for cases when no notification is needed
  NotifyObservers(hostPort);

  mChangedList = PR_TRUE;
  Write();
  return NS_OK;
}

// This only adds a permission to memory (it doesn't save to disk), and doesn't
// bounds check aTypeIndex or aPermission. These are up to the caller.
nsresult
nsPermissionManager::AddInternal(const nsAFlatCString &aHost,
                                 PRInt32 aTypeIndex,
                                 PRUint32 aPermission)
{
  if (!gHostArena) {
    gHostArena = new PLArenaPool;
    if (!gHostArena)
      return NS_ERROR_OUT_OF_MEMORY;    
    PL_INIT_ARENA_POOL(gHostArena, "PermissionHostArena", HOST_ARENA_SIZE);
  }

  // When an entry already exists, AddEntry will return that, instead
  // of adding a new one
  nsHostEntry *entry = mHostTable.PutEntry(aHost.get());
  if (!entry) return NS_ERROR_FAILURE;

  if (entry->PermissionsAreEmpty()) {
    ++mHostCount;
  }
  entry->SetPermission(aTypeIndex, aPermission);

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::Remove(const nsACString &aHost,
                            const char       *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  PRInt32 typeIndex = GetTypeIndex(aType, PR_FALSE);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) return NS_OK;

  nsHostEntry* entry = mHostTable.GetEntry(PromiseFlatCString(aHost).get());
  if (entry) {
    entry->SetPermission(typeIndex, nsIPermissionManager::UNKNOWN_ACTION);

    // If no more types are present, remove the entry
    if (entry->PermissionsAreEmpty()) {
      mHostTable.RawRemoveEntry(entry);
      --mHostCount;
    }
    mChangedList = PR_TRUE;
    Write();

    // Notify Observers
    NotifyObservers(aHost);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::RemoveAll()
{
  RemoveAllFromMemory();
  Write();
  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::TestPermission(nsIURI     *aURI,
                                    const char *aType,
                                    PRUint32   *aPermission)
{
  NS_ASSERTION(aURI, "could not get uri");
  NS_ASSERTION(aPermission, "no permission pointer");
  NS_ENSURE_ARG_POINTER(aType);

  // set the default
  *aPermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  // Don't error on no host. Just return UNKNOWN_ACTION as permission.
  if (hostPort.IsEmpty()) {
    return NS_OK;
  }
  
  PRInt32 typeIndex = GetTypeIndex(aType, PR_FALSE);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) return NS_OK;

  PRUint32 offset = 0;
  do {
    nsHostEntry *entry = mHostTable.GetEntry(hostPort.get() + offset);
    if (entry) {
      *aPermission = entry->GetPermission(typeIndex);
      if (*aPermission != nsIPermissionManager::UNKNOWN_ACTION)
        break;
    }
    offset = hostPort.FindChar('.', offset) + 1;

  // walk up the domaintree (we stop as soon as we find a match,
  // which will be the most specific domain we have an entry for).
  // this is new behavior; we only used to do this for popups.
  // see bug 176950.
  } while (offset > 0);

  return NS_OK;
}

// A little helper function to add the hostname to the list.
// The hostname comes from an arena, and so it won't go away
PR_STATIC_CALLBACK(PLDHashOperator)
AddHostToList(nsHostEntry *entry, void *arg)
{
  // arg is a double-ptr to an string entry in the hostList array,
  // so we dereference it twice to assign the |const char*| string
  // and once so we can increment the |const char**| array location
  // ready for the next assignment.
  const char*** elementPtr = NS_STATIC_CAST(const char***, arg);
  **elementPtr = entry->GetKey();
  ++(*elementPtr);
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP nsPermissionManager::GetEnumerator(nsISimpleEnumerator **aEnum)
{
  *aEnum = nsnull;
  // get the host list, to hand to the enumerator.
  // the enumerator takes ownership of the list.

  // create a new host list. enumerator takes ownership of it.
  const char* *hostList = new const char*[mHostCount];
  if (!hostList) return NS_ERROR_OUT_OF_MEMORY;

  // Make a copy of the pointer, so we can increase it without loosing
  // the original pointer
  const char** hostListCopy = hostList;
  mHostTable.EnumerateEntries(AddHostToList, &hostListCopy);

  nsPermissionEnumerator* permissionEnum = new nsPermissionEnumerator(&mHostTable, hostList, mHostCount, NS_CONST_CAST(const char**, mTypeArray));
  if (!permissionEnum) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(permissionEnum);
  *aEnum = permissionEnum;
  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change.
    
    // Dump current permission.  This will be done by calling 
    // RemoveAllFromMemory which clears the memory-resident
    // permission table.  The reason the permission file does not
    // need to be updated is because the file was updated every time
    // the memory-resident table changed (i.e., whenever a new permission
    // was accepted).  If this condition ever changes, the permission
    // file would need to be updated here.

    RemoveTypeStrings();
    RemoveAllFromMemory();
    
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
      if (mPermissionsFile) {
        mPermissionsFile->Remove(PR_FALSE);
      }
  }  
  else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has aleady changed.    
    // Now just read them from the new profile location.

    // Re-get the permissions file
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mPermissionsFile));
    if (NS_SUCCEEDED(rv)) {
      rv = mPermissionsFile->AppendNative(NS_LITERAL_CSTRING(kPermissionsFileName));
    }
    Read();
  }

  return rv;
}

//*****************************************************************************
//*** nsPermissionManager private methods
//*****************************************************************************

nsresult
nsPermissionManager::RemoveAllFromMemory()
{
  mHostTable.Clear();
  mHostCount = 0;
  if (gHostArena) {
    PL_FinishArenaPool(gHostArena);
    delete gHostArena;
  }
  gHostArena = nsnull;
  mChangedList = PR_TRUE;
  return NS_OK;
}

void
nsPermissionManager::RemoveTypeStrings()
{
  for (PRUint32 i = NUMBER_OF_TYPES; i--; ) {
    if (mTypeArray[i]) {
      PL_strfree(mTypeArray[i]);
      mTypeArray[i] = nsnull;
    }
  }
}

// Returns -1 on failure
PRInt32
nsPermissionManager::GetTypeIndex(const char *aType,
                                  PRBool      aAdd)
{
  PRInt32 firstEmpty = -1;

  for (PRUint32 i = 0; i < NUMBER_OF_TYPES; ++i) {
    if (!mTypeArray[i]) {
      if (firstEmpty == -1)
        // Don't break, the type might be later in the array
        firstEmpty = i;
    } else if (!strcmp(aType, mTypeArray[i])) {
      return i;
    }
  }

  if (!aAdd || firstEmpty == -1)
    // Not found, but that is ok - we were just looking.
    // Or, no free spots left - error.
    return -1;

  // This type was not registered before.
  // Can't use ToNewCString here, other strings in the array are allocated
  // with PL_strdup too, and they all need to be freed the same way
  mTypeArray[firstEmpty] = PL_strdup(aType);
  if (!mTypeArray[firstEmpty])
    return -1;

  return firstEmpty;
}


// broadcast a notification that a permission has changed
nsresult
nsPermissionManager::NotifyObservers(const nsACString &aHost)
{
  if (mObserverService) {
    return mObserverService->NotifyObservers(NS_STATIC_CAST(nsIPermissionManager *, this),
                                             kPermissionChangeNotification,
                                             NS_ConvertUTF8toUCS2(aHost).get());
  }
  return NS_ERROR_FAILURE;
}

// Note:
// We don't do checkbox states here anymore.
// When a consumer wants it back, that is up to the consumer, not this backend
// For cookies, it is now done with a persist in the dialog xul file.

static const char kTab = '\t';
static const char kNew = '\n';
static const char kTrue = 'T';
static const char kFalse = 'F';
static const char kFirstLetter = 'a';
static const char kTypeSign = '%';

nsresult
nsPermissionManager::Read()
{
  nsresult rv;
  
  if (!mPermissionsFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), mPermissionsFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) return rv;

  /* format is:
   * host \t number permission \t number permission ... \n
   * if this format isn't respected we move onto the next line in the file.
   * permission is T or F for accept or deny, otherwise a lowercase letter,
   * with a=0, b=1 etc
   */

  nsAutoString bufferUnicode;
  nsCAutoString buffer;
  nsASingleFragmentCString::char_iterator iter;
  PRBool isMore = PR_TRUE;
  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(bufferUnicode, &isMore))) {
    CopyUCS2toASCII(bufferUnicode, buffer);
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    if (buffer.First() == kTypeSign) {
      // A permission string line

      PRInt32 stringIndex;
      
      if ((stringIndex = buffer.FindChar(kTab) + 1) == 0) {
        continue;      
      }

      PRUint32 type;
      if (!PR_sscanf(buffer.get() + 1, "%u", &type) || type >= NUMBER_OF_TYPES) {
        continue;
      }
      mTypeArray[type] = PL_strdup(buffer.get() + stringIndex);

      continue;
    }

    PRInt32 hostIndex, permissionIndex;
    PRUint32 nextPermissionIndex = 0;
    hostIndex = 0;

    if ((permissionIndex = buffer.FindChar('\t', hostIndex) + 1) == 0) {
      continue;      
    }

    // ignore leading periods in host name
    while (hostIndex < permissionIndex && (buffer.CharAt(hostIndex) == '.')) {
      ++hostIndex;
    }

    // nullstomp the trailing tab, to get a flat string
    buffer.BeginWriting(iter);
    *(iter += permissionIndex - 1) = char(0);
    nsDependentCString host(buffer.get() + hostIndex, iter);

    for (;;) {
      if (nextPermissionIndex == buffer.Length()+1) {
        break;
      }
      if ((nextPermissionIndex = buffer.FindChar(kTab, permissionIndex) + 1) == 0) {
        nextPermissionIndex = buffer.Length()+1;
      }
      const nsASingleFragmentCString &permissionString = Substring(buffer, permissionIndex, nextPermissionIndex - permissionIndex - 1);
      permissionIndex = nextPermissionIndex;

      PRInt32 type = 0;
      PRUint32 index = 0;

      if (permissionString.IsEmpty()) {
        continue; // empty permission entry -- should never happen
      }
      // Parse "2T"
      char c = permissionString.CharAt(index);
      while (index < permissionString.Length() && c >= '0' && c <= '9') {
        type = 10*type + (c-'0');
        c = permissionString.CharAt(++index);
      }
      if (index >= permissionString.Length()) {
        continue; // bad format for this permission entry
      }
      PRUint32 permission;
      if (permissionString.CharAt(index) == kTrue)
        permission = nsIPermissionManager::ALLOW_ACTION;
      else if (permissionString.CharAt(index) == kFalse)
        permission = nsIPermissionManager::DENY_ACTION;
      else
        permission = permissionString.CharAt(index) - kFirstLetter;

      // |permission| is unsigned, so no need to check for < 0
      if (permission >= NUMBER_OF_PERMISSIONS)
        continue;
    
      // Ignore @@@ as host. Old style checkbox status
      if (!permissionString.IsEmpty() && !host.Equals(NS_LITERAL_CSTRING("@@@@"))) {
        rv = AddInternal(host, type, permission);
        if (NS_FAILED(rv)) return rv;
      }

    }
  }

  mChangedList = PR_FALSE;

  // For backwards compatibility, add those types when not yet set.
  if (!mTypeArray[0]) mTypeArray[0] = PL_strdup("cookie");
  if (!mTypeArray[1]) mTypeArray[1] = PL_strdup("image");
  if (!mTypeArray[2]) mTypeArray[2] = PL_strdup("popup");

  return NS_OK;
}

// A helper function that adds the pointer to the entry to the list.
// This is not threadsafe, and only safe if the consumer does not 
// modify the list. It is only used in Write() where we are sure
// that nothing else changes the list while we are saving.
PR_STATIC_CALLBACK(PLDHashOperator)
AddEntryToList(nsHostEntry *entry, void *arg)
{
  // arg is a double-ptr to an entry in the hostList array,
  // so we dereference it twice to assign the |nsHostEntry*|
  // and once so we can increment the |nsHostEntry**| array location
  // ready for the next assignment.
  nsHostEntry*** elementPtr = NS_STATIC_CAST(nsHostEntry***, arg);
  **elementPtr = entry;
  ++(*elementPtr);
  return PL_DHASH_NEXT;
}


nsresult
nsPermissionManager::Write()
{
  nsresult rv;

  if (!mChangedList) {
    return NS_OK;
  }

  if (!mPermissionsFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), mPermissionsFile);
  if (NS_FAILED(rv)) return rv;

  // get a buffered output stream 4096 bytes big, to optimize writes
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream), fileOutputStream, 4096);
  if (NS_FAILED(rv)) return rv;

  static const char kHeader[] = 
    "# HTTP Permission File\n"
    "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
    "# This is a generated file!  Do not edit.\n\n";

  bufferedOutputStream->Write(kHeader, sizeof(kHeader) - 1, &rv);

  // Write the type strings.
  // Format: % index \t typestring \n
  // (% sign is to distinguish type string lines from permission lines)

  PRUint32 i;
  for (i = 0; i < NUMBER_OF_TYPES; ++i) {
    if (mTypeArray[i]) {
      bufferedOutputStream->Write(&kTypeSign, 1, &rv);

      char typeNumber[3];
      PRUint32 len = PR_snprintf(typeNumber, sizeof(typeNumber), "%u", i);
      bufferedOutputStream->Write(typeNumber, len, &rv);

      bufferedOutputStream->Write(&kTab, 1, &rv);
      bufferedOutputStream->Write(mTypeArray[i], strlen(mTypeArray[i]), &rv);
      bufferedOutputStream->Write(&kNew, 1, &rv);
    }
  }
  bufferedOutputStream->Write(&kNew, 1, &rv);

  /* format shall be:
   * host \t permission \t permission ... \n
   */

  // create a new host list
  nsHostEntry* *hostList = new nsHostEntry*[mHostCount];
  if (!hostList) return NS_ERROR_OUT_OF_MEMORY;

  // Make a copy of the pointer, so we can increase it without loosing
  // the original pointer
  nsHostEntry** hostListCopy = hostList;
  mHostTable.EnumerateEntries(AddEntryToList, &hostListCopy);

  for (i = 0; i < mHostCount; ++i) {
    nsHostEntry *entry = NS_STATIC_CAST(nsHostEntry*, hostList[i]);
    NS_ASSERTION(entry, "corrupt permission list");

    bufferedOutputStream->Write(entry->GetHost().get(), entry->GetHost().Length(), &rv);

    for (PRInt32 type = 0; type < NUMBER_OF_TYPES; ++type) {
    
      PRUint32 permission = entry->GetPermission(type);
      if (permission) {
        bufferedOutputStream->Write(&kTab, 1, &rv);

        char typeString[5];
        PRUint32 len = PR_snprintf(typeString, sizeof(typeString), "%d", type);
        bufferedOutputStream->Write(typeString, len, &rv);

        if (permission == nsIPermissionManager::ALLOW_ACTION) {
          bufferedOutputStream->Write(&kTrue, 1, &rv);
        } else if (permission == nsIPermissionManager::DENY_ACTION) {
          bufferedOutputStream->Write(&kFalse, 1, &rv);
        } else {
          char permString[2];
          PR_snprintf(permString, sizeof(permString), "%c", kFirstLetter + permission);
          bufferedOutputStream->Write(permString, 1, &rv);
        }
      }
    }
    bufferedOutputStream->Write(&kNew, 1, &rv);
  }

  delete[] hostList;
  mChangedList = PR_FALSE;

  return NS_OK;
}

