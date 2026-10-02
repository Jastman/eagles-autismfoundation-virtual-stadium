"""Synthesizes the placeholder sounds into SourceData/Audio/ (no recordings, so nothing to license).

  SW_LFF_AmbientPlaza.wav         24 s seamless loop: low plaza and crowd murmur. No horns, whistles or fireworks.
  SW_LFF_CheerBed.wav             only with --synth-cheer: a synthesized stand-in for the seat bed. The real bed is
                                  cut from the Free Crowd Cheering Sounds pack by Tools/Audio/make_cheer_bed.py.
  SW_LFF_PreviewCrowdMuffled.wav  8 s, plays once: a muffled crowd swell and distant music for the optional
                                  "loud music, horns and whistles" preview. Heard as if through a wall.

Run from the repo root:  python3 Tools/Audio/make_lff_audio.py
Then import with Tools/Editor/import_lff_content.py (Docs/ManualSetup.md). Needs numpy.

Everything is shaped in the frequency domain over the whole buffer, so the ambient bed loops with no seam, and
every envelope is slow (under 4 Hz), so nothing clicks, pops or startles.
"""
import os
import sys
import wave

import numpy as np

RATE = 32000
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "SourceData", "Audio")
rng = np.random.default_rng(20261018)  # fixed seed: the same files every run


def shaped_noise(n, gain_of_hz):
    """White noise filtered by a magnitude response, circularly (so the result loops)."""
    spectrum = np.fft.rfft(rng.standard_normal(n))
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    return np.fft.irfft(spectrum * gain_of_hz(freqs), n)


def band(centre, width):
    return lambda f: np.exp(-0.5 * ((f - centre) / width) ** 2)


def lowpass(cutoff, order=2):
    return lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff) ** (2 * order))


def slow_envelope(n, max_hz, floor=0.0):
    """A smooth, positive, looping envelope with no energy above max_hz."""
    env = shaped_noise(n, lambda f: (f < max_hz).astype(float))
    env = (env - env.min()) / (env.max() - env.min() + 1e-9)
    return floor + (1.0 - floor) * env ** 1.5


def babble(n, voices, env_hz, spread):
    """Many overlapping talkers: formant-band noise, each with its own slow syllable envelope."""
    out = np.zeros(n)
    for _ in range(voices):
        centre = rng.uniform(*spread)
        out += shaped_noise(n, band(centre, centre * 0.28)) * slow_envelope(n, env_hz, floor=0.15)
    return out / np.sqrt(voices)


def normalise(x, rms_db, peak_db):
    x = x * (10 ** (rms_db / 20) / (np.sqrt(np.mean(x ** 2)) + 1e-12))
    peak = 10 ** (peak_db / 20)
    return peak * np.tanh(x / peak)  # soft ceiling, no hard clipping


def write(name, x):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.normpath(os.path.join(OUT, name))
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"wrote {path}: {len(x) / RATE:.1f} s, rms {20 * np.log10(np.sqrt(np.mean(x ** 2))):.1f} dBFS, "
          f"peak {20 * np.log10(np.max(np.abs(x))):.1f} dBFS")


def ambient_plaza():
    n = RATE * 24
    talk = babble(n, voices=28, env_hz=2.5, spread=(320, 1500))
    rumble = shaped_noise(n, lowpass(160, 3)) * 0.8
    air = shaped_noise(n, band(2400, 900)) * 0.04
    # Distance: most of the brightness gone, the way a plaza sounds from a few metres back.
    mix = np.fft.irfft(np.fft.rfft(talk + rumble + air) * lowpass(1400, 2)(np.fft.rfftfreq(n, 1.0 / RATE)), n)
    mix *= slow_envelope(n, 0.15, floor=0.8)  # the crowd ebbs and flows a little
    return normalise(mix, rms_db=-24, peak_db=-8)


def cheer_bed():
    n = RATE * 24
    # Many more voices than the plaza, pitched up a little and brighter: a stadium roar heard from the seats.
    roar = babble(n, voices=60, env_hz=1.5, spread=(350, 2200)) * 1.2
    ooh = shaped_noise(n, band(500, 180)) * slow_envelope(n, 0.3, floor=0.4) * 0.5  # slow "ooh" swells
    rumble = shaped_noise(n, lowpass(140, 3)) * 0.6
    mix = np.fft.irfft(np.fft.rfft(roar + ooh + rumble) * lowpass(2600, 2)(np.fft.rfftfreq(n, 1.0 / RATE)), n)
    mix *= slow_envelope(n, 0.12, floor=0.7)  # the roar rises and settles; nothing sudden
    return normalise(mix, rms_db=-23, peak_db=-8)


def preview_crowd_muffled():
    n = RATE * 8
    t = np.arange(n) / RATE
    crowd = babble(n, voices=40, env_hz=3.5, spread=(300, 1300)) * 1.4
    # Distant music: a low chord with a soft 2 Hz pulse (a smooth swell, not a beat with an attack).
    chord = sum(np.sin(2 * np.pi * hz * t) * a for hz, a in ((110, 1.0), (164.8, 0.7), (220, 0.5), (277.2, 0.35)))
    music = chord * (0.6 + 0.4 * np.sin(2 * np.pi * 2.0 * t) ** 2) * 0.35
    rumble = shaped_noise(n, lowpass(120, 3)) * 0.9
    # "Through a wall": everything above about 600 Hz rolled off steeply.
    mix = np.fft.irfft(np.fft.rfft(crowd + music + rumble) * lowpass(600, 3)(np.fft.rfftfreq(n, 1.0 / RATE)), n)
    fade_in, fade_out = 1.5, 2.5
    env = np.ones(n)
    a = t < fade_in
    env[a] = 0.5 - 0.5 * np.cos(np.pi * t[a] / fade_in)
    b = t > t[-1] - fade_out
    env[b] = 0.5 - 0.5 * np.cos(np.pi * (t[-1] - t[b]) / fade_out)
    return normalise(mix * env, rms_db=-20, peak_db=-8)


if __name__ == "__main__":
    write("SW_LFF_AmbientPlaza.wav", ambient_plaza())
    if "--synth-cheer" in sys.argv[1:]:  # otherwise keep the pack's bed (make_cheer_bed.py)
        write("SW_LFF_CheerBed.wav", cheer_bed())
    write("SW_LFF_PreviewCrowdMuffled.wav", preview_crowd_muffled())
