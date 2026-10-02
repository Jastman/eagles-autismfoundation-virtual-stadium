"""Word timings for the karaoke highlight: when each word of a step card starts in its voice line.

  python3 Tools/Audio/make_vo_words.py        # writes SourceData/Audio/VO/VO_*.words.json
Then Tools/Editor/finish_pass.py step "voicewords" writes them onto the voice SoundWaves as subtitle cues (one per
word, suppressed on screen), which ULFFJourneySubsystem reads while the voice plays.

Nothing leaves the machine: the timings are aligned locally from the known script and the audio itself. The card's words
(title, then body, split at spaces, exactly as the card shows them) are expanded to how they were spoken ("10:00 AM"
was sent as "ten A M", VO_MANIFEST.md) and weighted by syllables. Pauses in the voice are found from its energy (10 ms
frames); a small dynamic program matches pauses to word boundaries, preferring the ones after punctuation (the title
and body are 0.7 s apart), and the words between two matched pauses share that stretch of speech by syllables. Good to
a word or so, not to the phoneme: the highlight moves word by word with the voice.
Needs numpy.
"""
import json
import os
import re
import wave

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
JOURNEY = os.path.join(ROOT, "SourceData", "Journey", "LFF_GoldenPath.json")
VO = os.path.join(ROOT, "SourceData", "Audio", "VO")
FRAME = 0.01
MIN_PAUSE = 0.12

ONES = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen " \
       "seventeen eighteen nineteen".split()
TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()


def number_words(n):
    if n < 20:
        return [ONES[n]]
    if n < 100:
        return [TENS[n // 10]] + ([ONES[n % 10]] if n % 10 else [])
    return [ONES[n // 100], "hundred"] + (number_words(n % 100) if n % 100 else [])


def spoken(token):
    """How a card word was said (VO_MANIFEST.md: times and numbers written out)."""
    t = token.strip(".,!?;:\"'()")
    m = re.fullmatch(r"(\d{1,2}):(\d{2})", t)
    if m:
        h, mm = int(m.group(1)), int(m.group(2))
        return number_words(h) + ([] if mm == 0 else (["oh"] + number_words(mm) if mm < 10 else number_words(mm)))
    if re.fullmatch(r"\d+", t):
        return number_words(int(t))
    if t in ("AM", "PM"):
        return [t[0], "M"]
    return [t] if t else []


def syllables(word):
    w = word.lower()
    if len(w) <= 1:
        return 1  # a letter said on its own ("A", "M")
    groups = re.findall(r"[aeiouy]+", w)
    n = len(groups)
    if w.endswith("e") and not w.endswith(("le", "ee")) and n > 1:
        n -= 1
    return max(1, n)


def weight(token):
    words = spoken(token)
    return 0.35 + sum(syllables(w) for w in words) if words else 0.35


def envelope(path):
    with wave.open(path) as w:
        rate = w.getframerate()
        s = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)
        if w.getnchannels() > 1:
            s = s.reshape(-1, w.getnchannels()).mean(1)
    hop = int(rate * FRAME)
    frames = s[: len(s) // hop * hop].reshape(-1, hop)
    return np.sqrt((frames ** 2).mean(1)) + 1e-9


def speech_and_pauses(env):
    db = 20 * np.log10(env / env.max())
    voiced = db > -38
    idx = np.flatnonzero(voiced)
    start, end = idx[0], idx[-1] + 1
    pauses = []
    i = start
    while i < end:
        if not voiced[i]:
            j = i
            while j < end and not voiced[j]:
                j += 1
            if (j - i) * FRAME >= MIN_PAUSE:
                pauses.append((i * FRAME, j * FRAME))
            i = j
        else:
            i += 1
    return start * FRAME, end * FRAME, pauses


def align(tokens, start, end, pauses):
    w = np.array([weight(t) for t in tokens])
    cum = np.concatenate([[0], np.cumsum(w)])  # boundary k is before token k
    # Voiced time (pauses taken out): where each pause sits in it.
    voiced_total = (end - start) - sum(b - a for a, b in pauses)
    v_pos, acc, last = [], 0.0, start
    for a, b in pauses:
        acc += a - last
        v_pos.append(acc)
        last = b
    rate = voiced_total / cum[-1]
    punct = [bool(re.search(r"[.,!?;:]$", t)) for t in tokens]
    n, m = len(tokens), len(pauses)
    SKIP = 4.0  # an unplaced pause (a breath mid-phrase)

    def match_cost(j, k):
        """Pause j on the boundary before token k."""
        d = (cum[k] * rate - v_pos[j]) / max(rate * 2.5, 1e-3)
        long_pause = pauses[j][1] - pauses[j][0] > 0.3
        return d * d + (0.0 if punct[k - 1] else (1.5 if long_pause else 0.6))

    # f[j][k]: best cost after the first j pauses, the last matched boundary being k (0: none yet).
    INF = float("inf")
    f = [[INF] * n for _ in range(m + 1)]
    back = [[None] * n for _ in range(m + 1)]
    f[0][0] = 0.0
    for j in range(m):
        for k in range(n):
            if f[j][k] == INF:
                continue
            if f[j][k] + SKIP < f[j + 1][k]:
                f[j + 1][k], back[j + 1][k] = f[j][k] + SKIP, (k, False)
            for k2 in range(k + 1, n):
                c = f[j][k] + match_cost(j, k2)
                if c < f[j + 1][k2]:
                    f[j + 1][k2], back[j + 1][k2] = c, (k, True)
    k = min(range(n), key=lambda kk: f[m][kk])
    matches = {}
    for j in range(m, 0, -1):
        prev, matched = back[j][k]
        if matched:
            matches[k] = j - 1
        k = prev
    # Anchors in real time: (boundary index, time the next word starts).
    anchors = [(0, start)] + sorted((k, pauses[p][1]) for k, p in matches.items() if k > 0) + [(n, end)]
    ends = {k: pauses[p][0] for k, p in matches.items() if k > 0}
    times = []
    for (k0, t0), (k1, t1) in zip(anchors, anchors[1:]):
        stop = ends.get(k1, t1)
        span = max(stop - t0, 0.05)
        for k in range(k0, k1):
            times.append(round(t0 + span * (cum[k] - cum[k0]) / (cum[k1] - cum[k0]), 3))
    return times


def main():
    rows = json.load(open(JOURNEY, encoding="utf-8"))
    for row in rows:
        wave_path = row.get("VoiceWave") or ""
        if not wave_path:
            continue
        name = wave_path.rsplit("/", 1)[-1].split(".")[0]
        path = os.path.join(VO, f"{name}.wav")
        if not os.path.exists(path):
            print(f"{name}: no wav, skipped")
            continue
        tokens = f"{row['Title']} {row['Body']}".split()
        start, end, pauses = speech_and_pauses(envelope(path))
        times = align(tokens, start, end, pauses)
        out = os.path.join(VO, f"{name}.words.json")
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"wave": name, "words": [{"w": t, "t": s} for t, s in zip(tokens, times)]}, f, indent=0)
        print(f"{name}: {len(tokens)} words, speech {start:.2f}-{end:.2f} s, {len(pauses)} pauses")


if __name__ == "__main__":
    main()
