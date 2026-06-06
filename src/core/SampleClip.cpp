/*
 * SampleClip.cpp
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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
 
#include "SampleClip.h"

#include <algorithm>
#include <cmath>

#include <QDomElement>
#include <QFileInfo>
#include <QMetaObject>

#include "PatternStore.h"
#include "PathUtil.h"
#include "SampleClipView.h"
#include "SampleTrack.h"
#include "Song.h"
#include "TimeStretch.h"

namespace lmms
{

SampleClip::SampleClip(Track* _track, Sample sample, bool isPlaying):
	Clip(_track),
	m_sample(std::move(sample)),
	m_isPlaying(false),
	m_startFrameOffset(0)
{
	saveJournallingState( false );
	setSampleFile( "" );
	restoreJournallingState();

	// we need to receive bpm-change-events, because then we have to
	// change length of this Clip
	connect(Engine::getSong(), &Song::tempoChanged, this, &SampleClip::tempoChanged, Qt::DirectConnection);
	connect( Engine::getSong(), SIGNAL(timeSignatureChanged(int,int)),
					this, SLOT(updateLength()));

	//playbutton clicked or space key / on Export Song set isPlaying to false
	connect( Engine::getSong(), SIGNAL(playbackStateChanged()),
			this, SLOT(playbackPositionChanged()), Qt::DirectConnection );
	//care about loops and jumps
	connect(Engine::getSong(), &Song::playbackPositionJumped,
			this, &SampleClip::playbackPositionChanged, Qt::DirectConnection);
	//care about mute Clips
	connect( this, SIGNAL(dataChanged()), this, SLOT(playbackPositionChanged()));
	//care about mute track
	connect( getTrack()->getMutedModel(), SIGNAL(dataChanged()),
			this, SLOT(playbackPositionChanged()), Qt::DirectConnection );
	//care about Clip position
	connect( this, SIGNAL(positionChanged()), this, SLOT(updateTrackClips()));

	initTempoSync();
	updateTrackClips();
}

SampleClip::SampleClip(Track* track)
	: SampleClip(track, Sample(), false)
{
}

SampleClip::SampleClip(const SampleClip& orig) :
	Clip(orig),
	m_sample(std::move(orig.m_sample)),
	m_isPlaying(orig.m_isPlaying),
	m_startFrameOffset(orig.m_startFrameOffset)
{
	saveJournallingState( false );
	setSampleFile( "" );
	restoreJournallingState();

	// we need to receive bpm-change-events, because then we have to
	// change length of this Clip
	connect(Engine::getSong(), &Song::tempoChanged, this, &SampleClip::tempoChanged, Qt::DirectConnection);
	connect( Engine::getSong(), SIGNAL(timeSignatureChanged(int,int)),
					this, SLOT(updateLength()));

	//playbutton clicked or space key / on Export Song set isPlaying to false
	connect( Engine::getSong(), SIGNAL(playbackStateChanged()),
			this, SLOT(playbackPositionChanged()), Qt::DirectConnection );
	//care about loops and jumps
	connect(Engine::getSong(), &Song::playbackPositionJumped,
			this, &SampleClip::playbackPositionChanged, Qt::DirectConnection);
	//care about mute Clips
	connect( this, SIGNAL(dataChanged()), this, SLOT(playbackPositionChanged()));
	//care about mute track
	connect( getTrack()->getMutedModel(), SIGNAL(dataChanged()),
			this, SLOT(playbackPositionChanged()), Qt::DirectConnection );
	//care about Clip position
	connect( this, SIGNAL(positionChanged()), this, SLOT(updateTrackClips()));

	initTempoSync();
	updateTrackClips();
}




SampleClip::~SampleClip()
{
	auto sampletrack = dynamic_cast<SampleTrack*>(getTrack());
	if ( sampletrack )
	{
		sampletrack->updateClips();
	}
}




void SampleClip::changeLength( const TimePos & _length )
{
	Clip::changeLength(std::max(static_cast<int>(_length), 1));
}



const QString& SampleClip::sampleFile() const
{
	return m_sample.sampleFile();
}

bool SampleClip::hasSampleFileLoaded(const QString & filename) const
{
	return m_sample.sampleFile() == filename;
}

void SampleClip::setSampleBuffer(std::shared_ptr<const SampleBuffer> sb)
{
	{
		const auto guard = Engine::audioEngine()->requestChangesGuard();
		m_sample = Sample(std::move(sb));
	}
	if (m_syncToTempoModel.value())
	{
		// New audio while synced: re-capture the pristine source and re-stretch.
		m_originalBuffer = m_sample.buffer();
		m_targetLengthTicks = barAlignedTargetTicks();
		applyStretch();
	}
	updateLength();

	emit sampleChanged();

	Engine::getSong()->setModified();
}

void SampleClip::setSampleFile(const QString& sf)
{
	// Remove any prior offset in the clip
	setStartTimeOffset(0);
	if (!sf.isEmpty())
	{
		m_sample = Sample(SampleBuffer::fromFile(sf));
		if (m_syncToTempoModel.value())
		{
			m_originalBuffer = m_sample.buffer();
			m_targetLengthTicks = barAlignedTargetTicks();
			applyStretch();
		}
		updateLength();
	}
	else
	{
		// If there is no sample, make the clip a bar long
		float nom = Engine::getSong()->getTimeSigModel().getNumerator();
		float den = Engine::getSong()->getTimeSigModel().getDenominator();
		changeLength(DefaultTicksPerBar * (nom / den));
	}

	emit sampleChanged();
	emit playbackPositionChanged();
}




void SampleClip::toggleRecord()
{
	m_recordModel.setValue( !m_recordModel.value() );
	emit dataChanged();
}




void SampleClip::playbackPositionChanged()
{
	Engine::audioEngine()->removePlayHandlesOfTypes( getTrack(), PlayHandle::Type::SamplePlayHandle );
	auto st = dynamic_cast<SampleTrack*>(getTrack());
	st->setPlayingClips( false );
}




void SampleClip::updateTrackClips()
{
	auto sampletrack = dynamic_cast<SampleTrack*>(getTrack());
	if( sampletrack)
	{
		sampletrack->updateClips();
	}
}




bool SampleClip::isPlaying() const
{
	return m_isPlaying;
}




void SampleClip::setIsPlaying(bool isPlaying)
{
	m_isPlaying = isPlaying;
}




void SampleClip::updateLength()
{
	// If the clip has already been manually resized, don't automatically resize it.
	if (getAutoResize())
	{
		if (getTrack()->trackContainer() == Engine::patternStore())
		{
			changeLength(TimePos::ticksPerBar() * Engine::patternStore()->lengthOfPattern(getTrack()->getClipNum(this)));
			return;
		}
		// When synced to tempo the clip is locked to a fixed musical length
		// (the audio is stretched to fill it), so it stays put across BPM changes.
		if (m_syncToTempoModel.value() && m_targetLengthTicks > 0)
		{
			changeLength(m_targetLengthTicks);
		}
		else
		{
			changeLength(sampleLength());
		}
		setStartTimeOffset(0);
	}

	emit sampleChanged();
}


void SampleClip::tempoChanged()
{
	Clip::setStartTimeOffset(std::round(1.0f * m_startFrameOffset / Engine::framesPerTick()));
	updateLength();
	// Re-stretch the audio to fit the (tempo-dependent) real duration of the
	// locked musical length. Debounced so dragging the BPM spinbox coalesces
	// into a single stretch instead of one per intermediate value.
	if (m_syncToTempoModel.value() && timeStretchAvailable()
			&& Engine::getSong()->getTempo() != m_lastStretchBpm)
	{
		// tempoChanged() may arrive on the audio thread (tempo automation), but a
		// QTimer must be started from the thread it lives on. Marshal to that thread.
		QMetaObject::invokeMethod(&m_restretchTimer,
			[this] { m_restretchTimer.start(150); }, Qt::QueuedConnection);
	}
	emit sampleChanged();
}

void SampleClip::setStartTimeOffset(const TimePos& startTimeOffset)
{
	m_startFrameOffset = startTimeOffset * Engine::framesPerTick();
	Clip::setStartTimeOffset(startTimeOffset);
}


void SampleClip::initTempoSync()
{
	m_restretchTimer.setSingleShot(true);
	connect(&m_restretchTimer, &QTimer::timeout, this, &SampleClip::applyStretch);
	connect(&m_syncToTempoModel, &Model::dataChanged, this, &SampleClip::onSyncToggled);
}


int SampleClip::barAlignedTargetTicks() const
{
	const int ticksPerBar = TimePos::ticksPerBar();
	const int natural = static_cast<int>(sampleLength());
	if (natural <= 0) { return ticksPerBar; }
	const int bars = std::max(1, (natural + ticksPerBar / 2) / ticksPerBar);
	return bars * ticksPerBar;
}


double SampleClip::computeStretchRatio() const
{
	if (!m_originalBuffer || m_originalBuffer->empty() || m_targetLengthTicks <= 0)
	{
		return 1.0;
	}
	const double originalFrames = static_cast<double>(m_originalBuffer->size());
	if (originalFrames < 1.0) { return 1.0; }
	// Frames (in the original sample's rate domain) needed to fill the locked
	// musical length at the current tempo.
	const double desiredFrames = m_targetLengthTicks * Engine::framesPerTick(m_originalBuffer->sampleRate());
	return std::clamp(desiredFrames / originalFrames, 0.1, 10.0);
}


void SampleClip::applyStretch()
{
	if (!m_syncToTempoModel.value() || !timeStretchAvailable()) { return; }
	if (!m_originalBuffer) { m_originalBuffer = m_sample.buffer(); }
	// Skip tiny buffers: stretching a handful of frames is meaningless and risky.
	if (!m_originalBuffer || m_originalBuffer->size() < 64) { return; }
	if (m_targetLengthTicks <= 0) { m_targetLengthTicks = barAlignedTargetTicks(); }

	const double ratio = computeStretchRatio();
	const bool reversed = m_sample.reversed();

	// Heavy DSP happens OUTSIDE the audio-engine lock; only the pointer swap is guarded.
	auto stretched = std::make_shared<const SampleBuffer>(timeStretch(*m_originalBuffer, ratio));

	{
		const auto guard = Engine::audioEngine()->requestChangesGuard();
		m_sample = Sample(std::move(stretched));
		m_sample.setReversed(reversed);
	}

	m_lastStretchBpm = Engine::getSong()->getTempo();
	updateLength();
	emit sampleChanged();
}


void SampleClip::onSyncToggled()
{
	if (m_syncToTempoModel.value())
	{
		if (!timeStretchAvailable()) { return; }
		m_originalBuffer = m_sample.buffer();
		if (m_targetLengthTicks <= 0) { m_targetLengthTicks = barAlignedTargetTicks(); }
		applyStretch();
	}
	else
	{
		// Restore the pristine, unstretched audio.
		const bool reversed = m_sample.reversed();
		if (m_originalBuffer)
		{
			const auto guard = Engine::audioEngine()->requestChangesGuard();
			m_sample = Sample(m_originalBuffer);
			m_sample.setReversed(reversed);
		}
		m_targetLengthTicks = 0;
		m_lastStretchBpm = 0;
		updateLength();
		emit sampleChanged();
	}
	Engine::getSong()->setModified();
}


TimePos SampleClip::sampleLength() const
{
	return static_cast<int>(m_sample.sampleSize() / Engine::framesPerTick(m_sample.sampleRate()));
}




void SampleClip::setSampleStartFrame(f_cnt_t startFrame)
{
	m_sample.setStartFrame(startFrame);
}




void SampleClip::setSamplePlayLength(f_cnt_t length)
{
	m_sample.setEndFrame(length);
}




void SampleClip::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	if( _this.parentNode().nodeName() == "clipboard" )
	{
		_this.setAttribute( "pos", -1 );
	}
	else
	{
		_this.setAttribute( "pos", startPosition() );
	}
	_this.setAttribute( "len", length() );
	_this.setAttribute( "muted", isMuted() );
	_this.setAttribute( "src", sampleFile() );
	_this.setAttribute( "off", startTimeOffset() );
	_this.setAttribute("autoresize", QString::number(getAutoResize()));
	_this.setAttribute("synctotempo", m_syncToTempoModel.value() ? 1 : 0);
	_this.setAttribute("synctargetlen", m_targetLengthTicks);
	if( sampleFile() == "" )
	{
		// Persist the pristine (unstretched) audio when synced; the stretched
		// version is re-derived from it on load, so we must not save it instead.
		const auto buffer = (m_syncToTempoModel.value() && m_originalBuffer)
			? m_originalBuffer : m_sample.buffer();
		_this.setAttribute("data", buffer->toBase64());
	}

	_this.setAttribute( "sample_rate", m_sample.sampleRate());
	if (const auto& c = color())
	{
		_this.setAttribute("color", c->name());
	}
	if (m_sample.reversed())
	{
		_this.setAttribute("reversed", "true");
	}
	// TODO: start- and end-frame
}




void SampleClip::loadSettings( const QDomElement & _this )
{
	if( _this.attribute( "pos" ).toInt() >= 0 )
	{
		movePosition( _this.attribute( "pos" ).toInt() );
	}

	if (const auto srcFile = _this.attribute("src"); !srcFile.isEmpty())
	{
		if (QFileInfo(PathUtil::toAbsolute(srcFile)).exists())
		{
			setSampleFile(srcFile);
		}
		else { Engine::getSong()->collectError(QString("%1: %2").arg(tr("Sample not found"), srcFile)); }
	}

	if( sampleFile().isEmpty() && _this.hasAttribute( "data" ) )
	{
		auto sampleRate = _this.hasAttribute("sample_rate") ? _this.attribute("sample_rate").toInt() :
			Engine::audioEngine()->outputSampleRate();

		auto buffer = SampleBuffer::fromBase64(_this.attribute("data"), sampleRate);
		m_sample = Sample(std::move(buffer));
	}
	changeLength( _this.attribute( "len" ).toInt() );
	setMuted( _this.attribute( "muted" ).toInt() );
	setStartTimeOffset( _this.attribute( "off" ).toInt() );
	setAutoResize(_this.attribute("autoresize", "1").toInt());

	if (_this.hasAttribute("color"))
	{
		setColor(QColor{_this.attribute("color")});
	}

	if(_this.hasAttribute("reversed"))
	{
		m_sample.setReversed(true);
		emit wasReversed(); // tell SampleClipView to update the view
	}

	// Restore tempo-sync last, once the (pristine) audio and length are in place.
	// Enabling the model triggers onSyncToggled() -> applyStretch(), which captures
	// the original buffer and stretches it to the current tempo.
	m_targetLengthTicks = _this.attribute("synctargetlen", "0").toInt();
	if (_this.attribute("synctotempo", "0").toInt() != 0)
	{
		m_syncToTempoModel.setValue(true);
	}
}




gui::ClipView * SampleClip::createView( gui::TrackView * _tv )
{
	return new gui::SampleClipView( this, _tv );
}


} // namespace lmms
