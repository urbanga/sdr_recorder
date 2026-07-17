# SDR Recorder

`sdr-recorder` monitors one RTL-SDR frequency and writes each detected radio
session to a separate MP3 file. It talks directly to `librtlsdr` and
`libmp3lame`; it does not start `rtl_fm`, `ffmpeg`, or any other process.

## Build

Required development/runtime packages:

- C++20 compiler and CMake 3.20+
- `librtlsdr`
- `libmp3lame`
- GoogleTest (only for building tests)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
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

Use `--help` for all parameters. Files are named using the local session start
time and frequency, for example:

```text
recordings/20260716_153045_446006250Hz_000.mp3
```

The recording begins with the first block whose signal level reaches the
squelch threshold. Short pauses are retained as digital silence if the signal
resumes before the configured boundary. Once `--silence` expires, that pending
trailing pause is discarded and the MP3 is closed. A later transmission creates
a new file.

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
before MP3 encoding.
