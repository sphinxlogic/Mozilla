/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s):
 */

var COOKIEPERMISSION = 0;
var IMAGEPERMISSION = 1;

function viewImages() {
  window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","_blank",
                    "chrome,resizable=yes", "imageManager" );
}

function viewCookies() {
  window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","_blank",
                    "chrome,resizable=yes", "cookieManager");
}  

function viewCookiesFromIcon() {
  window.openDialog("chrome://communicator/content/wallet/CookieViewer.xul","_blank",
                    "chrome,resizable=yes", "cookieManagerFromIcon");
}  

function viewP3P() {
  window.openDialog
    ("chrome://cookie/content/p3p.xul","_blank","chrome,resizable=no");
}  
