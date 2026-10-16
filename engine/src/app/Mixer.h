// The app's sound mixer (SDL audio): plays what the engine asks through SBSound.
#pragma once

bool MixerOpen();          // opens the device and hands SBSound its player
void MixerPause(bool pause);
void MixerStopAll();
void MixerFlushCache();    // a new level: the old samples go
void MixerClose();
