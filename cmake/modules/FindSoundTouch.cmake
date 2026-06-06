# Locate the SoundTouch time-stretch / pitch-shift library.
# Defines the imported target SoundTouch::SoundTouch on success.

include(ImportedTargetHelpers)

find_package_config_mode_with_fallback(SoundTouch SoundTouch::SoundTouch
	LIBRARY_NAMES "SoundTouch" "soundtouch"
	INCLUDE_NAMES "soundtouch/SoundTouch.h" "SoundTouch.h"
	PKG_CONFIG soundtouch
	PREFIX SoundTouch
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SoundTouch
	REQUIRED_VARS SoundTouch_LIBRARY SoundTouch_INCLUDE_DIRS
)
