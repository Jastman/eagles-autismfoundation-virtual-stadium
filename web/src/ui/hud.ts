// Port of ULFFHUDWidget + ULFFJourneyPanel (+ the panels built by Tools/Editor/build_wbp_lff_modes.py): the one HUD.
// It holds the corner groups, picks which panels are on screen for the snapshot, fades them in, and runs its own focus
// ring. Widgets hold no journey state of their own: every change arrives as a snapshot.
import { GameMode } from "../config";
import type { Journey } from "../journey/journey";
import { MY_SEAT_AMENITY_ID } from "../journey/journey";
import type { JourneySnapshot, MapLevel } from "../journey/types";
import { AMENITIES, MAP_LEVELS, amenityBit, isCrowdShown, isReducedMotionEffective, isSoundEffective, isVoiceEffective } from "../journey/types";
import { Box2, Vec2 } from "../util/ue";
import { CalloutLayer } from "./callouts";
import { button, enable, h, isShown, setText, show, tapOnly } from "./dom";
import { cornerPanels, ticketBounds, welcomeBottomInset } from "./layout";
import { unobstructedViewport } from "../journey/rules";

const CHAPTERS = ["Arrival", "Gates", "Plaza", "To my seat", "Game"];
const LEVEL_NAMES: Record<MapLevel, string> = {
    Field: "Field", LowerConcourse: "Lower concourse", Club: "Club level", Suites: "Suites", UpperDeck: "Upper deck",
};
const AMENITY_NAMES = ["Sensory rooms", "Quiet zones", "Guest Services", "Restrooms", "Food"];
const FADE_SECONDS = 0.25;
const LARGE_TEXT_SCALE = 1.25;
const TICKET_RISE_SECONDS = 1.2;
const TICKET_RISE_DISTANCE = 800;
const TICKET_FRAME_SECONDS = 0.1;

const onOff = (on: boolean) => (on ? "On" : "Off");

const VOICE_ON = `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M3 9v6h4l5 4V5L7 9H3z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M15.5 8.5a5 5 0 0 1 0 7M18.5 5.5a9 9 0 0 1 0 13"/></svg>`;
const VOICE_OFF = `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M3 9v6h4l5 4V5L7 9H3z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M16 9l5 6M21 9l-5 6"/></svg>`;

export interface HudOptions {
    touch: boolean;
    ticketFrames: string[];
    /** Logical units per CSS pixel (1080 / shortest side). */
    unitsPerPixel: () => number;
}

/** A panel that fades in over 0.25 s (ease-out cubic) when shown; instant with less movement or Calm Mode. */
function showPanel(el: HTMLElement, shown: boolean, instant: boolean): boolean {
    const wasShown = !el.classList.contains("collapsed");
    if (!shown) {
        el.classList.add("collapsed");
        return false;
    }
    el.classList.remove("collapsed");
    if (!wasShown) {
        fadeIn(el, instant);
        return true;
    }
    return false;
}

function fadeIn(el: HTMLElement, instant: boolean): void {
    el.getAnimations().forEach((a) => a.cancel());
    if (instant) return;
    el.animate([{ opacity: 0 }, { opacity: 1 }], { duration: FADE_SECONDS * 1000, easing: "cubic-bezier(0.33, 1, 0.68, 1)" });
}

export class Hud {
    readonly root: HTMLElement;
    private snapshot!: JourneySnapshot;
    private readonly callouts: CalloutLayer;
    private lastStepIndex = -1;
    private focused: HTMLButtonElement | null = null;
    private readonly focusRing = h("div", { class: "focus-ring collapsed" });
    private timelineOpen = false;
    private levelsOpen = false;
    private karaokeWord = -1;
    private karaokeTitle = "";
    private karaokeBody = "";
    private calmInitialized = false;
    private calmState = false;
    private ticketShown = false;
    private ticketStill = false;
    private ticketStart = 0;
    private chapterColumns = 5;

    // Panels and groups.
    private readonly infoGroup = h("div", { class: "info-group" });
    private readonly controlGroup = h("div", { class: "control-group" });
    private readonly identity = h("div", { class: "surface identity" });
    private readonly narrative = h("div", { class: "surface narrative" });
    private readonly stageChrome = h("div", { class: "surface stage-chrome" });
    private readonly chapters = h("div", { class: "surface chapters" });
    private readonly tourNav = h("div", { class: "nav-row" });
    private readonly exploreNav = h("div");
    private readonly places = h("div", { class: "surface places" });
    private readonly welcome = h("div", { class: "welcome" });
    private readonly modal = h("div", { class: "modal-layer" });
    private readonly ticket = h("div", { class: "ticket collapsed" });
    private readonly ticketImage = h("img", { alt: "Mobile ticket: Section 111, Row 10, Seat 8" });
    private readonly flyPad = h("div", { class: "fly-pad collapsed" });

    // Narrative.
    private readonly captionScale = h("div", { class: "caption-scale" });
    private readonly stepNumber = h("div", { class: "step-number" });
    private readonly title = h("div", { class: "caption-title", "aria-live": "polite" });
    private readonly body = h("div", { class: "caption-body" });
    private readonly finaleLogo = h("div", { class: "finale-logo collapsed" }, h("div", { class: "mark", role: "img", "aria-label": "Eagles Autism Foundation" }));
    private readonly previewRow = h("div", { class: "preview-row collapsed" });
    private readonly previewNote = h("div", { class: "note" });
    private readonly previewButton: HTMLButtonElement;
    private readonly targetMissing = h("div", { class: "target-missing collapsed", text: GameMode.TargetMissingText });
    private readonly timelineToggle: HTMLButtonElement;
    private readonly timelinePanel = h("div", { class: "timeline-panel collapsed" });
    private readonly timelineMarks = h("div", { class: "timeline-marks" });
    private markButtons: { step: number; el: HTMLButtonElement }[] = [];
    private readonly finaleActions = h("div", { class: "finale-actions collapsed" });
    private readonly finaleStartOver: HTMLButtonElement;
    private readonly finaleLearnMore: HTMLButtonElement;

