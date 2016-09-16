/***************************************************************************
 * ROM Properties Page shell extension. (libromdata/tests)                 *
 * AesCipherTest.cpp: AesCipher class test.                                *
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

// Google Test
#include "gtest/gtest.h"

// AesCipher
#include "libromdata/crypto/AesCipher.hpp"

// C includes. (C++ namespace)
#include <cstdio>

// C++ includes.
#include <iostream>
#include <string>
using std::endl;
using std::string;

namespace LibRomData { namespace Tests {

class AesCipherTest : public ::testing::Test
{
	protected:
		AesCipherTest() { }

		virtual void SetUp(void) override;
		virtual void TearDown(void) override;

	public:
		AesCipher *m_cipher;

		// AES-256 encryption key.
		// AES-128 and AES-192 use the first
		// 16 and 24 bytes of this key.
		static const uint8_t aes_key[32];

		// IV for AES-CBC.
		static const uint8_t aes_iv[16];

		// Test string.
		static const char test_string[64];

		/**
		 * Compare two byte arrays.
		 * The byte arrays are converted to hexdumps and then
		 * compared using EXPECT_EQ().
		 * @param expected Expected data.
		 * @param actual Actual data.
		 * @param size Size of both arrays.
		 * @param data_type Data type.
		 */
		void CompareByteArrays(
			const uint8_t *expected,
			const uint8_t *actual,
			unsigned int size,
			const char *data_type);
};

// AES-256 encryption key.
// AES-128 and AES-192 use the first
// 16 and 24 bytes of this key.
const uint8_t AesCipherTest::aes_key[32] = {
	0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
	0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,
	0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE,
	0xEF,0xCD,0xAB,0x89,0x67,0x45,0x23,0x01
};

// IV for AES-CBC.
const uint8_t AesCipherTest::aes_iv[16] = {
	0xD9,0x83,0xC2,0xA0,0x1C,0xFA,0x8B,0x88,
	0x3A,0xE3,0xA4,0xBD,0x70,0x1F,0xC1,0x0B
};

// Test string.
const char AesCipherTest::test_string[64] =
	"This is a test string. It should be encrypted and decrypted! =P";

/**
 * Compare two byte arrays.
 * The byte arrays are converted to hexdumps and then
 * compared using EXPECT_EQ().
 * @param expected Expected data.
 * @param actual Actual data.
 * @param size Size of both arrays.
 * @param data_type Data type.
 */
void AesCipherTest::CompareByteArrays(
	const uint8_t *expected,
	const uint8_t *actual,
	unsigned int size,
	const char *data_type)
{
	// Output format: (assume ~64 bytes per line)
	// 0000: 01 23 45 67 89 AB CD EF  01 23 45 67 89 AB CD EF
	const unsigned int bufSize = ((size / 16) + !!(size % 16)) * 64;
	char printf_buf[16];
	string s_expected, s_actual;
	s_expected.reserve(bufSize);
	s_actual.reserve(bufSize);

	// TODO: Use stringstream instead?
	const uint8_t *pE = expected, *pA = actual;
	for (unsigned int i = 0; i < size; i++, pE++, pA++) {
		if (i % 16 == 0) {
			// New line.
			if (i > 0) {
				// Append newlines.
				s_expected += '\n';
				s_actual += '\n';
			}

			snprintf(printf_buf, sizeof(printf_buf), "%04X: ", i);
			s_expected += printf_buf;
			s_actual += printf_buf;
		}

		// Print the byte.
		snprintf(printf_buf, sizeof(printf_buf), "%02X", *pE);
		s_expected += printf_buf;
		snprintf(printf_buf, sizeof(printf_buf), "%02X", *pA);
		s_actual += printf_buf;

		if (i % 16 == 7) {
			s_expected += "  ";
			s_actual += "  ";
		} else if (i % 16  < 15) {
			s_expected += ' ';
			s_actual += ' ';
		}
	}

	// Compare the byte arrays, and
	// print the strings on failure.
	EXPECT_EQ(0, memcmp(expected, actual, size)) <<
		"Expected " << data_type << ":" << endl << s_expected << endl <<
		"Actual " << data_type << ":" << endl << s_actual << endl;
}

