// Port of FLFFJourneyStateMachine (Source/VirtualStadium/Private/Journey/LFFJourneyStateMachine.cpp).
// Pure journey state: step index, phase, pause, transition serial and the Guided tour / Free explore mode. No world, no
// time, no DOM, so it is unit tested headless (test/stateMachine.test.ts ports LFF.Journey.StateMachine.*).
// Never wraps. Free explore never writes the step: Next, Back, Start and jumps are rejected there, and Resume tour returns
// to the bookmark taken on the way in.
import { resolveTourStep } from "./rules";
import type { CommandResult, JourneyPhase, JourneyStep, SimulationMode, TourStep } from "./types";
import { NONE } from "./types";

/** Where the tour was when the visitor switched to Free explore. Resume tour goes back here. */
export class TourBookmark {
    StepIndex = NONE;
    TourStep: TourStep = "Arrival";
    /** False when Free explore was picked on the welcome screen: Resume tour then starts the tour at step 1. */
    bTourStarted = false;

    isSet(): boolean {
        return this.StepIndex !== NONE;
    }
}

export class JourneyStateMachine {
    private steps: JourneyStep[] = [];
    private tourStepFirstSteps: number[] = [];
    private index = NONE;
    private phase: JourneyPhase = "Empty";
    private mode: SimulationMode = "GuidedTour";
    private bookmark = new TourBookmark();
    private paused = false;
    private transitionId = 0;

    load(steps: JourneyStep[]): void {
        this.steps = [...steps];
        this.paused = false;
        this.mode = "GuidedTour";
        this.bookmark = new TourBookmark();
        ++this.transitionId;
        if (this.steps.length === 0) {
            this.index = NONE;
            this.phase = "Empty";
            return;
        }
        this.index = 0;
        this.phase = "Intro";
    }

    /** First step (0-based) of each tour chapter, in order. Steps before the first one count as Arrival. */
    setTourSteps(firstSteps: number[]): void {
        this.tourStepFirstSteps = [...firstSteps];
    }

