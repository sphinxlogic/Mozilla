/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2002 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com> 
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Brian Stell <bstell@ix.netcom.com>
 *   Morten Nilsen <morten@nilsen.com>
 *   Jungshik Shin <jshin@mailaps.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsISupportsUtils.h"
#include "nsIServiceManagerUtils.h"
#include "nsIPref.h"
#include "nsFontMetricsXft.h"
#include "prenv.h"
#include "prprf.h"
#include "prlink.h"
#include "nsQuickSort.h"
#include "nsFont.h"
#include "nsIDeviceContext.h"
#include "nsRenderingContextGTK.h"
#include "nsDeviceContextGTK.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsITimelineService.h"
#include "nsICharsetConverterManager.h"
#include "nsICharRepresentable.h"
#include "nsIPersistentProperties2.h"
#include "nsCompressedCharMap.h"
#include "nsNetUtil.h"
#include "plhash.h"

#include <gdk/gdkx.h>
#include <freetype/tttables.h>
#include <freetype/freetype.h>

#define FORCE_PR_LOG
#include "prlog.h"

// Abstract class nsFontXft is the base class for nsFontXftUnicode and
// nsFontXftCustom, either of which is an instance of fonts.  The 
// |nsFontMetricsXft| class is made up of a collection of these little 
// fonts, really.

class nsFontXft {
public:
    nsFontXft(FcPattern *aPattern, FcPattern *aFontName);
    virtual ~nsFontXft() = 0;

    XftFont   *GetXftFont (void);
    virtual nsresult GetTextExtents32 (const FcChar32 *aString, PRUint32 aLen, 
                                       XGlyphInfo &aGlyphInfo);
    gint     GetWidth32               (const FcChar32 *aString, PRUint32 aLen);

#ifdef MOZ_MATHML
    nsresult GetBoundingMetrics32 (const FcChar32 *aString, 
                                   PRUint32 aLength,
                                   nsBoundingMetrics &aBoundingMetrics);
#endif /* MOZ_MATHML */

    PRInt16    GetMaxAscent(void);
    PRInt16    GetMaxDescent(void);

    virtual PRBool     HasChar(PRUint32 aChar) = 0;
    virtual FT_UInt    CharToGlyphIndex(FcChar32 aChar);

    virtual nsresult   FillDrawStringSpec(FcChar32* aString, PRUint32 aLen,
                                          void *aData);
                                      
    // a reference to the font loaded and information about it.  
    XftFont   *mXftFont;
    FcPattern *mPattern;
    FcPattern *mFontName;
    FcCharSet *mCharset;
};

class nsFontXftInfo;

// class for regular 'Unicode' fonts that are actually what they claim to be. 
class nsFontXftUnicode : public nsFontXft {
public:
    nsFontXftUnicode(FcPattern *aPattern, FcPattern *aFontName);
    virtual ~nsFontXftUnicode();

    virtual PRBool     HasChar (PRUint32 aChar);
};

// class for custom encoded fonts that pretend to have Unicode cmap.
// There are two kinds of them: 
// 1. 'narrow' custom encoded fonts such as  mathematica fonts, 
// TeX Computer Roman fonts, Symbol font, etc.
// 2. 'wide' custom  encoded fonts such as 
// Korean Jamo fonts, non-opentype fonts for Indic scripts.
//
// The biggest difference between 'narrow' and 'wide' fonts is that 
// the result of calling mConverter for 'wide' fonts is a sequence of 
// 16bit pseudo-Unicode code points.  For 'narrow' fonts, it's 
// a sequence of 8bit code points. 
class nsFontXftCustom : public nsFontXft {
public:
    nsFontXftCustom(FcPattern* aPattern, 
                    FcPattern* aFontName, 
                    nsFontXftInfo* aFontInfo);

    virtual ~nsFontXftCustom();

    virtual PRBool   HasChar            (PRUint32 aChar);
    virtual FT_UInt  CharToGlyphIndex   (FcChar32 aChar);
    virtual nsresult GetTextExtents32   (const FcChar32 *aString, 
                                         PRUint32 aLen, XGlyphInfo &aGlyphInfo);
    virtual nsresult FillDrawStringSpec (FcChar32* aString, PRUint32 aLen,
                                         void *aData);

private:
    nsFontXftInfo *mFontInfo; 

    // freetype fontface : used for direct access to glyph indices
    // in GetWidth32() and FillDrawStringSpec().
    FT_Face    mFT_Face;
    nsresult   SetFT_FaceCharmap (void);
};

enum nsXftFontType {
    eFontTypeUnicode,
    eFontTypeCustom,
    eFontTypeCustomWide
};

// a class to hold some essential information about font. 
// it's shared and cached with 'family name' as hash key.
class nsFontXftInfo : public PLHashEntry {
    public:
    nsFontXftInfo() : mCCMap(nsnull), mConverter(0), 
                      mFontType(eFontTypeUnicode) 
                      { }

    ~nsFontXftInfo() {
        if (mCCMap)
            FreeCCMap(mCCMap);
    }

    // Char. coverage map(replacing mCharset in nsFontXft)
    PRUint16*                   mCCMap;
    // converter from Unicode to font-specific encoding
    nsCOMPtr<nsIUnicodeEncoder> mConverter;
    // Unicode, Custom, CustomWide
    nsXftFontType               mFontType;
    // Truetype cmap to use for direct retrieval of GIDs with FT_Get_Char_Index
    // for 'narrow' custom fonts.
    FT_Encoding                 mFT_Encoding;
};

PR_STATIC_CALLBACK(void*)        fontmapAllocTable(void *pool, size_t size);
PR_STATIC_CALLBACK(void)         fontmapFreeTable (void *pool, void *item);
PR_STATIC_CALLBACK(PLHashEntry*) fontmapAllocEntry(void *pool,
                                                   const void *key);
PR_STATIC_CALLBACK(void)         fontmapFreeEntry (void *pool,
                                                   PLHashEntry *he,
                                                   PRUint32 flag);

PLHashAllocOps fontmapHashAllocOps = {
    fontmapAllocTable, fontmapFreeTable,
    fontmapAllocEntry, fontmapFreeEntry
};


struct MozXftLangGroup {
    const char    *mozLangGroup;
    FcChar32       character;
    const FcChar8 *XftLang;
};

static const MozXftLangGroup MozXftLangGroups[] = {
    { "x-western",      0x0041, (const FcChar8 *)"en" },
    { "x-central-euro", 0x0100, (const FcChar8 *)"pl" },
    { "x-cyrillic",     0x0411, (const FcChar8 *)"ru" },
    { "x-baltic",       0x0104, (const FcChar8 *)"lv" },
    { "x-devanagari",   0x0905, (const FcChar8 *)"hi" },
    { "x-tamil",        0x0B85, (const FcChar8 *)"ta" },
    { "x-unicode",      0x0000,                  0    },
    { "x-user-def",     0x0000,                  0    },
};

#define NUM_XFT_LANG_GROUPS (sizeof (MozXftLangGroups) / \
			                 sizeof (MozXftLangGroups[0]))

typedef struct _DrawStringData {
    nsFontMetricsXft      *metrics;
    nscoord                x;
    nscoord                y;
    const nscoord         *spacing;
    nscoord                xOffset;
    nsRenderingContextGTK *context;
    XftDraw               *draw;
    XftColor               color;
    PRUint32               specBufferLen;
    PRUint32               specBufferSize;
    XftGlyphFontSpec      *specBuffer;
    PRBool                 foundGlyph;
    float                  p2t;
} DrawStringData;

typedef struct _TextDimensionsData {
    nsFontMetricsXft *metrics;
    nsTextDimensions *dimensions;
} TextDimensionsData;

typedef struct _WidthData {
    nsFontMetricsXft *metrics;
    nscoord           width;
} WidthData;

#ifdef MOZ_MATHML
typedef struct _BoundingMetricsData {
    nsFontMetricsXft *metrics;
    nsBoundingMetrics *bm;
    PRBool firstTime;
} BoundingMetricsData;
#endif /* MOZ_MATHML */

class nsAutoBuffer;

static int      CalculateSlant   (PRUint8  aStyle);
static int      CalculateWeight  (PRUint16 aWeight);
static void     AddLangGroup     (FcPattern *aPattern, nsIAtom *aLangGroup);
static void     AddFFRE          (FcPattern *aPattern, nsCString *aFamily,
                                  PRBool aWeak);
static void     FFREToFamily     (nsACString &aFFREName, nsACString &oFamily);
static int      FFRECountHyphens (nsACString &aFFREName);
static int      CompareFontNames (const void* aArg1, const void* aArg2,
                                  void* aClosure);
static PRBool   IsASCIIFontName  (const nsString& aName);
static nsresult EnumFontsXft     (nsIAtom* aLangGroup, const char* aGeneric,
                                  PRUint32* aCount, PRUnichar*** aResult);

static const MozXftLangGroup* FindFCLangGroup (nsACString &aLangGroup);

static inline XftGlyphFontSpec *AllocSpecBuffer(PRUint32 aLen, PRUint32 &aSize);
static inline XftGlyphFontSpec *ReallocSpecBuffer(PRUint32 aOldLen, 
                                                  PRUint32 aNewLen,
                                                  XftGlyphFontSpec *aBuffer);
static inline void              FreeSpecBuffer (XftGlyphFontSpec *aBuffer);

static        void ConvertCharToUCS4    (const char *aString,
                                         PRUint32 aLength,
                                         nsAutoBuffer &aOutBuffer,
                                         PRUint32 *aOutLen);
static        void ConvertUnicharToUCS4 (const PRUnichar *aString,
                                         PRUint32 aLength,
                                         nsAutoBuffer &aOutBuffer,
                                         PRUint32 *aOutLen);
static    nsresult ConvertUCS4ToCustom  (FcChar32 *aSrc, PRUint32 aSrcLen,
                                         PRUint32& aDestLen, 
                                         nsIUnicodeEncoder *aConverter, 
                                         PRBool aIsWide, 
                                         nsAutoBuffer& aResult);

static nsresult StaticDrawStringCallback      (const FcChar32 *aString, 
                                               PRUint32 aLen,
                                               nsFontXft *aFont, 
                                               void *aData);
static nsresult StaticTextDimensionsCallback  (const FcChar32 *aString, 
                                               PRUint32 aLen,
                                               nsFontXft *aFont, 
                                               void *aData);
static nsresult StaticGetWidthCallback        (const FcChar32 *aString, 
                                               PRUint32 aLen,
                                               nsFontXft *aFont, 
                                               void *aData);
#ifdef MOZ_MATHML
static nsresult StaticBoundingMetricsCallback (const FcChar32 *aString, 
                                               PRUint32 aLen,
                                               nsFontXft *aFont, 
                                               void *aData);
#endif /* MOZ_MATHML */

#ifdef MOZ_WIDGET_GTK2
static void GdkRegionSetXftClip(GdkRegion *aGdkRegion, XftDraw *aDraw);
#endif

// This is the scaling factor that we keep fonts limited to against
// the display size.  If a pixel size is requested that is more than
// this factor larger than the height of the display, it's clamped to
// that value instead of the requested size.
#define FONT_MAX_FONT_SCALE 2

#define FONT_SPEC_BUFFER_SIZE 3000
#define UCS2_REPLACEMENT 0xFFFD

#define IS_NON_BMP(c) ((c) >> 16)
#define IS_NON_SURROGATE(c) ((c < 0xd800 || c > 0xdfff))

// a helper class for automatic  buffer allocation
class nsAutoBuffer {
public:
    nsAutoBuffer();
    ~nsAutoBuffer();
    void* GetArray(PRInt32 aMinLen = 0);

private:
    char* mArray;
    char  mAutoArray[FONT_SPEC_BUFFER_SIZE];
    PRInt32  mCount;
};

static XftGlyphFontSpec gFontSpecBuffer[FONT_SPEC_BUFFER_SIZE];

PRLogModuleInfo *gXftFontLoad = nsnull;
static int gNumInstances = 0;

#undef DEBUG_XFT_MEMORY
#ifdef DEBUG_XFT_MEMORY