    // Dock.
    private readonly tourMode: HTMLButtonElement;
    private readonly exploreMode: HTMLButtonElement;
    private readonly dockSettings: HTMLButtonElement;
    private readonly voiceButton: HTMLButtonElement;
    private readonly calmCell: HTMLButtonElement;
    private readonly crowdCell: HTMLButtonElement;
    private chapterButtons: HTMLButtonElement[] = [];
    private readonly backButton: HTMLButtonElement;
    private readonly nextButton: HTMLButtonElement;
    private readonly mySeat: HTMLButtonElement;
    private readonly resumeTour: HTMLButtonElement;

    // Places.
    private readonly viewing = h("div", { class: "viewing" });
    private readonly levelsToggle: HTMLButtonElement;
    private readonly levelsGroup = h("div", { class: "grid2 collapsed" });
    private readonly levelTabs = new Map<MapLevel, HTMLButtonElement>();
    private readonly amenityToggles: HTMLButtonElement[] = [];
    private readonly hideAll: HTMLButtonElement;
    private readonly poiCard = h("div", { class: "poi-card collapsed" });
    private readonly poiTitle = h("div", { class: "poi-title" });
    private readonly poiNote = h("div", { class: "poi-note" });
    private readonly showMe: HTMLButtonElement;
    private readonly pickHint = h("div", { class: "pick-hint", text: "Pick a marker to see the place." });

    // Welcome.
    private readonly welcomeStack = h("div", { class: "welcome-stack" });
    private readonly headline = h("div", { class: "welcome-headline", text: "GAMEDAY VISUAL\nSCHEDULE" });
    private readonly welcomeTour: HTMLButtonElement;
    private readonly welcomeExplore: HTMLButtonElement;
    private readonly welcomeSettings: HTMLButtonElement;
    private readonly calmToggle = h("div", { class: "calm-toggle" });
    private readonly calmSwitch: HTMLButtonElement;
    private readonly calmThumb = h("div", { class: "thumb", text: "Off" });
    private readonly calmHint = h("div", { class: "calm-hint collapsed", text: "Limits sounds, animations, and movement throughout the experience." });
    private calmHovered = false;

    // Modal layer.
    private readonly scrim = h("div", { class: "scrim collapsed" });
    private readonly settingsSheet = h("div", { class: "settings-sheet collapsed" });
    private readonly pausePanel = h("div", { class: "center-card collapsed", role: "dialog", "aria-label": "Paused" });
    private readonly restartPanel = h("div", { class: "center-card collapsed", role: "alertdialog", "aria-label": "Start again?" });
    private readonly settingRows: Record<string, HTMLButtonElement> = {};
    private readonly restartButton: HTMLButtonElement;

