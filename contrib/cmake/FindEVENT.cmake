FIND_PATH(EVENT_INCLUDE_DIR event2/event.h
  PATH_SUFFIXES include
  PATHS
  /usr/local
  /usr
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
  HINTS $ENV{EVENT_SRC}
)

FIND_LIBRARY(EVENT_LIBRARY_RELEASE
  NAMES event libevent
  HINTS $ENV{EVENT_SRC}/.libs/
)
if (EVENT_LIBRARY_RELEASE)
	list(APPEND EVENT_LIBRARY optimized ${EVENT_LIBRARY_RELEASE})
endif()

FIND_LIBRARY(EVENT_LIBRARY_DEBUG
  NAMES event_d libevent_d
  HINTS $ENV{EVENT_SRC}/.libs/
)
if (EVENT_LIBRARY_DEBUG)
	list(APPEND EVENT_LIBRARY debug ${EVENT_LIBRARY_DEBUG})
else()
	if (UNIX)
		list(APPEND EVENT_LIBRARY debug ${EVENT_LIBRARY_RELEASE})
	endif()
endif()

if (NOT WIN32)
	FIND_LIBRARY(EVENT_LIBRARY_THREADS
	  NAMES event_pthreads
	  HINTS $ENV{EVENT_SRC}/.libs/
	)
	list (APPEND EVENT_LIBRARY ${EVENT_LIBRARY_THREADS})
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EVENT DEFAULT_MSG EVENT_LIBRARY EVENT_INCLUDE_DIR)
MARK_AS_ADVANCED(EVENT_LIBRARY EVENT_INCLUDE_DIR)
