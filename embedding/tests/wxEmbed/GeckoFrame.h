/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GECKOFRAME_H
#define GECKOFRAME_H

#include "nsIWebBrowser.h"

#include "GeckoWindow.h"
#include "GeckoContainer.h"

class GeckoFrame :
    public wxFrame,
    public GeckoContainerUI
{
protected:
    GeckoWindow            *mGeckoWnd;
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
public:
    GeckoFrame();

    DECLARE_EVENT_TABLE()
    void OnActivate(wxActivateEvent &event);

    // This method searches for a "gecko" wxWindow and sets it up with the frame
    // as the container UI. The results are stored in the mGeckoWnd and mWebBrowser
    // member variables.
    bool SetupDefaultGeckoWindow();
    // This method sets up the specified window with the specified container UI
    // and returns the nsIWebBrowser interface to it.
    bool SetupGeckoWindow(GeckoWindow *aGeckoWindow, GeckoContainerUI *aUI, nsIWebBrowser **aWebBrowser) const;

    // GeckoContainerUI overrides
    void SetFocus();
};

#endif