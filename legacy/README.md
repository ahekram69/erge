# Windows 7 SP1 x86 compatibility candidate

This is an independent, explicitly TEST-labelled Qt 5.15.2 / MSVC v142 build.
It does not replace the Qt 6 Windows x64 version. Settings and installer identity
are isolated from the main application.

Implemented candidate paths: device selection, native UVC parameters and auto mode,
initial-state restore and saved parameters, preview formats, reconnect polling,
PNG snapshots, mirror/flip/rotation, full screen, hidden controls, system/Chinese/
English language, diagnostics, output folders, and MP4 recording using Windows
Media Foundation (not Qt 5's unsupported camera recorder).

Recording consumes transformed preview frames, with a bounded latest-frame mailbox
and up to 30 fps. Output is video-only, matching the main app's no-microphone capture
configuration. H.264 encoding depends on the installed Windows media components;
high resolutions such as 4K may be rejected on Windows 7. Failure must be shown,
not silently reported as successful recording. N/KN editions require media features.
Failed attempts may leave an incomplete MP4; do not describe it as a saved recording.

CI is Windows Server 2022 running an x86 process, NOT a Windows 7 real-device test.
Before release, test on Windows 7 SP1 x86 with the customer's camera: clean install,
VC2019 runtime/UCRT prerequisite handling, frame orientation, all parameter modes,
restore/preset, unplug/replug, every advertised format, and MP4 playback and timing.
Also test Windows 10 x86 and an extended recording session for responsiveness and
memory stability. Qt 5.15.2 and Windows 7 are old dependencies; keep this candidate
offline and do not treat it as a security-maintained substitute for a supported OS.

The normal build remains Windows 10 1809+ x64. No Win7 runtime acceptance or
feature-parity acceptance has been recorded yet.
