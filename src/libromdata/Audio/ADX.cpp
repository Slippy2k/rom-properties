/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * ADX.hpp: CRI ADX audio reader.                                          *
 *                                                                         *
 * Copyright (c) 2018 by David Korth.                                      *
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

#include "ADX.hpp"
#include "librpbase/RomData_p.hpp"

#include "adx_structs.h"

// librpbase
#include "librpbase/common.h"
#include "librpbase/byteswap.h"
#include "librpbase/file/IRpFile.hpp"
#include "libi18n/i18n.h"
using namespace LibRpBase;

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstring>

// C++ includes.
#include <string>
#include <sstream>
using std::ostringstream;
using std::string;

namespace LibRomData {

ROMDATA_IMPL(ADX)

class ADXPrivate : public RomDataPrivate
{
	public:
		ADXPrivate(ADX *q, IRpFile *file);

	private:
		typedef RomDataPrivate super;
		RP_DISABLE_COPY(ADXPrivate)

	public:
		// ADX header.
		// NOTE: **NOT** byteswapped in memory.
		ADX_Header adxHeader;
		const ADX_LoopData *pLoopData;

		/**
		 * Format a sample value as m:ss.cs.
		 * @param sample Sample value.
		 * @param rate Sample rate.
		 * @return m:ss.cs
		 */
		static string formatSampleAsTime(unsigned int sample, unsigned int rate);
};

/** ADXPrivate **/

ADXPrivate::ADXPrivate(ADX *q, IRpFile *file)
	: super(q, file)
	, pLoopData(nullptr)
{
	// Clear the ADX header struct.
	memset(&adxHeader, 0, sizeof(adxHeader));
}

/**
 * Format a sample value as m:ss.cs.
 * @param sample Sample value.
 * @param rate Sample rate.
 * @return m:ss.cs
 */
string ADXPrivate::formatSampleAsTime(unsigned int sample, unsigned int rate)
{
	// TODO: Move to TextFuncs or similar if this will be
	// used by multiple parsers.
	char buf[32];
	unsigned int min, sec, cs;

	const unsigned int cs_frames = (sample % rate);
	if (cs_frames != 0) {
		// Calculate centiseconds.
		cs = static_cast<unsigned int>(((float)cs_frames / (float)rate) * 100);
	} else {
		// No centiseconds.
		cs = 0;
	}

	sec = sample / rate;
	min = sec / 60;
	sec %= 60;

	int len = snprintf(buf, sizeof(buf), "%u:%02u.%02u", min, sec, cs);
	if (len >= (int)sizeof(buf))
		len = (int)sizeof(buf)-1;
	return string(buf, len);
}

/** ADX **/

/**
 * Read a CRI ADX audio file.
 *
 * A ROM image must be opened by the caller. The file handle
 * will be dup()'d and must be kept open in order to load
 * data from the ROM image.
 *
 * To close the file, either delete this object or call close().
 *
 * NOTE: Check isValid() to determine if this is a valid ROM.
 *
 * @param file Open ROM image.
 */
ADX::ADX(IRpFile *file)
	: super(new ADXPrivate(this, file))
{
	RP_D(ADX);
	d->className = "ADX";
	d->fileType = FTYPE_AUDIO_FILE;

	if (!d->file) {
		// Could not dup() the file handle.
		return;
	}

	// Read 4,096 bytes to ensure we have enough
	// data to detect the copyright string.
	uint8_t header[4096];
	d->file->rewind();
	size_t size = d->file->read(header, sizeof(header));
	if (size != sizeof(header))
		return;

	// Check if this file is supported.
	DetectInfo info;
	info.header.addr = 0;
	info.header.size = sizeof(header);
	info.header.pData = header;
	info.ext = nullptr;	// Not needed for ADX.
	info.szFile = 0;	// Not needed for ADX.
	d->isValid = (isRomSupported_static(&info) >= 0);

	if (!d->isValid)
		return;

	// Save the ROM header.
	memcpy(&d->adxHeader, header, sizeof(d->adxHeader));

	// Initialize pLoopData.
	switch (d->adxHeader.loop_data_style) {
		case 3:
			d->pLoopData = &d->adxHeader.loop_03.data;
			break;
		case 4:
			d->pLoopData = &d->adxHeader.loop_04.data;
			break;
		case 5:
		default:
			// No loop data.
			d->pLoopData = nullptr;
			break;
	}
}

/**
 * Is a ROM image supported by this class?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int ADX::isRomSupported_static(const DetectInfo *info)
{
	assert(info != nullptr);
	assert(info->header.pData != nullptr);
	assert(info->header.addr == 0);
	if (!info || !info->header.pData ||
	    info->header.addr != 0 ||
	    info->header.size < sizeof(ADX_Header))
	{
		// Either no detection information was specified,
		// or the header is too small.
		return -1;
	}

	const ADX_Header *const adx_header =
		reinterpret_cast<const ADX_Header*>(info->header.pData);

	// Check the ADX magic number.
	if (adx_header->magic != cpu_to_be16(ADX_MAGIC_NUM)) {
		// Not the ADX magic number.
		return -1;
	}

	// Check the format.
	switch (adx_header->format) {
		case ADX_FORMAT_FIXED_COEFF_ADPCM:
		case ADX_FORMAT_ADX:
		case ADX_FORMAT_ADX_EXP_SCALE:
		case ADX_FORMAT_AHX:
			// Valid format.
			break;

		default:
			// Not a valid format.
			return -1;
	}

	// Check the copyright string.
	unsigned int cpy_offset = be16_to_cpu(adx_header->data_offset);
	if (cpy_offset < 2) {
		// Invalid offset.
		return -1;
	}
	cpy_offset -= 2;
	if (cpy_offset + sizeof(ADX_MAGIC_STR) - 1 > info->header.size) {
		// Out of range.
		return -1;
	}
	if (memcmp(&info->header.pData[cpy_offset], ADX_MAGIC_STR, sizeof(ADX_MAGIC_STR)-1) != 0) {
		// Missing copyright string.
		return -1;
	}

	// This is an ADX file.
	return 0;
}

/**
 * Get the name of the system the loaded ROM is designed for.
 * @param type System name type. (See the SystemName enum.)
 * @return System name, or nullptr if type is invalid.
 */
const char *ADX::systemName(unsigned int type) const
{
	RP_D(const ADX);
	if (!d->isValid || !isSystemNameTypeValid(type))
		return nullptr;

	// ADX has the same name worldwide, so we can
	// ignore the region selection.
	static_assert(SYSNAME_TYPE_MASK == 3,
		"ADX::systemName() array index optimization needs to be updated.");

	static const char *const sysNames[4] = {
		"CRI ADX",
		"ADX",
		"ADX",
		nullptr
	};

	return sysNames[type & SYSNAME_TYPE_MASK];
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions include the leading dot,
 * e.g. ".bin" instead of "bin".
 *
 * NOTE 2: The array and the strings in the array should
 * *not* be freed by the caller.
 *
 * @return NULL-terminated array of all supported file extensions, or nullptr on error.
 */
const char *const *ADX::supportedFileExtensions_static(void)
{
	static const char *const exts[] = {
		".adx",
		".ahx",	// TODO: Is this used for AHX format?

		// TODO: AAX is two ADXes glued together.
		//".aax",

		nullptr
	};
	return exts;
}

/**
 * Load field data.
 * Called by RomData::fields() if the field data hasn't been loaded yet.
 * @return Number of fields read on success; negative POSIX error code on error.
 */
int ADX::loadFieldData(void)
{
	RP_D(ADX);
	if (d->fields->isDataLoaded()) {
		// Field data *has* been loaded...
		return 0;
	} else if (!d->file) {
		// File isn't open.
		return -EBADF;
	} else if (!d->isValid) {
		// Unknown file type.
		return -EIO;
	}

	// ADX header.
	const ADX_Header *const adxHeader = &d->adxHeader;
	d->fields->reserve(7);	// Maximum of 7 fields.

	// Format.
	const char *format;
	switch (adxHeader->format) {
		case ADX_FORMAT_FIXED_COEFF_ADPCM:
			format = C_("ADX|Format", "Fixed Coefficient ADPCM");
			break;
		case ADX_FORMAT_ADX:
			// NOTE: Not translatable.
			format = "ADX";
			break;
		case ADX_FORMAT_ADX_EXP_SCALE:
			format = C_("ADX|Format", "ADX with Exponential Scale");
			break;
		case ADX_FORMAT_AHX:
			// NOTE: Not translatable.
			format = "AHX";
			break;
		default:
			// NOTE: This should not be reachable...
			format = C_("ADX|Format", "Unknown");
			break;
	}
	d->fields->addField_string(C_("ADX", "Format"), format);

	// Sample rate and sample count.
	const uint32_t sample_rate = be32_to_cpu(adxHeader->sample_rate);
	const uint32_t sample_count = be32_to_cpu(adxHeader->sample_count);

	// Sample rate.
	// NOTE: Using ostringstream for localized numeric formatting.
	ostringstream oss;
	oss << sample_rate << " Hz";
	d->fields->addField_string(C_("ADX", "Sample Rate"), oss.str());

	// Length. (non-looping)
	d->fields->addField_string(C_("ADX", "Length"),
		d->formatSampleAsTime(sample_count, sample_rate));

#if 0
	// High-pass cutoff.
	// TODO: What does this value represent?
	// FIXME: Disabling until I figure this out.
	oss.str("");
	oss << be16_to_cpu(adxHeader->high_pass_cutoff) << " Hz";
	d->fields->addField_string(C_("ADX", "High-Pass Cutoff"), oss.str());
#endif

	// Encryption.
	d->fields->addField_string(C_("ADX", "Encrypted"),
		(adxHeader->flags & ADX_FLAG_ENCRYPTED
			? C_("ADX", "Yes")
			: C_("ADX", "No")));

	// Looping.
	const ADX_LoopData *const pLoopData = d->pLoopData;
	const bool isLooping = (pLoopData && pLoopData->loop_flag != 0);
	d->fields->addField_string(C_("ADX", "Looping"),
		(isLooping
			? C_("ADX", "Yes")
			: C_("ADX", "No")));
	if (isLooping) {
		d->fields->addField_string(C_("ADX", "Loop Start"),
			d->formatSampleAsTime(be32_to_cpu(pLoopData->start_sample), sample_rate));
		d->fields->addField_string(C_("ADX", "Loop End"),
			d->formatSampleAsTime(be32_to_cpu(pLoopData->end_sample), sample_rate));
	}

	// Finished reading the field data.
	return (int)d->fields->count();
}

}