extern "C" {
extern void XftMemReport(void);
extern void FcMemReport(void);
}
#endif

static nsresult
EnumFontsXft(nsIAtom* aLangGroup, const char* aGeneric,
             PRUint32* aCount, PRUnichar*** aResult);
 
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);

static PRBool                      gInitialized = PR_FALSE;
static nsIPersistentProperties*    gFontEncodingProperties = nsnull;
static nsICharsetConverterManager* gCharsetManager = nsnull;
static PLHashTable*                gFontXftMaps = nsnull;

static nsresult       GetEncoding(const char* aFontName,
                                  char **aEncoding,
                                  nsXftFontType &aType,
                                  FT_Encoding &aFTEncoding);
static nsresult       GetConverter(const char* aEncoding,
                                   nsIUnicodeEncoder** aConverter);
static nsresult       FreeGlobals(void);
static nsFontXftInfo* GetFontXftInfo(FcPattern* aPattern);

static PLHashNumber   HashKey(const void* aString);
static PRIntn         CompareKeys(const void* aStr1, const void* aStr2);

nsFontMetricsXft::nsFontMetricsXft(): mMiniFont(nsnull)
{
    if (!gXftFontLoad)
        gXftFontLoad = PR_NewLogModule("XftFontLoad");

    ++gNumInstances;
}

nsFontMetricsXft::~nsFontMetricsXft()
{
    delete mFont;

    if (mDeviceContext)
        mDeviceContext->FontMetricsDeleted(this);

    if (mPattern)
        FcPatternDestroy(mPattern);

    for (PRInt32 i= mLoadedFonts.Count() - 1; i >= 0; --i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        delete font;
    }

    if (mMiniFont)
        XftFontClose(GDK_DISPLAY(), mMiniFont);

    if (--gNumInstances == 0) {
        FreeGlobals();
#ifdef DEBUG_XFT_MEMORY
        XftMemReport();
        FcMemReport();
#endif
    }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsXft, nsIFontMetrics)

// nsIFontMetrics impl