/**
 * SetUp() function.
 * Run before each test.
 */
void AesCipherTest::SetUp(void)
{
	m_cipher = new AesCipher();
	ASSERT_TRUE(m_cipher->isInit());
}

/**
 * TearDown() function.
 * Run after each test.
 */
void AesCipherTest::TearDown(void)
{
	delete m_cipher;
	m_cipher = nullptr;
}

/**
 * AES-128 ECB decryption test.
 */
TEST_F(AesCipherTest, aes128ecb_decrypt)
{
	// AES-128 ECB ciphertext.
	// This is test_string[] encrypted using the first
	// 16 bytes of aes_key[].
	uint8_t buf[64] = {
		0xC7,0xE9,0x48,0x3D,0xF6,0x9F,0x50,0xFA,
		0x4A,0xF5,0x7E,0x62,0x5F,0x48,0xE8,0xC9,
		0x7C,0x01,0x3E,0xE8,0x2A,0x9D,0x25,0x15,
		0x64,0xFA,0x59,0xA6,0xCF,0xBD,0x85,0xBA,
		0x46,0x5F,0x61,0x36,0x09,0x73,0xF3,0x0C,
		0x46,0x7B,0x84,0x60,0x40,0xB2,0xC8,0x20,
		0xCC,0xB2,0xCD,0xA8,0xBE,0xC2,0x6A,0xF3,
		0x7F,0x4A,0x14,0x41,0xC9,0xA3,0x45,0x03
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 16));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_ECB));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

/**
 * AES-192 ECB decryption test.
 */
TEST_F(AesCipherTest, aes192ecb_decrypt)
{
	// AES-192 ECB ciphertext.
	// This is test_string[] encrypted using the first
	// 24 bytes of aes_key[].
	uint8_t buf[64] = {
		0xEC,0x90,0x1B,0x32,0x20,0xC2,0xD0,0x78,
		0xA0,0x43,0xA6,0xE5,0x13,0xE1,0xF6,0x6C,
		0xE6,0x25,0x4A,0x4D,0x8C,0xF1,0x02,0xE8,
		0x63,0x40,0xFF,0x94,0x00,0x62,0x7B,0x4E,
		0xEF,0x73,0x76,0xD5,0x44,0xE5,0x96,0x94,
		0x26,0x78,0xF5,0x6D,0x96,0x20,0x6B,0xB1,
		0x78,0xC9,0x23,0x04,0xA0,0x03,0x77,0xC6,
		0xC2,0x69,0x8E,0xE5,0xDE,0xBB,0x73,0x27
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 24));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_ECB));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

/**
 * AES-256 ECB decryption test.
 */
TEST_F(AesCipherTest, aes256ecb_decrypt)
{
	// AES-256 ECB ciphertext.
	// This is test_string[] encrypted using all
	// 32 bytes of aes_key[].
	uint8_t buf[64] = {
		0xF0,0x70,0x5F,0xFC,0x15,0x55,0x5A,0x7E,
		0x7C,0xAF,0xDA,0x82,0x12,0x6A,0x69,0x5E,
		0x20,0x55,0xD1,0x8E,0xC3,0x53,0xD1,0xF7,
		0xB3,0xC0,0xC5,0xFD,0x17,0x2E,0x39,0x30,
		0x4A,0x4A,0x68,0x84,0x6F,0xF0,0xE9,0xB2,
		0x0D,0x1C,0xE8,0xD0,0xF7,0x8B,0x22,0xEF,
		0x70,0xFA,0x81,0x71,0x5D,0x6B,0x9A,0x40,
		0x81,0xFC,0xB9,0xF5,0xBB,0x4F,0x3D,0x7C
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 32));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_ECB));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

/**
 * AES-128 CBC decryption test.
 */
