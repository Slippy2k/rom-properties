/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * RpFile_stdio.cpp: Standard file object. (stdio implementation)          *
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

#include "RpMemFile.hpp"

// C includes. (C++ namespace)
#include <cstring>

namespace LibRomData {

/**
 * Open an IRpFile backed by memory.
 * The resulting IRpFile is read-only.
 *
 * NOTE: The memory buffer is NOT copied; it must remain
 * valid as long as this object is still open.
 *
 * @param buf Memory buffer.
 * @param size Size of memory buffer.
 */
RpMemFile::RpMemFile(const void *buf, size_t size)
	: super()
	, m_buf(buf)
	, m_size((int64_t)size)
	, m_pos(0)
	, m_lastError(0)
{
	if (!buf) {
		// No buffer specified.
		m_lastError = EBADF;
	}
}

/**
 * Copy constructor.
 * @param other Other instance.
 */
RpMemFile::RpMemFile(const RpMemFile &other)
	: super()
	, m_buf(other.m_buf)
	, m_size(other.m_size)
	, m_pos(0)
	, m_lastError(0)
{
	if (!m_buf) {
		// No buffer specified.
		m_lastError = EBADF;
	}
}

/**
 * Assignment operator.
 * @param other Other instance.
 * @return This instance.
 */
RpMemFile &RpMemFile::operator=(const RpMemFile &other)
{
	m_buf = other.m_buf;
	m_size = other.m_size;
	m_pos = 0;

	// If there's no buffer specified, that's an error.
	m_lastError = (m_buf ? 0 : EBADF);
	return *this;
}

/**
 * Is the file open?
 * This usually only returns false if an error occurred.
 * @return True if the file is open; false if it isn't.
 */
bool RpMemFile::isOpen(void) const
{
	return (m_buf != nullptr);
}

/**
 * dup() the file handle.
 * Needed because IRpFile* objects are typically
 * pointers, not actual instances of the object.
 * @return dup()'d file, or nullptr on error.
 */
IRpFile *RpMemFile::dup(void)
{
	return new RpMemFile(*this);
}

/**
 * Close the file.
 */
void RpMemFile::close(void)
{
	m_buf = nullptr;
}

/**
 * Read data from the file.
 * @param ptr Output data buffer.
 * @param size Amount of data to read, in bytes.
 * @return Number of bytes read.
 */
size_t RpMemFile::read(void *ptr, size_t size)
{
	if (!m_buf) {
		m_lastError = EBADF;
		return 0;
	}

	// Convert the const void* to a const uint8_t*.
	const uint8_t *buf = reinterpret_cast<const uint8_t*>(m_buf);

	// Check if m_pos + size < m_size.
	size_t sizeToCopy;
	if (m_pos + size < m_size) {
		// We can read the whole thing.
		sizeToCopy = size;
	} else {
		// Not enough data.
		// Copy whatever's left in the buffer.
		sizeToCopy = m_size - m_pos;
	}

	if (sizeToCopy > 0) {
		// Copy the data.
		memcpy(ptr, &buf[m_pos], sizeToCopy);
		m_pos += sizeToCopy;
	}

	return sizeToCopy;
}

/**
 * Get the file position.
 * @return File position, or -1 on error.
 */
int64_t RpMemFile::tell(void)
{
	if (!m_buf) {
		m_lastError = EBADF;
		return 0;
	}

	return (int64_t)m_pos;
}

/**
 * Write data to the file.
 * (NOTE: Not valid for RpMemFile; this will always return 0.)
 * @param ptr Input data buffer.
 * @param size Amount of data to read, in bytes.
 * @return Number of bytes written.
 */
size_t RpMemFile::write(const void *ptr, size_t size)
{
	// Not a valid operation for RpMemFile.
	((void)ptr);
	((void)size);
	m_lastError = EBADF;
	return 0;
}

/**
 * Set the file position.
 * @param pos File position.
 * @return 0 on success; -1 on error.
 */
int RpMemFile::seek(int64_t pos)
{
	if (!m_buf) {
		m_lastError = EBADF;
		return -1;
	}

	// NOTE: m_pos is size_t, since it's referring to
	// a position within a memory buffer.
	if (pos <= 0) {
		m_pos = 0;
	} else if ((size_t)pos >= m_size) {
		m_pos = m_size;
	} else {
		m_pos = (size_t)pos;
	}

	return 0;
}

/**
 * Seek to the beginning of the file.
 */
void RpMemFile::rewind(void)
{
	if (!m_buf) {
		m_lastError = EBADF;
		return;
	}

	m_pos = 0;
}

/**
 * Get the file size.
 * @return File size, or negative on error.
 */
int64_t RpMemFile::fileSize(void)
{
	if (!m_buf) {
		m_lastError = EBADF;
		return -1;
	}

	return m_size;
}

/**
 * Get the last error.
 * @return Last POSIX error, or 0 if no error.
 */
int RpMemFile::lastError(void) const
{
	// TODO: Move up to IRpFile?
	return m_lastError;
}

}