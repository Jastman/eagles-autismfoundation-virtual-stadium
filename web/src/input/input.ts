// Port of ALFFPlayerController's input (Enhanced Input built in code): the journey keys, the HUD's focus ring, and free
// flight. On the iPad the same actions come from touch: drag the 3D view to look (as a mouse drag does), pinch to step
// forward or back (the mouse wheel), and the flight pad for WASD / E Q in Free explore (Docs/WEB_PARITY.md).
import type { Journey } from "../journey/journey";
import type { Hud } from "../ui/hud";
import { Vec2 } from "../util/ue";
import { h } from "../ui/dom";

const PINCH_PIXELS_PER_NOTCH = 120;

export class Input {
    private keys = new Set<string>();
    private look = new Vec2();
    private wheel = 0;
    private dragging = false;
    private lastPointer = new Map<number, Vec2>();
    private pinchDistance = 0;
    private stick = new Vec2();
    private rise = 0;

    constructor(private readonly journey: Journey, private readonly hud: Hud, private readonly surface: HTMLElement) {
        window.addEventListener("keydown", (e) => this.onKeyDown(e));
        window.addEventListener("keyup", (e) => this.keys.delete(e.code));
        window.addEventListener("blur", () => {
            this.keys.clear();
            this.endDrag();
        });
        surface.addEventListener("pointerdown", (e) => this.onPointerDown(e));
        window.addEventListener("pointermove", (e) => this.onPointerMove(e));
        window.addEventListener("pointerup", (e) => this.onPointerUp(e));
        window.addEventListener("pointercancel", (e) => this.onPointerUp(e));
        surface.addEventListener("wheel", (e) => {
            e.preventDefault();
            this.wheel += -Math.sign(e.deltaY);
        }, { passive: false });
        // iPad Safari: no page zoom or scroll from gestures on the stadium.
        for (const type of ["gesturestart", "gesturechange", "gestureend"]) document.addEventListener(type, (e) => e.preventDefault());
        this.buildFlyPad(hud.flyPadRoot);
    }

    private onKeyDown(e: KeyboardEvent): void {
        const j = this.journey;
        if (e.metaKey || e.ctrlKey || e.altKey) return;
        const flying = j.canFly();
        const first = !this.keys.has(e.code);
        this.keys.add(e.code);
        if (!first) {
            // Held keys repeat only for flight (polled each frame), never for journey commands.
            if (e.code === "Tab") e.preventDefault();
            return;
        }
        // The flight context (priority 1) wins over the journey's own keys while Free explore can fly.
        const flightKeys = ["KeyW", "KeyA", "KeyS", "KeyD", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "KeyE", "KeyQ", "KeyC", "Space",
            "PageUp", "PageDown", "ShiftLeft", "ShiftRight"];
        if (flying && flightKeys.includes(e.code)) {
            e.preventDefault();
            return;
        }
        let handled = true;
        switch (e.code) {
            case "ArrowRight": case "Space": case "PageDown": j.next(); break;
            case "ArrowLeft": case "Backspace": case "PageUp": j.back(); break;
            case "Enter": case "NumpadEnter":
                // Enter presses the button in the focus ring, if there is one.
                if (!this.hud.activateFocus()) j.confirm();
                break;
            case "Escape":
                this.hud.clearFocus();
                j.cancel();
                break;
            case "KeyP": j.togglePause(); break;
            case "KeyS": j.toggleSettings(); break;
            case "KeyR": j.requestRestart(); break;
            case "Digit1": j.toggleReducedMotion(); break;
            case "Digit2": j.toggleSound(); break;
            case "Digit3": j.toggleCalmMode(); break;
            case "Digit4": j.toggleLargeText(); break;
            case "Digit5": j.toggleVoice(); break;
            case "Digit6": j.toggleCrowd(); break;
            case "KeyV": j.togglePreview(); break;
            case "Tab": this.hud.moveFocus(e.shiftKey ? -1 : 1); break;
            case "KeyT": j.switchToMode("GuidedTour"); break;
            case "KeyE": j.switchToMode("FreeExplore"); break;
            // H means My seat only in Free explore; in the tour the seat is a step, reached with Next.
            case "KeyH": if (j.getMode() === "FreeExplore") j.goToMySeat(); break;
            default: handled = false;
        }
        if (handled) e.preventDefault();
    }

    private onPointerDown(e: PointerEvent): void {
        this.lastPointer.set(e.pointerId, new Vec2(e.clientX, e.clientY));
        if (this.lastPointer.size === 2) {
            const [a, b] = [...this.lastPointer.values()];
            this.pinchDistance = a.sub(b).size();
            this.dragging = false;
        } else if (this.lastPointer.size === 1) {
            this.dragging = true;
        }
        this.surface.setPointerCapture?.(e.pointerId);
    }