    constructor(private readonly journey: Journey, private readonly options: HudOptions) {
        const j = journey;
        const click = (fn: () => void) => () => {
            j.playUIClick();
            fn();
        };

        // ---- Narrative (WBP_LFF_Narrative) ----
        this.previewButton = button("pill", "Preview this? Off", click(() => j.togglePreview()));
        this.previewRow.append(this.previewButton, this.previewNote);
        this.timelineToggle = button("timeline-toggle", "Show day timeline", click(() => {
            this.timelineOpen = !this.timelineOpen;
            this.applyTimelineDrawer();
        }));
        this.timelinePanel.append(h("div", { class: "note", text: "Approximate times\nExample 1:00 PM game" }), this.timelineMarks);
        this.finaleStartOver = button("", "Start Over", click(() => j.next()));
        this.finaleLearnMore = button("primary", "Learn More", click(() => j.openLearnMore()));
        this.finaleActions.append(this.finaleStartOver, this.finaleLearnMore);
        const stack = h("div", { class: "caption-stack" }, this.stepNumber, this.title, this.body, this.finaleLogo, this.previewRow, this.targetMissing,
            this.timelineToggle, this.timelinePanel, this.finaleActions);
        this.captionScale.append(stack);
        this.narrative.append(this.captionScale);

        // ---- Identity (WBP_LFF_SidebarHeader) ----
        this.identity.append(h("div", { class: "logo", role: "img", "aria-label": "Eagles Autism Foundation" }), h("div", { class: "rule" }),
            h("div", {}, h("div", { class: "title", text: "Game day guide" }), h("div", { class: "place", text: "Lincoln Financial Field" })));

        // ---- Stage chrome (WBP_LFF_StageChrome) ----
        this.tourMode = button("", "Guided tour", click(() => j.switchToMode("GuidedTour")));
        this.exploreMode = button("", "Explore", click(() => j.switchToMode("FreeExplore")));
        this.dockSettings = button("", "Settings", click(() => j.toggleSettings()));
        this.voiceButton = button("voice-button", [h("span", { class: "icon" }), h("span", { class: "label", text: "Voice: Off" })],
            click(() => j.toggleVoiceFromCard()));
        this.calmCell = button("toggle-cell", [h("span", { class: "name", text: "Calm Mode" }), h("span", { class: "state", text: "Off" })],
            click(() => j.toggleCalmMode()));
        this.crowdCell = button("toggle-cell", [h("span", { class: "name", text: "Crowd" }), h("span", { class: "state", text: "Off" })],
            click(() => j.toggleCrowd()));
        this.stageChrome.append(h("div", { class: "mode-row" }, this.tourMode, this.exploreMode, this.dockSettings),
            h("div", { class: "options-row" }, this.voiceButton, this.calmCell, this.crowdCell));

        // ---- Chapters (WBP_LFF_TimelineArc, compact chips) ----
        this.chapterButtons = CHAPTERS.map((name, index) => {
            const chip = h("button", { class: "chapter", type: "button", tabindex: "-1" }, h("span", { class: "name", text: name }),
                h("span", { class: "progress" }));
            chip.addEventListener("click", () => {
                if (chip.disabled) return;
                j.playUIClick();
                j.jumpToStep(this.snapshot.TourStepFirstSteps[index] ?? 0);
            });
            tapOnly(chip);
            return chip;
        });
        this.chapters.append(...this.chapterButtons);

        // ---- Tour / Explore navigation ----
        this.backButton = button("", "Back", click(() => j.back()));
        this.nextButton = button("primary", "Next", click(() => j.next()));
        this.tourNav.append(this.backButton, this.nextButton);
        this.mySeat = button("primary", "My seat", click(() => j.goToMySeat()));
        this.resumeTour = button("small-label", "Resume tour", click(() => j.resumeTour()));
        this.exploreNav.append(h("div", { class: "nav-row" }, this.mySeat, this.resumeTour),
            h("div", { class: "fly-help", text: options.touch ? "Drag look  ·  Pinch closer  ·  Pad move  ·  Up / Down" : "WASD move  ·  E/Q up/down  ·  Drag look  ·  Shift faster" }));

        // ---- Places (WBP_LFF_ExploreMap) ----
        this.levelsToggle = button("levels-toggle", "Change level", click(() => {
            this.levelsOpen = !this.levelsOpen;
            show(this.levelsGroup, this.levelsOpen);
            setText(this.levelsToggle, this.levelsOpen ? "Close levels" : "Change level");
        }));
        for (const level of ["UpperDeck", "Suites", "Club", "LowerConcourse", "Field"] as MapLevel[]) {
            const tab = button("", LEVEL_NAMES[level], click(() => j.setMapLevel(level)));
            this.levelTabs.set(level, tab);
            this.levelsGroup.append(tab);
        }
        const amenityGrid = h("div", { class: "grid2" });
        AMENITIES.forEach((kind, index) => {
            const toggle = button("toggle-cell", [h("span", { class: "name", text: AMENITY_NAMES[index] }), h("span", { class: "state", text: "Off" })],
                click(() => j.toggleAmenity(kind)));
            this.amenityToggles.push(toggle);
            amenityGrid.append(toggle);
        });
        this.hideAll = button("row center", "Hide all", click(() => j.hideAllAmenities()));
        amenityGrid.append(this.hideAll);
        this.showMe = button("primary", "Focus place", click(() => {
            if (this.snapshot.SelectedAmenityId) j.focusAmenity(this.snapshot.SelectedAmenityId);
        }));
        this.poiCard.append(this.poiTitle, this.poiNote, this.showMe);
        this.places.append(h("h2", { text: "Places" }), h("div", { class: "scope", text: "Across all levels" }), this.viewing, this.levelsToggle,
            this.levelsGroup, amenityGrid, this.poiCard, this.pickHint);

        this.controlGroup.append(this.stageChrome, this.chapters, this.tourNav, this.exploreNav, this.places);
        this.infoGroup.append(this.identity, this.narrative);

        // ---- Welcome (WBP_LFF_Welcome + WBP_LFF_CalmToggle) ----
        this.welcomeTour = button("welcome-mode", "Guided Tour", click(() => j.switchToMode("GuidedTour")));
        this.welcomeExplore = button("welcome-mode", "Explore", click(() => j.switchToMode("FreeExplore")));
        this.calmSwitch = h("button", { class: "calm-switch", type: "button", tabindex: "-1", "aria-label": "Calm Mode: Off" },
            h("div", { class: "scene active" }), h("div", { class: "scene quiet" }), this.calmThumb);
        this.calmSwitch.addEventListener("click", () => {
            if (this.calmSwitch.disabled) return;
            j.playUIClick();
            j.toggleCalmMode();
        });
        this.calmSwitch.addEventListener("pointerenter", (e) => {
            if (e.pointerType === "mouse") this.calmHovered = true;
        });
        this.calmSwitch.addEventListener("pointerleave", () => (this.calmHovered = false));
        tapOnly(this.calmSwitch);
        this.calmToggle.append(this.calmSwitch, h("div", { class: "label", text: "Calm Mode" }), this.calmHint);
        this.welcomeStack.append(h("div", { class: "welcome-partners" }, h("div", { class: "eaf", role: "img", "aria-label": "Eagles Autism Foundation" }),
            h("div", { class: "fivebelow", role: "img", "aria-label": "Five Below Foundation" })), this.headline,
        h("div", { class: "welcome-actions" }, this.welcomeTour, this.welcomeExplore, this.calmToggle));
        this.welcomeSettings = button("pill welcome-settings", [h("span", { class: "gear" }), "Settings"], click(() => j.toggleSettings()));
        this.welcome.append(h("div", { class: "welcome-fade" }), this.welcomeStack, this.welcomeSettings);

        // ---- Modal layer (WBP_LFF_Settings) ----
        const row = (key: string, name: string, keycap: string | null, fn: () => void) => {
            const b = button("row setting-row", [h("span", { class: "name", text: name }), keycap ? h("span", { class: "keycap", text: keycap }) : null,
                h("span", { class: "state", text: "Off" })], click(fn));
            this.settingRows[key] = b;
            return b;
        };
        const closeSettings = button("pill", "Close", click(() => j.toggleSettings()));
        this.restartButton = button("pill", "Start again", click(() => j.requestRestart()));
        const settingsBody = h("div", { class: "settings-body", role: "dialog", "aria-label": "Settings" },
            h("div", { class: "settings-header" }, h("h2", { text: "Settings" }), closeSettings),
            h("div", { class: "settings-hint", text: "Change these at any time. Calm Mode turns sound and voice off and uses less movement." }),
            h("div", { class: "eyebrow settings-group-title first", text: "Comfort" }),
            row("calm", "Calm Mode", "3", () => j.toggleCalmMode()),
            row("motion", "Less movement", "1", () => j.toggleReducedMotion()),
            row("large", "Large text", "4", () => j.toggleLargeText()),
            row("contrast", "High contrast", null, () => j.toggleHighContrast()),
            h("div", { class: "eyebrow settings-group-title", text: "Sound and scene" }),
            row("sound", "Sound", "2", () => j.toggleSound()),
            row("voice", "Voice", "5", () => j.toggleVoice()),
            row("crowd", "Crowd", "6", () => j.toggleCrowd()),
            h("div", { class: "settings-fill" }),
            h("div", { class: "settings-foot" }, this.restartButton, h("span", { class: "keycap", text: "R" })),
            h("div", { class: "settings-keys", text: options.touch ? "Tap a setting to change it. Close returns to the stadium." : "Tab moves. Enter presses. Esc closes." }));
        this.settingsSheet.append(h("div", { class: "edge" }), settingsBody);
        const resume = button("primary big", "Resume", click(() => j.resume()));
        this.pausePanel.append(h("div", { class: "eyebrow", text: "Take your time" }), h("h2", { text: "Paused" }),
            h("p", { text: "Nothing will move until you resume." }),
            h("div", { class: "actions" }, resume, h("span", { class: "keycap", text: "P" }), h("span", { class: "hint", text: "to resume" })));
        this.restartPanel.append(h("h2", { text: "Start again?" }), h("p", { text: "You will go back to the beginning." }),
            h("div", { class: "actions" }, button("pill", "No, keep going", click(() => j.cancel())),
                button("primary", "Yes, start again", click(() => j.confirmRestart()))));
        this.scrim.addEventListener("pointerdown", (e) => e.stopPropagation());
        this.modal.append(this.scrim, this.settingsSheet, this.pausePanel, this.restartPanel);

        // ---- Ticket phone, callouts ----
        this.ticket.append(this.ticketImage);
        this.callouts = new CalloutLayer(journey, () => this.blockers(), options.unitsPerPixel);

        this.root = h("div", { class: "hud" }, this.callouts.root, this.infoGroup, this.controlGroup, this.ticket, this.flyPad, this.welcome, this.modal,
            this.focusRing);
        for (const el of [this.infoGroup, this.controlGroup, this.welcome, this.modal]) el.classList.add("collapsed");

        journey.onChanged((s) => this.applySnapshot(s));
        journey.onVoiceWord((index) => {
            this.karaokeWord = index;
            this.applyKaraoke();
        });
    }

