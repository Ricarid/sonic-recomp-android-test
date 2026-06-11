#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Backend de áudio Android — AAudio com fallback OpenSL ES
// ---------------------------------------------------------------------------

bool  Audio_Init();
void  Audio_Shutdown();
void  Audio_OnPause();
void  Audio_OnResume();
void  Audio_SubmitFrames(const float* frames, int32_t numFrames);
bool  Audio_IsRunning();
