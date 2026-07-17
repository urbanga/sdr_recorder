# SDR Recorder

`sdr-recorder` monitors one RTL-SDR frequency and writes each detected radio
session to a separate MP3 file. It talks directly to `librtlsdr` and
`libmp3lame`; it does not start `rtl_fm`, `ffmpeg`, or any other process.

## Build

Required development/runtime packages:

- C++20 compiler and CMake 3.18+
- `librtlsdr`
- `libmp3lame`
- GoogleTest (only for building tests)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
(cd build && ctest --output-on-failure)
```

The resulting application is `build/sdr-recorder`. The application is one
executable, but the system must provide the shared RTL-SDR, libusb, and LAME
libraries. A fully static portable build can be added for a specific target
Linux distribution.

## Usage

```bash
./build/sdr-recorder \
  --frequency 446.18125M \
  --modulation nfm \
  --squelch-dbfs -35 \
  --silence 10 \
  --level-meter \
  --output recordings
```

For a continuously transmitting FM broadcast station, force the squelch open
and use `--max-session` to split the recording into fixed-duration files:

```bash
./build/sdr-recorder \
  --frequency 96.6M \
  --modulation wfm \
  --squelch-dbfs -100 \
  --max-session 30 \
  --bitrate 128 \
  --output recordings
```

WFM uses 1.2 MS/s IQ, a 200 kHz broadcast channel, 50 us European
de-emphasis, and 48 kHz mono audio with a 30 Hz high-pass and 15 kHz low-pass.
Stereo decoding is not currently implemented. For mono broadcast audio,
128 kb/s MP3 is recommended.

Use `--help` for all parameters. Files are named using the local session start
time and frequency, for example:

```text
recordings/20260716_153045_247_446006250Hz.mp3
```

The millisecond timestamp keeps independently detected sessions and consecutive
parts created by `--max-session` unique. No additional counter suffix is used.

The recording begins with the first block whose signal level reaches the
squelch threshold. Short pauses are retained as digital silence if the signal
resumes before the configured boundary. Once `--silence` expires, that pending
trailing pause is discarded and the MP3 is closed. A later transmission creates
a new file.

When `--max-session SECONDS` is supplied, an active recording is split exactly
at that duration. A continuous station with `--max-session 30` therefore
creates a new MP3 every 30 seconds. Without this option, session length is
limited only by squelch and process shutdown.

## Calibrating squelch

The correct threshold depends on the receiver, antenna, tuner gain, and local
noise. Start with automatic gain and `-35 dBFS`. If recordings start when the
channel is idle, raise the value (for example to `-30`). If transmissions are
missed, lower it (for example to `-42`). For stable installations, manual
`--gain` generally gives more repeatable detection. `--level-meter` prints the
measured level once per second and whether the squelch is open.

## Removing a persistent receiver tone

An installation-specific narrow interference tone can be removed without
changing the normal speech filter:

```bash
--notch-frequency 1890 --notch-width 120
```

The notch is disabled unless `--notch-frequency` is supplied. It uses two
narrow stages for strong rejection while preserving more surrounding speech;
its width should be only large enough to cover the observed tone drift.

## Design

The core has three independent responsibilities:

- `Demodulator`: IQ to mono PCM and signal-level measurement
- `SessionRecorder`: the tested session/silence state machine
- `AudioSink`: an output boundary implemented by the MP3 encoder

Hardware and MP3 code remain at the application boundary, so unit tests run
without an RTL-SDR device and without creating files.

The tuner uses quarter-sample-rate offset tuning and digitally moves the wanted
channel back to baseband. This keeps the received carrier away from the common
RTL-SDR DC spike. A 129-tap channel filter suppresses that spike and other
out-of-channel energy before IQ decimation. NFM audio then passes through
50 us de-emphasis, a 150 Hz high-pass, and a 101-tap 3.4 kHz speech low-pass
before MP3 encoding. WFM uses a separate 1.2 MHz to 240 kHz channel stage so
the wide FM deviation is preserved before audio filtering and resampling to
48 kHz.
