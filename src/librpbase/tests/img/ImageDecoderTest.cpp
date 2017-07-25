/***************************************************************************
 * ROM Properties Page shell extension. (librpbase/tests)                  *
 * ImageDecoderTest.cpp: ImageDecoder class test.                          *
 *                                                                         *
 * Copyright (c) 2016-2017 by David Korth.                                 *
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

// Google Test
#include "gtest/gtest.h"

// zlib and libpng
#include <zlib.h>
#include "png_chunks.h"

#ifdef HAVE_PNG
#include <png.h>
#endif /* HAVE_PNG */

// gzclose_r() and gzclose_w() were introduced in zlib-1.2.4.
#if (ZLIB_VER_MAJOR > 1) || \
    (ZLIB_VER_MAJOR == 1 && ZLIB_VER_MINOR > 2) || \
    (ZLIB_VER_MAJOR == 1 && ZLIB_VER_MINOR == 2 && ZLIB_VER_REVISION >= 4)
// zlib-1.2.4 or later
#else
#define gzclose_r(file) gzclose(file)
#define gzclose_w(file) gzclose(file)
#endif

// librpbase
#include "common.h"
#include "byteswap.h"
#include "uvector.h"
#include "TextFuncs.hpp"

#include "file/RpFile.hpp"
#include "file/RpMemFile.hpp"
#include "file/FileSystem.hpp"
#include "img/rp_image.hpp"
#include "img/RpImageLoader.hpp"

// TODO: Separate out the actual DDS texture loader
// from the RomData subclass?
#include "DirectDrawSurface.hpp"
using LibRomData::DirectDrawSurface;

// DirectDraw Surface structs.
#include "dds_structs.h"

// C includes.
#include <stdint.h>
#include <stdlib.h>

// C includes. (C++ namespace)
#include <cctype>
#include <cstring>

// C++ includes.
#include <memory>
#include <string>
using std::string;
using std::unique_ptr;

namespace LibRpBase { namespace Tests {

struct ImageDecoderTest_mode
{
	rp_string dds_gz_filename;	// Source texture to test.
	rp_string png_filename;		// PNG image for comparison.

	ImageDecoderTest_mode(
		const rp_char *dds_gz_filename,
		const rp_char *png_filename)
		: dds_gz_filename(dds_gz_filename)
		, png_filename(png_filename)
	{ }

	// May be required for MSVC 2010?
	ImageDecoderTest_mode(const ImageDecoderTest_mode &other)
		: dds_gz_filename(other.dds_gz_filename)
		, png_filename(other.png_filename)
	{ }

	// Required for MSVC 2010.
	ImageDecoderTest_mode &operator=(const ImageDecoderTest_mode &other)
	{
		dds_gz_filename = other.dds_gz_filename;
		png_filename = other.png_filename;
		return *this;
	}
};

// Maximum file size for images.
static const int64_t MAX_DDS_IMAGE_FILESIZE = 2*1024*1024;
static const int64_t MAX_PNG_IMAGE_FILESIZE =    512*1024;

class ImageDecoderTest : public ::testing::TestWithParam<ImageDecoderTest_mode>
{
	protected:
		ImageDecoderTest()
			: ::testing::TestWithParam<ImageDecoderTest_mode>()
			, m_gzDds(nullptr)
			, m_f_dds(nullptr)
			, m_romData(nullptr)
		{ }

		virtual void SetUp(void) override final;
		virtual void TearDown(void) override final;

	public:
		/**
		 * Compare two rp_image objects.
		 * If either rp_image is CI8, a copy of the image
		 * will be created in ARGB32 for comparison purposes.
		 * @param pImgExpected	[in] Expected image data.
		 * @param pImgActual	[in] Actual image data.
		 */
		static void Compare_RpImage(
			const rp_image *pImgExpected,
			const rp_image *pImgActual);

	public:
		// Image buffers.
		ao::uvector<uint8_t> m_dds_buf;
		ao::uvector<uint8_t> m_png_buf;

