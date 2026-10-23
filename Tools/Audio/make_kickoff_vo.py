"""Voice lines for the kickoff templates (SourceData/Journey/LFF_KickoffTemplates.json).

A card whose words differ from the 1:00 PM tour gets its own line, named <base line>__<template Id> (or __g<game date>
when it names the opponent); web/src/journey/gameday.ts makes the names and web/tools/kickoff_vo_lines.ts lists them in
SourceData/Audio/VO/Kickoff/KICKOFF_VO_LINES.json. Recorded exactly as the main lines (VO_MANIFEST.md, "Recording"):
ElevenLabs voice Sarah (EXAVITQu4vr4xnSDxMaL), eleven_multilingual_v2, stability 0.7, similarity 0.75, style 0,
speed 0.92; title, <break time="0.7s" />, body, with times and numbers written out the way they are said.

  cd web && npx vite-node tools/kickoff_vo_lines.ts          # the list
  python3 Tools/Audio/make_kickoff_vo.py script               # KICKOFF_VO_SCRIPT.json: the text to send per line
  (record each line into one folder as <name>.mp3 or .wav)
  python3 Tools/Audio/make_kickoff_vo.py master <folder>      # trimmed, -20 LUFS, 0.5 s head, 0.8 s tail, mono 48 kHz
  python3 Tools/Audio/make_kickoff_vo.py words                # <name>.words.json for the karaoke highlight
Needs ffmpeg and numpy.
"""
import importlib.util
import json
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "SourceData", "Audio", "VO", "Kickoff")
LINES = os.path.join(OUT, "KICKOFF_VO_LINES.json")
SCRIPT = os.path.join(OUT, "KICKOFF_VO_SCRIPT.json")

ONES = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen " \
       "seventeen eighteen nineteen".split()
TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()
ORDINAL = {1: "first", 2: "second", 3: "third", 4: "fourth", 5: "fifth", 6: "sixth", 7: "seventh", 8: "eighth",
           9: "ninth", 10: "tenth", 11: "eleventh", 12: "twelfth", 13: "thirteenth", 14: "fourteenth", 15: "fifteenth",
           16: "sixteenth", 17: "seventeenth", 18: "eighteenth", 19: "nineteenth", 20: "twentieth", 30: "thirtieth"}
MONTHS = "January February March April May June July August September October November December".split()


def words(n):
    if n < 20:
        return ONES[n]
    return TENS[n // 10] + ("-" + ONES[n % 10] if n % 10 else "")


def ordinal(n):
    if n in ORDINAL:
        return ORDINAL[n]
    return TENS[n // 10] + "-" + ORDINAL[n % 10]


def section(n):
    """121 -> "one twenty-one", 206 -> "two oh six"."""
    hundreds, rest = divmod(n, 100)
    return f"{ONES[hundreds]} " + (f"oh {ONES[rest]}" if rest < 10 else words(rest))


def spoken(text):
    """The card's words as they are said (the card itself is unchanged)."""
    def time(m):
        h, mm, half = int(m.group(1)), int(m.group(2)), m.group(3)
        said = ONES[h] if h < 20 else words(h)
        if mm:
            said += " oh " + ONES[mm] if mm < 10 else " " + words(mm)
        return f"{said} {half[0]} M"
    text = re.sub(r"(\d{1,2}):(\d{2})\s*(AM|PM)", time, text)
    text = re.sub(r"\b(" + "|".join(MONTHS) + r") (\d{1,2})\b", lambda m: f"{m.group(1)} {ordinal(int(m.group(2)))}", text)
    text = text.replace("50/50 EAF", "fifty-fifty E A F")
    text = re.sub(r"\b(1\d\d|2\d\d)\b", lambda m: section(int(m.group(1))), text)
    text = text.replace(" vs. ", " versus ")
    return text


def write_script():
    lines = json.load(open(LINES, encoding="utf-8"))
    script = {name: f'{spoken(l["title"])} <break time="0.7s" /> {spoken(l["body"])}' for name, l in lines.items()}
    with open(SCRIPT, "w", encoding="utf-8") as f:
        json.dump(script, f, indent=2)
        f.write("\n")
    print(f"{len(script)} lines -> {SCRIPT}")


def master(folder):
    lines = json.load(open(LINES, encoding="utf-8"))
    for name in lines:
        raw = next((os.path.join(folder, f"{name}{ext}") for ext in (".wav", ".mp3", ".pcm")
                    if os.path.exists(os.path.join(folder, f"{name}{ext}"))), None)
        if raw is None:
            print(f"{name}: not recorded, skipped")
            continue
        src = ["-f", "s16le", "-ar", "48000", "-ac", "1", "-i", raw] if raw.endswith(".pcm") else ["-i", raw]
        chain = ("silenceremove=start_periods=1:start_threshold=-50dB:start_silence=0.05,"
                 "areverse,silenceremove=start_periods=1:start_threshold=-50dB:start_silence=0.05,areverse,"
                 "loudnorm=I=-20:TP=-3:LRA=7,adelay=500:all=1,apad=pad_dur=0.8")
        out = os.path.join(OUT, f"{name}.wav")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *src, "-af", chain, "-ac", "1", "-ar", "48000",
                        "-sample_fmt", "s16", out], check=True)
        print(f"{name}: mastered")


def align_words():
    spec = importlib.util.spec_from_file_location("make_vo_words", os.path.join(os.path.dirname(__file__), "make_vo_words.py"))
    vo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(vo)
    lines = json.load(open(LINES, encoding="utf-8"))
    for name, l in lines.items():
        path = os.path.join(OUT, f"{name}.wav")
        if not os.path.exists(path):
            print(f"{name}: no wav, skipped")
            continue
        tokens = f"{l['title']} {l['body']}".split()
        start, end, pauses = vo.speech_and_pauses(vo.envelope(path))
        times = vo.align(tokens, start, end, pauses)
        with open(os.path.join(OUT, f"{name}.words.json"), "w", encoding="utf-8") as f:
            json.dump({"wave": name, "words": [{"w": t, "t": s} for t, s in zip(tokens, times)]}, f, indent=0)
        print(f"{name}: {len(tokens)} words, speech {start:.2f}-{end:.2f} s, {len(pauses)} pauses")


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "script":
        write_script()
    elif command == "master" and len(sys.argv) > 2:
        master(sys.argv[2])
    elif command == "words":
        align_words()
    else:
        print(__doc__)
        sys.exit(1)
