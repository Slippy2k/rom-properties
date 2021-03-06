PROJECT(rom-properties-xfce)

# XFCE (Thunar) frontend for rom-properties.

# Find packages.
FIND_PACKAGE(GLib2 ${REQUIRE_XFCE} 2.26.0)
FIND_PACKAGE(GObject2 ${REQUIRE_XFCE} 2.26.0)
FIND_PACKAGE(GdkPixbuf2 ${REQUIRE_XFCE})
# GTK2 minimum is 2.0.0, so no version check is needed.
FIND_PACKAGE(GTK2 ${REQUIRE_XFCE} COMPONENTS gtk)
IF(GLib2_FOUND AND GObject2_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)
	# Find the ThunarX 2 library.
	FIND_PACKAGE(ThunarX2 ${REQUIRE_XFCE})
	IF(NOT ThunarX2_FOUND)
		# ThunarX 2 was not found.
		# Disable the XFCE frontend.
		SET(BUILD_XFCE OFF CACHE "" INTERNAL FORCE)
	ENDIF(NOT ThunarX2_FOUND)
ELSE(GLib2_FOUND AND GObject2_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)
	# GTK+ 2.x (or required dependencies) were not found.
	# Disable the XFCE frontend.
	SET(BUILD_XFCE OFF CACHE "" INTERNAL FORCE)
ENDIF(GLib2_FOUND AND GObject2_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)

# Sources and headers. (GTK+ common)
STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_SRCS  "${rom-properties-gtk_SRCS}")
STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_SRCS2 "${rom-properties-gtk2_SRCS}")
STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_H     "${rom-properties-gtk_H}")
STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_H2    "${rom-properties-gtk2_H}")

# CPU-specific and optimized sources.
IF(rom-properties-gtk2_SSSE3_SRCS)
	STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_IFUNC_SRCS "${rom-properties-gtk2_IFUNC_SRCS}")
	STRING(REGEX REPLACE "([^;]+)" "../\\1" rom-properties-xfce_SSSE3_SRCS "${rom-properties-gtk2_SSSE3_SRCS}")

	# Disable LTO on the IFUNC files if LTO is known to be broken.
	IF(GCC_5xx_LTO_ISSUES)
		FOREACH(ifunc_file ${rom-properties-xfce_IFUNC_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${ifunc_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " -fno-lto ")
		ENDFOREACH()
	ENDIF(GCC_5xx_LTO_ISSUES)

	IF(MSVC AND NOT CMAKE_CL_64)
		SET(SSSE3_FLAG "/arch:SSE2")
	ELSEIF(NOT MSVC)
		# TODO: Other compilers?
		SET(SSSE3_FLAG "-mssse3")
	ENDIF()
	IF(SSSE3_FLAG)
		FOREACH(ssse3_file ${rom-properties-xfce_SSSE3_SRCS})
			SET_SOURCE_FILES_PROPERTIES(${ssse3_file}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " ${SSSE3_FLAG} ")
		ENDFOREACH(ssse3_file ${rom-properties-xfce_SSSE3_SRCS})
	ENDIF(SSSE3_FLAG)
ENDIF()
UNSET(arch)

# Sources and headers. (XFCE-specific)
SET(rom-properties-xfce_SRCS
	${rom-properties-xfce_SRCS} ${rom-properties-xfce_SRCS2}
	${rom-properties-xfce_IFUNC_SRCS}
	${rom-properties-xfce_SSSE3_SRCS}
	rom-properties-plugin.c
	rom-properties-provider.cpp
	rom-properties-page.cpp
	)
SET(rom-properties-xfce_H
	${rom-properties-xfce_H} ${rom-properties-xfce_H2}
	rom-properties-provider.hpp
	rom-properties-page.hpp
	)

IF(BUILD_XFCE)
	ADD_LIBRARY(rom-properties-xfce MODULE
		${rom-properties-xfce_SRCS}
		)
	SET_TARGET_PROPERTIES(rom-properties-xfce PROPERTIES PREFIX "")
	DO_SPLIT_DEBUG(rom-properties-xfce)
	TARGET_INCLUDE_DIRECTORIES(rom-properties-xfce
		PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
		PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
			$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
			$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
			$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
		)
	TARGET_INCLUDE_DIRECTORIES(rom-properties-xfce PUBLIC ${GTK2_INCLUDE_DIRS})

	TARGET_LINK_LIBRARIES(rom-properties-xfce cachemgr romdata rpbase)
	IF(ENABLE_NLS)
		TARGET_LINK_LIBRARIES(rom-properties-xfce i18n)
	ENDIF(ENABLE_NLS)
	TARGET_LINK_LIBRARIES(rom-properties-xfce Xfce::thunarx-2)
	TARGET_LINK_LIBRARIES(rom-properties-xfce ${GTK2_LIBRARIES} GdkPixbuf2::gdkpixbuf2 GLib2::gobject GLib2::glib)
	# TODO: Move GTK2_DEFINITIONS to TARGET_COMPILE_DEFINITIONS.
	# (Requires removing the "-D" switches.)
	ADD_DEFINITIONS(${GTK2_DEFINITIONS})
	TARGET_COMPILE_DEFINITIONS(rom-properties-xfce
		PRIVATE G_LOG_DOMAIN=\"rom-properties-xfce\"
		)
ENDIF(BUILD_XFCE)

#######################
# Install the plugin. #
#######################

IF(BUILD_XFCE)
	# FIXME: ${ThunarX2_EXTENSIONS_DIR} always uses the system prefix.
	# This should be adjusted to ${CMAKE_INSTALL_PREFIX}.
	INSTALL(TARGETS rom-properties-xfce
		LIBRARY DESTINATION "${ThunarX2_EXTENSIONS_DIR}"
		COMPONENT "plugin"
		)

	# Check if a split debug file should be installed.
	IF(INSTALL_DEBUG)
		# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
		GET_TARGET_PROPERTY(DEBUG_FILENAME rom-properties-xfce PDB)
		IF(DEBUG_FILENAME)
			INSTALL(FILES "${DEBUG_FILENAME}"
				DESTINATION "lib/debug/${ThunarX2_EXTENSIONS_DIR}"
				COMPONENT "debug"
				)
		ENDIF(DEBUG_FILENAME)
	ENDIF(INSTALL_DEBUG)
ENDIF(BUILD_XFCE)