    private onPointerMove(e: PointerEvent): void {
        const last = this.lastPointer.get(e.pointerId);
        if (!last) return;
        const now = new Vec2(e.clientX, e.clientY);
        this.lastPointer.set(e.pointerId, now);
        if (this.lastPointer.size >= 2) {
            const [a, b] = [...this.lastPointer.values()];
            const distance = a.sub(b).size();
            this.wheel += (distance - this.pinchDistance) / PINCH_PIXELS_PER_NOTCH;
            this.pinchDistance = distance;
            return;
        }
        if (this.dragging) {
            // Drag the scene with the finger: the view turns the opposite way the content moves, as a mouse-look does.
            const delta = now.sub(last);
            const touch = e.pointerType === "touch";
            this.look = this.look.add(new Vec2(touch ? -delta.x : delta.x, touch ? delta.y : -delta.y));
        }
    }

    private onPointerUp(e: PointerEvent): void {
        this.lastPointer.delete(e.pointerId);
        if (this.lastPointer.size === 0) this.endDrag();
        else if (this.lastPointer.size === 1) this.pinchDistance = 0;
    }

    private endDrag(): void {
        this.dragging = false;
        this.lastPointer.clear();
        // Capture loss never turns into camera motion.
        this.look = new Vec2();
        this.journey.resetLookInput();
    }

    private buildFlyPad(root: HTMLElement): void {
        const knob = h("div", { class: "knob" });
        const stick = h("div", { class: "fly-stick", role: "application", "aria-label": "Fly: drag to move" }, knob);
        let active: number | null = null;
        const update = (e: PointerEvent) => {
            const r = stick.getBoundingClientRect();
            const radius = r.width / 2;
            const v = new Vec2((e.clientX - (r.left + radius)) / radius, (e.clientY - (r.top + radius)) / radius);
            const n = Math.min(v.size(), 1);
            const dir = v.size() > 0 ? v.scale(n / v.size()) : v;
            this.stick = new Vec2(dir.x, -dir.y);
            knob.style.translate = `${dir.x * radius * 0.6}px ${dir.y * radius * 0.6}px`;
        };
        stick.addEventListener("pointerdown", (e) => {
            e.stopPropagation();
            active = e.pointerId;
            stick.setPointerCapture(e.pointerId);
            update(e);
        });
        stick.addEventListener("pointermove", (e) => {
            if (e.pointerId === active) update(e);
        });
        const release = (e: PointerEvent) => {
            if (e.pointerId !== active) return;
            active = null;
            this.stick = new Vec2();
            knob.style.translate = "";
        };
        stick.addEventListener("pointerup", release);
        stick.addEventListener("pointercancel", release);
        const hold = (label: string, value: number) => {
            const b = h("button", { class: "btn", type: "button", tabindex: "-1", text: label });
            b.addEventListener("pointerdown", (e) => {
                e.stopPropagation();
                b.setPointerCapture(e.pointerId);
                this.rise = value;
            });
            const stop = () => {
                if (this.rise === value) this.rise = 0;
            };
            b.addEventListener("pointerup", stop);
            b.addEventListener("pointercancel", stop);
            return b;
        };
        root.append(stick, h("div", { class: "fly-rise" }, hold("Up", 1), hold("Down", -1)));
    }

    /** Called every frame, idle too, so flight eases to a stop. */
    tick(dt: number): void {
        const k = (...codes: string[]) => codes.some((c) => this.keys.has(c));
        const flying = this.journey.canFly();
        let move = new Vec2();
        let rise = 0;
        let fast = false;
        if (flying) {
            move = new Vec2((k("KeyD", "ArrowRight") ? 1 : 0) - (k("KeyA", "ArrowLeft") ? 1 : 0), (k("KeyW", "ArrowUp") ? 1 : 0) - (k("KeyS", "ArrowDown") ? 1 : 0));
            rise = (k("KeyE", "Space", "PageUp") ? 1 : 0) - (k("KeyQ", "KeyC", "PageDown") ? 1 : 0);
            fast = k("ShiftLeft", "ShiftRight");
            move = move.add(this.stick);
            rise = Math.max(-1, Math.min(1, rise + this.rise));
        }
        const look = this.journey.canLookAround() ? this.look : new Vec2();
        if (!this.journey.canLookAround()) this.journey.resetLookInput();
        this.journey.flyTick(move, rise, look, flying ? this.wheel : 0, fast, dt);
        this.look = new Vec2();
        this.wheel = 0;
    }
}
