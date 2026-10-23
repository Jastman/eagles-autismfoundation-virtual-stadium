// Small DOM helpers for the HUD.

type Child = Node | string | null | undefined | false;

export function h<K extends keyof HTMLElementTagNameMap>(tag: K, attrs: Record<string, unknown> = {}, ...children: Child[]): HTMLElementTagNameMap[K] {
    const el = document.createElement(tag);
    for (const [key, value] of Object.entries(attrs)) {
        if (value === undefined || value === null || value === false) continue;
        if (key === "class") el.className = String(value);
        else if (key === "text") el.textContent = String(value);
        else if (key.startsWith("on") && typeof value === "function") el.addEventListener(key.slice(2).toLowerCase(), value as EventListener);
        else if (key === "style" && typeof value === "object") Object.assign(el.style, value);
        else el.setAttribute(key, value === true ? "" : String(value));
    }
    for (const child of children) {
        if (child === null || child === undefined || child === false) continue;
        el.append(child);
    }
    return el;
}

export function show(el: Element | null | undefined, shown: boolean): void {
    el?.classList.toggle("collapsed", !shown);
}

export function isShown(el: HTMLElement): boolean {
    for (let node: HTMLElement | null = el; node; node = node.parentElement) {
        if (node.classList.contains("collapsed") || node.classList.contains("hidden")) return false;
    }
    return el.getClientRects().length > 0;
}

export function setText(el: Element | null | undefined, text: string): void {
    if (el && el.textContent !== text) el.textContent = text;
}

/**
 * A HUD button. Like Unreal's (IsFocusable = false) it never takes the browser's keyboard focus, so the arrow keys and
 * Space keep driving the journey; the HUD's own ring (Tab) and screen readers still reach it.
 */
export function button(className: string, label: Child | Child[], onClick: () => void, aria?: string): HTMLButtonElement {
    const children = Array.isArray(label) ? label : [label];
    const b = h("button", { class: `btn ${className}`, type: "button", tabindex: "-1", "aria-label": aria }, ...children);
    b.addEventListener("click", (event) => {
        event.preventDefault();
        if (!b.disabled) onClick();
    });
    tapOnly(b);
    return b;
}

/**
 * iPad: a tap clicks at once (no 300 ms wait, no double-tap zoom), but a finger that moved (scrolling a panel) doesn't.
 */
export function tapOnly(el: HTMLElement): void {
    let startX = 0, startY = 0, moved = false;
    el.addEventListener("touchstart", (event) => {
        const t = event.touches[0];
        startX = t.clientX;
        startY = t.clientY;
        moved = false;
    }, { passive: true });
    el.addEventListener("touchmove", (event) => {
        const t = event.touches[0];
        if (Math.hypot(t.clientX - startX, t.clientY - startY) > 10) moved = true;
    }, { passive: true });
    el.addEventListener("touchend", (event) => {
        event.preventDefault();
        if (!moved && !(el as HTMLButtonElement).disabled) el.click();
    }, { passive: false });
}

export function enable(el: HTMLButtonElement | null | undefined, enabled: boolean): void {
    if (el && el.disabled === enabled) el.disabled = !enabled;
}
