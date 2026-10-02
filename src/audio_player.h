#pragma once

// Plays a 16-bit PCM WAV file (mono or stereo) from LittleFS through the ES8311 codec
// and speaker. Blocks until playback finishes. Wire must already be started.
bool playWav(const char *path);
