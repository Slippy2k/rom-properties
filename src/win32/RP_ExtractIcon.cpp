/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * RP_ExtractIcon.cpp: IExtractIcon implementation.                        *
 *                                                                         *
 * Copyright (c) 2016 by David Korth.                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

// Reference: http://www.codeproject.com/Articles/338268/COM-in-C
#include "stdafx.h"
#include "RP_ExtractIcon.hpp"
#include "RegKey.hpp"

// libromdata
#include "libromdata/RomData.hpp"
#include "libromdata/RomDataFactory.hpp"
#include "libromdata/rp_image.hpp"
#include "libromdata/RpFile.hpp"
using namespace LibRomData;

// C includes. (C++ namespace)
#include <cassert>
#include <cstdio>
#include <cstring>

// C++ includes.
#include <string>
using std::wstring;

// CLSID
const CLSID CLSID_RP_ExtractIcon =
	{0xe51bc107, 0xe491, 0x4b29, {0xa6, 0xa3, 0x2a, 0x43, 0x09, 0x25, 0x98, 0x02}};

/** IUnknown **/
// Reference: https://msdn.microsoft.com/en-us/library/office/cc839627.aspx

STDMETHODIMP RP_ExtractIcon::QueryInterface(REFIID riid, LPVOID *ppvObj)
{
	// Always set out parameter to NULL, validating it first.
	if (!ppvObj)
		return E_INVALIDARG;
	*ppvObj = NULL;

	// Check if this interface is supported.
	// NOTE: static_cast<> is required due to vtable shenanigans.
	// Also, IID_IUnknown must always return the same pointer.
	// References:
	// - http://stackoverflow.com/questions/1742848/why-exactly-do-i-need-an-explicit-upcast-when-implementing-queryinterface-in-a
	// - http://stackoverflow.com/a/2812938
	if (riid == IID_IUnknown || riid == IID_IExtractIcon) {
		*ppvObj = static_cast<IExtractIcon*>(this);
	} else if (riid == IID_IPersistFile) {
		*ppvObj = static_cast<IPersistFile*>(this);
	} else {
		// Interface is not supported.
		return E_NOINTERFACE;
	}

	// Make sure we count this reference.
	AddRef();
	return NOERROR;
}

/**
 * Register the COM object.
 * @return ERROR_SUCCESS on success; Win32 error code on error.
 */
LONG RP_ExtractIcon::Register(void)
{
	static const wchar_t description[] = L"ROM Properties Page - Icon Extractor";
	extern const wchar_t RP_ProgID[];

	// Convert the CLSID to a string.
	wchar_t clsid_str[48];	// maybe only 40 is needed?
	LONG lResult = StringFromGUID2(__uuidof(RP_ExtractIcon), clsid_str, sizeof(clsid_str)/sizeof(clsid_str[0]));
	if (lResult <= 0)
		return ERROR_INVALID_PARAMETER;

	// Register the COM object.
	lResult = RegKey::RegisterComObject(__uuidof(RP_ExtractIcon), RP_ProgID, description);
	if (lResult != ERROR_SUCCESS)
		return lResult;

	// Register as an "approved" shell extension.
	lResult = RegKey::RegisterApprovedExtension(__uuidof(RP_ExtractIcon), description);
	if (lResult != ERROR_SUCCESS)
		return lResult;

	// Register as the icon handler for this ProgID.
	// Create/open the ProgID key.
	RegKey hkcr_ProgID(HKEY_CLASSES_ROOT, RP_ProgID, KEY_WRITE, true);
	if (!hkcr_ProgID.isOpen())
		return hkcr_ProgID.lOpenRes();

	// Create/open the "ShellEx" key.
	RegKey hkcr_ShellEx(hkcr_ProgID, L"ShellEx", KEY_WRITE, true);
	if (!hkcr_ShellEx.isOpen())
		return hkcr_ShellEx.lOpenRes();
	// Create/open the "IconHandler" key.
	RegKey hkcr_IconHandler(hkcr_ShellEx, L"IconHandler", KEY_WRITE, true);
	if (!hkcr_IconHandler.isOpen())
		return hkcr_IconHandler.lOpenRes();
	// Set the default value to this CLSID.
	lResult = hkcr_IconHandler.write(nullptr, clsid_str);
	if (lResult != ERROR_SUCCESS)
		return lResult;
	hkcr_IconHandler.close();
	hkcr_ShellEx.close();

	// Create/open the "DefaultIcon" key.
	RegKey hkcr_DefaultIcon(hkcr_ProgID, L"DefaultIcon", KEY_WRITE, true);
	if (!hkcr_DefaultIcon.isOpen())
		return SELFREG_E_CLASS;
	// Set the default value to "%1".
	lResult = hkcr_DefaultIcon.write(nullptr, L"%1");
	if (lResult != ERROR_SUCCESS)
		return SELFREG_E_CLASS;
	hkcr_DefaultIcon.close();
	hkcr_ProgID.close();

	// COM object registered.
	return ERROR_SUCCESS;
}

