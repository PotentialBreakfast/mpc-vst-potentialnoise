# Changelog

## 2.4.0 (2026-11)
- **MANUAL page.** The plugin's manual is now on the MPC's screen: tap **MANUAL**, then the page buttons along the top (what it does, the controls, how it works with the other PotentialSuite plugins, and good-to-know notes). The full PotentialSuite manual is on the [PotentialSuite page](https://github.com/PotentialBreakfast/potential-plugins).
- One parameter was added at the end of the list (Manual Page); saved projects open as before. Sound and controls are unchanged.

## 2.3.0 (2026-11)
- **SIGNAL**: four things to play, one tap each. **PINK NOISE** (as before), **TEST TONE** (a steady sine at 1 kHz, 100, 60 or 40 Hz, at the LEVEL you pick: 1 kHz for gain staging, the low ones to hear how far down your speakers go), **OCTAVE BAND** (pink noise in one octave only, 31 Hz to 16 kHz, to balance the tracks in that range) and **SPEAKER CHECK** (left, right, both in phase, both out of phase, 2 seconds each, with the tip line saying which: a quick check that your speakers are the right way round and wired in phase).
- Switching signal, tone, band, channels or range while it plays fades out and back in, so there are no clicks.
- The GUIDE page switches to pink noise when it starts.
- The settings under SIGNAL change with it. Three parameters were added at the end of the list (Signal, Tone, Octave Band); saved projects open as before.

## 2.2.0 (2026-11)
- **GUIDE page**: the pink-noise method one tap per track. Pull your faders down and tap **NEXT**: the noise comes on and the guide names each part of your beat in turn (kick, snare / clap, bass, hi-hats, percussion, main sample / chords, melody / lead, vocals, pads / texture, FX), with what to listen for. Parts you don't have are one tap to skip, **BACK** goes back one, and the last tap switches the noise off. **DRUMS FIRST** or **SAMPLE FIRST** sets the order, and your project remembers it.
- Seven parameters were added at the end of the list for the page; saved projects open as before.

## 2.1.2 (2026-11)
- **CPU figure recorded.** Measured on an MPC One with the Open MPC bench: **PASS**, 1.6% of an audio block at the 99th percentile and 1.9% at worst, so it runs comfortably alongside a full project. It shows on the catalog card and in INSTALL.md.
- Nothing else changed: same sound, controls and saved projects as 2.1.1.

## 2.1.1 (2026-11)
- **Ready for MPC OS 3.10.1's plugin folders.** Every download now carries the `plugin.json` that MPC OS 3.10.1 looks for, so the plugin folder can be copied by hand straight into `VST2 Plugins`.
- **Gen2 MPCs in the Open MPC catalog.** A separate 64-bit download (`-mpc-aarch64.zip`), so the Plugin Manager can install it on Gen2 MPCs, and the catalog's 3.10.1 download (`-mpc-sdk.zip`).
- Built with the current Open MPC kit. Sound, controls and saved projects are unchanged.

## 2.1.0 (2026-11)
- **Three looks.** A new default look, **Print Plate** (a warm printed panel), plus **Night Rack** (dark rack unit) and **Colour Block** (flat, big type), each in PotentialNoise's own colour. Pick one with the big LOOK keys on the SETTINGS page; it shows the next time the plugin loads (take it off and add it again, or load your project again), and it is remembered on your MPC for every project.
- Sound, controls and saved projects are unchanged. One parameter was added at the end of the list (LOOK); it is never saved in a project.
- **Akai's plugin folder format** for MPC OS 3.10.1 and later: a second download (`-akai-vst2.zip`) with 32-bit and 64-bit builds, so it now also runs on Gen2 MPCs. Copy the folder into `VST2 Plugins` and turn on External Plugin Support: no hacking needed.

## 2.0.2 (2026-11)
- Fixed: pressing **STOP** or **Overdub** while the MPC was playing could switch the noise on for a moment. The MPC's host briefly changes a plugin's first parameter when it re-prepares audio, and that was ON/OFF. That first slot now does nothing; ON/OFF is a new parameter at the end (same switch, same Q-Link). Saved projects open as before.

## 2.0.1 (2026-11)
- The plugin's name on its page now reads **PotentialNoise** (was all capitals), matching its name in the MPC's plugin list. Nothing else changed.

## 2.0.0 (2026-11)
- **Pink Noise is now PotentialNoise**, with the new Potential look and its own repository. It installs as a separate plugin, so remove the old Pink Noise.
- Rewritten engine: calibrated levels (measured, not estimated), fades on switching and level changes (no clicks), and peaks rounded off just below 0 dBFS instead of clipping.
- MIDI from other tracks can no longer change the settings.

## Pink Noise 1.1.0 / 1.0.0 (before the rename)
- One-tap RMS levels from -24 to -12 dB, mono/stereo, full/limited range, RMS and peak reading, a 30-segment level meter.
