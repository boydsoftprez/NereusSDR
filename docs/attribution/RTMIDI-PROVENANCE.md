# RtMidi Provenance - NereusSDR vendored-library inventory

This document catalogs RtMidi as vendored into NereusSDR under
`third_party/rtmidi/`. The desktop app reads MIDI controllers through it
(`src/gui/hotkeys/midi/MidiInput.cpp`). It is compiled into `NereusSDRLib` and
its `NereusGui_LTO` twin only, never `NereusCore` or `nereusd` (R-HK-18,
`tests/tst_core_has_no_gui_includes.cpp`, and a check in `release.yml`).

RtMidi's licence is MIT-style, compatible with NereusSDR's GPLv3.

## Upstream

- **Project:** RtMidi (Gary P. Scavone)
- **Repository:** https://github.com/thestk/rtmidi
- **Tag:** `6.0.0`
- **Commit:** `1e5b49925aa60065db52de44c366d446a902547b`
- **Archive:** https://codeload.github.com/thestk/rtmidi/tar.gz/refs/tags/6.0.0
- **Archive SHA-256:** `ef7bcda27fee6936b651c29ebe9544c74959d0b1583b716ce80a1c6fea7617f0`
- **Vendored:** 2026-11-06 (hot keys plan, Task 10)

The per-file hashes are in `third_party/rtmidi/VERSION.txt`. This is the upstream
tag, not AetherSDR's snapshot.

## License

The upstream `LICENSE` is kept unchanged at `third_party/rtmidi/LICENSE` and
reproduced in `packaging/third-party-licenses/rtmidi.txt`.

## Files vendored

`RtMidi.h`, `RtMidi.cpp` and `LICENSE`. Left out: the C wrapper, tests,
documentation, the Android and Windows contrib folders, and upstream's build files.

## Local change

One patch, to `RtMidi.cpp`: the `RtMidi.cpp` part of AetherSDR commit `5769f480`
("Fix CoreMIDI init crash during MIDI auto-connect", #1949). `throw()` on
`getCoreMidiClientSingleton` ended the program with `std::terminate` when the
CoreMIDI client could not be created, because the function reports that by throwing
`RtMidiError`. With it gone, the error reaches the caller's `catch`. The same commit
corrects one error message's class name. The whole change:

```diff
@@ -108,7 +108,7 @@
   std::string getPortName( unsigned int portNumber );
 
  protected:
-  MIDIClientRef getCoreMidiClientSingleton(const std::string& clientName) throw();
+  MIDIClientRef getCoreMidiClientSingleton(const std::string& clientName);
   void initialize( const std::string& clientName );
 };
 
@@ -128,7 +128,7 @@
   void sendMessage( const unsigned char *message, size_t size );
 
  protected:
-  MIDIClientRef getCoreMidiClientSingleton(const std::string& clientName) throw();
+  MIDIClientRef getCoreMidiClientSingleton(const std::string& clientName);
   void initialize( const std::string& clientName );
 };
 
@@ -1151,7 +1151,7 @@
   delete data;
 }
 
-MIDIClientRef MidiInCore::getCoreMidiClientSingleton(const std::string& clientName) throw() {
+MIDIClientRef MidiInCore::getCoreMidiClientSingleton(const std::string& clientName) {
 
   if (CoreMidiClientSingleton == 0){
       // Set up our client.
@@ -1486,7 +1486,7 @@
   delete data;
 }
 
-MIDIClientRef MidiOutCore::getCoreMidiClientSingleton(const std::string& clientName) throw() {
+MIDIClientRef MidiOutCore::getCoreMidiClientSingleton(const std::string& clientName) {
 
   if (CoreMidiClientSingleton == 0){
       // Set up our client.
@@ -1496,7 +1496,7 @@
       OSStatus result = MIDIClientCreate(name, NULL, NULL, &client );
       if ( result != noErr ) {
         std::ostringstream ost;
-        ost << "MidiInCore::initialize: error creating OS-X MIDI client object (" << result << ").";
+        ost << "MidiOutCore::initialize: error creating OS-X MIDI client object (" << result << ").";
         errorString_ = ost.str();
         error( RtMidiError::DRIVER_ERROR, errorString_ );
         return 0;
```

## Build wiring

The root `CMakeLists.txt` lists `third_party/rtmidi/RtMidi.cpp` in `GUI_SOURCES`,
with warnings off and no precompiled header for that one source.
`nereus_apply_gui_deps` sets the backend and links its system libraries:

| Platform | Define | Libraries |
| --- | --- | --- |
| macOS | `__MACOSX_CORE__` | CoreMIDI, CoreAudio, CoreFoundation |
| Windows | `__WINDOWS_MM__`, `RTMIDI_DO_NOT_ENSURE_UNIQUE_PORTNAMES` | winmm |
| Linux | `__LINUX_ALSA__` | ALSA |

`RTMIDI_DO_NOT_ENSURE_UNIQUE_PORTNAMES` is upstream's own switch. Without it a
Windows port's name ends in its place in the list, which moves when another device
comes or goes and would read as an unplug. On Linux without ALSA, `RtMidi.cpp` is
left out, `NEREUS_HAVE_RTMIDI` stays undefined, and the app reports that this build
has no MIDI support.

## Updating

1. Copy the three files from the new tag into `third_party/rtmidi/`.
2. Check whether the patch above is still needed, and apply it if so.
3. Update `third_party/rtmidi/VERSION.txt`, this file, and the two rows and the text
   file in `packaging/third-party-licenses/`.
4. Run `python3 scripts/check-third-party-licenses.py` and `tst_midi_decoder`,
   `tst_midi_profile` and `tst_core_has_no_gui_includes`.