    /** Touch flight controls (Explore on touch screens), placed by main.ts. */
    get flyPadRoot(): HTMLElement {
        return this.flyPad;
    }

    private blockers(): HTMLElement[] {
        return [this.infoGroup, this.controlGroup].filter((el) => !el.classList.contains("collapsed"));
    }

    // ---- per snapshot (HandleJourneyChanged + ApplySnapshot) ----

    applySnapshot(s: JourneySnapshot): void {
        this.snapshot = s;
        const instant = isReducedMotionEffective(s.Settings);
        const modal = s.bSettingsOpen || s.bRestartConfirmOpen || s.bPaused;
        const modalOnly = s.bSettingsOpen || s.bRestartConfirmOpen;
        const welcome = !s.bStarted && s.Mode === "GuidedTour";
        const explore = s.Mode === "FreeExplore";
        document.documentElement.classList.toggle("hc", s.Settings.bHighContrast);

        showPanel(this.welcome, welcome, instant);
        showPanel(this.callouts.root, !welcome, instant);
        for (const group of [this.infoGroup, this.controlGroup]) show(group, !welcome);
        showPanel(this.identity, !welcome, instant);
        showPanel(this.stageChrome, !welcome, instant);
        this.setChaptersShown(!welcome && !explore, instant);
        const narrativeAppeared = showPanel(this.narrative, !welcome && !explore, instant);
        showPanel(this.places, !welcome && explore, instant);
        showPanel(this.tourNav, !welcome && !explore, instant);
        showPanel(this.exploreNav, !welcome && explore, instant);
        showPanel(this.modal, modal, instant);
        show(this.flyPad, this.options.touch && explore && !modal);
        if (!explore && s.bStarted && !narrativeAppeared && s.StepIndex !== this.lastStepIndex) fadeIn(this.narrative, instant);
        this.lastStepIndex = s.bStarted ? s.StepIndex : -1;

        // Caption.
        setText(this.stepNumber, `Step ${s.StepIndex + 1}${s.ClockText ? ` · ${s.ClockText}` : ""}`);
        if (s.Title !== this.karaokeTitle || s.Body !== this.karaokeBody) {
            this.karaokeTitle = s.Title;
            this.karaokeBody = s.Body;
            this.renderKaraokeText();
        }
        this.karaokeWord = s.bVoicePlaying ? this.journey.getVoiceWordIndex() : -1;
        this.applyKaraoke();
        show(this.targetMissing, s.bStarted && s.bTargetMissing);
        setText(this.previewButton, s.bPreviewOn ? "Preview this? Soft" : "Preview this? Off");
        setText(this.previewNote, previewNote(s));
        show(this.previewRow, s.bStarted && s.bShowPreviewControls);
        enable(this.previewButton, s.bShowPreviewControls && !s.bPaused && !modalOnly && (s.bPreviewAvailable || s.bPreviewOn));
        const finale = s.bStarted && s.bAtEnd;
        show(this.finaleActions, finale);
        show(this.finaleLogo, finale);
        enable(this.finaleStartOver, finale && !modalOnly);
        enable(this.finaleLearnMore, finale && !modalOnly);
        this.captionScale.style.transform = s.Settings.bLargeText ? `scale(${LARGE_TEXT_SCALE})` : "";
        this.captionScale.style.width = s.Settings.bLargeText ? `${100 / LARGE_TEXT_SCALE}%` : "";

        // Timeline marks.
        if (this.markButtons.length !== s.TimelineMarks.length) this.rebuildTimeline(s);
        for (const mark of this.markButtons) {
            const current = mark.step === s.StepIndex && s.bStarted;
            mark.el.classList.toggle("current", current);
            mark.el.classList.toggle("done", s.bStarted && mark.step < s.StepIndex);
            enable(mark.el, s.bStarted && !modal);
        }
        this.applyTimelineDrawer();

        // Dock.
        const voiceOn = isVoiceEffective(s.Settings);
        const icon = this.voiceButton.querySelector(".icon")!;
        if (icon.getAttribute("data-on") !== String(voiceOn)) {
            icon.innerHTML = voiceOn ? VOICE_ON : VOICE_OFF;
            icon.setAttribute("data-on", String(voiceOn));
        }
        setText(this.voiceButton.querySelector(".label"), `Voice: ${onOff(voiceOn)}`);
        this.voiceButton.title = voiceOn ? "Voice is on. Turn it off (key 5)" : "Voice is off. Turn it on (key 5)";
        enable(this.voiceButton, !s.Settings.bCalmMode && !modalOnly);
        this.applyToggle(this.calmCell, s.Settings.bCalmMode, onOff(s.Settings.bCalmMode));
        enable(this.calmCell, !modalOnly && !s.bPaused);
        const crowd = isCrowdShown(s.Settings);
        this.applyToggle(this.crowdCell, crowd, crowd ? "Low" : "Off");
        enable(this.crowdCell, !s.Settings.bCalmMode);
        const inSession = s.bStarted || explore;
        this.tourMode.classList.toggle("selected", inSession && !explore);
        this.exploreMode.classList.toggle("selected", inSession && explore);
        enable(this.tourMode, !modalOnly && !s.bPaused);
        enable(this.exploreMode, !modalOnly && !s.bPaused);
        enable(this.dockSettings, !s.bRestartConfirmOpen);

        // Chapters.
        const current = s.bStarted ? chapterOf(s.StepIndex, s.TourStepFirstSteps) : -1;
        this.chapterButtons.forEach((chip, index) => {
            const { place, count } = chapterPlace(index, s);
            const isCurrent = index === current;
            const done = current !== -1 && index < current;
            chip.classList.toggle("current", isCurrent);
            setText(chip.querySelector(".progress"), isCurrent ? `${place} of ${count}` : done ? "Done" : count === 1 ? "1 step" : `${count} steps`);
            chip.title = `Go to ${CHAPTERS[index]}`;
            enable(chip, s.bStarted && !modal);
        });

        // Navigation.
        enable(this.nextButton, s.bCanNext);
        enable(this.backButton, s.bCanBack);
        setText(this.resumeTour, resumeText(s));
        enable(this.resumeTour, s.bCanResumeTour);
        enable(this.mySeat, explore && !modalOnly && !s.bPaused);

        // Places.
        this.applyPlaces(s, explore, modal);

        // Welcome.
        enable(this.welcomeTour, !modalOnly && !s.bPaused);
        enable(this.welcomeExplore, !modalOnly && !s.bPaused);
        this.applyCalmToggle(s, modalOnly);

        // Settings.
        const rows = this.settingRows;
        this.applyToggle(rows.calm, s.Settings.bCalmMode, onOff(s.Settings.bCalmMode));
        this.applyToggle(rows.motion, isReducedMotionEffective(s.Settings), onOff(isReducedMotionEffective(s.Settings)));
        this.applyToggle(rows.large, s.Settings.bLargeText, onOff(s.Settings.bLargeText));
        this.applyToggle(rows.contrast, s.Settings.bHighContrast, onOff(s.Settings.bHighContrast));
        this.applyToggle(rows.sound, isSoundEffective(s.Settings), onOff(isSoundEffective(s.Settings)));
        this.applyToggle(rows.voice, voiceOn, onOff(voiceOn));
        this.applyToggle(rows.crowd, crowd, crowd ? "Low" : "Off");
        enable(rows.sound, !s.Settings.bCalmMode);
        enable(rows.motion, !s.Settings.bCalmMode);
        enable(rows.voice, isSoundEffective(s.Settings));
        enable(rows.crowd, !s.Settings.bCalmMode);
        enable(this.restartButton, s.bStarted);
        show(this.restartPanel, s.bRestartConfirmOpen);
        show(this.settingsSheet, s.bSettingsOpen && !s.bRestartConfirmOpen);
        show(this.pausePanel, s.bPaused && !modalOnly);
        show(this.scrim, modal);

        // Ticket.
        this.showTicket(s.bStarted && !welcome && !explore && !modal && s.bShowTicketPhone && !s.bTargetMissing, instant);

        this.callouts.apply(s);
        this.layout(true);
        this.placeFocusRing();
    }