NS_IMETHODIMP
nsFontMetricsXft::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                       nsIDeviceContext *aContext)
{
    mFont = new nsFont(aFont);
    mLangGroup = aLangGroup;

    // Hang onto the device context
    mDeviceContext = aContext;

    mPointSize = NSTwipsToIntPoints(mFont->size);

    // pixels -> twips ; twips -> points
    float dev2app;
    mDeviceContext->GetDevUnitsToAppUnits(dev2app);
    nscoord screenTwips = NSIntPixelsToTwips(gdk_screen_height(), dev2app);
    nscoord screenPoints = NSTwipsToIntPoints(screenTwips);

    // Make sure to clamp the point size to something reasonable so we
    // don't make the X server blow up.
    mPointSize = PR_MIN(screenPoints * FONT_MAX_FONT_SCALE, mPointSize);

    // enumerate over the font names passed in
    mFont->EnumerateFamilies(nsFontMetricsXft::EnumFontCallback, this);

    nsCOMPtr<nsIPref> prefService;
    prefService = do_GetService(NS_PREF_CONTRACTID);
    if (!prefService)
        return NS_ERROR_FAILURE;
        
    nsXPIDLCString value;

    // Set up the default font name if it's not set
    if (!mGenericFont) {
        prefService->CopyCharPref("font.default", getter_Copies(value));

        if (value.get())
            mDefaultFont = value.get();
        else
            mDefaultFont = "serif";
        
        mGenericFont = &mDefaultFont;
    }

    // set up the minimum sizes for fonts
    if (mLangGroup) {
        nsCAutoString name("font.min-size.");

        if (mGenericFont->Equals("monospace"))
            name.Append("fixed");
        else
            name.Append("variable");

        name.Append(char('.'));

        const char* langGroup;
        mLangGroup->GetUTF8String(&langGroup);

        name.Append(langGroup);

        PRInt32 minimum = 0;
        nsresult res;
        res = prefService->GetIntPref(name.get(), &minimum);
        if (NS_FAILED(res))
            prefService->GetDefaultIntPref(name.get(), &minimum);

        if (minimum < 0)
            minimum = 0;

        // convert the minimum size into points
        float P2T;
        mDeviceContext->GetDevUnitsToAppUnits(P2T);
        minimum = NSTwipsToIntPoints(NSFloatPixelsToTwips(minimum, P2T));

        if (mPointSize < minimum)
            mPointSize = minimum;
    }

    // Make sure that the point size is at least greater than zero
    if (mPointSize < 1) {
#ifdef DEBUG
        printf("*** Warning: nsFontMetricsXft was passed a point size of %d\n",
               mPointSize);
#endif
        mPointSize = 1;
    }
    if (!gInitialized) {
        nsServiceManager::GetService(kCharsetConverterManagerCID,
        NS_GET_IID(nsICharsetConverterManager), (nsISupports**) &gCharsetManager);
        if (!gCharsetManager) {
            FreeGlobals();
            return NS_ERROR_FAILURE;
        }
        if (!gFontXftMaps) { 
            gFontXftMaps = PL_NewHashTable(0, HashKey, CompareKeys, nsnull, 
                                           &fontmapHashAllocOps, nsnull);
        }
        if (!gFontXftMaps) { // error checking
            FreeGlobals();
            return NS_ERROR_OUT_OF_MEMORY;
        }

        gInitialized = PR_TRUE;
    }

    if (NS_FAILED(RealizeFont()))
        return NS_ERROR_FAILURE;

    // Set up the mini-font in case it needs to be used later
    SetupMiniFont();

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::Destroy()
{
    mDeviceContext = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetFont(const nsFont *&aFont)
{
    aFont = mFont;
    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetLangGroup(nsIAtom** aLangGroup)
{
    *aLangGroup = mLangGroup;
    NS_IF_ADDREF(*aLangGroup);

    return NS_OK;
}

NS_IMETHODIMP
nsFontMetricsXft::GetFontHandle(nsFontHandle &aHandle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIFontMetricsXft impl

nsresult
nsFontMetricsXft::GetWidth(const char* aString, PRUint32 aLength,
                           nscoord& aWidth,
                           nsRenderingContextGTK *aContext)
{
    NS_TIMELINE_MARK_FUNCTION("GetWidth");

    XftFont *font = mWesternFont->GetXftFont();
    XGlyphInfo glyphInfo;

    // casting away const for aString but it  should be safe
    XftTextExtents8(GDK_DISPLAY(), font, (FcChar8 *)aString,
                    aLength, &glyphInfo);

    float f;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    aWidth = NSToCoordRound(glyphInfo.xOff * f);

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                           nscoord& aWidth, PRInt32 *aFontID,
                           nsRenderingContextGTK *aContext)
{
    NS_TIMELINE_MARK_FUNCTION("GetWidth");
    if (!aLength) {
        aWidth = 0;
        return NS_OK;
    }

    gint rawWidth = RawGetWidth(aString, aLength);

    float f;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    aWidth = NSToCoordRound(rawWidth * f);

    if (aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const PRUnichar* aString,
                                    PRUint32 aLength,
                                    nsTextDimensions& aDimensions, 
                                    PRInt32* aFontID,
                                    nsRenderingContextGTK *aContext)
{
    NS_TIMELINE_MARK_FUNCTION("GetTextDimensions");
    aDimensions.Clear();

    if (!aLength)
        return NS_OK;

    TextDimensionsData data;
    data.metrics = this;
    data.dimensions = &aDimensions;

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    ConvertUnicharToUCS4(aString, aLength, charBuffer, &len);
    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return 0;

    nsresult rv;
    rv = EnumerateGlyphs(chars, len, StaticTextDimensionsCallback, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    float P2T;
    mDeviceContext->GetDevUnitsToAppUnits(P2T);

    aDimensions.width = NSToCoordRound(aDimensions.width * P2T);
    aDimensions.ascent = NSToCoordRound(aDimensions.ascent * P2T);
    aDimensions.descent = NSToCoordRound(aDimensions.descent * P2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const char*         aString,
                                    PRInt32             aLength,
                                    PRInt32             aAvailWidth,
                                    PRInt32*            aBreaks,
                                    PRInt32             aNumBreaks,
                                    nsTextDimensions&   aDimensions,
                                    PRInt32&            aNumCharsFit,
                                    nsTextDimensions&   aLastWordDimensions,
                                    PRInt32*            aFontID,
                                    nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetTextDimensions");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontMetricsXft::GetTextDimensions(const PRUnichar*    aString,
                                    PRInt32             aLength,
                                    PRInt32             aAvailWidth,
                                    PRInt32*            aBreaks,
                                    PRInt32             aNumBreaks,
                                    nsTextDimensions&   aDimensions,
                                    PRInt32&            aNumCharsFit,
                                    nsTextDimensions&   aLastWordDimensions,
                                    PRInt32*            aFontID,
                                    nsRenderingContextGTK *aContext)
{
    NS_NOTREACHED("GetTextDimensions");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFontMetricsXft::DrawString(const char *aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
    NS_TIMELINE_MARK_FUNCTION("DrawString");

    // The data we will carry through the function
    DrawStringData data;
    memset(&data, 0, sizeof(data));

    data.metrics = this;
    data.x = aX;
    data.y = aY;
    data.spacing = aSpacing;
    data.context = aContext;
    mDeviceContext->GetDevUnitsToAppUnits(data.p2t);

    data.specBuffer = AllocSpecBuffer(aLength, data.specBufferSize);
    if (!data.specBuffer)
        return NS_ERROR_FAILURE;

    PrepareToDraw(aContext, aSurface, &data.draw, data.color);

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    // Convert the incoming string into an array of UCS4 chars
    ConvertCharToUCS4(aString, aLength, charBuffer, &len);
    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = EnumerateGlyphs(chars, len, StaticDrawStringCallback, &data);
    if (NS_FAILED(rv)) {
        FreeSpecBuffer(data.specBuffer);
        return rv;
    }

    // go forth and blit!
    if (data.foundGlyph)
        XftDrawGlyphFontSpec(data.draw, &data.color, data.specBuffer, len);

    FreeSpecBuffer(data.specBuffer);

    return NS_OK;
}

nsresult
nsFontMetricsXft::DrawString(const PRUnichar* aString, PRUint32 aLength,
                             nscoord aX, nscoord aY,
                             PRInt32 aFontID,
                             const nscoord* aSpacing,
                             nsRenderingContextGTK *aContext,
                             nsDrawingSurfaceGTK *aSurface)
{
    NS_TIMELINE_MARK_FUNCTION("DrawString");

    // The data we will carry through the function
    DrawStringData data;
    memset(&data, 0, sizeof(data));

    data.metrics = this;
    data.x = aX;
    data.y = aY;
    data.spacing = aSpacing;
    data.context = aContext;
    mDeviceContext->GetDevUnitsToAppUnits(data.p2t);

    data.specBuffer = AllocSpecBuffer(aLength, data.specBufferSize);
    if (!data.specBuffer)
        return NS_ERROR_FAILURE;

    // set up our colors and clip regions
    PrepareToDraw(aContext, aSurface, &data.draw, data.color);

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    // Convert the incoming string into an array of UCS4 chars
    ConvertUnicharToUCS4(aString, aLength, charBuffer, &len);

    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = EnumerateGlyphs(chars, len, StaticDrawStringCallback, &data);
    if (NS_FAILED(rv)) {
        FreeSpecBuffer(data.specBuffer);
        return rv;
    }

    // Go forth and blit!
    if (data.foundGlyph)
        XftDrawGlyphFontSpec(data.draw, &data.color,
                             data.specBuffer, data.specBufferLen);

    FreeSpecBuffer(data.specBuffer);

    return NS_OK;
}

#ifdef MOZ_MATHML

nsresult
nsFontMetricsXft::GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     nsRenderingContextGTK *aContext)
{
    aBoundingMetrics.Clear(); 

    if (!aString || !aLength)
        return NS_ERROR_FAILURE;

    // The data we will carry through the function
    BoundingMetricsData data;
    data.metrics = this;
    data.bm = &aBoundingMetrics;
    // the beginning of a string needs a special treatment (see
    // 'operator +' definition of nsBoundingMetrics.)
    data.firstTime = PR_TRUE; 

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    // Convert the incoming string into an array of UCS4 chars
    ConvertCharToUCS4(aString, aLength, charBuffer, &len);
    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = EnumerateGlyphs(chars, len, StaticBoundingMetricsCallback, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    float P2T;
    mDeviceContext->GetDevUnitsToAppUnits(P2T);

    aBoundingMetrics.leftBearing =
        NSToCoordRound(aBoundingMetrics.leftBearing * P2T);
    aBoundingMetrics.rightBearing =
        NSToCoordRound(aBoundingMetrics.rightBearing * P2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * P2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * P2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * P2T);

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetBoundingMetrics(const PRUnichar *aString,
                                     PRUint32 aLength,
                                     nsBoundingMetrics &aBoundingMetrics,
                                     PRInt32 *aFontID,
                                     nsRenderingContextGTK *aContext)
{
    aBoundingMetrics.Clear(); 

    if (!aString || !aLength)
        return NS_ERROR_FAILURE;

    // The data we will carry through the function
    BoundingMetricsData data;
    data.metrics = this;
    data.bm = &aBoundingMetrics;
    // the beginning of a string needs a special treatment (see
    // 'operator +' definition of nsBoundingMetrics.)
    data.firstTime = PR_TRUE; 

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    ConvertUnicharToUCS4(aString, aLength, charBuffer, &len);
    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return 0;

    nsresult rv;
    rv = EnumerateGlyphs(chars, len, StaticBoundingMetricsCallback, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    float P2T;
    mDeviceContext->GetDevUnitsToAppUnits(P2T);

    aBoundingMetrics.leftBearing =
        NSToCoordRound(aBoundingMetrics.leftBearing * P2T);
    aBoundingMetrics.rightBearing =
        NSToCoordRound(aBoundingMetrics.rightBearing * P2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * P2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * P2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * P2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

#endif /* MOZ_MATHML */

GdkFont*
nsFontMetricsXft::GetCurrentGDKFont(void)
{
    return nsnull;
}

PRUint32
nsFontMetricsXft::GetHints(void)
{
    return 0;
}

nsresult
nsFontMetricsXft::RealizeFont(void)
{
    // make sure that the western font is loaded since we need that to
    // get the font metrics
    mWesternFont = FindFont('a');
    if (!mWesternFont)
        return NS_ERROR_FAILURE;

    return CacheFontMetrics();
}

nsresult
nsFontMetricsXft::CacheFontMetrics(void)
{
    // Get our scale factor
    float f;
    float val;
    mDeviceContext->GetDevUnitsToAppUnits(f);
    
    // Get our font face
    FT_Face face;
    TT_OS2 *os2;
    XftFont *xftFont = mWesternFont->GetXftFont();

    face = XftLockFace(xftFont);
    os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);

    // mEmHeight (size in pixels of EM height)
    int size;
    if (FcPatternGetInteger(mWesternFont->mPattern, FC_PIXEL_SIZE, 0, &size) !=
        FcResultMatch) {
        size = 12;
    }
    mEmHeight = PR_MAX(1, nscoord(size * f));

    // mMaxAscent
    mMaxAscent = nscoord(xftFont->ascent * f);

    // mMaxDescent
    mMaxDescent = nscoord(xftFont->descent * f);

    nscoord lineHeight = mMaxAscent + mMaxDescent;

    // mLeading (needs ascent and descent and EM height) 
    if (lineHeight > mEmHeight)
        mLeading = lineHeight - mEmHeight;
    else
        mLeading = 0;

    // mMaxHeight (needs ascent and descent)
    mMaxHeight = lineHeight;

    // mEmAscent (needs maxascent, EM height, ascent and descent)
    mEmAscent = nscoord(mMaxAscent * mEmHeight / lineHeight);

    // mEmDescent (needs EM height and EM ascent
    mEmDescent = mEmHeight - mEmAscent;

    // mMaxAdvance
    mMaxAdvance = nscoord(xftFont->max_advance_width * f);

    // mSpaceWidth (width of a space)
    gint rawWidth;
    PRUnichar unispace(' ');
    rawWidth = RawGetWidth(&unispace, 1);
    mSpaceWidth = NSToCoordRound(rawWidth * f);

    // mAveCharWidth (width of an 'average' char)
    PRUnichar xUnichar('x');
    rawWidth = RawGetWidth(&xUnichar, 1);
    mAveCharWidth = NSToCoordRound(rawWidth * f);

    // mXHeight (height of an 'x' character)
    if (FcCharSetHasChar(mWesternFont->mCharset, xUnichar)) {
        XGlyphInfo extents;
        XftTextExtents16(GDK_DISPLAY(), xftFont, &xUnichar, 1, &extents);
        mXHeight = extents.height;
    }
    else {
        // 56% of ascent, best guess for non-true type or asian fonts
        mXHeight = nscoord(((float)mMaxAscent) * 0.56);
    }
    mXHeight = nscoord(mXHeight * f);

    // mUnderlineOffset (offset for underlines)
    val = face->underline_position >> 16;
    if (val) {
        mUnderlineOffset = NSToIntRound(val * f);
    }
    else {
        mUnderlineOffset =
            -NSToIntRound(PR_MAX(1, floor(0.1 * xftFont->height + 0.5)) * f);
    }

    // mUnderlineSize (thickness of an underline)
    val = face->underline_thickness >> 16;
    if (val) {
        mUnderlineSize = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mUnderlineSize =
            NSToIntRound(PR_MAX(1, floor(0.05 * xftFont->height + 0.5)) * f);
    }

    // mSuperscriptOffset
    if (os2 && os2->ySuperscriptYOffset) {
        val = os2->ySuperscriptYOffset >> 16;
        mSuperscriptOffset = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mSuperscriptOffset = mXHeight;
    }

    // mSubscriptOffset
    if (os2 && os2->ySubscriptYOffset) {
        val = os2->ySubscriptYOffset >> 16;
        mSubscriptOffset = nscoord(PR_MAX(f, NSToIntRound(val * f)));
    }
    else {
        mSubscriptOffset = mXHeight;
    }

    // mStrikeoutOffset
    mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0);

    // mStrikeoutSize
    mStrikeoutSize = mUnderlineSize;

    XftUnlockFace(xftFont);

    return NS_OK;
}

nsFontXft *
nsFontMetricsXft::FindFont(PRUnichar aChar)
{

    // If mPattern is null, set up the base bits of it so we can
    // match.  If we need to match later we don't have to set it up
    // again.
    if (!mPattern) {
        SetupFCPattern();
        // did we fail to set it up?
        if (!mPattern)
            return nsnull;
    }

    // go forth and find us some fonts
    if (!mMatched)
        DoMatch();

    // Now that we have the fonts loaded and ready to run, return the
    // font in our loaded list that supports the character
    for (PRInt32 i = 0, end = mLoadedFonts.Count(); i < end; ++i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        if (font->HasChar(PRUint32(aChar)))
            return font;
    }

    // If we got this far, none of the fonts support this character.
    // Return nothing.
    return nsnull;
}

void
nsFontMetricsXft::SetupFCPattern(void)
{
    if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
        printf("[%p] setting up pattern with the following specification:\n",
               (void *)this);

        // non-generic families
        if (mFontList.Count() && !mFontIsGeneric[0]) {
            printf("\tadding non-generic families: ");
            for (int i=0; i < mFontList.Count(); ++i) {
                if (mFontIsGeneric[i])
                    break;

                nsCString *familyName = mFontList.CStringAt(i);
                printf("%s, ", familyName->get());
            }
            printf("\n");
        }

        // language group
        const char *name;
        mLangGroup->GetUTF8String(&name);
        printf("\tlang group: %s\n", name);


    }

    mPattern = FcPatternCreate();
    if (!mPattern)
        return;

    // XXX need to add user defined family

    // Add CSS names - walk the list of fonts, adding the generic as
    // the last font
    for (int i=0; i < mFontList.Count(); ++i) {
        // if this was a generic name, break out of the loop since we
        // don't want to add it to the pattern yet
        if (mFontIsGeneric[i])
            break;;

        nsCString *familyName = mFontList.CStringAt(i);
        AddFFRE(mPattern, familyName, PR_FALSE);
    }

    // Add the language group.  Note that we do this before adding any
    // generics.  That's because the language is more important than
    // any generic font.
    AddLangGroup (mPattern, mLangGroup);

    // If there's a generic add a pref for the generic if there's one
    // set.
    if (mGenericFont) {
        nsCString name;
        name += "font.name.";
        name += mGenericFont->get();
        name += ".";

        nsString langGroup;
        mLangGroup->ToString(langGroup);

        name.AppendWithConversion(langGroup);

        nsCOMPtr<nsIPref> pref;
        pref = do_GetService(NS_PREF_CONTRACTID);
        if (pref) {
            nsresult rv;
            nsXPIDLCString value;
            rv = pref->GetCharPref(name.get(), getter_Copies(value));

            // we ignore prefs that have three hypens since they are X
            // style prefs.
            if (FFRECountHyphens(value) < 3) {
                nsCString tmpstr;
                tmpstr.Append(value);

                if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
                    printf("\tadding generic font from preferences: %s\n",
                           tmpstr.get());
                }

                AddFFRE(mPattern, &tmpstr, PR_FALSE);
            }
        }
    }

    // Add the generic if there is one.
    if (mGenericFont)
        AddFFRE(mPattern, mGenericFont, PR_FALSE);

    if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
        // generic font
        if (mGenericFont) {
            printf("\tadding generic family: %s\n", mGenericFont->get());
        }

        // point size
        printf("\tpoint,pixel size: %d,%d\n", mPointSize, mFont->size);

        // slant type
        printf("\tslant: ");
        switch(mFont->style) {
        case NS_FONT_STYLE_ITALIC:
            printf("italic\n");
            break;
        case NS_FONT_STYLE_OBLIQUE:
            printf("oblique\n");
            break;
        default:
            printf("roman\n");
            break;
        }

        // weight
        printf("\tweight: (orig,calc) %d,%d\n",
               mFont->weight, CalculateWeight(mFont->weight));

    }        

    // add the point size
    FcPatternAddInteger(mPattern, FC_SIZE, mPointSize);

    // Add the slant type
    FcPatternAddInteger(mPattern, FC_SLANT,
                        CalculateSlant(mFont->style));

    // Add the weight
    FcPatternAddInteger(mPattern, FC_WEIGHT,
                        CalculateWeight(mFont->weight));

    // Set up the default substitutions for this font
    FcConfigSubstitute(0, mPattern, FcMatchPattern);
    XftDefaultSubstitute(GDK_DISPLAY(), DefaultScreen(GDK_DISPLAY()),
                         mPattern);
}

void
nsFontMetricsXft::DoMatch(void)
{
    FcFontSet *set = nsnull;
    // now that we have a pattern we can match
    FcCharSet *charset;
    FcResult   result;

    set = FcFontSort(0, mPattern, FcTrue, &charset, &result);

    // we don't need the charset since we're going to break it up into
    // specific fonts anyway
    if (charset)
        FcCharSetDestroy(charset);

    // did we not match anything?
    if (!set) {
        goto loser;
    }

    if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
        printf("matched the following (%d) fonts:\n", set->nfont);
    }

    // Create a list of new font objects based on the fonts returned
    // as part of the query
    for (int i=0; i < set->nfont; ++i) {
        if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
            char *name;
            FcPatternGetString(set->fonts[i], FC_FAMILY, 0, (FcChar8 **)&name);
            printf("\t%s\n", name);
        }

        nsFontXft *font;
        nsFontXftInfo *info;
        nsCOMPtr<nsIUnicodeEncoder> converter = 0;

        info = GetFontXftInfo(set->fonts[i]);
        if (info) {
            if (info->mFontType == eFontTypeUnicode)
                font = new nsFontXftUnicode(mPattern, set->fonts[i]);
            else
                font = new nsFontXftCustom(mPattern, set->fonts[i], info);
        }
        else {  // if null is returned, treat it as Unicode font.
            font = new nsFontXftUnicode(mPattern, set->fonts[i]);
        }

        if (!font)
            goto loser;

        // append this font to our list of loaded fonts
        mLoadedFonts.AppendElement((void *)font);
    }

    // we're done with the set now
    FcFontSetDestroy(set);
    set = nsnull;

    // Done matching!
    mMatched = PR_TRUE;
    return;

    // if we got this far, something went terribly wrong
 loser:
    NS_WARNING("nsFontMetricsXft::DoMatch failed to match anything");

    if (set)
        FcFontSetDestroy(set);

    for (PRInt32 i = mLoadedFonts.Count() - 1; i >= 0; --i) {
        nsFontXft *font = (nsFontXft *)mLoadedFonts.ElementAt(i);
        mLoadedFonts.RemoveElementAt(i);
        delete font;
    }
}

gint
nsFontMetricsXft::RawGetWidth(const PRUnichar* aString, PRUint32 aLength)
{
    WidthData data;
    data.metrics = this;
    data.width = 0;

    PRUint32 len;
    nsAutoBuffer charBuffer;
    FcChar32 *chars;

    ConvertUnicharToUCS4(aString, aLength, charBuffer, &len);
    if (!len || !(chars = NS_STATIC_CAST(FcChar32 *, charBuffer.GetArray())))
        return 0;

    if (NS_FAILED(EnumerateGlyphs(chars, len, StaticGetWidthCallback, &data))) 
        data.width = 0;

    return data.width;
}

nsresult
nsFontMetricsXft::SetupMiniFont(void)
{
    FcPattern *pattern = nsnull;
    XftFont *font = nsnull;
    XftFont *xftFont = mWesternFont->GetXftFont();
    FcPattern *pat = nsnull;

    mMiniFontAscent = xftFont->ascent;
    mMiniFontDescent = xftFont->descent;

    pattern = FcPatternCreate();
    if (!pattern)
        return NS_ERROR_FAILURE;

    FcPatternAddString(pattern, FC_FAMILY, (FcChar8 *)"monospace");

    FcPatternAddInteger(pattern, FC_SIZE, 
                        (int)(0.5 * mPointSize));

    FcPatternAddInteger(pattern, FC_WEIGHT,
                        CalculateWeight(mFont->weight));

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    XftDefaultSubstitute(GDK_DISPLAY(), DefaultScreen(GDK_DISPLAY()),
                         pattern);

    FcFontSet *set = nsnull;
    FcResult res;
    
    set = FcFontSort(0, pattern, FcTrue, NULL, &res);

    if (set) {
        pat = FcFontRenderPrepare(0, pattern, set->fonts[0]);
    }

    if (pat) {
        font = XftFontOpenPattern(GDK_DISPLAY(), pat);

        if (font) {
            mMiniFont = font;
            pat = nsnull; // the font owns the pattern now
        }
        else {
            font = xftFont;
        }
    }

    // now that the font has been loaded, measure the fonts to find
    // the bounds
    for (int i=0; i < 16; ++i) {
        // create a little mini-string that contains what we want to
        // measure.
        char c = i < 10 ? '0' + i : 'A' + i - 10;
        char str[2];
        str[0] = c;
        str[1] = '\0';

        XGlyphInfo extents;
        XftTextExtents8(GDK_DISPLAY(), font,
                        (FcChar8 *)str, 1, &extents);

        mMiniFontWidth = PR_MAX (mMiniFontWidth, extents.width);
        mMiniFontHeight = PR_MAX (mMiniFontHeight, extents.height);
    }

    if (!mMiniFont) {
        mMiniFontWidth /= 2;
        mMiniFontHeight /= 2;
    }

    mMiniFontPadding = PR_MAX(mMiniFontHeight / 10, 1);
    mMiniFontYOffset = ((mMiniFontAscent + mMiniFontDescent) -
                        (mMiniFontHeight * 2 + mMiniFontPadding * 5)) / 2;


    if (pat)
        FcPatternDestroy(pat);
    if (pattern)
        FcPatternDestroy(pattern);
    if (set)
        FcFontSetSortDestroy(set);

    return NS_OK;
}

nsresult
nsFontMetricsXft::DrawUnknownGlyph(FcChar32   aChar,
                                   nscoord    aX,
                                   nscoord    aY,
                                   XftColor  *aColor,
                                   XftDraw   *aDraw)
{
    int width,height;
    int ndigit = (IS_NON_BMP(aChar)) ? 3 : 2;

    // ndigit characters + padding around the fonts.  From left to
    // right it would be one padding for the glyph box, one for the
    // padding in between the box and the left most digit, ndigit
    // padding following each of ndigit letters and one for the
    // rightmost part of the box.  This pattern is used throughout the
    // rest of this function.
    width = mMiniFontWidth * ndigit + mMiniFontPadding * (ndigit + 3);
    height = mMiniFontHeight * 2 + mMiniFontPadding * 5;

    // Draw an outline of a box.  We do with this with four calls
    // since with one call it would fill in the box.
    
    // horizontal lines
    XftDrawRect(aDraw, aColor,
                aX, aY - height,
                width, mMiniFontPadding);
    XftDrawRect(aDraw, aColor,
                aX, aY - mMiniFontPadding,
                width, mMiniFontPadding);

    // vertical lines
    XftDrawRect(aDraw, aColor,
                aX,
                aY - height + mMiniFontPadding,
                mMiniFontPadding, height - mMiniFontPadding * 2);
    XftDrawRect(aDraw, aColor,
                aX + width - mMiniFontPadding,
                aY - height + mMiniFontPadding,
                mMiniFontPadding, height - mMiniFontPadding * 2);

    // If for some reason the mini font couldn't be loaded, just
    // return - the box is enough.
    if (!mMiniFont)
        return NS_OK;

    // now draw the characters
    char buf[7];
    PR_snprintf (buf, sizeof(buf), "%0*X", ndigit * 2, aChar);

    // Draw the 'ndigit * 2' characters
    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontPadding * 2,
                   aY - mMiniFontHeight - mMiniFontPadding * 3,
                   (FcChar8 *)&buf[0], 1);
    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontWidth + mMiniFontPadding * 3,
                   aY - mMiniFontHeight - mMiniFontPadding * 3,
                   (FcChar8 *)&buf[1], 1);

    if (ndigit == 2) {
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontPadding * 2,
                       aY - mMiniFontPadding * 2,
                       (FcChar8 *)&buf[2], 1);
        XftDrawString8(aDraw, aColor, mMiniFont,
                       aX + mMiniFontWidth + mMiniFontPadding * 3,
                       aY - mMiniFontPadding * 2,
                       (FcChar8 *)&buf[3], 1);

        return NS_OK;
    }

    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontWidth * 2 + mMiniFontPadding * 4,
                   aY - mMiniFontHeight - mMiniFontPadding * 3,
                   (FcChar8 *)&buf[2], 1);
    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontPadding * 2,
                   aY - mMiniFontPadding * 2,
                   (FcChar8 *)&buf[3], 1);
    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontWidth + mMiniFontPadding * 3,
                   aY - mMiniFontPadding * 2,
                   (FcChar8 *)&buf[4], 1);
    XftDrawString8(aDraw, aColor, mMiniFont,
                   aX + mMiniFontWidth * 2 + mMiniFontPadding * 4,
                   aY - mMiniFontPadding * 2,
                   (FcChar8 *)&buf[5], 1);

    return NS_OK;
}