/**
 * Unregister the COM object.
 * @return ERROR_SUCCESS on success; Win32 error code on error.
 */
LONG RP_ExtractIcon::Unregister(void)
{
	extern const wchar_t RP_ProgID[];

	// Unegister the COM object.
	LONG lResult = RegKey::UnregisterComObject(__uuidof(RP_ExtractIcon), RP_ProgID);
	if (lResult != ERROR_SUCCESS)
		return lResult;

	// TODO
	return ERROR_SUCCESS;
}

/** Image conversion functions. **/

/**
 * Convert an rp_image to HBITMAP.
 * @return image rp_image.
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RP_ExtractIcon::rpToHBITMAP(const rp_image *image)
{
	if (!image || !image->isValid())
		return nullptr;

	// FIXME: Only 256-color images are supported right now.
	// FIXME: Alpha transparency doesn't seem to work in 256-color icons on Windows XP.
	if (image->format() != rp_image::FORMAT_CI8)
		return nullptr;

	// References:
	// - http://stackoverflow.com/questions/2886831/win32-c-c-load-image-from-memory-buffer
	// - http://stackoverflow.com/a/2901465

	// BITMAPINFO with 256-color palette.
	uint8_t bmi[sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD)*256)];
	BITMAPINFOHEADER *bmiHeader = reinterpret_cast<BITMAPINFOHEADER*>(&bmi[0]);
	RGBQUAD *palette = reinterpret_cast<RGBQUAD*>(&bmi[sizeof(BITMAPINFOHEADER)]);

	// Initialize the BITMAPINFOHEADER.
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = image->width();
	bmiHeader->biHeight = -image->height();	// negative for top-down
	bmiHeader->biPlanes = 1;
	bmiHeader->biBitCount = 8;
	bmiHeader->biCompression = BI_RGB;
	bmiHeader->biSizeImage = 0;	// TODO?
	bmiHeader->biXPelsPerMeter = 0;	// TODO
	bmiHeader->biYPelsPerMeter = 0;	// TODO
	bmiHeader->biClrUsed = image->palette_len();
	bmiHeader->biClrImportant = bmiHeader->biClrUsed;	// TODO?

	// Copy the palette from the image.
	memcpy(palette, image->palette(), bmiHeader->biClrUsed * sizeof(uint32_t));

	// Create the bitmap.
	uint8_t *pvBits;
	HBITMAP hBitmap = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bmi),
		DIB_RGB_COLORS, reinterpret_cast<void**>(&pvBits), nullptr, 0);
	if (!hBitmap)
		return nullptr;

	// Copy the data from the rp_image into the bitmap.
	memcpy(pvBits, image->bits(), image->data_len());

	// Return the bitmap.
	return hBitmap;
}

/**
 * Convert an rp_image to an HBITMAP.
 * @return image rp_image.
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RP_ExtractIcon::rpToHBITMAP_mask(const LibRomData::rp_image *image)
{
	if (!image || !image->isValid())
		return nullptr;

	// FIXME: Only 256-color images are supported right now.
	if (image->format() != rp_image::FORMAT_CI8)
		return nullptr;

	// References:
	// - http://stackoverflow.com/questions/2886831/win32-c-c-load-image-from-memory-buffer
	// - http://stackoverflow.com/a/2901465

	/**
	 * Create a monochrome bitmap twice as tall as the image.
	 * Top half is the AND mask; bottom half is the XOR mask.
	 *
	 * Icon truth table:
	 * AND=0, XOR=0: Black
	 * AND=0, XOR=1: White
	 * AND=1, XOR=0: Screen (transparent)
	 * AND=1, XOR=1: Reverse screen (inverted)
	 *
	 * References:
	 * - https://msdn.microsoft.com/en-us/library/windows/desktop/ms648059%28v=vs.85%29.aspx
	 * - https://msdn.microsoft.com/en-us/library/windows/desktop/ms648052%28v=vs.85%29.aspx
	 */
	const int width8 = (image->width() + 8) & ~8;	// Round up to 8px width.
	const int icon_sz = width8 * image->height() / 8;

	BITMAPINFO bmi;
	BITMAPINFOHEADER *bmiHeader = &bmi.bmiHeader;

	// Initialize the BITMAPINFOHEADER.
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = width8;
	bmiHeader->biHeight = image->height();	// FIXME: Top-down isn't working for monochrome...
	bmiHeader->biPlanes = 1;
	bmiHeader->biBitCount = 1;
	bmiHeader->biCompression = BI_RGB;
	bmiHeader->biSizeImage = 0;	// TODO?
	bmiHeader->biXPelsPerMeter = 0;	// TODO
	bmiHeader->biYPelsPerMeter = 0;	// TODO
	bmiHeader->biClrUsed = 0;
	bmiHeader->biClrImportant = 0;

	// Create the bitmap.
	uint8_t *pvBits;
	HBITMAP hBitmap = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bmi),
		DIB_RGB_COLORS, reinterpret_cast<void**>(&pvBits), nullptr, 0);
	if (!hBitmap)
		return nullptr;

	// TODO: Convert the image to a mask.
	// For now, assume the entire image is opaque.
	// NOTE: Windows doesn't support top-down for monochrome icons,
	// so this is vertically flipped.

	// XOR mask: all 0 to disable inversion.
	memset(&pvBits[icon_sz], 0, icon_sz);

	// AND mask: Parse the original image.

	// Get the transparent color index.
	int tr_idx = image->tr_idx();
	if (tr_idx < 0) {
		// tr_idx isn't set.
		// FIXME: This means the image has alpha transparency,
		// so it should be converted to ARGB32.
		DeleteObject(hBitmap);
		return nullptr;
	}

	if (tr_idx >= 0) {
		// Find all pixels matching tr_idx.
		uint8_t *dest = pvBits;
		for (int y = image->height()-1; y >= 0; y--) {
			const uint8_t *src = reinterpret_cast<const uint8_t*>(image->scanLine(y));
			// FIXME: Handle images that aren't a multiple of 8px wide.
			assert(image->width() % 8 == 0);
			for (int x = image->width(); x > 0; x -= 8) {
				uint8_t pxMono = 0;
				for (int px = 8; px > 0; px--, src++) {
					// MSB == left-most pixel.
					pxMono <<= 1;
					pxMono |= (*src != tr_idx);
				}
				*dest++ = pxMono;
			}
		}
	} else {
		// No transparent color.
		memset(pvBits, 0xFF, icon_sz);
	}

	// Return the bitmap.
	return hBitmap;
}