    private applyToggle(b: HTMLButtonElement, on: boolean, state: string): void {
        b.classList.toggle("selected", on);
        setText(b.querySelector(".state"), state);
        b.setAttribute("aria-pressed", String(on));
    }

    private applyPlaces(s: JourneySnapshot, explore: boolean, modal: boolean): void {
        const canUse = explore && !modal;
        enable(this.levelsToggle, canUse);
        for (const [level, tab] of this.levelTabs) {
            tab.classList.toggle("selected", level === s.MapLevel);
            enable(tab, canUse);
        }
        setText(this.viewing, `Viewing: ${LEVEL_NAMES[s.MapLevel]}`);
        const mapped = this.journey.mappedAmenityMask();
        AMENITIES.forEach((kind, index) => {
            const bit = amenityBit(kind);
            const isMapped = (mapped & bit) !== 0;
            const on = isMapped && (s.AmenityMask & bit) !== 0;
            this.applyToggle(this.amenityToggles[index], on, !isMapped ? "Not mapped" : onOff(on));
            enable(this.amenityToggles[index], canUse && isMapped);
            this.amenityToggles[index].title = isMapped ? `Show or hide ${AMENITY_NAMES[index]} across all levels`
                : "No location data for this category is included. This does not mean the stadium has none.";
        });
        enable(this.hideAll, canUse && (s.AmenityMask & mapped) !== 0);
        const picked = this.journey.getAmenity(s.SelectedAmenityId);
        show(this.poiCard, picked !== undefined);
        show(this.pickHint, picked === undefined);
        if (picked) {
            setText(this.poiTitle, picked.Label);
            setText(this.poiNote, `${LEVEL_NAMES[picked.Level]} · ${picked.Note}`);
        }
        enable(this.showMe, canUse && picked !== undefined);
        void MY_SEAT_AMENITY_ID;
    }

