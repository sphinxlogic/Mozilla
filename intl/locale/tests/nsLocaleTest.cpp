/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
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
#include <stdlib.h>
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include "nsILocaleService.h"

#if defined(XP_WIN) || defined(XP_OS2)
#include "nsIWin32Locale.h"
#include <windows.h>
#endif
#ifdef XP_UNIX
#include "nsIPosixLocale.h"
#endif
#ifdef XP_MAC
#include "nsIMacLocale.h"
#endif

NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
NS_DEFINE_IID(kILocaleServiceIID,NS_ILOCALESERVICE_IID);
NS_DEFINE_CID(kLocaleServiceCID,NS_LOCALESERVICE_CID);
NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#if defined(XP_WIN) || defined(XP_OS2)
NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);

#define USER_DEFINED_PRIMARYLANG	0x0200
#define USER_DEFINED_SUBLANGUAGE	0x20

#endif
#ifdef XP_UNIX
NS_DEFINE_CID(kPosixLocaleFactoryCID, NS_POSIXLOCALEFACTORY_CID);
NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
#endif
#ifdef XP_MAC
NS_DEFINE_CID(kMacLocaleFactoryCID, NS_MACLOCALEFACTORY_CID);
NS_DEFINE_IID(kIMacLocaleIID, NS_IMACLOCALE_IID);
#endif

char* localeCategoryList[6] = { "NSILOCALE_TIME",
								"NSILOCALE_COLLATE",
								"NSILOCALE_CTYPE",
								"NSILOCALE_MONETARY",
								"NSILOCALE_MESSAGES",
								"NSILOCALE_NUMERIC"
};

void
serivce_create_interface(void)
{
	nsresult			result;
	nsILocaleService*	localeService;

	result = nsComponentManager::CreateInstance(kLocaleServiceCID,
									NULL,
									kILocaleServiceIID,
									(void**)&localeService);
	NS_ASSERTION(localeService!=NULL,"nsLocaleTest: service_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: service_create_interface failed");

	localeService->Release();
}

void
factory_create_interface(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsIFactory*			genericFactory;

	result = nsComponentManager::CreateInstance(kLocaleFactoryCID,
									NULL,
									kILocaleFactoryIID,
									(void**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	localeFactory->Release();

	result = nsComponentManager::CreateInstance(kLocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&genericFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	genericFactory->Release();
}

void 
factory_test_isupports(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsISupports*		genericInterface1, *genericInterface2;
	nsIFactory*			genericFactory1, *genericFactory2;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	//
	// test AddRef
	localeFactory->AddRef();

	//
	// test Release
	//
	localeFactory->Release();

	//
	// test generic interface
	//
	result = localeFactory->QueryInterface(kISupportsIID,(void**)&genericInterface1);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface1!=NULL,"nsLocaleTest: factory_test_isupports failed.");

	result = localeFactory->QueryInterface(kISupportsIID,(void**)&genericInterface2);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface2!=NULL,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericInterface1==genericInterface2,"nsLocaleTest: factory_test_isupports failed.");

	genericInterface1->Release();
	genericInterface2->Release();

	//
	// test generic factory
	//
	result = localeFactory->QueryInterface(kIFactoryIID,(void**)&genericFactory1);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1!=NULL,"nsLocaleTest: factory_test_isupports failed.");

	result = localeFactory->QueryInterface(kIFactoryIID,(void**)&genericFactory2);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1!=NULL,"nsLocaleTest: factory_test_isupports failed.");
	NS_ASSERTION(genericFactory1==genericFactory2,"nsLocaleTest: factory_test_isupports failed.");

	genericFactory1->Release();
	genericFactory2->Release();

	localeFactory->Release();
}