    /** Intro -> first step. The camera is already at the first step, so no transition starts. */
    start(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.mode === "FreeExplore") return "RejectedInExplore";
        if (this.phase !== "Intro") return "RejectedAlreadyStarted";
        this.index = 0;
        this.phase = "AtStep";
        return "Accepted";
    }

    next(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.mode === "FreeExplore") return "RejectedInExplore";
        if (this.phase === "Intro") return "RejectedNotStarted";
        if (this.paused) return "RejectedPaused";
        if (this.index >= this.steps.length - 1) return "RejectedAtEnd";
        if (!this.steps[this.index].bNextEnabled) return "RejectedDisabledByStep";
        return this.beginMoveTo(this.index + 1);
    }

    back(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.mode === "FreeExplore") return "RejectedInExplore";
        if (this.phase === "Intro") return "RejectedNotStarted";
        if (this.paused) return "RejectedPaused";
        if (this.index <= 0) return "RejectedAtStart";
        if (!this.steps[this.index].bBackEnabled) return "RejectedDisabledByStep";
        return this.beginMoveTo(this.index - 1);
    }

    /** Straight to any step (a timeline mark). Rejected before Start, while paused, out of range, or to the same step. */
    jumpTo(newIndex: number): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.mode === "FreeExplore") return "RejectedInExplore";
        if (this.phase === "Intro") return "RejectedNotStarted";
        if (this.paused) return "RejectedPaused";
        if (newIndex < 0 || newIndex >= this.steps.length) return newIndex < 0 ? "RejectedAtStart" : "RejectedAtEnd";
        if (newIndex === this.index) return "RejectedDisabledByStep";
        return this.beginMoveTo(newIndex);
    }

    /** Any state -> Intro at the first step, unpaused, in the Guided tour. Invalidates any transition in flight. */
    restart(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        this.index = 0;
        this.phase = "Intro";
        this.mode = "GuidedTour";
        this.bookmark = new TourBookmark();
        this.paused = false;
        ++this.transitionId;
        return "Accepted";
    }

    pause(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.phase === "Intro") return "RejectedNotStarted";
        this.paused = true;
        return "Accepted";
    }

    resume(): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        this.paused = false;
        return "Accepted";
    }

    /**
     * Free explore bookmarks the tour's step and chapter and holds them, retiring any tour transition so an arrival
     * callback can't restart tour cues after the visitor takes control. Guided tour is resumeTour(). Rejected while paused.
     */
    switchToMode(newMode: SimulationMode): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (newMode === this.mode) return "RejectedSameMode";
        // Paused means nothing changes until Resume, the mode included.
        if (this.paused) return "RejectedPaused";
        if (newMode === "GuidedTour") return this.resumeTour();

        // Keep the destination as the bookmark, but retire travel and its arrival callbacks: Explore takes control now.
        this.bookmark.StepIndex = this.index;
        this.bookmark.TourStep = this.tourStepOf(this.index);
        this.bookmark.bTourStarted = this.hasStarted();
        this.mode = "FreeExplore";
        if (this.phase === "Transitioning") {
            this.phase = "AtStep";
        }
        ++this.transitionId;
        return "Accepted";
    }

    /**
     * Free explore -> Guided tour at the bookmark, or at atIndex (a chapter chip). Always starts a new transition, even
     * back to the same step, because the explore camera is somewhere else. Starts the tour if Free explore was picked on
     * the welcome screen.
     */
    resumeTour(atIndex: number = NONE): CommandResult {
        if (this.phase === "Empty") return "RejectedEmpty";
        if (this.mode !== "FreeExplore") return "RejectedSameMode";
        if (this.paused) return "RejectedPaused";
        // The intro sits on step 1, so a tour that never started resumes at the first step.
        const target = atIndex !== NONE ? atIndex : this.bookmark.isSet() ? this.bookmark.StepIndex : 0;
        if (target < 0 || target >= this.steps.length) return target < 0 ? "RejectedAtStart" : "RejectedAtEnd";
        this.mode = "GuidedTour";
        this.bookmark = new TourBookmark();
        // A move even to the same step: the camera is wherever Free explore left it. The new id retires any older move.
        return this.beginMoveTo(target);
    }

    /** Returns true if this finished the current transition. Stale ids are ignored. */
    notifyTransitionFinished(id: number): boolean {
        if (this.phase !== "Transitioning" || id !== this.transitionId) return false;
        this.phase = "AtStep";
        return true;
    }

    hasStarted(): boolean {
        return this.phase === "Transitioning" || this.phase === "AtStep";
    }

    canNext(): boolean {
        return this.mode === "GuidedTour" && this.hasStarted() && !this.paused && this.index < this.steps.length - 1
            && this.steps[this.index].bNextEnabled;
    }

    canBack(): boolean {
        return this.mode === "GuidedTour" && this.hasStarted() && !this.paused && this.index > 0 && this.steps[this.index].bBackEnabled;
    }

    isAtEnd(): boolean {
        return this.hasStarted() && this.index === this.steps.length - 1;
    }

    isPaused(): boolean {
        return this.paused;
    }

    getIndex(): number {
        return this.index;
    }

    num(): number {
        return this.steps.length;
    }

    getPhase(): JourneyPhase {
        return this.phase;
    }

    getMode(): SimulationMode {
        return this.mode;
    }

    getBookmark(): TourBookmark {
        return this.bookmark;
    }

    /** The current step's chapter; in Free explore, the bookmarked one. */
    getTourStep(): TourStep {
        return this.mode === "FreeExplore" && this.bookmark.isSet() ? this.bookmark.TourStep : this.tourStepOf(this.index);
    }

    tourStepOf(stepIndex: number): TourStep {
        return resolveTourStep(stepIndex, this.tourStepFirstSteps);
    }

    getTourStepFirstSteps(): number[] {
        return this.tourStepFirstSteps;
    }

    getTransitionId(): number {
        return this.transitionId;
    }

    getCurrentStep(): JourneyStep | undefined {
        return this.index >= 0 && this.index < this.steps.length ? this.steps[this.index] : undefined;
    }

    getSteps(): readonly JourneyStep[] {
        return this.steps;
    }

    private beginMoveTo(newIndex: number): CommandResult {
        this.index = newIndex;
        this.phase = "Transitioning";
        ++this.transitionId;
        return "Accepted";
    }
}
