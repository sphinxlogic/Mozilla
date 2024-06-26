/* 
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $Source: /cvsroot/mozilla/security/nss/lib/pkix/src/RDNSequence/Attic/GetRelativeDistinguishedNameCount.c,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:14:11 $ $Name: NSS_CLIENT_TAG $";
#endif /* DEBUG */

#ifndef PKIX_H
#include "pkix.h"
#endif /* PKIX_H */

/*
 * NSSPKIXRDNSequence_GetRelativeDistinguishedNameCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_IMPLEMENT PRInt32
NSSPKIXRDNSequence_GetRelativeDistinguishedNameCount
(
  NSSPKIXRDNSequence *rdnseq
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssPKIXRDNSequence_verifyPointer(rdnseq) ) {
    return -1;
  }
#endif /* DEBUG */

  return nssPKIXRDNSequence_GetRelativeDistinguishedNameCount(rdnseq);
}