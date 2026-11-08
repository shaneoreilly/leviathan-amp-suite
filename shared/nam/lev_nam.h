// Leviathan <-> Neural Amp Modeler (NeuralAmpModelerCore, MIT): a small C interface shared by the Mac (Swift)
// and Windows (JUCE) apps, so both run exactly the same code.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct LevNam LevNam;

/// Every capture is normalised so lev_nam_measure_level() lands here (dB); the Output knob trims from there.
/// Calibrated so a NAM capture through a cab sits at about the same level as the built-in amp presets.
#define LEV_NAM_TARGET_LEVEL (-13.0)

/// Loads a .nam file. Returns NULL on failure and writes a message into err.
LevNam* lev_nam_load(const char* path, char* err, int errLen);
void lev_nam_free(LevNam* m);

/// Prepares for processing (allocates; call off the audio thread). maxBlock: largest block process() will get.
void lev_nam_reset(LevNam* m, double sampleRate, int maxBlock);
/// Mono, real-time safe once reset. in and out may be the same buffer.
void lev_nam_process(LevNam* m, const float* in, float* out, int frames);

/// Rate the capture was made at (Hz), or 0 if unknown.
double lev_nam_expected_rate(const LevNam* m);
/// Loudness of the capture's output in dB (NAM's normalisation metadata); has_* is 0 when the file doesn't say.
int lev_nam_has_loudness(const LevNam* m);
double lev_nam_loudness(const LevNam* m);
/// Output level (dB RMS) for a fixed 1 s guitar-like test signal, measured by running the model at `sampleRate`
/// (then resetting it). Works for every capture, with or without loudness metadata, so captures can be level-matched.
/// Call off the audio thread, before handing the model to it.
double lev_nam_measure_level(LevNam* m, double sampleRate);
/// The same, with the test signal scaled by `inputGain` first (as input calibration does to the guitar).
double lev_nam_measure_level_in(LevNam* m, double sampleRate, float inputGain);
/// +1, or -1 if the capture inverts the signal (some real amps do), measured like lev_nam_measure_level. Both apps
/// multiply the capture's gain by it, so two captures blended as amp A and amp B don't cancel each other.
int lev_nam_polarity(LevNam* m, double sampleRate);
/// Calibrated input level (dBu that corresponds to 0 dBFS when the capture was made), if the file has it.
int lev_nam_has_input_level(LevNam* m);
double lev_nam_input_level(LevNam* m);

#ifdef __cplusplus
}
#endif
