"""Writes the voiceover script from the journey data, so the recording can never drift from the cards.

The card is the script: each line is the step's Title, then its Body, exactly as on screen (no extra words).

  python3 Tools/Audio/make_vo_script.py                  # SourceData/Audio/VO/VO_MANIFEST.md + one VO_NN_Name.txt per step
  python3 Tools/Audio/make_vo_script.py --placeholders   # also writes PLACEHOLDER wavs with macOS `say`

The placeholder wavs are a flat synthetic voice for testing playback, ducking and "a new step stops the voice".
They are marked as placeholders in the manifest and must be replaced by the recorded voice before any release.
Unreal never calls a voice service; recordings are dropped into SourceData/Audio/VO/ with the same names and
imported by Tools/Editor/import_lff_content.py.
"""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
JOURNEY = os.path.join(ROOT, "SourceData", "Journey", "LFF_GoldenPath.json")
OUT = os.path.join(ROOT, "SourceData", "Audio", "VO")
RATE = 32000

VOICE_BRIEF = """\
## Voice

- One adult voice for every line. Warm, unhurried, plain. Not an announcer, no hype, no sing-song.
- First person, as written ("I will..."). Read the card exactly: title, a short pause, then the body.
- Pace about 130-140 words per minute, with a clear pause (about 0.6 s) between sentences.
- Leave 0.5 s of silence at the start and 0.8 s at the end of each file. No music, no crowd, no effects baked in.
- Even level from line to line: -20 LUFS integrated, peaks under -3 dBFS. No sibilance spikes.
- Say times the way the card shows them: "ten A M", "eleven thirty A M", "twelve fifty P M", "one P M".
- Deliver mono WAV, 48 kHz (or 44.1 kHz), 16- or 24-bit, named exactly as below.
"""

RECORDING = """\
## Recording

The wavs in this folder are ElevenLabs text-to-speech, generated outside Unreal (never at runtime):

- Voice: Sarah, `voice_id` `EXAVITQu4vr4xnSDxMaL` (premade, calm American adult).
- Model `eleven_multilingual_v2`, stability 0.7, similarity 0.75, style 0, speed 0.92, output `pcm_48000`.
- Text sent: the title, `<break time="0.7s" />`, then the body, with times and section numbers written out the way
  they are said ("ten A M", "twelve fifty P M", "one twenty-one"). The on-screen card text is unchanged.
- Mastered with ffmpeg: leading and trailing silence trimmed, gain to -20 LUFS (peak under -3 dBFS), 0.5 s head and
  0.8 s tail of silence, mono 48 kHz 16-bit.

To redo a line, regenerate it with the same voice and settings so it matches the others.
"""


def load_steps():
    with open(JOURNEY, encoding="utf-8") as f:
        rows = json.load(f)
    steps = []
    for row in rows:
        wave = row.get("VoiceWave", "")
        name = wave.rsplit("/", 1)[-1].split(".", 1)[0] if wave else ""
        if not name:
            continue
        steps.append({"id": row["Name"], "file": name, "title": row["Title"].strip(), "body": row["Body"].strip(),
                      "clock": row.get("ClockText", "")})
    return steps


def script_text(step):
    return f"{step['title']} {step['body']}".strip()


def write_manifest(steps, placeholders):
    lines = ["# Voiceover manifest", "",
             "Lincoln Financial Field virtual game-day guide. Example game: Sunday, October 18, 2026,",
             "Carolina Panthers at Philadelphia Eagles, kickoff 1:00 PM ET.", "",
             "Generated from `SourceData/Journey/LFF_GoldenPath.json` by `Tools/Audio/make_vo_script.py`. "
             "Do not edit the lines here; edit the journey data and regenerate.", "",
             "Each file is the step card's title, then its body, word for word. The splash (step 01) has no voice.", "",
             VOICE_BRIEF,
             "## Delivery", "",
             "Put the finished files in `SourceData/Audio/VO/` with exactly these names, then run "
             "`Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py \"LFF content\"` with the editor closed. "
             "A missing file is skipped at runtime; the card still shows.", ""]
    if not placeholders:
        lines += [RECORDING]
    if placeholders:
        lines += ["> **The wavs in this folder are placeholders** (macOS `say`, a synthetic voice) for testing playback only. "
                  "Replace every one with the recorded voice before a release.", ""]
    lines += ["## Lines", "", "| File | Step | Clock | Script (read exactly) |", "|---|---|---|---|"]
    for s in steps:
        lines.append(f"| `{s['file']}.wav` | {s['id']} | {s['clock'] or '-'} | {script_text(s).replace('|', '/')} |")
    lines += ["", f"{len(steps)} files. Every line is also in its own `.txt` next to this manifest.", ""]
    with open(os.path.join(OUT, "VO_MANIFEST.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def write_placeholder(step):
    wav = os.path.join(OUT, f"{step['file']}.wav")
    with tempfile.TemporaryDirectory() as tmp:
        aiff = os.path.join(tmp, "line.aiff")
        # A title, a pause, then the body; about 150 wpm.
        text = f"{step['title']} [[slnc 600]] {step['body']}"
        subprocess.run(["say", "-r", "150", "-o", aiff, text], check=True)
        subprocess.run(["afconvert", "-f", "WAVE", "-d", f"LEI16@{RATE}", "-c", "1", aiff, wav], check=True)
    return wav


def main():
    placeholders = "--placeholders" in sys.argv[1:]
    os.makedirs(OUT, exist_ok=True)
    steps = load_steps()
    for s in steps:
        with open(os.path.join(OUT, f"{s['file']}.txt"), "w", encoding="utf-8") as f:
            f.write(script_text(s) + "\n")
        if placeholders:
            write_placeholder(s)
    write_manifest(steps, placeholders)
    print(f"wrote {len(steps)} lines{' and placeholder wavs' if placeholders else ''} to {OUT}")


if __name__ == "__main__":
    main()
