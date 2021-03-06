/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * ITab.hpp: Property sheet base class for rp-config.                      *
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

#ifndef __ROMPROPERTIES_WIN32_CONFIG_ITAB_HPP__
#define __ROMPROPERTIES_WIN32_CONFIG_ITAB_HPP__

#include "librpbase/common.h"
#include "libwin32common/RpWin32_sdk.h"

class ITab
{
	protected:
		ITab() { }
	public:
		virtual ~ITab() = 0;

	private:
		RP_DISABLE_COPY(ITab)

	public:
		/**
		 * Load a resource using the current i18n settings.
		 * @param lpType Resource type.
		 * @param dwResId Resource ID.
		 * @return Pointer to resource, or nullptr if not found.
		 */
		static LPVOID LoadResource_i18n(LPCTSTR lpType, DWORD dwResId);

		/**
		 * Load a dialog resource using the current i18n settings.
		 * @param dwResId Dialog resource ID.
		 * @return Pointer to dialog resource, or nullptr if not found.
		 */
		static inline LPCDLGTEMPLATE LoadDialog_i18n(DWORD dwResId)
		{
			return reinterpret_cast<LPCDLGTEMPLATE>(LoadResource_i18n(RT_DIALOG, dwResId));
		}

		/**
		 * Load a menu resource using the current i18n settings.
		 * @param dwResId Menu resource ID.
		 * @return HMENU created from the menu resource, or nullptr if not found.
		 */
		static inline HMENU LoadMenu_i18n(DWORD dwResId)
		{
			const MENUTEMPLATE *lpcMenuTemplate =
				reinterpret_cast<const MENUTEMPLATE*>(LoadResource_i18n(RT_MENU, dwResId));
			return (lpcMenuTemplate ? LoadMenuIndirect(lpcMenuTemplate) : nullptr);
		}

	public:
		/**
		 * Create the HPROPSHEETPAGE for this tab.
		 *
		 * NOTE: This function can only be called once.
		 * Subsequent invocations will return nullptr.
		 *
		 * @return HPROPSHEETPAGE.
		 */
		virtual HPROPSHEETPAGE getHPropSheetPage(void) = 0;

		/**
		 * Reset the contents of this tab.
		 */
		virtual void reset(void) = 0;

		/**
		 * Load the default configuration.
		 * This does NOT save, and will only emit modified()
		 * if it's different from the current configuration.
		 */
		virtual void loadDefaults(void) = 0;

		/**
		 * Save the contents of this tab.
		 */
		virtual void save(void) = 0;
};

/**
 * Both gcc and MSVC fail to compile unless we provide
 * an empty implementation, even though the function is
 * declared as pure-virtual.
 */
inline ITab::~ITab() { }

#endif /* __ROMPROPERTIES_WIN32_CONFIG_ITAB_HPP__ */
