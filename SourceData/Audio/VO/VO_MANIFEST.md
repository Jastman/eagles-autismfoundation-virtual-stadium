# Voiceover manifest

Lincoln Financial Field virtual game-day guide. Example game: Sunday, October 18, 2026,
Carolina Panthers at Philadelphia Eagles, kickoff 1:00 PM ET.

Generated from `SourceData/Journey/LFF_GoldenPath.json` by `Tools/Audio/make_vo_script.py`. Do not edit the lines here; edit the journey data and regenerate.

Each file is the step card's title, then its body, word for word. The splash (step 01) has no voice.

## Voice

- One adult voice for every line. Warm, unhurried, plain. Not an announcer, no hype, no sing-song.
- First person, as written ("I will..."). Read the card exactly: title, a short pause, then the body.
- Pace about 130-140 words per minute, with a clear pause (about 0.6 s) between sentences.
- Leave 0.5 s of silence at the start and 0.8 s at the end of each file. No music, no crowd, no effects baked in.
- Even level from line to line: -20 LUFS integrated, peaks under -3 dBFS. No sibilance spikes.
- Say times the way the card shows them: "ten A M", "eleven thirty A M", "twelve fifty P M", "one P M".
- Deliver mono WAV, 48 kHz (or 44.1 kHz), 16- or 24-bit, named exactly as below.

## Delivery

Put the finished files in `SourceData/Audio/VO/` with exactly these names, then run `Tools/Editor/run_headless.sh Tools/Editor/import_lff_content.py "LFF content"` with the editor closed. A missing file is skipped at runtime; the card still shows.

## Recording

The wavs in this folder are ElevenLabs text-to-speech, generated outside Unreal (never at runtime):

- Voice: Sarah, `voice_id` `EXAVITQu4vr4xnSDxMaL` (premade, calm American adult).
- Model `eleven_multilingual_v2`, stability 0.7, similarity 0.75, style 0, speed 0.92, output `pcm_48000`.
- Text sent: the title, `<break time="0.7s" />`, then the body, with times and section numbers written out the way
  they are said ("ten A M", "twelve fifty P M", "one twenty-one"). The on-screen card text is unchanged.
- Mastered with ffmpeg: leading and trailing silence trimmed, gain to -20 LUFS (peak under -3 dBFS), 0.5 s head and
  0.8 s tail of silence, mono 48 kHz 16-bit.

To redo a line, regenerate it with the same voice and settings so it matches the others.

## Lines

| File | Step | Clock | Script (read exactly) |
|---|---|---|---|
| `VO_02_Intro.wav` | intro | ~9:00 AM | Today I am going to the Eagles game at Lincoln Financial Field. The Eagles are playing the Carolina Panthers on October 18. Kickoff is at 1:00 PM. We will go one step at a time. You choose when to move on. |
| `VO_03_Parking.wav` | parking | ~9:00 AM | When I arrive, we will park the car in the parking lot. The parking lots are already open before noon for a 1:00 PM game. We can take our time. |
| `VO_04_Line.wav` | entrance-line | ~9:30 AM | I will walk to the entrance and wait in line. There may be a line. Waiting is normal. |
| `VO_05_Security.wav` | security | ~9:45 AM | I will walk through security to keep myself and others safe. Bags are checked here. I can go slowly. |
| `VO_06_PlazaGates.wav` | plaza-gates | 10:00 AM | The Pepsi Plaza gates open at 10:00 AM. That is about three hours before kickoff. The gate can open when it is time. |
| `VO_07_ClearBags.wav` | clear-bags | - | The stadium only allows clear bags. I will use a clear bag or no bag. |
| `VO_08_Ticket.wav` | mobile-ticket | - | When I get to the front of the line, I will show my mobile ticket. The ticket is on a phone. I hold it so staff can see it. |
| `VO_09_Plaza.wav` | plaza | - | Once inside, I can look for activities on the plaza. The plaza can be busy and loud. I can leave when I want. |
| `VO_10_Popcorn.wav` | popcorn | - | I can visit the Popcorn for the People stand on Pepsi Plaza. This stand helps the Eagles Autism Foundation. I do not have to buy anything. |
| `VO_11_SensoryVehicle.wav` | sensory-vehicle | - | If I need a break on the plaza, I can go to the Sensory Activation Vehicle. It is a quieter space outside. I can use it if I want. |
| `VO_12_ClubGates.wav` | club-gates | 11:00 AM | The club suite gates open at 11:00 AM. That is about two hours before kickoff. This step is only to show the time. |
| `VO_13_InnerGates.wav` | inner-gates | 11:30 AM | The inner stadium gates open at 11:30 AM. That is about one and a half hours before kickoff. I can walk toward my section when I am ready. |
| `VO_14_Flares.wav` | gate-flares | 11:50 AM, 12:00 PM, 12:15 PM | There will be gate flares around 11:50 AM, 12:00 PM, and 12:15 PM. The flares remind people to start walking to their seats. They can be bright and loud. Preview is optional. |
| `VO_15_WalkToSeats.wav` | walk-to-seats | - | I can start walking to my seats in my section. I can go early if I want a quieter walk. |
| `VO_16_LoudSounds.wav` | loud-sounds | - | I will hear loud music, horns, and whistles at the game. Preview is optional. I can skip it. |
| `VO_17_BrightLights.wav` | bright-lights | - | There may be bright stadium lights during the game. Preview is optional and will not strobe. I can skip it. |
| `VO_18_Anthem.wav` | anthem | 12:50 PM | Fireworks and the national anthem happen around 12:50 PM. Pending weather, a bald eagle may fly over the field during the anthem. Preview is optional. |
| `VO_19_Kickoff.wav` | kickoff | 1:00 PM | The game begins at about 1:00 PM. The Eagles are playing the Carolina Panthers. |
| `VO_20_GuestServices.wav` | guest-services | - | If I need a break or a sensory item, I can go to Guest Services. Booths are at sections 121, 136, 206, and 226. |
| `VO_21_SensoryRoom.wav` | sensory-room | - | If I need a longer break, I can go to the sensory room. It is on the West Lower Suite Level. Use the elevators near sections 105 and 205. |
| `VO_22_Food.wav` | food | - | If I am hungry or thirsty, I can buy food or drink on the main concourse. I can also wait. |
| `VO_23_Staff.wav` | staff | - | If I have questions, I can ask a staff member. Staff can help me find things. |
| `VO_24_Finale.wav` | finale | - | Have fun and Go Eagles! This preview is an illustration. It is not directions for the real stadium. |

23 files. Every line is also in its own `.txt` next to this manifest.
