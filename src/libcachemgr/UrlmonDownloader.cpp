/***************************************************************************
 * ROM Properties Page shell extension. (libcachemgr)                      *
 * UrlmonDownloader.cpp: urlmon-based file downloader.                     *
 *                                                                         *
 * Copyright (c) 2016-2018 by David Korth.                                 *
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
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ***************************************************************************/

#include "stdafx.h"
#include "UrlmonDownloader.hpp"

// libwin32common
#include "libwin32common/RpWin32_sdk.h"
#include "libwin32common/w32time.h"

// librpbase
#include "librpbase/TextFuncs.hpp"
#include "librpbase/file/RpFile.hpp"
using LibRpBase::IRpFile;
using LibRpBase::RpFile;

// C includes. (C++ namespace)
#include <cstring>

// C++ includes.
#include <memory>
#include <string>
using std::unique_ptr;
using std::string;

// Windows includes.
#include <urlmon.h>
#include <wininet.h>

namespace LibCacheMgr {

UrlmonDownloader::UrlmonDownloader()
	: super()
{ }

UrlmonDownloader::UrlmonDownloader(const char *url)
	: super(url)
{ }

UrlmonDownloader::UrlmonDownloader(const string &url)
	: super(url)
{ }

/**
 * Download the file.
 * @return 0 on success; non-zero on error. [TODO: HTTP error codes?]
 */
int UrlmonDownloader::download(void)
{
	// Reference: https://msdn.microsoft.com/en-us/library/ms775122(v=vs.85).aspx
	// TODO: IBindStatusCallback to enforce data size?
	// TODO: Check Content-Length to prevent large files in the first place?
	// TODO: Replace with WinInet?

	// Buffer for cache filename.
	wchar_t szFileName[MAX_PATH];

	HRESULT hr = URLDownloadToCacheFile(nullptr, U82W_s(m_url),
		szFileName, sizeof(szFileName)/sizeof(szFileName[0]),
		0, nullptr /* TODO */);

	if (FAILED(hr)) {
		// Failed to download the file.
		return hr;
	}

	// Open the cached file.
	unique_ptr<IRpFile> file(new RpFile(W2U8(szFileName), RpFile::FM_OPEN_READ));
	if (!file || !file->isOpen()) {
		// Unable to open the file.
		return -1;
	}

	// Get the cache information.
	DWORD cbCacheEntryInfo = 0;
	BOOL bRet = GetUrlCacheEntryInfo(U82W_s(m_url), nullptr, &cbCacheEntryInfo);
	if (bRet) {
		uint8_t *pCacheEntryInfoBuf =
			static_cast<uint8_t*>(malloc(cbCacheEntryInfo));
		if (!pCacheEntryInfoBuf) {
			// ENOMEM
			return -ENOMEM;
		}
		INTERNET_CACHE_ENTRY_INFO *pCacheEntryInfo =
			reinterpret_cast<INTERNET_CACHE_ENTRY_INFO*>(pCacheEntryInfoBuf);
		bRet = GetUrlCacheEntryInfo(U82W_s(m_url), pCacheEntryInfo, &cbCacheEntryInfo);
		if (bRet) {
			// Convert from Win32 FILETIME to Unix time.
			m_mtime = FileTimeToUnixTime(&pCacheEntryInfo->LastModifiedTime);
		}
		free(pCacheEntryInfoBuf);
	}

	// Read the file into the data buffer.
	const int64_t fileSize = file->size();
	m_data.resize(static_cast<size_t>(fileSize));
	size_t ret = file->read(m_data.data(), static_cast<size_t>(fileSize));
	if (ret != fileSize) {
		// Error reading the file.
		m_data.clear();
		return -2;
	}

	// Data loaded.
	// TODO: Delete the cached file?
	return 0;
}

}
