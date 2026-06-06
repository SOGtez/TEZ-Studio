/*
 * TimeStretch.cpp - pitch-preserving time-stretch utility (SoundTouch)
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

#include "TimeStretch.h"

#include "SampleFrame.h"
#include "lmmsconfig.h"

#ifdef LMMS_HAVE_SOUNDTOUCH
#include <algorithm>
#include <vector>
#include <soundtouch/SoundTouch.h>
#endif

namespace lmms {

bool timeStretchAvailable()
{
#ifdef LMMS_HAVE_SOUNDTOUCH
	return true;
#else
	return false;
#endif
}

SampleBuffer timeStretch(const SampleBuffer& src, double ratio)
{
#ifdef LMMS_HAVE_SOUNDTOUCH
	if (src.empty() || ratio <= 0.0)
	{
		return src;
	}

	static_assert(sizeof(SampleFrame) == 2 * sizeof(float),
		"timeStretch assumes an interleaved stereo SampleFrame layout");

	constexpr int channels = 2;
	soundtouch::SoundTouch st;
	st.setSampleRate(src.sampleRate());
	st.setChannels(channels);
	// SoundTouch's tempo control sets outputLength = inputLength / tempo.
	// We want outputLength = inputLength * ratio, so tempo = 1 / ratio.
	st.setTempo(1.0 / ratio);

	const auto numIn = static_cast<unsigned int>(src.size());

	std::vector<SampleFrame> out;
	out.reserve(static_cast<size_t>(src.size() * ratio) + 64);

	constexpr unsigned int kChunk = 4096;
	std::vector<float> inBuf(static_cast<size_t>(kChunk) * channels);
	std::vector<float> recvBuf(static_cast<size_t>(kChunk) * channels);

	const auto drain = [&]() {
		unsigned int got = 0;
		while ((got = st.receiveSamples(recvBuf.data(), kChunk)) > 0)
		{
			const auto oldSize = out.size();
			out.resize(oldSize + got);
			copyToSampleFrames(out.data() + oldSize, recvBuf.data(), got);
		}
	};

	unsigned int pos = 0;
	while (pos < numIn)
	{
		const unsigned int n = std::min(kChunk, numIn - pos);
		copyFromSampleFrames(inBuf.data(), src.data() + pos, n);
		st.putSamples(inBuf.data(), n);
		pos += n;
		drain();
	}
	st.flush();
	drain();

	if (out.empty())
	{
		return src;
	}
	return SampleBuffer(std::move(out), src.sampleRate(), src.audioFile());
#else
	(void)ratio;
	return src;
#endif
}

} // namespace lmms
