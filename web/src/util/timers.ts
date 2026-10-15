// A small FTimerManager: timers advance on the app's frame clock (the same delta the camera rig and cues tick with), so
// a hitch or a backgrounded tab delays them together instead of letting a timer fire against a frozen scene.

export class TimerHandle {
    id = 0;
}

interface Entry {
    fn: () => void;
    remaining: number;
    interval: number;
    loop: boolean;
    nextTick: boolean;
}

export class Timers {
    private entries = new Map<number, Entry>();
    private serial = 0;

    setTimer(handle: TimerHandle, fn: () => void, seconds: number, loop = false): void {
        this.clearTimer(handle);
        handle.id = ++this.serial;
        this.entries.set(handle.id, { fn, remaining: Math.max(seconds, 0), interval: Math.max(seconds, 1e-3), loop, nextTick: false });
    }

    /** Runs fn on the next tick (Unreal's SetTimerForNextTick); never re-enters the caller. */
    setTimerForNextTick(fn: () => void): void {
        this.entries.set(++this.serial, { fn, remaining: 0, interval: 0, loop: false, nextTick: true });
    }

    clearTimer(handle: TimerHandle): void {
        if (handle.id !== 0) {
            this.entries.delete(handle.id);
            handle.id = 0;
        }
    }

    isTimerActive(handle: TimerHandle): boolean {
        return handle.id !== 0 && this.entries.has(handle.id);
    }

    tick(dt: number): void {
        // Snapshot: timers set while firing wait for the next tick, as in Unreal.
        const due: [number, Entry][] = [];
        for (const [id, entry] of this.entries) {
            if (entry.nextTick) {
                due.push([id, entry]);
                continue;
            }
            entry.remaining -= dt;
            if (entry.remaining <= 0) due.push([id, entry]);
        }
        for (const [id, entry] of due) {
            if (!this.entries.has(id)) continue;
            if (entry.loop) {
                entry.remaining += entry.interval;
                if (entry.remaining <= 0) entry.remaining = entry.interval;
            } else {
                this.entries.delete(id);
            }
            entry.fn();
        }
    }
}
