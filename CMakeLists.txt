CMAKE_MINIMUM_REQUIRED( VERSION 3.11 )

MARK_AS_ADVANCED(CMAKE_BACKWARDS_COMPATIBILITY)
# allow more human readable "if then else" constructs
SET( CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE )

PROJECT(Descent3)

IF (UNIX)
SET (D3_GAMEDIR "~/Descent3/")

  SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wno-write-strings -m32")
  SET(CMAKE_CXX_COMPILER "g++")
  SET(CMAKE_CXX_FLAGS "-O0 -g -Wno-write-strings -Wno-multichar -m32")
  SET(CMAKE_C_FLAGS "-O0 -g -m32")
  SET(CMAKE_C_COMPILER "gcc")
  SET(CMAKE_FIND_LIBRARY_PREFIXES "lib")
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  FIND_PACKAGE( SDL REQUIRED )
  FIND_PACKAGE( SDL_image REQUIRED )
MESSAGE( "SDL Include Dir is " ${SDL_INCLUDE_DIR} )
ENDIF()

IF (UNIX AND NOT APPLE)
  MESSAGE("Building for Linux")

  ADD_DEFINITIONS( -D_DEBUG -D__LINUX__ -DLINUX -D_MAX_PATH=260 -D_MAX_FNAME=256 -D_REENRANT -DMONO -D__32BIT__ -DHAVEALLOCA_H -D_USE_OGL_ACTIVE_TEXTURES)
  SET(PLATFORM_INCLUDES "lib/linux" ${SDL_INCLUDE_DIR} )
ENDIF()

IF (APPLE)
	SET(D3_GAMEDIR "~/Descent3X")	
  MESSAGE("Building for MAC OSX")
  ADD_DEFINITIONS( -DMONO -D_DEBUG -D__LINUX__ -DLINUX -D_MAX_PATH=260 -D_MAX_FNAME=256 -D_REENRANT -DMACOSX=1 -D_USE_OGL_ACTIVE_TEXTURES)
  SET(PLATFORM_INCLUDES "lib/linux" ${SDL_INCLUDE_DIR} "/usr/X11/include" )
ENDIF()