    private applyCalmToggle(s: JourneySnapshot, modal: boolean): void {
        const changed = this.calmState !== s.Settings.bCalmMode;
        this.calmState = s.Settings.bCalmMode;
        // Only this short, deliberate switch transition is allowed; it snaps when it isn't the one on screen.
        const instant = !this.calmInitialized || s.Settings.bReducedMotion || s.bStarted || s.Mode === "FreeExplore" || s.bSettingsOpen
            || s.bPaused || s.bRestartConfirmOpen;
        this.calmToggle.classList.toggle("animate", !instant && changed);
        this.calmToggle.classList.toggle("on", this.calmState);
        setText(this.calmThumb, onOff(this.calmState));
        this.calmSwitch.setAttribute("aria-label", `Calm Mode: ${onOff(this.calmState)}`);
        enable(this.calmSwitch, !modal && !s.bPaused);
        this.calmInitialized = true;
    }

    private setChaptersShown(shown: boolean, instant: boolean): void {
        const wasShown = !this.chapters.classList.contains("collapsed");
        show(this.chapters, shown);
        if (shown && !wasShown && !instant) {
            this.chapters.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 180, easing: "ease-in-out" });
        }
    }

    private rebuildTimeline(s: JourneySnapshot): void {
        this.timelineMarks.replaceChildren();
        this.markButtons = s.TimelineMarks.map((mark) => {
            const el = h("button", { class: "mark", type: "button", tabindex: "-1", text: mark.Label });
            el.addEventListener("click", () => {
                if (el.disabled) return;
                this.journey.playUIClick();
                this.journey.jumpToStep(mark.StepIndex);
            });
            tapOnly(el);
            this.timelineMarks.append(el);
            return { step: mark.StepIndex, el };
        });
    }

    private applyTimelineDrawer(): void {
        const s = this.snapshot;
        if (!s) return;
        const available = s.bStarted && s.TimelineMarks.length > 0;
        const modal = s.bSettingsOpen || s.bRestartConfirmOpen || s.bPaused;
        show(this.timelineToggle, available);
        enable(this.timelineToggle, available && !modal);
        setText(this.timelineToggle, this.timelineOpen ? "Hide day timeline" : "Show day timeline");
        show(this.timelinePanel, available && this.timelineOpen);
    }

    // ---- karaoke (ULFFKaraokeDecorator): words split at spaces, as the voice timings are ----

    private renderKaraokeText(): void {
        const words = (text: string) => text.split(/\s+/).filter(Boolean);
        const fill = (el: HTMLElement, text: string) => {
            el.replaceChildren();
            words(text).forEach((word, i) => {
                if (i > 0) el.append(" ");
                el.append(h("span", { class: "word", text: word }));
            });
        };
        fill(this.title, this.karaokeTitle);
        fill(this.body, this.karaokeBody);
        show(this.body, this.karaokeBody !== "");
    }

    private applyKaraoke(): void {
        const all = [...this.title.querySelectorAll(".word"), ...this.body.querySelectorAll(".word")];
        all.forEach((el, i) => el.classList.toggle("spoken", i === this.karaokeWord));
    }

    // ---- ticket phone ----

    private showTicket(shown: boolean, still: boolean): void {
        if (!shown) {
            if (this.ticketShown) {
                show(this.ticket, false);
                this.ticketShown = false;
            }
            return;
        }
        if (this.ticketShown && still === this.ticketStill) return;
        const wasShown = this.ticketShown;
        this.ticketShown = true;
        this.ticketStill = still;
        show(this.ticket, true);
        if (this.options.ticketFrames.length > 0) this.ticketImage.src = this.options.ticketFrames[0];
        if (!wasShown) this.ticketStart = performance.now() / 1000;
    }

    /** Per frame: the ticket's rise and frames, the corner layout, the callouts and the focus ring. */
    tick(): void {
        if (this.ticketShown) {
            const elapsed = performance.now() / 1000 - this.ticketStart;
            const rise = this.ticketStill ? 1 : Math.min(Math.max(elapsed / TICKET_RISE_SECONDS, 0), 1);
            // Ease-out cubic: up quickly, then a slow settle. No bounce.
            this.ticket.style.translate = `0 ${(TICKET_RISE_DISTANCE * Math.pow(1 - rise, 3)) / 10}rem`;
            const frames = this.options.ticketFrames;
            if (!this.ticketStill && rise >= 1 && frames.length > 0) {
                const frame = Math.floor((elapsed - TICKET_RISE_SECONDS) / TICKET_FRAME_SECONDS) % frames.length;
                const src = frames[Math.max(frame, 0)];
                if (!this.ticketImage.src.endsWith(src)) this.ticketImage.src = src;
            }
        }
        this.callouts.tick();
        this.layout(false);
        this.updateWelcomeHint();
        this.placeFocusRing();
    }

    private lastLayoutKey = "";

    /** NativeTick's layout: corner widths from the logical viewport, the map's safe rectangle, the ticket's slot. */
    private layout(force: boolean): void {
        const s = this.snapshot;
        if (!s) return;
        const upp = this.options.unitsPerPixel();
        const size = new Vec2(window.innerWidth * upp, window.innerHeight * upp);
        const key = `${size.x.toFixed(1)}x${size.y.toFixed(1)}:${s.Settings.bLargeText}`;
        if (force || key !== this.lastLayoutKey) {
            this.lastLayoutKey = key;
            const layout = cornerPanels(size, s.Settings.bLargeText);
            const rem = (units: number) => `${units / 10}rem`;
            this.infoGroup.style.width = rem(layout.InformationWidth + layout.EdgeInset);
            this.infoGroup.style.padding = `${rem(layout.EdgeInset)} 0 0 ${rem(layout.EdgeInset)}`;
            this.infoGroup.style.maxHeight = rem(layout.MaximumHeight + layout.EdgeInset);
            this.narrative.style.maxHeight = rem(layout.NarrativeHeight);
            this.controlGroup.style.width = rem(layout.ControlWidth + layout.EdgeInset);
            this.controlGroup.style.padding = `${rem(layout.EdgeInset)} ${rem(layout.EdgeInset)} 0 0`;
            this.controlGroup.style.maxHeight = rem(layout.MaximumHeight + layout.EdgeInset);
            // Chapter chips: 5, 3, 2 or 1 columns for the dock's width (LayoutChapters).
            const cell = s.Settings.bLargeText ? 128 : 100;
            const capacity = Math.max(1, Math.floor(Math.max(0, layout.ControlWidth - 24) / cell));
            this.chapterColumns = capacity >= 5 ? 5 : capacity >= 3 ? 3 : capacity >= 2 ? 2 : 1;
            this.chapters.style.gridTemplateColumns = `repeat(${this.chapterColumns}, minmax(0, 1fr))`;
            // Welcome: width, headline size and the bottom inset that keeps Settings on its own row when narrow.
            const width = Math.min(Math.max(size.x - 96, 320), 1060);
            this.welcomeStack.style.width = rem(width);
            this.welcomeStack.style.bottom = rem(welcomeBottomInset(size.x));
            this.headline.style.fontSize = rem(Math.min(Math.max(Math.floor(width / 14), 36), 72));
        }
        // The unobstructed rectangle for framing places (SetMapViewportFrame).
        const panels = this.blockers().map((el) => {
            const r = el.getBoundingClientRect();
            return new Box2(new Vec2(r.left * upp, r.top * upp), new Vec2(r.right * upp, r.bottom * upp));
        }).filter((b) => b.size().x > 1 && b.size().y > 1);
        const safe = unobstructedViewport(size, panels, Math.max(48, 64 * 0.5), 112);
        const normalized = safe.valid ? new Box2(new Vec2(safe.min.x / size.x, safe.min.y / size.y), new Vec2(safe.max.x / size.x, safe.max.y / size.y)) : Box2.invalid();
        this.journey.setMapViewportFrame(size, normalized, 2500);
        if (this.ticketShown) this.layoutTicket(size, upp);
    }

    private layoutTicket(size: Vec2, upp: number): void {
        const box = (el: HTMLElement) => {
            if (el.classList.contains("collapsed")) return Box2.invalid();
            const r = el.getBoundingClientRect();
            return r.width > 1 ? new Box2(new Vec2(r.left * upp, r.top * upp), new Vec2(r.right * upp, r.bottom * upp)) : Box2.invalid();
        };
        const bounds = ticketBounds(size, box(this.infoGroup), box(this.controlGroup), new Vec2(180, 360));
        show(this.ticket, bounds.valid);
        if (!bounds.valid) return;
        Object.assign(this.ticket.style, {
            left: `${bounds.min.x / 10}rem`, top: `${bounds.min.y / 10}rem`,
            width: `${(bounds.max.x - bounds.min.x) / 10}rem`, height: `${(bounds.max.y - bounds.min.y) / 10}rem`,
        });
    }

    private updateWelcomeHint(): void {
        const s = this.snapshot;
        if (!s) return;
        const modal = s.bSettingsOpen || s.bRestartConfirmOpen || s.bPaused;
        const shownWelcome = !this.welcome.classList.contains("collapsed");
        const wanted = shownWelcome && !modal && !this.calmSwitch.disabled && (this.calmHovered || this.focused === this.calmSwitch);
        show(this.calmHint, wanted);
    }

    // ---- focus ring (MoveFocus / ActivateFocus / ClearFocus) ----

    private focusable(): HTMLButtonElement[] {
        const s = this.snapshot;
        const modal = s.bSettingsOpen || s.bRestartConfirmOpen || s.bPaused;
        const welcome = !s.bStarted && s.Mode === "GuidedTour";
        // Caption first, then the dock, chapters, navigation, places and spatial markers. Modal cards and the welcome
        // screen keep the ring inside themselves.
        const order: HTMLElement[] = modal ? [this.modal] : welcome ? [this.welcome] : [this.narrative, this.stageChrome, this.chapters, this.tourNav,
            this.exploreNav, this.places, this.callouts.root];
        return order.flatMap((panel) => [...panel.querySelectorAll<HTMLButtonElement>("button")])
            .filter((b) => !b.disabled && !b.classList.contains("passive") && isShown(b));
    }

    moveFocus(direction: number): void {
        const buttons = this.focusable();
        if (buttons.length === 0) {
            this.clearFocus();
            return;
        }
        const current = this.focused ? buttons.indexOf(this.focused) : -1;
        const next = current === -1 ? (direction >= 0 ? 0 : buttons.length - 1) : (current + (direction >= 0 ? 1 : -1) + buttons.length) % buttons.length;
        this.focused = buttons[next];
        this.focused.scrollIntoView({ block: "nearest", inline: "nearest", behavior: "instant" as ScrollBehavior });
        this.placeFocusRing();
    }

    activateFocus(): boolean {
        const b = this.focused;
        if (!b || b.disabled || !isShown(b)) {
            this.clearFocus();
            return false;
        }
        b.click();
        return true;
    }

    clearFocus(): void {
        this.focused = null;
        show(this.focusRing, false);
    }

    private placeFocusRing(): void {
        const b = this.focused;
        if (!b) return;
        if (b.disabled || !isShown(b)) {
            // The button went away (new step, card closed): drop the ring rather than leave it floating.
            this.clearFocus();
            return;
        }
        const r = b.getBoundingClientRect();
        const style = getComputedStyle(b);
        // Gap + ring + halo: 10 units outside the button, following its shape.
        const px = 10 / this.options.unitsPerPixel();
        Object.assign(this.focusRing.style, {
            left: `${r.left - px}px`, top: `${r.top - px}px`, width: `${r.width + px * 2}px`, height: `${r.height + px * 2}px`,
            borderRadius: style.borderRadius === "50%" ? "50%" : `calc(${style.borderRadius} + ${px}px)`,
        });
        show(this.focusRing, true);
    }
}

