"""Cuts SourceData/Audio/SW_LFF_CheerBed.wav (the seat, kickoff and finale bed) from the Free Crowd Cheering Sounds
pack, replacing the synthesized cheer that Tools/Audio/make_lff_audio.py used to write.

Every cheering loop in the pack swells by 9-21 LU (a surprise cheer), so the bed uses the pack's "Ambience" take,
and only its steady middle: the take fades in over its first 15 s and out over its last 4 s. From 15-31 s it holds
within about 1.5 LU. That stretch becomes a seamless 14 s loop (its last 2 s cross-fade into its first 2 s), set to
the synthesized bed's level (-23 LUFS integrated), so BedVolume and the 8 dB duck under the voice are unchanged.

  Tools/Editor/run_headless.sh Tools/Editor/export_cheer_pack.py "LFF cheer"   # Saved/CheerExport/*.wav
  python3 Tools/Audio/make_cheer_bed.py
Then import with Tools/Editor/import_lff_content.py (step "audio"). Needs numpy and ffmpeg (for the loudness check).
"""
import os
import re
import subprocess
import wave

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Saved", "CheerExport", "WAV_Crowd_Cheering_Ambience.wav")
OUT = os.path.join(ROOT, "SourceData", "Audio", "SW_LFF_CheerBed.wav")
START_S, END_S, FADE_S = 15.0, 31.0, 2.0
TARGET_LUFS = -23.0


def read(path):
    with wave.open(path) as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        if width != 2:
            raise RuntimeError(f"{path}: expected 16-bit PCM, got {8 * width}-bit")
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
    return rate, x.reshape(-1, ch)


def write(path, rate, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(x, -1, 1) * 32767).round().astype("<i2").tobytes())


def loudness(path):
    err = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-af", "ebur128=peak=true", "-f", "null", "-"],
                         capture_output=True, text=True).stderr
    summary = err[err.rfind("Summary:"):]
    return (float(re.search(r"I:\s+(-?[\d.]+) LUFS", summary).group(1)),
            float(re.search(r"LRA:\s+(-?[\d.]+) LU", summary).group(1)),
            float(re.search(r"Peak:\s+(-?[\d.]+) dBFS", summary).group(1)))


def main():
    rate, x = read(SRC)
    seg = x[int(START_S * rate):int(END_S * rate)]
    fade = int(FADE_S * rate)
    # Loop body: the segment after its first 2 s, with its last 2 s cross-faded (equal power) into the first 2 s.
    # The loop's end then runs straight into seg[fade], which is where the body starts.
    body = seg[fade:].copy()
    t = np.linspace(0.0, 1.0, fade, endpoint=False)[:, None]
    body[-fade:] = seg[-fade:] * np.cos(t * np.pi / 2) + seg[:fade] * np.sin(t * np.pi / 2)
    write(OUT, rate, body)
    lufs, _, _ = loudness(OUT)
    write(OUT, rate, body * 10 ** ((TARGET_LUFS - lufs) / 20))
    lufs, lra, peak = loudness(OUT)
    print(f"wrote {os.path.relpath(OUT, ROOT)}: {len(body) / rate:.1f} s loop, {body.shape[1]} ch, {rate} Hz, "
          f"{lufs:.1f} LUFS, LRA {lra:.1f} LU, peak {peak:.1f} dBFS")


if __name__ == "__main__":
    main()