/**
 * Convert an rp_image to HICON.
 * @param image rp_image.
 * @return HICON, or nullptr on error.
 */
HICON RP_ExtractIcon::rpToHICON(const rp_image *image)
{
	if (!image || !image->isValid())
		return nullptr;

	// Convert to HBITMAP first.
	HBITMAP hBitmap = rpToHBITMAP(image);
	if (!hBitmap)
		return nullptr;

	// Convert the image to an icon mask.
	HBITMAP hbmMask = rpToHBITMAP_mask(image);
	if (!hbmMask) {
		DeleteObject(hBitmap);
		return nullptr;
	}

	// Convert to an icon.
	// Reference: http://forums.codeguru.com/showthread.php?441251-CBitmap-to-HICON-or-HICON-from-HBITMAP&p=1661856#post1661856
	ICONINFO ii;
	ii.fIcon = TRUE;
	ii.xHotspot = 0;
	ii.yHotspot = 0;
	ii.hbmColor = hBitmap;
	ii.hbmMask = hbmMask;

	// Create the icon.
	HICON hIcon = CreateIconIndirect(&ii);

	// Delete the original bitmaps and we're done.
	DeleteObject(hBitmap);
	DeleteObject(hbmMask);
	return hIcon;
}

/** IExtractIcon **/
// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/bb761854(v=vs.85).aspx