nsresult
nsFontMetricsXft::EnumerateGlyphs(const FcChar32 *aString, PRUint32 aLen,
                                  GlyphEnumeratorCallback aCallback,
                                  void *aCallbackData)
{
    nsFontXft* prevFont = nsnull;
    PRUint32 start = 0;
    nsresult rv = NS_OK;
    PRUint32 i = 0;

    for ( ; i < aLen; i ++) {
        FcChar32 c = aString[i];
        nsFontXft *currFont = nsnull;
        nsFontXft *font = nsnull;

        // Walk the list of fonts, looking for a font that supports
        // this character.
        for (PRInt32 j = 0, end = mLoadedFonts.Count(); j < end; ++j) {
            font = (nsFontXft *)mLoadedFonts.ElementAt(j);
            if (font->HasChar(c)) {
                currFont = font;
                goto FoundFont; // for speed -- avoid "if" statement
            }
        }

        // We get here when a font is not found so we need  to take care
        // of the unknown glyph after processing any left  over text.
        if (prevFont) {
            rv = aCallback(&aString[start], i - start, prevFont,
                           aCallbackData);
            NS_ENSURE_SUCCESS(rv, rv);

            prevFont = nsnull;
        }

        rv = aCallback(&c, 1, nsnull,  aCallbackData);
        NS_ENSURE_SUCCESS(rv, rv);

        // Jump to the top of the loop
        continue;

        // FoundFont is jumped to when a suitable font is found that
        // matches the character in question. 
    FoundFont:
        if (prevFont) {
            if (currFont != prevFont) {
                rv = aCallback(&aString[start], i - start, prevFont, 
                               aCallbackData);
                NS_ENSURE_SUCCESS(rv, rv);
                start = i;
                prevFont = currFont;
            }
        }
        else {
            prevFont = currFont;
            start = i;
        }
    }

    if (prevFont) 
        rv = aCallback(&aString[start], i - start, prevFont, aCallbackData);

    return rv;
}

void
nsFontMetricsXft::PrepareToDraw(nsRenderingContextGTK *aContext,
                                nsDrawingSurfaceGTK *aSurface,
                                XftDraw **aDraw, XftColor &aColor)
{
    // Set our color
    nscolor rccolor;

    aContext->GetColor(rccolor);

    aColor.pixel = gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(rccolor));
    aColor.color.red = (NS_GET_R(rccolor) << 8) | NS_GET_R(rccolor);
    aColor.color.green = (NS_GET_G(rccolor) << 8) | NS_GET_G(rccolor);
    aColor.color.blue = (NS_GET_B(rccolor) << 8) | NS_GET_B(rccolor);
    aColor.color.alpha = 0xffff;

    *aDraw = aSurface->GetXftDraw();

    nsCOMPtr<nsIRegion> lastRegion;
    nsCOMPtr<nsIRegion> clipRegion;

    aSurface->GetLastXftClip(getter_AddRefs(lastRegion));
    aContext->GetClipRegion(getter_AddRefs(clipRegion));

    // avoid setting the clip, if possible
    if (!lastRegion || !clipRegion || !lastRegion->IsEqual(*clipRegion)) {
        aSurface->SetLastXftClip(clipRegion);

        GdkRegion *rgn = nsnull;
        clipRegion->GetNativeRegion((void *&)rgn);

#ifdef MOZ_WIDGET_GTK
        GdkRegionPrivate  *priv = (GdkRegionPrivate *)rgn;
        XftDrawSetClip(*aDraw, priv->xregion);
#endif

#ifdef MOZ_WIDGET_GTK2
        GdkRegionSetXftClip(rgn, *aDraw);
#endif
    }
}

nsresult
nsFontMetricsXft::DrawStringCallback(const FcChar32 *aString, PRUint32 aLen,
                                     nsFontXft *aFont, void *aData)
{
    DrawStringData *data = (DrawStringData *)aData;

    // If there was no font found for this character, just draw the
    // unknown glyph character
    if (!aFont) {
        // position in X is the location offset in the string plus
        // whatever offset is required for the spacing argument
        nscoord x = data->x + data->xOffset;
        nscoord y = data->y;

        // convert this into device coordinates
        data->context->GetTranMatrix()->TransformCoord(&x, &y);

        NS_ASSERTION(aLen == 1, "An unknown glyph should come by itself");
        DrawUnknownGlyph(*aString, x, y + mMiniFontYOffset, &data->color,
                         data->draw);

        if (data->spacing) {
            data->xOffset += *data->spacing;
            data->spacing += IS_NON_BMP(*aString) ? 2 : 1;
        }
        else {
            data->xOffset +=
                NSToCoordRound((mMiniFontWidth*(IS_NON_BMP(*aString) ? 3 : 2) +
                                mMiniFontPadding*(IS_NON_BMP(*aString) ? 6:5))*
                                data->p2t);
        }

        // We're done.
        return NS_OK;
    }

    // actually fill up the specbuffer converting the input string
    // to custom font code  if necessary.
    return aFont->FillDrawStringSpec(NS_CONST_CAST(FcChar32 *, aString), 
                                     aLen, data);
}

