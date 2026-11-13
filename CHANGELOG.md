# Changelog

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