TEST_F(AesCipherTest, aes128cbc_decrypt)
{
	// AES-128 CBC ciphertext.
	// This is test_string[] encrypted using the first
	// 16 bytes of aes_key[] and aes_iv[] as the IV.
	uint8_t buf[64] = {
		0xD4,0x71,0xDF,0xDE,0x04,0xE7,0x0A,0x67,
		0x2B,0xD4,0x82,0x4B,0xD1,0x10,0x71,0x62,
		0xE9,0x09,0x49,0x5D,0x3D,0xAE,0x4C,0xBC,
		0x0C,0x6F,0x3A,0xBE,0x32,0x78,0x39,0xF3,
		0x33,0x07,0x94,0xAF,0xFE,0xF0,0xB4,0xF3,
		0xA5,0x3E,0xFB,0x22,0xA8,0x33,0xFA,0x02,
		0xB8,0x73,0x44,0xF5,0xDC,0x78,0xDA,0x9A,
		0xD4,0xB5,0x8C,0x17,0xEF,0x59,0xB2,0xBF
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 16));
	EXPECT_EQ(0, m_cipher->setIV(aes_iv, 16));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_CBC));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

/**
 * AES-192 CBC decryption test.
 */
TEST_F(AesCipherTest, aes192cbc_decrypt)
{
	// AES-192 CBC ciphertext.
	// This is test_string[] encrypted using the first
	// 24 bytes of aes_key[] and aes_iv[] as the IV.
	uint8_t buf[64] = {
		0x41,0x28,0x37,0x74,0x5B,0x88,0x08,0xDA,
		0xCC,0xC4,0x14,0xF0,0x2F,0x8D,0xF4,0x6A,
		0xBE,0xE6,0xF0,0xB7,0xE1,0x9E,0xCB,0x00,
		0x7A,0x86,0xC0,0x76,0xF0,0xA7,0x10,0x62,
		0xE4,0x5C,0x04,0xBA,0xD6,0x52,0xA8,0x32,
		0x15,0x93,0x50,0xD3,0x56,0x25,0xBB,0x92,
		0xA8,0xA0,0x64,0x26,0xA6,0xE3,0x68,0x00,
		0xBD,0x99,0x47,0x4B,0x83,0xC3,0xAD,0xF4
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 24));
	EXPECT_EQ(0, m_cipher->setIV(aes_iv, 16));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_CBC));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

/**
 * AES-256 CBC decryption test.
 */
TEST_F(AesCipherTest, aes256cbc_decrypt)
{
	// AES-256 CBC ciphertext.
	// This is test_string[] encrypted using all
	// 32 bytes of aes_key[] and aes_iv[] as the IV.
	uint8_t buf[64] = {
		0x70,0x96,0xEB,0xE1,0x4B,0xC3,0xCA,0xD4,
		0xF3,0x85,0x55,0x42,0xF6,0x98,0xB9,0x19,
		0x14,0xB9,0x61,0xA3,0xF5,0xB5,0x3D,0x44,
		0x74,0xA5,0x14,0x0C,0x44,0x07,0xF6,0x78,
		0x5F,0x36,0x5A,0x3C,0xDD,0x75,0xD4,0x90,
		0x7B,0x20,0xFE,0x7F,0x6B,0x25,0x69,0xCD,
		0xAD,0x72,0xBA,0x39,0x5E,0x19,0xF2,0xBF,
		0xCE,0x35,0xAF,0x78,0x8A,0x0B,0x38,0xDB
	};

	EXPECT_EQ(0, m_cipher->setKey(aes_key, 32));
	EXPECT_EQ(0, m_cipher->setIV(aes_iv, 16));
	EXPECT_EQ(0, m_cipher->setChainingMode(AesCipher::CM_CBC));
	EXPECT_EQ(sizeof(buf), m_cipher->decrypt(buf, sizeof(buf)));

	// Compare the buffer to the known plaintext.
	CompareByteArrays(reinterpret_cast<const uint8_t*>(test_string),
			buf, sizeof(buf), "plaintext data");
}

} }

/**
 * Test suite main function.
 */
int main(int argc, char *argv[])
{
	fprintf(stderr, "LibRomData test suite: AesCipher tests.\n\n");
	fflush(nullptr);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}