IF (WIN32)
	SET(D3_GAMEDIR "c:/games/Descent3/")	
	set (CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "lib/win" "lib/win/directx" "thirdparty/OpenAL/libs/Win32") 
	SET(CMAKE_CXX_FLAGS_DEBUG "/Od /EHsc /RTC1 /MTd /MP /W3 /nologo /c /ZI /TP /errorReport:prompt")
	SET(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /FD /EHsc /MT /W3 /nologo /c /Zi /TP /errorReport:prompt")
	
	
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /SUBSYSTEM:WINDOWS /NODEFAULTLIB:LIBC")
	SET(CMAKE_MODULE_LINKER_FLAGS "/SAFESEH:NO /SUBSYSTEM:WINDOWS /NODEFAULTLIB:LIBC")
	ADD_DEFINITIONS (-DIS_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DMONO -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)

	SET(PLATFORM_INCLUDES "lib/win/directx" "lib/win" "thirdparty/OpenAL/include" )
	
	SET(CMAKE_FIND_LIBRARY_PREFIXES "")
	SET(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
	
	FIND_LIBRARY(DSOUND_LIBRARY NAMES dsound "${CMAKE_SOURCE_DIR}/lib/win" "${CMAKE_SOURCE_DIR}/lib/win/directx")
	FIND_LIBRARY(DINPUT_LIBRARY NAMES dinput "${CMAKE_SOURCE_DIR}/lib/win" "${CMAKE_SOURCE_DIR}/lib/win/directx")
	FIND_LIBRARY(DXGUID_LIBRARY NAMES dxguid "${CMAKE_SOURCE_DIR}/lib/win" "${CMAKE_SOURCE_DIR}/lib/win/directx")
	FIND_LIBRARY(DDRAW_LIBRARY NAMES ddraw "${CMAKE_SOURCE_DIR}/lib/win" "${CMAKE_SOURCE_DIR}/lib/win/directx")
	#TODO: May need to find OpenAL64 eventually
	FIND_LIBRARY(OPENAL_LIBRARY NAMES openal32 "${CMAKE_SOURCE_DIR}/thirdparty/OpenAL/libs" "${CMAKE_SOURCE_DIR}/thirdparty/OpenAL/libs/Win32")
ENDIF ()

MESSAGE("Install will copy files to ${D3_GAMEDIR}")

include_directories("lib" "Descent3" ${PLATFORM_INCLUDES})

ADD_SUBDIRECTORY (AudioEncode)
ADD_SUBDIRECTORY (bitmap)
ADD_SUBDIRECTORY (cfile)
ADD_SUBDIRECTORY (d3music)

IF (WIN32)
ADD_SUBDIRECTORY (dd_grwin32)
ADD_SUBDIRECTORY (dd_vidwin32)
ADD_SUBDIRECTORY (ddio_win)
ADD_SUBDIRECTORY (win32)
ADD_SUBDIRECTORY (dd_sndlib)
ENDIF()

IF (UNIX)
ADD_SUBDIRECTORY(linux)
ADD_SUBDIRECTORY(dd_lnxsound)
ADD_SUBDIRECTORY(ddvid_lnx)
ADD_SUBDIRECTORY(lnxcontroller)
ADD_SUBDIRECTORY(ddio_lnx)
ENDIF()

ADD_SUBDIRECTORY (ddio_common)
ADD_SUBDIRECTORY (fix)
ADD_SUBDIRECTORY (grtext)
ADD_SUBDIRECTORY (manage)
ADD_SUBDIRECTORY (mem)
ADD_SUBDIRECTORY (misc)
ADD_SUBDIRECTORY (model)
ADD_SUBDIRECTORY (module)
ADD_SUBDIRECTORY (movie)
ADD_SUBDIRECTORY (music)
ADD_SUBDIRECTORY (mve)
ADD_SUBDIRECTORY (networking)
ADD_SUBDIRECTORY (physics)
ADD_SUBDIRECTORY (renderer)
ADD_SUBDIRECTORY (rtperformance)
ADD_SUBDIRECTORY (sndlib)
ADD_SUBDIRECTORY (stream_audio)
ADD_SUBDIRECTORY (ui)
ADD_SUBDIRECTORY (unzip)
ADD_SUBDIRECTORY (vecmat)
# ADD_SUBDIRECTORY (libmve)
ADD_SUBDIRECTORY (md5)
ADD_SUBDIRECTORY (libacm)
ADD_SUBDIRECTORY (netgames)
ADD_SUBDIRECTORY (Descent3)

# For now we don't need to build the scripts under windows, so we'll only include
# the directory when building for linux/osx. In the future we may want to to fix bugs, etc.
#[ISB] TODO: Needs fixing for new build system
#IF(UNIX)
#ADD_SUBDIRECTORY (netcon)
#ADD_SUBDIRECTORY (scripts)
#ENDIF()

#From GZDOOM
add_custom_target( revision_check ALL
	COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_SOURCE_DIR}/tools/UpdateRevision.cmake" Descent3/gitinfo.h
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# set default cmake build type to Debug (None Debug Release RelWithDebInfo MinSizeRel)
IF( NOT CMAKE_BUILD_TYPE )
SET( CMAKE_BUILD_TYPE "Debug" )
ENDIF()

SET (DESCENT3_SOURCES
		${AUDIOENCODE_SORUCES}
		${BITMAP_SOURCES}
		${CFILE_SOURCES}
		${D3MUSIC_SOURCES}
		${DDIO_COMMON_SOURCES}
		${FIX_SOURCES}
		${GRTEXT_SOURCES}
		${MANAGE_SOURCES}
		${MEM_SOURCES}
		${MISC_SOURCES}
		${MODEL_SOURCES}
		${MODULE_SOURCES}
		${MOVIE_SOURCES}
		${MUSIC_SOURCES}
		${MVE_SOURCES}
		${NETWORKING_SOURCES}
		${PHYSICS_SOURCES}
		${RENDERER_SOURCES}
		${RTPERFORMANCE_SOURCES}
		${SNDLIB_SOURCES}
		${STREAM_AUDIO_SOURCES}
		${UI_SOURCES}
		${UNZIP_SOURCES}
		${VECMAT_SOURCES}
		${MD5_SOURCES}
		${LIBACM_SOURCES}
		${MAIN_SOURCES})
		
IF (WIN32)
SET (DESCENT3_SOURCES
	${DESCENT3_SOURCES}
	${DD_GRWIN32_SOURCES}
	${DD_VIDWIN32_SOURCES}
	${DDIO_WIN_SOURCES}
	${WIN32_SOURCES}
	${DD_SNDLIB_SOURCES}
	Descent3.manifest)
ENDIF()

IF (UNIX)
SET (DESCENT3_SOURCES
	${DESCENT3_SOURCES}
	${LINUX_SOURCES}
	${DDVID_LNX_SOURCES}
	${DD_LNXSOUND_SOURCES}
	${DDIO_SOURCES})
ENDIF()		

IF (WIN32)
SET (PLATFORM_LIBS wsock32.lib winmm.lib Glu32.lib OpenGL32.lib dbghelp.lib xinput.lib
	${DSOUND_LIBRARY} ${DINPUT_LIBRARY} ${DXGUID_LIBRARY} ${DDRAW_LIBRARY} ${OPENAL_LIBRARY})
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /SUBSYSTEM:WINDOWS /NODEFAULTLIB:LIBC")
ENDIF()

IF (UNIX AND NOT APPLE)
SET (PLATFORM_LIBS ${SDL_LIBRARY} caca asound audio esd aa directfb m dl GLU)
SET(CMAKE_EXE_LINKER_FLAGS "/usr/lib/libpulse-simple.so.0")
ENDIF()
	
IF (APPLE)
SET (PLATFORM_LIBS ${SDL_LIBRARY} ncurses )
SET(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework Cocoa -framework OpenGL -framework Carbon")
ENDIF()

add_executable(PiccuEngine ${DESCENT3_SOURCES})
target_link_libraries(PiccuEngine 
	${PLATFORM_LIBS})
	
target_compile_definitions(PiccuEngine PUBLIC "$<$<CONFIG:RELEASE>:RELEASE>")
target_compile_definitions(PiccuEngine PUBLIC "$<$<CONFIG:MINSIZEREL>:RELEASE>")
target_compile_definitions(PiccuEngine PUBLIC "$<$<CONFIG:RELWITHDEBINFO>:RELEASE>")

add_library(dmfc SHARED ${DMFC_SOURCES})
target_compile_definitions(dmfc PUBLIC -DOUTRAGE_VERSION -DDMFC_DLL)
target_include_directories(dmfc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/netgames/includes")
add_dependencies(PiccuEngine revision_check)
	
install(TARGETS PiccuEngine DESTINATION ${D3_GAMEDIR})

source_group("AudioEncode" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/AudioEncode/.+")
source_group("bitmap" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/bitmap/.+")
source_group("cfile" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/cfile/.+")
source_group("d3music" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/d3music/.+")
source_group("dd_grwin32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/dd_grwin32/.+")
source_group("dd_lnxsound" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/dd_lnxsound/.+")
source_group("dd_sndlib" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/dd_sndlib/.+")
source_group("dd_vidwin32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/dd_vidwin32/.+")
source_group("ddio_common" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/ddio_common/.+")
source_group("ddio_lnx" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/ddio_lnx/.+")
source_group("ddio_win" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/ddio_win/.+")
source_group("ddvid_lnx" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/ddvid_lnx/.+")
source_group("main" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/Descent3/.+")
source_group("fix" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/fix/.+")
source_group("grtext" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/grtext/.+")
source_group("libacm" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/libacm/.+")
source_group("linux" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/linux/.+")
source_group("manage" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/manage/.+")
source_group("md5" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/md5/.+")
source_group("mem" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/mem/.+")
source_group("misc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/misc/.+")
source_group("model" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/model/.+")
source_group("module" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/module/.+")
source_group("movie" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/movie/.+")
source_group("music" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/music/.+")
source_group("mve" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/mve/.+")
source_group("networking" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/networking/.+")
source_group("netgames" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/netgames/.+")
source_group("netgames/dmfc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/netgames/dmfc/.+")
source_group("physics" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/physics/.+")
source_group("renderer" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/renderer/.+")
source_group("rtperformance" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/rtperformance/.+")
source_group("sndlib" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/sndlib/.+")
source_group("stream_audio" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/stream_audio/.+")
source_group("ui" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/ui/.+")
source_group("unzip" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/unzip/.+")
source_group("vecmat" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/vecmat/.+")
source_group("win32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/win32/.+")