function previewNote(s: JourneySnapshot): string {
    switch (s.IntensityPreview) {
        case "Sound":
            if (!s.bPreviewAvailable) return "Turn Sound on first to hear a quiet sample.";
            return s.bPreviewOn ? "Playing a short, quiet, muffled sample." : "Soft plays a short, quiet, muffled sample.";
        case "Lights":
            return "Soft is a slow, gentle brightening. No flashing.";
        case "Anthem":
            return s.bPreviewOn ? "The eagle flies over once, with soft fireworks. The anthem plays when Sound is on."
                : "Soft replays the eagle's flight with soft fireworks. No flashing. The anthem plays when Sound is on.";
        default:
            return "";
    }
}

function chapterOf(stepIndex: number, firstSteps: number[]): number {
    let found = -1;
    firstSteps.forEach((first, index) => {
        if (stepIndex >= first) found = index;
    });
    return found;
}

function chapterPlace(chapter: number, s: JourneySnapshot): { place: number; count: number } {
    const first = s.TourStepFirstSteps[chapter] ?? 0;
    const end = chapter + 1 < s.TourStepFirstSteps.length ? s.TourStepFirstSteps[chapter + 1] : s.StepCount;
    const count = Math.max(end - first, 1);
    return { place: Math.min(Math.max(s.StepIndex - first + 1, 1), count), count };
}

function resumeText(s: JourneySnapshot): string {
    if (!s.bStarted) return "Start the tour";
    const chapter = chapterOf(s.StepIndex, s.TourStepFirstSteps);
    if (chapter === -1) return "Resume tour";
    const { place, count } = chapterPlace(chapter, s);
    return `Resume tour · ${CHAPTERS[chapter]}, ${place} of ${count}`;
}

export { MAP_LEVELS };
