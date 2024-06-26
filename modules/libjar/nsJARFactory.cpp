/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include <string.h>

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsJAR.h"
#include "nsIJARFactory.h"
#include "nsRecyclingAllocator.h"
#include "nsXPTZipLoader.h"

extern nsRecyclingAllocator *gZlibAllocator;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPTZipLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAR)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZipReaderCache)

// The list of components we register
static const nsModuleComponentInfo components[] = 
{
    { "XPT Zip Reader",
      NS_XPTZIPREADER_CID,
      NS_XPTLOADER_CONTRACTID_PREFIX "zip",
      nsXPTZipLoaderConstructor
    },
    { "Zip Reader", 
       NS_ZIPREADER_CID,
      "@mozilla.org/libjar/zip-reader;1", 
      nsJARConstructor
    },
    { "Zip Reader Cache", 
       NS_ZIPREADERCACHE_CID,
      "@mozilla.org/libjar/zip-reader-cache;1", 
      nsZipReaderCacheConstructor
    }
};

// Jar module shutdown hook
static void PR_CALLBACK nsJarShutdown(nsIModule *module)
{
    // Release cached buffers from zlib allocator
    delete gZlibAllocator;
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsJarModule, components, nsJarShutdown);