		// gzip file handle for .dds.gz.
		// Placed here so it can be freed by TearDown() if necessary.
		gzFile m_gzDds;

		// RomData class pointer for .dds.gz.
		// Placed here so it can be freed by TearDown() if necessary.
		// The underlying RpMemFile is here as well, since we can't
		// delete it before deleting the RomData object.
		RpMemFile *m_f_dds;
		RomData *m_romData;

	public:
		/** Test case parameters. **/

		/**
		 * Test case suffix generator.
		 * @param info Test parameter information.
		 * @return Test case suffix.
		 */
		static string test_case_suffix_generator(const ::testing::TestParamInfo<ImageDecoderTest_mode> &info);
};

/**
 * Formatting function for ImageDecoderTest.
 */
inline ::std::ostream& operator<<(::std::ostream& os, const ImageDecoderTest_mode& mode)
{
	return os << rp_string_to_utf8(mode.dds_gz_filename);
};

/**
 * SetUp() function.
 * Run before each test.
 */
void ImageDecoderTest::SetUp(void)
{
	if (::testing::UnitTest::GetInstance()->current_test_info()->value_param() == nullptr) {
		// Not a parameterized test.
		return;
	}

	// Parameterized test.
	const ImageDecoderTest_mode &mode = GetParam();

	// Open the gzipped DDS texture file being tested.
	rp_string path = _RP("ImageDecoder_data");
	path += _RP_CHR(DIR_SEP_CHR);
	path += mode.dds_gz_filename;
	m_gzDds = gzopen(rp_string_to_utf8(path).c_str(), "rb");
	ASSERT_TRUE(m_gzDds != nullptr) << "gzopen() failed to open the DDS file:"
		<< rp_string_to_utf8(mode.dds_gz_filename);

	// Get the decompressed file size.
	// gzseek() does not support SEEK_END.
	// Read through the file until we hit an EOF.
	// NOTE: We could optimize this by reading the uncompressed
	// file size if gzdirect() == 1, but this is a test case,
	// so it doesn't really matter.
	uint8_t buf[4096];
	uint32_t ddsSize = 0;
	while (!gzeof(m_gzDds)) {
		int sz_read = gzread(m_gzDds, buf, sizeof(buf));
		ASSERT_NE(sz_read, -1) << "gzread() failed.";
		ddsSize += sz_read;
	}
	gzrewind(m_gzDds);

	ASSERT_GT(ddsSize, 4+sizeof(DDS_HEADER))
		<< "DDS test image is too small.";
	ASSERT_LE(ddsSize, MAX_DDS_IMAGE_FILESIZE)
		<< "DDS test image is too big.";

	// Read the DDS image into memory.
	m_dds_buf.resize(ddsSize);
	ASSERT_EQ((size_t)ddsSize, m_dds_buf.size());
	int sz = gzread(m_gzDds, m_dds_buf.data(), ddsSize);
	gzclose_r(m_gzDds);
	m_gzDds = nullptr;

	ASSERT_EQ(ddsSize, (uint32_t)sz) << "Error loading DDS image file: "
		<< rp_string_to_utf8(mode.dds_gz_filename);

	// Open the PNG image file being tested.
	path.resize(18);	// Back to "ImageDecoder_data/".
	path += mode.png_filename;
	unique_ptr<IRpFile> file(new RpFile(path, RpFile::FM_OPEN_READ));
	ASSERT_TRUE(file.get() != nullptr);
	ASSERT_TRUE(file->isOpen());

	// Maximum image size.
	ASSERT_LE(file->size(), MAX_PNG_IMAGE_FILESIZE) << "PNG test image is too big.";

	// Read the PNG image into memory.
	const size_t pngSize = (size_t)file->size();
	m_png_buf.resize(pngSize);
	ASSERT_EQ(pngSize, m_png_buf.size());
	size_t readSize = file->read(m_png_buf.data(), pngSize);
	ASSERT_EQ(pngSize, readSize) << "Error loading PNG image file: "
		<< rp_string_to_utf8(mode.png_filename);
}

/**
 * TearDown() function.
 * Run after each test.
 */
void ImageDecoderTest::TearDown(void)
{
	if (m_romData) {
		m_romData->unref();
		m_romData = nullptr;
	}

	delete m_f_dds;
	m_f_dds = nullptr;

	if (m_gzDds) {
		gzclose_r(m_gzDds);
		m_gzDds = nullptr;
	}
}

/**
 * Compare two rp_image objects.
 * If either rp_image is CI8, a copy of the image
 * will be created in ARGB32 for comparison purposes.
 * @param pImgExpected	[in] Expected image data.
 * @param pImgActual	[in] Actual image data.
 */
void ImageDecoderTest::Compare_RpImage(
	const rp_image *pImgExpected,
	const rp_image *pImgActual)
{
	// Make sure we have two ARGB32 images with equal sizes.
	ASSERT_TRUE(pImgExpected->isValid()) << "pImgExpected is not valid.";
	ASSERT_TRUE(pImgActual->isValid())   << "pImgActual is not valid.";
	ASSERT_EQ(pImgExpected->width(),  pImgActual->width())  << "Image sizes don't match.";
	ASSERT_EQ(pImgExpected->height(), pImgActual->height()) << "Image sizes don't match.";

	// Ensure we delete temporary images if they're created.
	unique_ptr<rp_image> tmpImg_expected;
	unique_ptr<rp_image> tmpImg_actual;

	switch (pImgExpected->format()) {
		case rp_image::FORMAT_ARGB32:
			// No conversion needed.
			break;

		case rp_image::FORMAT_CI8:
			// Convert to ARGB32.
			tmpImg_expected.reset(pImgExpected->dup_ARGB32());
			ASSERT_TRUE(tmpImg_expected != nullptr);
			ASSERT_TRUE(tmpImg_expected->isValid());
			pImgExpected = tmpImg_expected.get();
			break;

		default:
			ASSERT_TRUE(false) << "pImgExpected: Invalid pixel format for this test.";
			break;
	}

	switch (pImgActual->format()) {
		case rp_image::FORMAT_ARGB32:
			// No conversion needed.
			break;

		case rp_image::FORMAT_CI8:
			// Convert to ARGB32.
			tmpImg_actual.reset(pImgActual->dup_ARGB32());
			ASSERT_TRUE(tmpImg_actual != nullptr);
			ASSERT_TRUE(tmpImg_actual->isValid());
			pImgActual = tmpImg_actual.get();
			break;

		default:
			ASSERT_TRUE(false) << "pImgActual: Invalid pixel format for this test.";
			break;
	}

	// Compare the two images.
	// TODO: rp_image::operator==()?
	const uint8_t *pBitsExpected = static_cast<const uint8_t*>(pImgExpected->bits());
	const uint8_t *pBitsActual   = static_cast<const uint8_t*>(pImgActual->bits());
	const int row_bytes = pImgExpected->width() * sizeof(uint32_t);
	const int stride_expected = pImgExpected->stride();
	const int stride_actual   = pImgActual->stride();
	for (unsigned int y = (unsigned int)pImgExpected->height(); y > 0; y--) {
		ASSERT_EQ(0, memcmp(pBitsExpected, pBitsActual, row_bytes)) <<
			"Decoded image does not match the expected PNG image.";
		pBitsExpected += stride_expected;
		pBitsActual   += stride_actual;
	}
}

/**
 * Run an ImageDecoder test.
 */
TEST_P(ImageDecoderTest, decodeTest)
{
	// Load the PNG image.
	unique_ptr<RpMemFile> f_png(new RpMemFile(m_png_buf.data(), m_png_buf.size()));
	ASSERT_TRUE(f_png->isOpen()) << "Could not create RpMemFile for the PNG image.";
	unique_ptr<rp_image> img_png(RpImageLoader::load(f_png.get()));
	ASSERT_TRUE(img_png != nullptr) << "Could not load the PNG image as rp_image.";
	ASSERT_TRUE(img_png->isValid()) << "Could not load the PNG image as rp_image.";

	// Attempt to decode the DDS image.
	// TODO: Separate out the actual DDS texture loader
	// from the RomData subclass?
	m_f_dds = new RpMemFile(m_dds_buf.data(), m_dds_buf.size());
	ASSERT_TRUE(m_f_dds->isOpen()) << "Could not create RpMemFile for the DDS image.";
	m_romData = new DirectDrawSurface(m_f_dds);
	ASSERT_TRUE(m_romData->isValid()) << "Could not load the DDS image.";
	ASSERT_TRUE(m_romData->isOpen()) << "Could not load the DDS image.";

	// Get the DDS image as an rp_image.
	const rp_image *const img_dds = m_romData->image(RomData::IMG_INT_IMAGE);
	ASSERT_TRUE(img_dds != nullptr) << "Could not load the DDS image as rp_image.";

	// Compare the image data.
	ASSERT_NO_FATAL_FAILURE(Compare_RpImage(img_png.get(), img_dds));
}

/**
 * Test case suffix generator.
 * @param info Test parameter information.
 * @return Test case suffix.
 */
string ImageDecoderTest::test_case_suffix_generator(const ::testing::TestParamInfo<ImageDecoderTest_mode> &info)
{
	rp_string suffix = info.param.dds_gz_filename;

	// Replace all non-alphanumeric characters with '_'.
	// See gtest-param-util.h::IsValidParamName().
	for (int i = (int)suffix.size()-1; i >= 0; i--) {
		rp_char chr = suffix[i];
		if (!isalnum(chr) && chr != '_') {
			suffix[i] = '_';
		}
	}

	// TODO: rp_string_to_ascii()?
	return rp_string_to_utf8(suffix);
}

// Test cases.

// DirectDrawSurface tests.
INSTANTIATE_TEST_CASE_P(DDS, ImageDecoderTest,
	::testing::Values(
		ImageDecoderTest_mode(
			_RP("dxt1-rgb.dds.gz"),
			_RP("dxt1-rgb.png")),
		ImageDecoderTest_mode(
			_RP("dxt2-rgb.dds.gz"),
			_RP("dxt2-rgb.png")),
		ImageDecoderTest_mode(
			_RP("dxt2-argb.dds.gz"),
			_RP("dxt2-argb.png")),
		ImageDecoderTest_mode(
			_RP("dxt3-rgb.dds.gz"),
			_RP("dxt3-rgb.png")),
		ImageDecoderTest_mode(
			_RP("dxt3-argb.dds.gz"),
			_RP("dxt3-argb.png")),
		ImageDecoderTest_mode(
			_RP("dxt4-rgb.dds.gz"),
			_RP("dxt4-rgb.png")),
		ImageDecoderTest_mode(
			_RP("dxt4-argb.dds.gz"),
			_RP("dxt4-argb.png")),
		ImageDecoderTest_mode(
			_RP("dxt5-rgb.dds.gz"),
			_RP("dxt5-rgb.png")),
		ImageDecoderTest_mode(
			_RP("dxt5-argb.dds.gz"),
			_RP("dxt5-argb.png")))
	, ImageDecoderTest::test_case_suffix_generator);

} }

/**
 * Test suite main function.
 * Called by gtest_init.c.
 */
extern "C" int gtest_main(int argc, char *argv[])
{
	fprintf(stderr, "LibRpBase test suite: RpImageLoader tests.\n\n");
	fflush(nullptr);

	// Make sure the CRC32 table is initialized.
	get_crc_table();

	// coverity[fun_call_w_exception]: uncaught exceptions cause nonzero exit anyway, so don't warn.
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
