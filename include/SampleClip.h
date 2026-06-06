/*
 * SampleClip.h
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

#ifndef LMMS_SAMPLE_CLIP_H
#define LMMS_SAMPLE_CLIP_H

#include <memory>
#include <QTimer>
#include "Clip.h"
#include "Sample.h"

namespace lmms
{

class SampleBuffer;

namespace gui
{

class SampleClipView;

} // namespace gui


class SampleClip : public Clip
{
	Q_OBJECT
	mapPropertyFromModel(bool,isRecord,setRecord,m_recordModel);
	mapPropertyFromModel(bool,isSyncToTempo,setSyncToTempo,m_syncToTempoModel);
public:
	SampleClip(Track* track, Sample sample, bool isPlaying);
	SampleClip(Track* track);
	~SampleClip() override;

	SampleClip& operator=( const SampleClip& that ) = delete;

	void changeLength( const TimePos & _length ) override;
	const QString& sampleFile() const;
	bool hasSampleFileLoaded(const QString & filename) const;

	void saveSettings( QDomDocument & _doc, QDomElement & _parent ) override;
	void loadSettings( const QDomElement & _this ) override;
	inline QString nodeName() const override
	{
		return "sampleclip";
	}

	Sample& sample()
	{
		return m_sample;
	}

	TimePos sampleLength() const;
	void setSampleStartFrame( f_cnt_t startFrame );
	void setSamplePlayLength( f_cnt_t length );
	void setStartTimeOffset(const TimePos& startTimeOffset) override;
	gui::ClipView * createView( gui::TrackView * _tv ) override;


	bool isPlaying() const;
	void setIsPlaying(bool isPlaying);
	void setSampleBuffer(std::shared_ptr<const SampleBuffer> sb);

	SampleClip* clone() override
	{
		return new SampleClip(*this);
	}

public slots:
	void setSampleFile(const QString& sf);
	void updateLength();
	void toggleRecord();
	void playbackPositionChanged();
	void updateTrackClips();
	void tempoChanged();

protected:
	SampleClip( const SampleClip& orig );

private slots:
	// Re-derive the stretched buffer from the original for the current tempo.
	void applyStretch();
	// React to the "Sync to tempo" toggle being switched on/off.
	void onSyncToggled();

private:
	// Connect tempo-sync signals/timer; called from both constructors.
	void initTempoSync();
	// Stretch factor (outFrames/inFrames) for the current tempo and target length.
	double computeStretchRatio() const;
	// Natural sample length rounded to whole bars (min one bar), in ticks.
	int barAlignedTargetTicks() const;

	Sample m_sample;
	BoolModel m_recordModel;
	BoolModel m_syncToTempoModel;
	bool m_isPlaying;
	int m_startFrameOffset;

	// Tempo-synced time-stretch state
	std::shared_ptr<const SampleBuffer> m_originalBuffer;
	int m_targetLengthTicks = 0;
	bpm_t m_lastStretchBpm = 0;
	QTimer m_restretchTimer;

	friend class gui::SampleClipView;


signals:
	void sampleChanged();
	void wasReversed();
} ;


} // namespace lmms

#endif // LMMS_SAMPLE_CLIP_H