void
factory_new_locale(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsILocale*			locale;
	nsString*			localeName, *category, *value;
	int	i;
	nsString**			categoryList, **valueList;
	PRUnichar *lc_name_unichar;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");


	//
	// test NewLocale
	//
	localeName = new nsString("ja-JP");
	result = localeFactory->NewLocale(localeName,&locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_new_interface failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_new_interface failed");

	for(i=0;i<6;i++)
	{
		category = new nsString(localeCategoryList[i]);
		value = new nsString();

		result = locale->GetCategory(category->get(),&lc_name_unichar);
		NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_new_interface failed");
	
    value->SetString(lc_name_unichar);
		delete category;
		delete value;
	}
	delete localeName;
	locale->Release();

	categoryList = new nsString*[6];
	valueList = new nsString*[6];

	for(i=0;i<6;i++)
	{
		categoryList[i] = new nsString(localeCategoryList[i]);
		valueList[i] = new nsString("x-netscape");
	}

	result = localeFactory->NewLocale(categoryList,valueList,6,&locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_new_interface failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_new_interface failed");

	for(i=0;i<6;i++)
	{
		value = new nsString();
		result = locale->GetCategory(categoryList[i]->get(),&lc_name_unichar);
		NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_new_interface failed");

    value->SetString(lc_name_unichar);
		delete value;
	}

	for(i=0;i<6;i++)
	{
		delete categoryList[i];
		delete valueList[i];
	}

	delete [] categoryList;
	delete [] valueList;

	locale->Release();

	localeFactory->Release();
}


void
factory_get_locale(void)
{
	nsresult			result;
	nsILocaleFactory*	localeFactory;
	nsILocale*			locale;
	nsString*			category;
	nsString*			value;
	const char*			acceptLangString = "ja;q=0.9,en;q=1.0,*";
	PRUnichar *lc_name_unichar;

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&localeFactory);
	NS_ASSERTION(localeFactory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	//
	// get the application locale
	//
	result = localeFactory->GetApplicationLocale(&locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	category = new nsString("NSILOCALE_CTYPE");
	value = new nsString();

	result = locale->GetCategory(category->get(),&lc_name_unichar);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");

  value->SetString(lc_name_unichar);
	locale->Release();
	locale->Release();

	delete category;
	delete value;

	//
	// test GetSystemLocale
	//
	result = localeFactory->GetSystemLocale(&locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	category = new nsString("NSILOCALE_CTYPE");
	value = new nsString();

	result = locale->GetCategory(category->get(),&lc_name_unichar);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");

  value->SetString(lc_name_unichar);
	locale->Release();
	locale->Release();

	delete category;
	delete value;

	//
	// test GetLocaleFromAcceptLanguage
	//
	result = localeFactory->GetLocaleFromAcceptLanguage(acceptLangString,&locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");
	NS_ASSERTION(locale!=NULL,"nsLocaleTest: factory_get_locale failed");

	//
	// test and make sure the locale is a valid Interface
	//
	locale->AddRef();

	category = new nsString("NSILOCALE_CTYPE");
	value = new nsString();

	result = locale->GetCategory(category->get(),&lc_name_unichar);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_get_locale failed");

  value->SetString(lc_name_unichar);
	locale->Release();
	locale->Release();

	delete category;
	delete value;

	localeFactory->Release();

}


#if defined(XP_WIN) || defined(XP_OS2)

void
win32factory_create_interface(void)
{
	nsresult			result;
	nsIFactory*			factory;
	nsIWin32Locale*		win32Locale;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&factory);
	NS_ASSERTION(factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	factory->Release();

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	win32Locale->Release();
}

void
win32locale_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsString*			locale;
	LCID				loc_id;

	//
	// test with a simple locale
	//
	locale = new nsString("en-US");
	loc_id = 0;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;

	//
	// test with a not so simple locale
	//
	locale = new nsString("x-netscape");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(USER_DEFINED_PRIMARYLANG,USER_DEFINED_SUBLANGUAGE),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;

	locale = new nsString("en");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");

	delete locale;
	win32Locale->Release();
}

void
win32locale_conversion_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsString*			locale;
	LCID				loc_id;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	//
	// check english variants
	//
	locale = new nsString("en");	// generic english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-US");	// US english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-GB");	// UK english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_UK),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("en-CA");	// Canadian english
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_CAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// japanese
	//
	locale = new nsString("ja");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("ja-JP");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// chinese Locales
	//
	locale = new nsString("zh");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("zh-CN");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("zh-TW");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_TRADITIONAL),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// german and variants
	//
	locale = new nsString("de");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("de-DE");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("de-AT");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN_AUSTRIAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// french and it's variants
	//
	locale = new nsString("fr");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_DEFAULT),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("fr-FR");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	locale = new nsString("fr-CA");
	loc_id = 0;

	result = win32Locale->GetPlatformLocale(locale,&loc_id);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(loc_id==MAKELCID(MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH_CANADIAN),SORT_DEFAULT),
		"nsLocaleTest: GetPlatformLocale failed.");
	delete locale;

	//
	// delete the XPCOM inteface
	//
	win32Locale->Release();
}

void
win32locale_reverse_conversion_test(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	//
	// english and variants
	//
	win32Locale->Release();
}