nsresult
nsFontMetricsXft::TextDimensionsCallback(const FcChar32 *aString, PRUint32 aLen,
                                         nsFontXft *aFont, void *aData)
{
    TextDimensionsData *data = (TextDimensionsData *)aData;

    if (!aFont) {
        NS_ASSERTION(aLen == 1, "An unknown glyph should come by itself");
        data->dimensions->width += 
            mMiniFontWidth * (IS_NON_BMP(*aString) ? 3 : 2) +
            mMiniFontPadding * (IS_NON_BMP(*aString) ? 6 : 5);

        if (data->dimensions->ascent < mMiniFontAscent)
            data->dimensions->ascent = mMiniFontAscent;
        if (data->dimensions->descent < mMiniFontDescent)
            data->dimensions->descent = mMiniFontDescent;

        return NS_OK;
    }

    // get the metric after converting the input string to
    // custom font code if necessary.
    XGlyphInfo glyphInfo;
    nsresult rv = aFont->GetTextExtents32(aString, aLen, glyphInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    data->dimensions->width += glyphInfo.xOff;

    nscoord tmpMaxAscent = aFont->GetMaxAscent();
    nscoord tmpMaxDescent = aFont->GetMaxDescent();

    if (data->dimensions->ascent < tmpMaxAscent)
        data->dimensions->ascent = tmpMaxAscent;
    if (data->dimensions->descent < tmpMaxDescent)
        data->dimensions->descent = tmpMaxDescent;

    return NS_OK;
}

nsresult
nsFontMetricsXft::GetWidthCallback(const FcChar32 *aString, PRUint32 aLen,
                                   nsFontXft *aFont, void *aData)
{
    WidthData *data = (WidthData *)aData;

    if (!aFont) {
        NS_ASSERTION(aLen == 1, "An unknown glyph should come by itself");
        data->width += mMiniFontWidth * (IS_NON_BMP(*aString) ? 3 : 2) + 
                       mMiniFontPadding * (IS_NON_BMP(*aString) ? 6 : 5);
        return NS_OK;
    }

    data->width += aFont->GetWidth32(aString, aLen);
    return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsFontMetricsXft::BoundingMetricsCallback(const FcChar32 *aString, 
                                          PRUint32 aLen, nsFontXft *aFont, 
                                          void *aData)
{
    BoundingMetricsData *data = (BoundingMetricsData *)aData;
    nsBoundingMetrics bm;

    if (!aFont) {
        NS_ASSERTION(aLen == 1, "An unknown glyph should come by itself");
        bm.width = mMiniFontWidth * (IS_NON_BMP(*aString) ? 3 : 2) +
                   mMiniFontPadding * (IS_NON_BMP(*aString) ? 6 : 5);
        bm.rightBearing = bm.width; // no need to set leftBearing.
        bm.ascent = mMiniFontAscent;
        bm.descent = mMiniFontDescent;
    }
    else {
        nsresult rv;
        rv = aFont->GetBoundingMetrics32(aString, aLen, bm);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (data->firstTime) {  
        *(data->bm) = bm;
        data->firstTime = PR_FALSE;
    }
    else {
        *(data->bm) += bm;
    }

    return NS_OK;
}
#endif /* MOZ_MATHML */

/* static */
nsresult
nsFontMetricsXft::FamilyExists(nsIDeviceContext *aDevice,
                               const nsString &aName)
{
    if (!IsASCIIFontName(aName))
        return NS_ERROR_FAILURE;

    NS_ConvertUCS2toUTF8 name(aName);

    FcFontSet *set = nsnull;
    FcObjectSet *os = nsnull;

    FcPattern *pat = FcPatternCreate();
    if (!pat)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_ERROR_FAILURE;
    
    // Build a list of familes and walk the list looking to see if we
    // have it.
    os = FcObjectSetBuild(FC_FAMILY, 0);
    if (!os)
        goto end;

    set = FcFontList(0, pat, os);

    if (!set || !set->nfont)
        goto end;

    for (int i = 0; i < set->nfont; ++i) {
        const char *tmpname = NULL;
        if (FcPatternGetString(set->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **)&tmpname) != FcResultMatch) {
            continue;
        }

        // do they match?
        if (!Compare(nsDependentCString(tmpname), name,
                     nsCaseInsensitiveCStringComparator())) {
            rv = NS_OK;
            break;
        }
    }

 end:
    if (set)
        FcFontSetDestroy(set);
    if (os)
        FcObjectSetDestroy(os);

    FcPatternDestroy(pat);

    return rv;
}

/* static */
PRBool
nsFontMetricsXft::EnumFontCallback(const nsString &aFamily, PRBool aIsGeneric,
                                   void *aData)
{
    // make sure it's an ascii name, if not then return and continue
    // enumerating
    if (!IsASCIIFontName(aFamily))
        return PR_TRUE;

    nsCAutoString name;
    name.AssignWithConversion(aFamily.get());
    ToLowerCase(name);
    nsFontMetricsXft *metrics = (nsFontMetricsXft *)aData;
    metrics->mFontList.AppendCString(name);
    metrics->mFontIsGeneric.AppendElement((void *)aIsGeneric);
    if (aIsGeneric) {
        metrics->mGenericFont = 
            metrics->mFontList.CStringAt(metrics->mFontList.Count() - 1);
        return PR_FALSE; // stop processing
    }

    return PR_TRUE; // keep processing
}

// nsFontEnumeratorXft class

nsFontEnumeratorXft::nsFontEnumeratorXft()
{
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorXft, nsIFontEnumerator)

NS_IMETHODIMP
nsFontEnumeratorXft::EnumerateAllFonts(PRUint32 *aCount, PRUnichar ***aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    NS_ENSURE_ARG_POINTER(aCount);
    *aCount = 0;

    return EnumFontsXft(nsnull, nsnull, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorXft::EnumerateFonts(const char *aLangGroup,
                                    const char *aGeneric,
                                    PRUint32 *aCount, PRUnichar ***aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    NS_ENSURE_ARG_POINTER(aCount);
    *aCount = 0;

    // aLangGroup=null or ""  means any (i.e., don't care)
    // aGeneric=null or ""  means any (i.e, don't care)
    nsCOMPtr<nsIAtom> langGroup;
    if (aLangGroup && *aLangGroup)
      langGroup = do_GetAtom(aLangGroup);
    const char* generic = nsnull;
    if (aGeneric && *aGeneric)
      generic = aGeneric;

    return EnumFontsXft(langGroup, generic, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorXft::HaveFontFor(const char *aLangGroup, PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = PR_FALSE;
    NS_ENSURE_ARG_POINTER(aLangGroup);

    *aResult = PR_TRUE; // always return true for now.
    // Finish me - ftang
    return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorXft::GetDefaultFont(const char *aLangGroup, 
  const char *aGeneric, PRUnichar **aResult)
{
  // aLangGroup=null or ""  means any (i.e., don't care)
  // aGeneric=null or ""  means any (i.e, don't care)

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

#if 0
  //XXX Some voodoo inspired from the thread:
  //XXX http://www.mail-archive.com/fonts@xfree86.org/msg01344.html
  //XXX please iterate to turn this on when you are happy/ready

  FcResult res;
  FcPattern* match_pattern = NULL;
  if (aGeneric && *aGeneric)
    match_pattern = FcNameParse(aGeneric);
  else
    match_pattern = FcPatternCreate();

  if (!match_pattern)
    return NS_OK; // not fatal, just return an empty default name

  if (aLangGroup && *aLangGroup) {
    nsCOMPtr<nsIAtom> langGroup = do_GetAtom(aLangGroup);
    AddLangGroup(match_pattern, langGroup);
  }

  FcConfigSubstitute(0, match_pattern, FcMatchPattern); 
  FcDefaultSubstitute(match_pattern);
  FcPattern* result_pattern = FcFontMatch(0, match_pattern, &res);
  if (result_pattern) {
    char *family;
    FcPatternGetString(result_pattern, FC_FAMILY, 0, (FcChar8 **)&family);
    PRUnichar* name = NS_STATIC_CAST(PRUnichar *,
                              nsMemory::Alloc ((strlen (family) + 1)
                                               * sizeof (PRUnichar)));
    if (name) {
      PRUnichar *r = name;
      for (char *f = family; *f; ++f)
        *r++ = *f;
      *r = '\0';

      *aResult = name;
    }

    FcPatternDestroy(result_pattern);
  }
  FcPatternDestroy(match_pattern);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorXft::UpdateFontList(PRBool *_retval)
{
    *_retval = PR_FALSE; // always return false for now
    return NS_OK;
}

// class nsFontXft

nsFontXft::nsFontXft(FcPattern *aPattern, FcPattern *aFontName)
{
    // save our pattern - we own it now
    mPattern = aPattern;
    mFontName = aFontName;
    // don't destroy it out from under us
    FcPatternReference(aPattern);
    FcPatternReference(mFontName);

    mXftFont = nsnull;

    // set up our charset
    mCharset = nsnull;
    FcCharSet *charset = nsnull;

    // this references an internal charset, we need to copy it
    FcPatternGetCharSet(aFontName, FC_CHARSET, 0, &charset);
    if (charset)
        mCharset = FcCharSetCopy(charset);
}

nsFontXft::~nsFontXft()
{
    if (mXftFont)
        XftFontClose(GDK_DISPLAY(), mXftFont);
    if (mCharset)
        FcCharSetDestroy(mCharset);
    if (mPattern)
        FcPatternDestroy(mPattern);
    if (mFontName)
        FcPatternDestroy(mFontName);
}

XftFont *
nsFontXft::GetXftFont(void)
{
    if (!mXftFont) {
        FcPattern *pat = FcFontRenderPrepare(0, mPattern, mFontName);
        if (!pat)
            return nsnull;

        // bug 196312 : work around problem with CJK bi-width fonts
        // and fontconfig prior to 2.2. CJK bi-width fonts are regarded
        // as genuinely monospace by fontconfig that  sets FC_SPACING
        // to FC_MONOSPACE, which makes Xft drawing/measuring routines
        // use the global advance width (the width of double width glyphs 
        // and twice the width of single width glyphs) even for single width
        // characters (e.g. Latin letters and digits). This results in
        // spaced-out ('s p a c e d - o u t') rendering. By deleting
        // FC_SPACING here, we're emulating the behavior of fontconfig 2.2 or 
        // later that does not set FC_SPACING for any font.
        if (FcGetVersion() < 20300)
            FcPatternDel(pat, FC_SPACING);

        mXftFont = XftFontOpenPattern(GDK_DISPLAY(), pat);
        if (!mXftFont)
            FcPatternDestroy(pat);
    }

    return mXftFont;
}

// used by GetWidth32, GetBoundingMetrics32, and TextDimensionCallback
nsresult
nsFontXft::GetTextExtents32(const FcChar32 *aString, PRUint32 aLen, 
                            XGlyphInfo &aGlyphInfo)
{
    if (!mXftFont)
        GetXftFont();

    XftTextExtents32(GDK_DISPLAY(), mXftFont, aString, aLen, &aGlyphInfo);

    return NS_OK;
}

gint
nsFontXft::GetWidth32(const FcChar32 *aString, PRUint32 aLen)
{
    XGlyphInfo glyphInfo;
    GetTextExtents32(aString, aLen, glyphInfo);

    return glyphInfo.xOff;
}

#ifdef MOZ_MATHML
nsresult
nsFontXft::GetBoundingMetrics32(const FcChar32*    aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics)
{
    aBoundingMetrics.Clear ();

    if (aString && aLength) {
        XGlyphInfo glyphInfo;
        GetTextExtents32 (aString, aLength, glyphInfo);
        aBoundingMetrics.leftBearing = - glyphInfo.x;
        aBoundingMetrics.rightBearing = glyphInfo.width - glyphInfo.x;
        aBoundingMetrics.ascent = glyphInfo.y;
        aBoundingMetrics.descent = glyphInfo.height - glyphInfo.y;
        aBoundingMetrics.width = glyphInfo.xOff;
    }
    return NS_OK;
}
#endif /* MOZ_MATHML */

PRInt16
nsFontXft::GetMaxAscent(void)
{
    if (!mXftFont)
        GetXftFont();

    return mXftFont->ascent;
}

PRInt16
nsFontXft::GetMaxDescent(void)
{
    if (!mXftFont)
        GetXftFont();

    return mXftFont->descent;
}

FT_UInt
nsFontXft::CharToGlyphIndex(FcChar32 aChar)
{
    return XftCharIndex(GDK_DISPLAY(), mXftFont, aChar);
}

// used by DrawStringCallback
nsresult
nsFontXft::FillDrawStringSpec(FcChar32 *aString, PRUint32 aLen, void *aData)
{
    DrawStringData *data = (DrawStringData *)aData;

    if (!mXftFont)
        GetXftFont();

    XftGlyphFontSpec *const specBuffer = data->specBuffer;
    PRUint32& specBufferLen = data->specBufferLen;

    FcChar32 *pstr = aString;
    const FcChar32 *end = aString + aLen;

    while(pstr < end) {
        nscoord x = data->x + data->xOffset;               
        nscoord y = data->y;                        
        /* Convert to device coordinate. */   
        data->context->GetTranMatrix()->TransformCoord(&x, &y);
                                                                 
        /* position in X is the location offset in the string 
           plus whatever offset is required for the spacing   
           argument                                           
        */                                                  

        specBuffer[specBufferLen].x = x;                    
        specBuffer[specBufferLen].y = y;                   
        specBuffer[specBufferLen].font = mXftFont;
        FT_UInt glyph = CharToGlyphIndex(*pstr);
        specBuffer[specBufferLen].glyph = glyph;
                                                               
        /* check to see if this glyph is non-empty */ 
        if (!data->foundGlyph) {                           
            XGlyphInfo info;                        
            XftGlyphExtents(GDK_DISPLAY(), mXftFont, &glyph, 1, &info);
            if (info.width && info.height)                  
              data->foundGlyph = PR_TRUE;                      
        }                                                 

        if (data->spacing) {
            data->xOffset += *data->spacing;
            data->spacing += IS_NON_BMP(*pstr) ? 2 : 1; 
        }
        else {
            XGlyphInfo info;                        
            XftGlyphExtents(GDK_DISPLAY(), mXftFont, &glyph, 1, &info);
            data->xOffset += NSToCoordRound(info.xOff * data->p2t);
        }

        ++specBufferLen;
        ++pstr;
    }                                                          
    return NS_OK;
}

// class nsFontXftUnicode impl
  
inline
nsFontXftUnicode::nsFontXftUnicode(FcPattern* aPattern, FcPattern* aFontName) :
                                   nsFontXft(aPattern, aFontName)
{
}

inline
nsFontXftUnicode::~nsFontXftUnicode()
{
}

PRBool
nsFontXftUnicode::HasChar(PRUint32 aChar)
{
    return FcCharSetHasChar(mCharset, (FcChar32) aChar);
}

// class nsFontXftCustom impl

inline
nsFontXftCustom::nsFontXftCustom(FcPattern* aPattern, FcPattern* aFontName,
                                 nsFontXftInfo* aFontInfo) :
                                 nsFontXft(aPattern, aFontName),
                                 mFontInfo(aFontInfo), mFT_Face(nsnull)
{
}

nsFontXftCustom::~nsFontXftCustom()
{
    if (mXftFont && mFT_Face)
       XftUnlockFace(mXftFont);
}

// used by GetWidth32, GetBoundingMetrics32, and TextDimensionCallback
// Convert the input to custom font code before measuring.
nsresult
nsFontXftCustom::GetTextExtents32(const FcChar32 *aString, PRUint32 aLen, 
                                  XGlyphInfo &aGlyphInfo)
{
    nsAutoBuffer buffer;
    nsresult rv;
    PRUint32 destLen = aLen;
    PRBool isWide = (mFontInfo->mFontType == eFontTypeCustomWide); 

    // we won't use this again. it's safe to cast away const.
    rv = ConvertUCS4ToCustom(NS_CONST_CAST(FcChar32 *, aString), 
                             aLen, destLen, mFontInfo->mConverter,
                             isWide, buffer);
    NS_ENSURE_SUCCESS(rv, rv);
      
    FcChar32 *str = NS_STATIC_CAST(FcChar32 *, buffer.GetArray());
    if (!str)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mXftFont)
        GetXftFont();

    // short cut for the common case
    if (isWide) { 
        XftTextExtents32(GDK_DISPLAY(), mXftFont, str, destLen, &aGlyphInfo);
        return NS_OK;
    }

    // If FT_SelectCharMap succeeds for MacRoman or Unicode,
    // use glyph indices directly for 'narrow' custom encoded fonts.
    rv = SetFT_FaceCharmap();
    NS_ENSURE_SUCCESS(rv, rv);

    // replace chars with glyphs in place. 
    for (PRUint32 i = 0; i < destLen; i++) {
        str[i] = FT_Get_Char_Index (mFT_Face, FT_ULong(str[i]));
    }

    XftGlyphExtents(GDK_DISPLAY(), mXftFont, str, destLen, &aGlyphInfo);
        
    return NS_OK;
}

PRBool
nsFontXftCustom::HasChar(PRUint32 aChar)
{
    return (mFontInfo->mCCMap &&
            CCMAP_HAS_CHAR_EXT(mFontInfo->mCCMap, aChar)); 
}

FT_UInt
nsFontXftCustom::CharToGlyphIndex(FcChar32 aChar)
{
    if (mFontInfo->mFontType == eFontTypeCustomWide)
        return XftCharIndex(GDK_DISPLAY(), mXftFont, aChar);
    else
        return FT_Get_Char_Index(mFT_Face, aChar);
}

// used by DrawStringCallback
// Convert the input to custom font code before filling up the buffer.
nsresult
nsFontXftCustom::FillDrawStringSpec(FcChar32* aString, PRUint32 aLen,
                                    void* aData)
{
    DrawStringData *data = (DrawStringData *)aData;

    nsresult rv = NS_OK;
    nsAutoBuffer buffer;
    PRUint32 destLen = aLen;
    PRBool isWide = (mFontInfo->mFontType == eFontTypeCustomWide); 

    rv = ConvertUCS4ToCustom(aString, aLen, destLen, mFontInfo->mConverter, 
                             isWide, buffer);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mXftFont)
        GetXftFont();

    // The string after the conversion can be longer than the original.
    // Realloc if necessary.
    if (destLen > aLen && 
        data->specBufferLen + destLen > data->specBufferSize) {
        data->specBuffer = ReallocSpecBuffer(data->specBufferSize, 
                            data->specBufferSize + (destLen - aLen) * 2,
                            data->specBuffer);
        if (!data->specBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        data->specBufferSize += (destLen - aLen) * 2;
    }

    if (!isWide) {
        // For some narrow fonts(Mathematica, Symbol, and MTExtra),  
        // what we get back  after the conversion  is in the encoding 
        // of a specific  FT_Charmap (Apple Roman) instead of Unicode 
        // so that we  can't call XftCharIndex() which regards input 
        // as a Unicode codepoint. Instead, we have to directly invoke 
        // FT_Get_Char_Index() with FT_Face corresponding to XftFont 
        // after setting FT_Charmap to the cmap of our choice(Apple Roman).
        rv = SetFT_FaceCharmap();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    FcChar32 *str = NS_STATIC_CAST(FcChar32 *, buffer.GetArray());

    rv = nsFontXft::FillDrawStringSpec(str, destLen, aData);

    return rv;
}

nsresult
nsFontXftCustom::SetFT_FaceCharmap(void)
{
    if (!mXftFont)
        GetXftFont();

    if (mFT_Face)
        return NS_OK;

    mFT_Face = XftLockFace(mXftFont);

    NS_ENSURE_TRUE(mFT_Face != nsnull, NS_ERROR_UNEXPECTED);

    // Select FT_Encoding to use for custom narrow fonts in 
    // glyph index look-up with FT_Get_Char_Index succeeds.
        
    if (FT_Select_Charmap (mFT_Face, mFontInfo->mFT_Encoding))
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

// Hash table allocation  for gFontXftMaps made up of 
// (family, nsFontXftInfo) pairs.  Copied from nsFontMetricsWin.cpp. 
// XXX : jshin I'd love to use brand-new nsClassHashtable, but I can't 
// get it  compiled with g++ 3.2 under Linux.
PR_STATIC_CALLBACK(void*) fontmapAllocTable(void *pool, size_t size)
{
    return nsMemory::Alloc(size);
}

PR_STATIC_CALLBACK(void) fontmapFreeTable(void *pool, void *item)
{
    nsMemory::Free(item);
}

PR_STATIC_CALLBACK(PLHashEntry*) fontmapAllocEntry(void *pool, const void *key)
{
    return new nsFontXftInfo();
}

PR_STATIC_CALLBACK(void) fontmapFreeEntry(void *pool, PLHashEntry *he, 
                                          PRUint32 flag)
{
    // we don't have to free 'key' because key is from FcPatternGet()
    // that owns it. 
    if (flag == HT_FREE_ENTRY)  {
        nsFontXftInfo *fontInfo = NS_STATIC_CAST(nsFontXftInfo *, he);
        if (fontInfo)
            delete fontInfo;
    }
}

// class nsAutoBuffer

nsAutoBuffer::nsAutoBuffer()
    : mArray(mAutoArray),
      mCount(FONT_SPEC_BUFFER_SIZE)
{
}

nsAutoBuffer::~nsAutoBuffer()
{
    if (mArray && (mArray != mAutoArray)) {
        delete [] mArray;
    }
}

void* nsAutoBuffer::GetArray(PRInt32 aMinLen)
{
    if (aMinLen > mCount) {
        char* newArray = new char[aMinLen];
        if (!newArray) {
            return nsnull;
        }

        if (mArray != mAutoArray) {
            delete [] mArray;
        }

        mArray = newArray;
        mCount = aMinLen;
    }

    return (void *) mArray;
}

// Static functions

/* static */
int
CalculateSlant(PRUint8 aStyle)
{
    int fcSlant;

    switch(aStyle) {
    case NS_FONT_STYLE_ITALIC:
        fcSlant = FC_SLANT_ITALIC;
        break;
    case NS_FONT_STYLE_OBLIQUE:
        fcSlant = FC_SLANT_OBLIQUE;
        break;
    default:
        fcSlant = FC_SLANT_ROMAN;
        break;
    }

    return fcSlant;
}

/* static */
int
CalculateWeight (PRUint16 aWeight)
{
    /*
     * weights come in two parts crammed into one
     * integer -- the "base" weight is weight / 100,
     * the rest of the value is the "offset" from that
     * weight -- the number of steps to move to adjust
     * the weight in the list of supported font weights,
     * this value can be negative or positive.
     */
    PRInt32 baseWeight = (aWeight + 50) / 100;
    PRInt32 offset = aWeight - baseWeight * 100;

    /* clip weights to range 0 to 9 */
    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    /* Map from weight value to fcWeights index */
    static int fcWeightLookup[10] = {
        0, 0, 0, 0, 1, 1, 2, 3, 3, 4,
    };

    PRInt32 fcWeight = fcWeightLookup[baseWeight];

    /*
     * adjust by the offset value, make sure we stay inside the 
     * fcWeights table
     */
    fcWeight += offset;

    if (fcWeight < 0)
        fcWeight = 0;
    if (fcWeight > 4)
        fcWeight = 4;

    /* Map to final FC_WEIGHT value */
    static int fcWeights[5] = {
        FC_WEIGHT_LIGHT,      /* 0 */
        FC_WEIGHT_MEDIUM,     /* 1 */
        FC_WEIGHT_DEMIBOLD,	  /* 2 */
        FC_WEIGHT_BOLD,       /* 3 */
        FC_WEIGHT_BLACK,      /* 4 */
    };

    return fcWeights[fcWeight];

}

/* static */
void
AddLangGroup(FcPattern *aPattern, nsIAtom *aLangGroup)
{
    // Find the FC lang group for this lang group
    nsCAutoString cname;
    aLangGroup->ToUTF8String(cname);

    // see if the lang group needs to be translated from mozilla's
    // internal mapping into fontconfig's
    const struct MozXftLangGroup *langGroup;
    langGroup = FindFCLangGroup(cname);

    // if there's no lang group, just use the lang group as it was
    // passed to us
    //
    // we're casting away the const here for the strings - should be
    // safe.
    if (!langGroup)
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)cname.get());
    else if (langGroup->XftLang) 
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)langGroup->XftLang);
}

/* static */
void
AddFFRE(FcPattern *aPattern, nsCString *aFamily, PRBool aWeak)
{
    nsCAutoString family;
    FFREToFamily(*aFamily, family);

    FcValue v;
    v.type = FcTypeString;
    // casting away the const here, should be safe
    v.u.s = (FcChar8 *)family.get();

    if (aWeak)
        FcPatternAddWeak(aPattern, FC_FAMILY, v, FcTrue);
    else
        FcPatternAdd(aPattern, FC_FAMILY, v, FcTrue);
}

/* static */
void
FFREToFamily(nsACString &aFFREName, nsACString &oFamily)
{
  if (FFRECountHyphens(aFFREName) == 3) {
      PRInt32 familyHyphen = aFFREName.FindChar('-') + 1;
      PRInt32 registryHyphen = aFFREName.FindChar('-',familyHyphen);
      oFamily.Append(Substring(aFFREName, familyHyphen,
                               registryHyphen-familyHyphen));
  }
  else {
      oFamily.Append(aFFREName);
  }
}

/* static */
int
FFRECountHyphens (nsACString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

/* static */
int
CompareFontNames (const void* aArg1, const void* aArg2, void* aClosure)
{
    const PRUnichar* str1 = *((const PRUnichar**) aArg1);
    const PRUnichar* str2 = *((const PRUnichar**) aArg2);

    return nsCRT::strcmp(str1, str2);
}

PRBool
IsASCIIFontName(const nsString& aName)
{
    PRUint32 len = aName.Length();
    const PRUnichar* str = aName.get();
    for (PRUint32 i = 0; i < len; i++) {
        /*
         * X font names are printable ASCII, ignore others (for now)
         */
        if ((str[i] < 0x20) || (str[i] > 0x7E)) {
            return PR_FALSE;
        }
    }
  
    return PR_TRUE;
}

/* static */
nsresult
EnumFontsXft(nsIAtom* aLangGroup, const char* aGeneric,
             PRUint32* aCount, PRUnichar*** aResult)
{
    FcPattern   *pat = NULL;
    FcObjectSet *os  = NULL;
    FcFontSet   *fs  = NULL;
    nsresult     rv  = NS_ERROR_FAILURE;

    PRUnichar **array = NULL;
    PRUint32    narray = 0;
    PRInt32     serif = 0, sansSerif = 0, monospace = 0, nGenerics;

    *aCount = 0;
    *aResult = nsnull;

    pat = FcPatternCreate();
    if (!pat)
        goto end;

    os = FcObjectSetBuild(FC_FAMILY, FC_FOUNDRY, 0);
    if (!os)
        goto end;

    // take the pattern and add the lang group to it
    if (aLangGroup)
        AddLangGroup(pat, aLangGroup);

    // get the font list
    fs = FcFontList(0, pat, os);

    if (!fs)
        goto end;

    if (!fs->nfont) {
        rv = NS_OK;
        goto end;
    }

    // Fontconfig supports 3 generic fonts, "serif", "sans-serif", and
    // "monospace", slightly different from CSS's 5.
    if (!aGeneric)
        serif = sansSerif = monospace = 1;
    else if (!strcmp(aGeneric, "serif"))
        serif = 1;
    else if (!strcmp(aGeneric, "sans-serif"))
        sansSerif = 1;
    else if (!strcmp(aGeneric, "monospace"))
        monospace = 1;
    else if (!strcmp(aGeneric, "cursive") || !strcmp(aGeneric, "fantasy"))
        serif = sansSerif =  1;
    else
        NS_NOTREACHED("unexpected generic family");
    nGenerics = serif + sansSerif + monospace;

    array = NS_STATIC_CAST(PRUnichar **,
               nsMemory::Alloc((fs->nfont + nGenerics) * sizeof(PRUnichar *)));
    if (!array)
        goto end;

    if (serif) {
        PRUnichar *name = ToNewUnicode(NS_LITERAL_STRING("serif"));
        if (!name)
            goto end;
        array[narray++] = name;
    }

    if (sansSerif) {
        PRUnichar *name = ToNewUnicode(NS_LITERAL_STRING("sans-serif"));
        if (!name)
            goto end;
        array[narray++] = name;
    }

    if (monospace) {
        PRUnichar *name = ToNewUnicode(NS_LITERAL_STRING("monospace"));
        if (!name)
            goto end;
        array[narray++] = name;
    }

    for (int i=0; i < fs->nfont; ++i) {
        char *family;
        PRUnichar *name;

        // if there's no family, just move to the next iteration
        if (FcPatternGetString (fs->fonts[i], FC_FAMILY, 0,
                                (FcChar8 **) &family) != FcResultMatch) {
            continue;
        }

        name = NS_STATIC_CAST(PRUnichar *,
                              nsMemory::Alloc ((strlen (family) + 1)
                                               * sizeof (PRUnichar)));

        if (!name)
            goto end;

        PRUnichar *r = name;
        for (char *f = family; *f; ++f)
            *r++ = *f;
        *r = '\0';

        array[narray++] = name;
    }

    NS_QuickSort(array + nGenerics, narray - nGenerics, sizeof (PRUnichar*),
                 CompareFontNames, nsnull);

    *aCount = narray;
    if (narray)
        *aResult = array;
    else
        nsMemory::Free(array);

    rv = NS_OK;

 end:
    if (NS_FAILED(rv) && array) {
        while (narray)
            nsMemory::Free (array[--narray]);
        nsMemory::Free (array);
    }
    if (pat)
        FcPatternDestroy(pat);
    if (os)
        FcObjectSetDestroy(os);
    if (fs)
        FcFontSetDestroy(fs);

    return rv;
}

/* static */
const MozXftLangGroup*
FindFCLangGroup (nsACString &aLangGroup)
{
    for (unsigned int i=0; i < NUM_XFT_LANG_GROUPS; ++i) {
        if (aLangGroup.Equals(MozXftLangGroups[i].mozLangGroup,
                              nsCaseInsensitiveCStringComparator())) {
            return &MozXftLangGroups[i];
        }
    }

    return nsnull;
}

/* static inline */
XftGlyphFontSpec *
AllocSpecBuffer(PRUint32 aLen, PRUint32 &aSize)
{
    if (aLen > FONT_SPEC_BUFFER_SIZE) {
        aSize = aLen;
        return new XftGlyphFontSpec[aLen];
    }
    aSize = FONT_SPEC_BUFFER_SIZE;
    return gFontSpecBuffer;
}

/* static inline */
XftGlyphFontSpec *
ReallocSpecBuffer(PRUint32 aOldLen, PRUint32 aNewLen, XftGlyphFontSpec *aBuffer)
{
    XftGlyphFontSpec *buffer = new XftGlyphFontSpec[aNewLen];
    if (buffer) {
        memcpy(buffer, aBuffer, aOldLen * sizeof(XftGlyphFontSpec));
        FreeSpecBuffer(aBuffer);
        return buffer;
    }
    FreeSpecBuffer(aBuffer);
    return nsnull;
}

/* static inline */
void
FreeSpecBuffer (XftGlyphFontSpec *aBuffer)
{
    if (aBuffer != gFontSpecBuffer)
        delete [] aBuffer;
}

/* static */
void
ConvertCharToUCS4(const char *aString, PRUint32 aLength, 
                  nsAutoBuffer &aOutBuffer, PRUint32 *aOutLen)
{
    *aOutLen = 0;
    FcChar32 *outBuffer;

    // It takes 4bytes to represent a char. in UCS4.
    outBuffer  = NS_STATIC_CAST(FcChar32 *, aOutBuffer.GetArray(aLength * 4));
    if (!outBuffer)
        return;
    
    for (PRUint32 i=0; i < aLength; ++i) {
        outBuffer[i] = PRUint8(aString[i]); // to convert char >= 0x80 correctly
    }

    *aOutLen = aLength;
}

// Convert the incoming string into an array of UCS4 chars
  
/* static */
void
ConvertUnicharToUCS4(const PRUnichar *aString, PRUint32 aLength,
                     nsAutoBuffer &aOutBuffer, PRUint32 *aOutLen)
{
    *aOutLen = 0;
    FcChar32 *outBuffer;

    // It takes 4bytes to represent a char. in UCS4.
    outBuffer  = NS_STATIC_CAST(FcChar32 *, aOutBuffer.GetArray(aLength * 4));

    if (!outBuffer)
        return;

    PRUint32 outLen = 0;

    // Walk the passed in string looking for surrogates to convert to
    // their full ucs4 representation.
    for (PRUint32 i = 0; i < aLength; ++i) {
        PRUnichar c = aString[i];

        // Optimized for the non-surrogate case
        if (IS_NON_SURROGATE(c)) {
            outBuffer[outLen] = c;
        }
        else if (IS_HIGH_SURROGATE(aString[i])) {
            if (i + 1 < aLength && IS_LOW_SURROGATE(aString[i+1])) {
                outBuffer[outLen] = SURROGATE_TO_UCS4(c, aString[i + 1]);
                ++i;
            }
            else { // Unpaired high surrogate
                outBuffer[outLen] = UCS2_REPLACEMENT;
            }
        }
        else if (IS_LOW_SURROGATE(aString[i])) { // Unpaired low surrogate?
            outBuffer[outLen] = UCS2_REPLACEMENT;
        }

        outLen++;
    }

    *aOutLen = outLen;
}

/* static */
nsresult
StaticDrawStringCallback(const FcChar32 *aString, PRUint32 aLen, 
                         nsFontXft *aFont, void *aData)
{
    DrawStringData *data = (DrawStringData *)aData;
    return data->metrics->DrawStringCallback(aString, aLen, aFont, aData);
}

/* static */
nsresult
StaticTextDimensionsCallback(const FcChar32 *aString, PRUint32 aLen, 
                             nsFontXft *aFont, void *aData)
{
    TextDimensionsData *data = (TextDimensionsData *)aData;
    return data->metrics->TextDimensionsCallback(aString, aLen, aFont, aData);
}

/* static */
nsresult
StaticGetWidthCallback(const FcChar32 *aString, PRUint32 aLen, 
                       nsFontXft *aFont, void *aData)
{
    WidthData *data = (WidthData *)aData;
    return data->metrics->GetWidthCallback(aString, aLen, aFont, aData);
}

#ifdef MOZ_MATHML
/* static */
nsresult
StaticBoundingMetricsCallback(const FcChar32 *aString, PRUint32 aLen, 
                              nsFontXft *aFont, void *aData)
{
    BoundingMetricsData *data = (BoundingMetricsData *)aData;
    return data->metrics->BoundingMetricsCallback(aString, aLen, aFont, aData);
}
#endif /* MOZ_MATHML */

#ifdef MOZ_WIDGET_GTK2
/* static */
void
GdkRegionSetXftClip(GdkRegion *aGdkRegion, XftDraw *aDraw)
{
    GdkRectangle  *rects   = nsnull;
    int            n_rects = 0;

    gdk_region_get_rectangles(aGdkRegion, &rects, &n_rects);

    XRectangle *xrects = g_new(XRectangle, n_rects);

    for (int i=0; i < n_rects; ++i) {
        xrects[i].x = CLAMP(rects[i].x, G_MINSHORT, G_MAXSHORT);
        xrects[i].y = CLAMP(rects[i].y, G_MINSHORT, G_MAXSHORT);
        xrects[i].width = CLAMP(rects[i].width, G_MINSHORT, G_MAXSHORT);
        xrects[i].height = CLAMP(rects[i].height, G_MINSHORT, G_MAXSHORT);
    }

    XftDrawSetClipRectangles(aDraw, 0, 0, xrects, n_rects);

    g_free(xrects);
    g_free(rects);
}
#endif /* MOZ_WIDGET_GTK2 */

// Helper to determine if a font has a private encoding that we know
// something about
/* static */
nsresult
GetEncoding(const char *aFontName, char **aEncoding, nsXftFontType &aType,
            FT_Encoding &aFTEncoding)
{
  // below is a list of common used name for startup
    if ((!strcmp(aFontName, "Helvetica" )) ||
         (!strcmp(aFontName, "Times" )) ||
         (!strcmp(aFontName, "Times New Roman" )) ||
         (!strcmp(aFontName, "Courier New" )) ||
         (!strcmp(aFontName, "Courier" )) ||
         (!strcmp(aFontName, "Arial" )) ||
         (!strcmp(aFontName, "MS P Gothic" )) ||
        (!strcmp(aFontName, "Verdana" ))) {
        // error mean do not get a special encoding
        return NS_ERROR_NOT_AVAILABLE; 
    }

    nsCAutoString name;
    name.Assign(NS_LITERAL_CSTRING("encoding.") + 
                nsDependentCString(aFontName) + NS_LITERAL_CSTRING(".ttf"));

    name.StripWhitespace();
    ToLowerCase(name);

    // if we have not init the property yet, init it right now.
    if (!gFontEncodingProperties)
        NS_LoadPersistentPropertiesFromURISpec(&gFontEncodingProperties,
            NS_LITERAL_CSTRING("resource:/res/fonts/fontEncoding.properties"));

    nsAutoString encoding;
    *aEncoding = nsnull;
    if (gFontEncodingProperties) {
        nsresult rv = gFontEncodingProperties->GetStringProperty(name,
                                                                 encoding);
        if (NS_FAILED(rv)) 
            return NS_ERROR_NOT_AVAILABLE;  // Unicode font

        // '.wide' at the end indicates 'wide' font.
        if (encoding.Length() > 5 && 
            StringEndsWith(encoding, NS_LITERAL_STRING(".wide"))) {
            aType = eFontTypeCustomWide;
            encoding.Truncate(encoding.Length() - 5);
        }
        else  {
            aType = eFontTypeCustom;

            // Mathematica and TeX CM truetype fonts have both Apple Roman
            // (PID=1, EID=0) and Unicode (PID=3, EID=1) cmaps. In the
            // former, Unicode cmap uses codepoints in PUA beginning
            // at U+F000 not representable in a single byte encoding
            // like MathML encodings. ( On the other hand, TeX CM fonts
            // have 'pseudo-Unicode' cmap with codepoints below U+0100.)
            // Unicode to font encoding converters for MathML map input 
            // Unicode codepoints to 'pseudo-glyph indices' in Apple Roman 
            // for  Mathematica, Symbol and MTExtra fonts while it maps
            // input Unicode codepoints  to 'pseudo-glyph indices' in 
            // 'Unicode cmap' for TeX CM fonts. Therefore we have to select
            // different FT_Encoding for two groups to guarantee that
            // glyph index look-up with FT_Get_Char_Index succeeds for
            // all MathML fonts. Instead of hard-coding this relation,
            // it's put in fontEncoding.properties file and is parsed here.
          
            nsAutoString ftCharMap; 
            nsresult rv = gFontEncodingProperties->GetStringProperty(
                          Substring(name, 0, name.Length() - 4) + 
                          NS_LITERAL_CSTRING(".ftcmap"), ftCharMap);
          
            if (NS_FAILED(rv)) 
                aFTEncoding = ft_encoding_none;
            else if (ftCharMap.EqualsIgnoreCase("mac_roman"))
                aFTEncoding = ft_encoding_apple_roman;
            else if (ftCharMap.EqualsIgnoreCase("unicode"))
                aFTEncoding = ft_encoding_unicode;
        }

        // encoding name is always in US-ASCII so that there's no loss here.
        *aEncoding = ToNewCString(encoding);
        if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
          printf("\t\tc> it's %s and encoding is %s\n",
                  aType==eFontTypeCustom ? "narrow" : "wide", *aEncoding);
        }

        return NS_OK;
    }

    return NS_ERROR_NOT_AVAILABLE;
}

/* static */
static nsresult
GetConverter(const char* aEncoding, nsIUnicodeEncoder **aConverter)
{
    nsresult rv;

    if (!gCharsetManager) {
        nsServiceManager::GetService(kCharsetConverterManagerCID,
        NS_GET_IID(nsICharsetConverterManager), (nsISupports**)&gCharsetManager);
        if (!gCharsetManager) {
            FreeGlobals();
            return NS_ERROR_FAILURE;
        }
    }

    // encoding name obtained from fontEncoding.properties is
    // canonical so that we don't need the alias resolution. use 'Raw'
    // version.
    rv = gCharsetManager->GetUnicodeEncoderRaw(aEncoding, aConverter);
    NS_ENSURE_SUCCESS(rv, rv);
    if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
        printf("\t\tc> got the converter for %s \n", aEncoding);
    }

    return (*aConverter)->SetOutputErrorBehavior(
            (*aConverter)->kOnError_Replace, nsnull, '?');
}


/* static */
nsresult
FreeGlobals(void)
{
    gInitialized = 0;

    NS_IF_RELEASE(gFontEncodingProperties);
    NS_IF_RELEASE(gCharsetManager);

    if (gFontXftMaps) {
        PL_HashTableDestroy(gFontXftMaps);
        gFontXftMaps = nsnull;
    }

    return NS_OK;
}

/* static */
nsFontXftInfo*
GetFontXftInfo(FcPattern* aPattern)
{
    const char* family;

    // If there's no family, just treat it as if it's a normal Unicode font
    if (FcPatternGetString(aPattern, FC_FAMILY, 0, (FcChar8 **) &family) 
         != FcResultMatch) {
        return nsnull;
    }

    NS_ASSERTION(gFontXftMaps, "gFontXMaps should not be null by now.");

    // shouldn't be NULL, using it as a flag to catch bad changes
    PLHashEntry *he, **hep = nsnull; 
    PLHashNumber hash;

    hash = HashKey(family);
    hep = PL_HashTableRawLookup(gFontXftMaps, hash, family);
    he = *hep;
    if (he) {
      // cached entry found. 
      return (NS_STATIC_CAST(nsFontXftInfo *, he));
    }

    PRUint16* ccmap = nsnull;
    nsCOMPtr<nsIUnicodeEncoder> converter;
    nsXftFontType fontType =  eFontTypeUnicode; 
    nsXPIDLCString encoding;
    FT_Encoding ftEncoding = ft_encoding_unicode;

    // See if a font has a custom/private encoding by matching
    // its family name against the list in fontEncoding.properties 
    // with GetEncoding(). It also sets fonttype (wide or narrow). 
    // Then get the converter and see if has a valid coverage map. 
    
    if (NS_SUCCEEDED(GetEncoding(family, getter_Copies(encoding), 
                     fontType, ftEncoding)) &&
        NS_SUCCEEDED(GetConverter(encoding.get(), getter_AddRefs(converter)))) {
        nsCOMPtr<nsICharRepresentable> mapper(do_QueryInterface(converter));
        if (PR_LOG_TEST(gXftFontLoad, PR_LOG_DEBUG)) {
           printf("\t\tc> got the converter and CMap :%s !!\n",
                  encoding.get());
        }

        if (mapper) {
            ccmap = MapperToCCMap(mapper);
        }
    }

    // XXX Need to check if an identical map has already been added - Bug 75260
    // For Xft, this doesn't look as critical as in GFX Win.
    NS_ASSERTION(hep, "bad code");  

    nsFontXftInfo* info;

    // Put (family, info) pair to hash. family returned by FcPatternGet()
    // is just a reference so that we shouldn't free it. 
    he = PL_HashTableRawAdd(gFontXftMaps, hep, hash, family, nsnull);
    if (!he)
        return nsnull;

    info = NS_STATIC_CAST(nsFontXftInfo*, he);
    he->value = info;    
    // will be freed by ~nsFontXftInfo() invoked  by fontmapFreeEntry
    info->mCCMap =  ccmap;  
    info->mConverter = converter;
    info->mFontType = fontType;
    info->mFT_Encoding = ftEncoding;

    return info;
}

/* static */
// Convert input UCS4 string to "Pseudo-UCS4" string made of
// custom font-specific codepoints with Unicode converter.
nsresult
ConvertUCS4ToCustom(FcChar32 *aSrc,  PRUint32 aSrcLen,
                    PRUint32& aDestLen, nsIUnicodeEncoder *aConverter,
                    PRBool aIsWide, nsAutoBuffer& aResult)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIUnicodeEncoder> converter = aConverter;
    if (!converter )
        return NS_ERROR_UNEXPECTED;

    // Convert to UTF-16 because UnicodeEncoder expects input to be in
    // UTF-16.  We can get away with in-place replacement because
    // UTF-16 is at most as long as UCS-4 so that UCS-4(source) buffer
    // pointer is always ahead of UTF-16(target) buffer pointer and we
    // won't revisit the buffer we already processed.
    
    PRUnichar *utf16Src  = NS_REINTERPRET_CAST(PRUnichar *, aSrc);
    PRUnichar *utf16Ptr = utf16Src;
    for (PRUint32 i = 0; i < aSrcLen; ++i, ++utf16Ptr) {
        if (!IS_NON_BMP(aSrc[i])) {
            *utf16Ptr = PRUnichar(aSrc[i]);
        }
        else {
            *utf16Ptr = H_SURROGATE(aSrc[i]);
            *++utf16Ptr = L_SURROGATE(aSrc[i]);
        }
    }

    PRInt32 utf16SrcLen = utf16Ptr - utf16Src;
    PRInt32 medLen = utf16SrcLen;
    // Length can increase for 'Wide' custom fonts.
    if (aIsWide &&
        NS_FAILED(aConverter->GetMaxLength(utf16Src, utf16SrcLen, &medLen))) {
        return NS_ERROR_UNEXPECTED;
    }

    nsAutoBuffer medBuffer;
    char *med  = NS_STATIC_CAST(char *, medBuffer.GetArray(medLen));
    if (!med)
        return NS_ERROR_OUT_OF_MEMORY;

    // Convert utf16Src  to font-specific encoding with mConverter.
    rv = converter->Convert(utf16Src, &utf16SrcLen, med, &medLen);
    NS_ENSURE_SUCCESS(rv, rv);

    // Put pseudo-unicode str. into font specific pseudo-UCS-4 str.
    if (aIsWide) {
#ifdef IS_LITTLE_ENDIAN
        // Convert BE UTF-16 to LE UTF-16 for 'wide' custom fonts
        char* pstr = med;
        while (pstr < med + medLen) {
            PRUint8 tmp = pstr[0];
            pstr[0] = pstr[1];
            pstr[1] = tmp;
            pstr += 2; // swap every two bytes
        }
#endif
        // Convert 16bit  custom font codes to UCS4
        ConvertUnicharToUCS4(NS_REINTERPRET_CAST(PRUnichar *, med),
                             medLen >> 1, aResult, &aDestLen);
    }
    else {
        // Convert 8bit custom font codes to UCS4
        ConvertCharToUCS4(med, medLen, aResult, &aDestLen);
    }

    return rv;
}

/* static */
PLHashNumber
HashKey(const void* aString)
{
  return PLHashNumber(nsCRT::HashCode((const char *) aString));
}

/* static */
PRIntn
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp((const char *) aStr1, (const char *) aStr2) == 0; 
}
