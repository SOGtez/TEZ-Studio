/*
 * TimeStretch.h - pitch-preserving time-stretch utility (SoundTouch)
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_TIME_STRETCH_H
#define LMMS_TIME_STRETCH_H

#include "SampleBuffer.h"
#include "lmms_export.h"

namespace lmms {

//! True if this build was compiled with time-stretch support (SoundTouch).
LMMS_EXPORT bool timeStretchAvailable();

//! Return a new buffer that is `ratio` times the length of `src`, with pitch
//! preserved. `ratio` = outputFrames / inputFrames (e.g. 2.0 = twice as long).
//! If time-stretch is unavailable or the inputs are degenerate, returns a copy
//! of `src` unchanged.
LMMS_EXPORT SampleBuffer timeStretch(const SampleBuffer& src, double ratio);

} // namespace lmms

#endif // LMMS_TIME_STRETCH_H