void
win32_test_special_locales(void)
{
	nsresult			result;
	nsIWin32Locale*		win32Locale;
	nsILocale*			xp_locale;
	nsILocaleFactory*	xp_locale_factory;
	nsString*			locale, *result_locale, *category;
	LCID				sys_lcid, user_lcid;
	PRUnichar *lc_name_unichar;

	result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,
									NULL,
									kIWin32LocaleIID,
									(void**)&win32Locale);
	NS_ASSERTION(win32Locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&xp_locale_factory);
	NS_ASSERTION(xp_locale_factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	category = new nsString(localeCategoryList[0]);

	//
	// derive a system locale
	//
	result  = xp_locale_factory->GetSystemLocale(&xp_locale);
	NS_ASSERTION(xp_locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	sys_lcid = GetSystemDefaultLCID();
	locale = new nsString();
	result_locale = new nsString();

	result = win32Locale->GetXPLocale(sys_lcid,locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");
	result = xp_locale->GetCategory(category->get(),&lc_name_unichar);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

  result_locale->SetString(lc_name_unichar);
	delete locale;
	delete result_locale;
	xp_locale->Release();

	//
	// derive a system locale
	//
	result  = xp_locale_factory->GetApplicationLocale(&xp_locale);
	NS_ASSERTION(xp_locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	user_lcid = GetUserDefaultLCID();
	locale = new nsString();
	result_locale = new nsString();

	result = win32Locale->GetXPLocale(user_lcid,locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");
	result = xp_locale->GetCategory(category->get(),&lc_name_unichar);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

 result_locale->SetString(lc_name_unichar);
	delete locale;
	delete result_locale;
	xp_locale->Release();

	delete category;
	xp_locale_factory->Release();
	win32Locale->Release();


}
	
#endif

#ifdef XP_UNIX
void
posixfactory_create_interface(void)
{
	nsresult			  result;
	nsIFactory*			factory;
	nsIPosixLocale*	posix_locale;

	result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&factory);
	NS_ASSERTION(factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	factory->Release();

	result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
									NULL,
									kIPosixLocaleIID,
									(void**)&posix_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	posix_locale->Release();
}

void
posixlocale_test(void)
{
	nsresult			    result;
	nsIPosixLocale*		posix_locale;
	nsString*			    locale;
	char              posix_locale_string[9];

  //
  // create the locale object
  //
	result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
									NULL,
									kIPosixLocaleIID,
									(void**)&posix_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: create interface failed.\n");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: create interface failed\n");

	//
	// test with a simple locale
	//
	locale = new nsString("en-US");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_string,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.\n");
  NS_ASSERTION(strcmp("en_US",posix_locale_string)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// test with a not so simple locale
	//
	locale = new nsString("x-netscape");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_string,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.\n");
  NS_ASSERTION(strcmp("C",posix_locale_string)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

  //
  // test with a generic locale
  //
	locale = new nsString("en");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_string,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.\n");
  NS_ASSERTION(strcmp("en",posix_locale_string)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;


  //
  // release the locale interface
  //
  posix_locale->Release();
}

void
posixlocale_conversion_test()
{
	nsresult			    result;
	nsIPosixLocale*		posix_locale;
	nsString*			    locale;
  char              posix_locale_result[9];

	result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
                                              NULL,
                                              kIPosixLocaleIID,
                                              (void**)&posix_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	//
	// check english variants
	//
	locale = new nsString("en");	// generic english
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("en",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("en-US");	// US english
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("en_US",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("en-GB");	// UK english
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("en_GB",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("en-CA");	// Canadian english
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("en_CA",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// japanese
	//
	locale = new nsString("ja");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("ja",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("ja-JP");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("ja_JP",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// chinese Locales
	//
	locale = new nsString("zh");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("zh",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("zh-CN");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("zh_CN",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("zh-TW");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("zh_TW",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// german and variants
	//
	locale = new nsString("de");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("de",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("de-DE");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("de_DE",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("de-AT");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("de_AT",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// french and it's variants
	//
	locale = new nsString("fr");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("fr",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("fr-FR");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("fr_FR",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	locale = new nsString("fr-CA");
	result = posix_locale->GetPlatformLocale(locale,posix_locale_result,9);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetPlatformLocale failed.");
	NS_ASSERTION(strcmp("fr_CA",posix_locale_result)==0,"nsLocaleTest: GetPlatformLocale failed.\n");
	delete locale;

	//
	// delete the XPCOM inteface
	//
	posix_locale->Release();
}

void
posixlocale_reverse_conversion_test()
{
	nsresult			    result;
	nsIPosixLocale*		posix_locale;
	nsString*			    locale;

  //
  // create the locale object
  //
	result = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,
									NULL,
									kIPosixLocaleIID,
									(void**)&posix_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: create interface failed.\n");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: create interface failed\n");

	//
	// test with a simple locale
	//
	locale = new nsString("");
	result = posix_locale->GetXPLocale("en_US",locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetXPLocale failed.\n");
  NS_ASSERTION(*locale=="en-US","nsLocaleTest: GetXPLocale failed.\n");
	delete locale;

	locale = new nsString("");
	result = posix_locale->GetXPLocale("C",locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetXPLocale failed.\n");
  NS_ASSERTION(*locale=="en","nsLocaleTest: GetXPLocale failed.\n");
	delete locale;

	locale = new nsString("");
	result = posix_locale->GetXPLocale("en",locale);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetXPLocale failed.\n");
  NS_ASSERTION(*locale=="en","nsLocaleTest: GetXPLocale failed.\n");
	delete locale;

  posix_locale->Release();

}

void
posixlocale_test_special(void)
{
	nsresult			    result;
  nsILocaleFactory* xp_factory;
  nsILocale*        xp_locale;
  const PRUnichar *lc_name_unichar;
	nsString*			    locale, *result_locale;
  nsString*         lc_message;

  //
  // create the locale objects
  //
	result = nsComponentManager::FindFactory(kLocaleFactoryCID,
										(nsIFactory**)&xp_factory);

	NS_ASSERTION(xp_factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

  //
  // settup strings
  //
  locale = new nsString("en");
  result_locale = new nsString();
  lc_message = new nsString("NSILOCALE_MESSAGES");

	//
	// test GetSystemLocale
	//
  result = xp_factory->GetSystemLocale(&xp_locale);
  NS_ASSERTION(xp_locale!=NULL,"nsLocaleTest: GetSystemLocale failed.\n");
  NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetSystemLocale failed.\n");
  
  result = xp_locale->GetCategory(lc_message->get(),&lc_name_unichar);
  NS_ASSERTION(*result_locale==*locale,"nsLocaleTest: GetSystemLocale failed.\n");
  NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetSystemLocale failed.\n");
  
  result_locale->SetString(lc_name_unichar);
  xp_locale->Release();

  result = xp_factory->GetApplicationLocale(&xp_locale);
  NS_ASSERTION(xp_locale!=NULL,"nsLocaleTest: GetApplicationLocale failed.\n");
  NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetApplicationLocale failed.\n");
  
  
  result = xp_locale->GetCategory(lc_message->get(),&lc_name_unichar);
  NS_ASSERTION(*result_locale==*locale,"nsLocaleTest: GetSystemLocale failed.\n");
  NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: GetSystemLocale failed.\n");
  result_locale->SetString(lc_name_unichar);
  xp_locale->Release();

  //
  // delete strings
  //
  delete locale;
  delete result_locale;
  delete lc_message;

  xp_factory->Release();


}
  
#endif

#ifdef XP_MAC

void
macfactory_create_interface(void)
{
	nsresult			  result;
	nsIFactory*			  factory;
	nsIMacLocale*		  mac_locale;

	result = nsComponentManager::CreateInstance(kMacLocaleFactoryCID,
									NULL,
									kIFactoryIID,
									(void**)&factory);
	NS_ASSERTION(factory!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	factory->Release();

	result = nsComponentManager::CreateInstance(kMacLocaleFactoryCID,
									NULL,
									kIMacLocaleIID,
									(void**)&mac_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: factory_create_interface failed.");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: factory_create_interface failed");

	mac_locale->Release();
}

void
maclocale_test(void)
{
	nsresult			    result;
	nsIMacLocale*			mac_locale;
	nsString*			    locale;
	short              		script_code;

  //
  // create the locale object
  //
	result = nsComponentManager::CreateInstance(kMacLocaleFactoryCID,
									NULL,
									kIMacLocaleIID,
									(void**)&mac_locale);
	NS_ASSERTION(posix_locale!=NULL,"nsLocaleTest: create interface failed.\n");
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: create interface failed\n");

  	//
  	// release the locale interface
  	//
  	mac_locale->Release();
}


#endif

int
main(int argc, char** argv)
{

	//
	// what are we doing?
	//
	printf("Starting nsLocaleTest\n");
	printf("---------------------\n");
	printf("This test has completed successfully if no error messages are printed.\n");

	//
	// run the nsILocaleFactory tests (nsILocale gets tested in the prcoess)
	//
	factory_create_interface();
	factory_get_locale();
	factory_new_locale();

#if defined(XP_WIN) || defined(XP_OS2)

	//
	// run the nsIWin32LocaleFactory tests
	//
	win32factory_create_interface();
	win32locale_test();
	win32locale_conversion_test();
	win32locale_reverse_conversion_test();
	win32_test_special_locales();
#elif defined(XP_UNIX)

  //
  // do the younicks tests
  //
  posixfactory_create_interface();
  posixlocale_test();
  posixlocale_conversion_test();
  posixlocale_reverse_conversion_test();
  posixlocale_test_special();

#elif defined(XP_MAC)

  //
  // do the Mac specific tests
  //
  macfactory_create_interface();
  maclocale_test();
  
#endif

	//
	// we done
	//
	printf("---------------------\n");
	printf("Finished nsLocaleTest\n");

	return 0;
}