STDMETHODIMP RP_ExtractIcon::GetIconLocation(UINT uFlags,
	LPTSTR pszIconFile, UINT cchMax, int *piIndex, UINT *pwFlags)
{
	// TODO: If the icon is cached on disk, return a filename.
	// TODO: Enable ASYNC?
	// - https://msdn.microsoft.com/en-us/library/windows/desktop/bb761852(v=vs.85).aspx
	UNUSED(uFlags);
	UNUSED(pszIconFile);
	UNUSED(cchMax);
	UNUSED(piIndex);

#ifndef NDEBUG
	// Debug version. Don't cache icons.
	*pwFlags = GIL_NOTFILENAME | GIL_DONTCACHE;
#else /* !NDEBUG */
	// Release version. Cache icons.
	*pwFlags = GIL_NOTFILENAME;
#endif /* NDEBUG */

	return S_OK;
}

STDMETHODIMP RP_ExtractIcon::Extract(LPCTSTR pszFile, UINT nIconIndex,
	HICON *phiconLarge, HICON *phiconSmall, UINT nIconSize)
{
	// NOTE: pszFile should be nullptr here.
	// TODO: Fail if it's not nullptr?

	// TODO: Use nIconSize?
	UNUSED(pszFile);
	UNUSED(nIconIndex);
	UNUSED(nIconSize);

	// Make sure a filename was set by calling IPersistFile::Load().
	if (m_filename.empty())
		return E_INVALIDARG;

	// Attempt to open the ROM file.
	// TODO: RpQFile wrapper.
	// For now, using RpFile, which is an stdio wrapper.
	IRpFile *file = new RpFile(m_filename, RpFile::FM_OPEN_READ);
	if (!file || !file->isOpen()) {
		delete file;
		return E_FAIL;
	}

	// Get the appropriate RomData class for this ROM.
	RomData *romData = RomDataFactory::getInstance(file);
	delete file;	// file is dup()'d by RomData.

	if (!romData) {
		// ROM is not supported.
		return S_FALSE;
	}

	// ROM is supported. Get the internal icon.
	// TODO: Customize for internal icon, disc/cart scan, etc.?
	HRESULT ret = S_FALSE;
	const rp_image *icon = romData->image(RomData::IMG_INT_ICON);
	if (icon) {
		// Convert the icon to HICON.
		HICON hIcon = rpToHICON(icon);
		if (hIcon != nullptr) {
			// Icon converted.
			if (phiconLarge) {
				*phiconLarge = hIcon;
			} else {
				DeleteObject(hIcon);
			}

			if (phiconSmall) {
				// FIXME: is this valid?
				*phiconSmall = nullptr;
			}

			ret = S_OK;
		}
	}
	delete romData;

	return ret;
}

/** IPersistFile **/
// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/cc144067(v=vs.85).aspx#unknown_28177

STDMETHODIMP RP_ExtractIcon::GetClassID(CLSID *pClassID)
{
	*pClassID = CLSID_RP_ExtractIcon;
	return S_OK;
}

STDMETHODIMP RP_ExtractIcon::IsDirty(void)
{
	return E_NOTIMPL;
}

STDMETHODIMP RP_ExtractIcon::Load(LPCOLESTR pszFileName, DWORD dwMode)
{
	UNUSED(dwMode);	// TODO

	// pszFileName is the file being worked on.
#ifdef RP_UTF16
	m_filename = reinterpret_cast<const rp_char*>(pszFileName);
#else
	// FIXME: Not supported.
	#error RP_ExtractIcon requires UTF-16.
#endif
	return S_OK;
}

STDMETHODIMP RP_ExtractIcon::Save(LPCOLESTR pszFileName, BOOL fRemember)
{
	UNUSED(pszFileName);
	UNUSED(fRemember);
	return E_NOTIMPL;
}

STDMETHODIMP RP_ExtractIcon::SaveCompleted(LPCOLESTR pszFileName)
{
	UNUSED(pszFileName);
	return E_NOTIMPL;
}

STDMETHODIMP RP_ExtractIcon::GetCurFile(LPOLESTR *ppszFileName)
{
	UNUSED(ppszFileName);
	return E_NOTIMPL;
}