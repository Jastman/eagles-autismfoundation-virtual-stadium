// The Unreal audio components the journey drives (UGameplayStatics::CreateSound2D, FadeIn/FadeOut/AdjustVolume, the
// audio device's primary volume) as Web Audio nodes. iPad Safari only starts audio after a touch, so the context is
// created suspended and resumed on the first pointer/key event (unlock()).
//
// Sounds are named as in SourceData/Audio (e.g. "VO_02_Intro"); tools/prepare_assets.mjs writes the manifest that maps
// each name to its transcoded file.

/** Sound name -> file per format ("m4a", "mp3" or "wav"), written by tools/prepare_assets.mjs. */
type Manifest = Record<string, Record<string, string>>;

/** The first format this browser decodes: AAC on iPad Safari (smallest), MP3 elsewhere, the WAV source as a last resort. */
function pickFormat(): string[] {
    const probe = document.createElement("audio");
    const can = (type: string) => probe.canPlayType(type) !== "";
    return [can('audio/mp4; codecs="mp4a.40.2"') ? "m4a" : "", can("audio/mpeg") ? "mp3" : "", "wav"].filter(Boolean);
}

export class SoundHandle {
    private source: AudioBufferSourceNode | null = null;
    private readonly gain: GainNode;
    private startedAt = 0;
    private stopped = false;
    onFinished: (() => void) | null = null;
    /** Component volume multiplier (SetVolumeMultiplier); the fade gain sits on top of it. */
    private multiplier = 1;
    private fadeLevel = 1;
    readonly duration: number;

    constructor(private readonly engine: AudioEngine, readonly name: string, private readonly buffer: AudioBuffer, loop: boolean) {
        const ctx = engine.context;
        this.gain = ctx.createGain();
        this.gain.connect(engine.master);
        this.source = ctx.createBufferSource();
        this.source.buffer = buffer;
        this.source.loop = loop;
        this.source.connect(this.gain);
        this.duration = buffer.duration;
        this.source.onended = () => {
            if (!this.stopped) {
                this.stopped = true;
                this.onFinished?.();
            }
        };
    }

    /** Starts at a level (0..1), optionally fading in from silence over fadeSeconds (UAudioComponent::FadeIn). */
    play(volume: number, fadeSeconds = 0): void {
        const ctx = this.engine.context;
        this.fadeLevel = volume;
        const now = ctx.currentTime;
        this.gain.gain.cancelScheduledValues(now);
        if (fadeSeconds > 0) {
            this.gain.gain.setValueAtTime(0, now);
            this.gain.gain.linearRampToValueAtTime(volume * this.multiplier, now + fadeSeconds);
        } else {
            this.gain.gain.setValueAtTime(volume * this.multiplier, now);
        }
        this.startedAt = now;
        this.source?.start();
    }

    /** UAudioComponent::AdjustVolume: glide to a new level. */
    adjustVolume(seconds: number, volume: number): void {
        this.fadeLevel = volume;
        this.rampTo(volume * this.multiplier, seconds);
    }

    setVolumeMultiplier(multiplier: number): void {
        this.multiplier = multiplier;
        this.rampTo(this.fadeLevel * multiplier, 0.05);
    }

    /** UAudioComponent::FadeOut to silence, then stop. */
    fadeOut(seconds: number): void {
        this.rampTo(0, seconds);
        const source = this.source;
        window.setTimeout(() => {
            if (this.source === source) this.stop();
        }, Math.max(seconds, 0) * 1000 + 30);
    }

    stop(): void {
        if (this.stopped) return;
        this.stopped = true;
        try {
            this.source?.stop();
        } catch {
            /* never started */
        }
        this.source?.disconnect();
        this.source = null;
        this.gain.disconnect();
    }

    isPlaying(): boolean {
        return !this.stopped && this.source !== null;
    }

    /** Seconds into the sound (for the karaoke's word timings). */
    position(): number {
        if (!this.isPlaying()) return 0;
        const t = this.engine.context.currentTime - this.startedAt;
        return this.source?.loop ? t % this.buffer.duration : Math.min(t, this.buffer.duration);
    }

    private rampTo(value: number, seconds: number): void {
        const ctx = this.engine.context;
        const now = ctx.currentTime;
        const param = this.gain.gain;
        param.cancelScheduledValues(now);
        param.setValueAtTime(param.value, now);
        if (seconds > 0) param.linearRampToValueAtTime(value, now + seconds);
        else param.setValueAtTime(value, now);
    }
}

export class AudioEngine {
    readonly context: AudioContext;
    readonly master: GainNode;
    private manifest: Manifest = {};
    private buffers = new Map<string, Promise<AudioBuffer | null>>();
    private decoded = new Map<string, AudioBuffer>();
    /** Sounds that failed to load or decode: treated as missing from then on, never retried. */
    private failed = new Set<string>();
    private formats = pickFormat();
    private unlocked = false;

    constructor() {
        const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
        this.context = new Ctor({ latencyHint: "interactive" });
        this.master = this.context.createGain();
        this.master.connect(this.context.destination);
        const unlock = () => this.unlock();
        for (const type of ["pointerdown", "touchend", "keydown"]) {
            window.addEventListener(type, unlock, { capture: true, passive: true });
        }
    }

    async loadManifest(url: string): Promise<void> {
        try {
            this.manifest = (await (await fetch(url)).json()) as Manifest;
        } catch (error) {
            console.warn("Audio manifest missing; the experience runs silent.", error);
        }
    }

    /** iOS: resume inside a user gesture, and play one silent buffer so the hardware route opens. */
    unlock(): void {
        if (this.context.state !== "running") void this.context.resume();
        if (this.unlocked) return;
        this.unlocked = true;
        const silent = this.context.createBuffer(1, 1, 22050);
        const source = this.context.createBufferSource();
        source.buffer = silent;
        source.connect(this.context.destination);
        source.start(0);
    }

    /** The audio device's primary volume: Sound Off sets it to 0, so nothing at all can be heard. */
    setPrimaryVolume(volume: number): void {
        const now = this.context.currentTime;
        this.master.gain.cancelScheduledValues(now);
        this.master.gain.setValueAtTime(volume, now);
    }

    has(name: string): boolean {
        return name in this.manifest && !this.failed.has(name);
    }

    /** Starts fetching and decoding; resolves null when the sound is missing (a missing line is never an error). */
    preload(name: string): Promise<AudioBuffer | null> {
        if (!name || !this.has(name)) return Promise.resolve(null);
        let pending = this.buffers.get(name);
        if (pending === undefined) {
            const files = this.manifest[name];
            const url = this.formats.map((format) => files[format]).find(Boolean);
            pending = (url ? fetch(url) : Promise.reject(new Error("no playable format")))
                .then((r) => {
                    if (!r.ok) throw new Error(`HTTP ${r.status}`);
                    return r.arrayBuffer();
                })
                .then((data) => this.context.decodeAudioData(data))
                .then((buffer) => {
                    this.decoded.set(name, buffer);
                    return buffer;
                })
                .catch((error) => {
                    console.warn(`Sound ${name} could not be loaded.`, error);
                    this.failed.add(name);
                    return null;
                });
            this.buffers.set(name, pending);
        }
        return pending;
    }

    /** A decoded sound, or null if it isn't ready yet (the caller decides whether to wait). */
    ready(name: string): AudioBuffer | null {
        return this.decoded.get(name) ?? null;
    }

    create(name: string, loop = false): SoundHandle | null {
        const buffer = this.ready(name);
        return buffer === null ? null : new SoundHandle(this, name, buffer, loop);
    }
}
