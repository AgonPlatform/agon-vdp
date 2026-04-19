//
// Title:			Agon Video BIOS - Audio class
// Author:			Dean Belfield
// Contributors:	Steve Sims (enhancements for more sophisticated audio support)
// Created:			05/09/2022
// Last Updated:	04/08/2023
//
// Modinfo:

#ifndef AGON_AUDIO_H
#define AGON_AUDIO_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <fabgl.h>
#include <mutex>

std::mutex soundGeneratorMutex;

#include "agon.h"
#include "audio_channel.h"
#include "audio_sample.h"
#include "types.h"

// audio channels and their associated tasks
AudioChannel *audioChannels[MAX_AUDIO_CHANNELS];
TaskHandle_t audioTask;
// Storage for our sample data
std::unordered_map<uint16_t, std::shared_ptr<AudioSample>,
	std::hash<uint16_t>, std::equal_to<uint16_t>,
	psram_allocator<std::pair<const uint16_t, std::shared_ptr<AudioSample>>>> samples;
fabgl::SoundGenerator *soundGenerator;  // audio handling sub-system

bool channelEnabled(uint8_t channel);

// Audio channel driver task
//
void audioDriver(void * parameters) {
	while (true) {
		auto now = millis();
		for (int i=0; i<MAX_AUDIO_CHANNELS; i++) {
			if (audioChannels[i]) {
				audioChannels[i]->loop(now);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

BaseType_t initAudioTask() {
	return xTaskCreatePinnedToCore(audioDriver, "audioDriver",
		2048,
		nullptr,
		AUDIO_CHANNEL_PRIORITY,		// Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
		&audioTask,
		AUDIO_CORE
	);
}

BaseType_t initAudioChannel(int channel) {
	if (!channelEnabled(channel)) {
		audioChannels[channel] = new AudioChannel(channel);
		return pdPASS;
	}
	return 0;
}

// Change the sample rate
//
void setSampleRate(uint16_t sampleRate) {
	// make a new sound generator and re-attach all our active channels
	if (sampleRate == 65535) {
		sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
	}
	{
		// detach the old sound generator
		auto lock = std::unique_lock<std::mutex>(soundGeneratorMutex);
		// delete the old sound generator
		if (soundGenerator) {
			soundGenerator->clear();
			delete soundGenerator;
		}
		soundGenerator = new fabgl::SoundGenerator(sampleRate);
	}
	for (int chan=0; chan<MAX_AUDIO_CHANNELS; chan++) {
		if (audioChannels[chan]) {
			auto lock = audioChannels[chan]->lock();
			audioChannels[chan]->attachSoundGenerator();
		}
	}
	soundGenerator->play(true);
}

// Initialise the sound driver
//
void initAudio() {
	for (int i=0; i<MAX_AUDIO_CHANNELS; i++) {
		audioChannels[i] = nullptr;
	}
	setSampleRate(AUDIO_DEFAULT_SAMPLE_RATE);
	for (uint8_t i = 0; i < AUDIO_CHANNELS; i++) {
		initAudioChannel(i);
	}
	initAudioTask();
}

// Channel enabled?
//
bool channelEnabled(uint8_t channel) {
	return channel < MAX_AUDIO_CHANNELS && audioChannels[channel] != nullptr;
}

// Play a note
//
uint8_t playNote(uint8_t channel, uint8_t volume, uint16_t frequency, uint16_t duration) {
	if (audioChannels[channel]) {
		return audioChannels[channel]->playNote(volume, frequency, duration);
	}
	return 1;
}

// Get channel status
//
uint8_t getChannelStatus(uint8_t channel) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->getStatus();
	}
	return -1;
}

// Set channel volume
//
uint8_t setVolume(uint8_t channel, uint8_t volume) {
	if (channel == 255) {
		if (volume == 255) {
			return soundGenerator->volume();
		}
		soundGenerator->setVolume(volume < 128 ? volume : 127);
		return soundGenerator->volume();
	} else if (channelEnabled(channel)) {
		return audioChannels[channel]->setVolume(volume);
	}
	return 255;
}

// Set channel frequency
//
uint8_t setFrequency(uint8_t channel, uint16_t frequency) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->setFrequency(frequency);
	}
	return 0;
}

// Set channel waveform
//
uint8_t setWaveform(uint8_t channel, int8_t waveformType, uint16_t sampleId) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->setWaveform(waveformType, sampleId);
	}
	return 0;
}

// Seek to a position on a channel
//
uint8_t seekTo(uint8_t channel, uint32_t position) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->seekTo(position);
	}
	return 0;
}

// Set channel duration
//
uint8_t setDuration(uint8_t channel, uint16_t duration) {
	if (channelEnabled(channel)) {
		return audioChannels[channel]->setDuration(duration);
	}
	return 0;
}

// Set channel sample rate
//
uint8_t setSampleRate(uint8_t channel, uint16_t sampleRate) {
	if (channel == 255) {
		// set underlying sample rate
		setSampleRate(sampleRate);
		return 1;
	}
	if (channelEnabled(channel)) {
		return audioChannels[channel]->setSampleRate(sampleRate);
	}
	return 0;
}

// Enable a channel
//
uint8_t enableChannel(uint8_t channel) {
	if (channelEnabled(channel)) {
		// channel already enabled
		return 1;
	}
	if (channel >= 0 && channel < MAX_AUDIO_CHANNELS && audioChannels[channel] == nullptr) {
		// channel not enabled, so enable it
		if (initAudioChannel(channel) == pdPASS) {
			return 1;
		}
	}
	return 0;
}

// Disable a channel
//
uint8_t disableChannel(uint8_t channel) {
	if (channelEnabled(channel)) {
		audioChannels[channel]->goIdle();
		return 1;
	}
	return 0;
}

void audioTaskKill(int channel) {
	disableChannel(channel);
}

// Clear a sample
//
uint8_t clearSample(uint16_t sampleId) {
	debug_log("clearSample: sample %d\n\r", sampleId);
	if (samples.find(sampleId) == samples.end()) {
		debug_log("clearSample: sample %d not found\n\r", sampleId);
		return 1;
	}
	samples[sampleId] = nullptr;
	debug_log("reset sample\n\r");
	return 0;
}

// Reset samples
//
void resetSamples() {
	debug_log("resetSamples\n\r");
	samples.clear();
}

#endif // AGON_AUDIO_H
