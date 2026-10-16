// Port of Source/VirtualStadium/Private/Tests/LFFJourneyStateMachineTest.cpp (LFF.Journey.StateMachine.*, LFF.Journey.Rules).
// Keep the two in step: a rule change in one build needs the matching test change in the other (Docs/WEB_PARITY.md).
import { describe, expect, it } from "vitest";
import * as R from "../src/journey/rules";
import { JourneyStateMachine } from "../src/journey/stateMachine";
import type { JourneyStep, VisitorSettings } from "../src/journey/types";
import { NONE, defaultSettings, isCrowdShown, isReducedMotionEffective, isSoundEffective, isVoiceEffective } from "../src/journey/types";
import { Vec2 } from "../src/util/ue";

function makeSteps(count: number): JourneyStep[] {
    return Array.from({ length: count }, (_, i) => ({
        StepId: `step-${i}`, Title: "", Body: "", DisclaimerLevel: "Illustrative", ClockText: "", TimelineLabel: "",
        VerificationSource: "", CameraTargetId: `CT_${i}`, TransitionProfile: "Ease", TransitionSeconds: 0, LabelsToShow: [],
        PlacesToShow: [], AudioBed: "", IntensityPreview: "None", PreviewSound: "", PreviewImage: "", VoiceWave: "", SceneCues: [],
        bShowTicketPhone: false, bNextEnabled: true, bBackEnabled: true,
    }));
}

const settings = (patch: Partial<VisitorSettings> = {}): VisitorSettings => ({ ...defaultSettings(), ...patch });

describe("LFF.Journey.StateMachine.Navigation", () => {
    it("navigates without wrapping", () => {
        const m = new JourneyStateMachine();
        m.load(makeSteps(4));
        expect(m.getPhase()).toBe("Intro");
        expect(m.getIndex()).toBe(0);
        expect(m.next()).toBe("RejectedNotStarted");
        expect(m.pause()).toBe("RejectedNotStarted");
        expect(m.start()).toBe("Accepted");
        expect(m.getPhase()).toBe("AtStep");
        expect(m.start()).toBe("RejectedAlreadyStarted");
        expect(m.back()).toBe("RejectedAtStart");
        for (let expected = 1; expected < 4; ++expected) {
            expect(m.next()).toBe("Accepted");
            expect(m.getPhase()).toBe("Transitioning");
            expect(m.notifyTransitionFinished(m.getTransitionId())).toBe(true);
            expect(m.getIndex()).toBe(expected);
        }
        expect(m.isAtEnd()).toBe(true);
        expect(m.canNext()).toBe(false);
        expect(m.next()).toBe("RejectedAtEnd");
        expect(m.getIndex()).toBe(3);
        expect(m.back()).toBe("Accepted");
        expect(m.getIndex()).toBe(2);
    });
});

describe("LFF.Journey.StateMachine.PauseRestart", () => {
    it("pauses, retargets and restarts", () => {
        const m = new JourneyStateMachine();
        m.load(makeSteps(4));
        m.start();
        m.next();
        const firstMove = m.getTransitionId();
        expect(m.pause()).toBe("Accepted");
        expect(m.next()).toBe("RejectedPaused");
        expect(m.back()).toBe("RejectedPaused");
        expect(m.canNext()).toBe(false);
        m.resume();
        expect(m.next()).toBe("Accepted");
        expect(m.notifyTransitionFinished(firstMove)).toBe(false);
        expect(m.getPhase()).toBe("Transitioning");
        m.pause();
        expect(m.restart()).toBe("Accepted");
        expect(m.isPaused()).toBe(false);
        expect(m.getPhase()).toBe("Intro");
        expect(m.getIndex()).toBe(0);
        expect(m.notifyTransitionFinished(m.getTransitionId() - 1)).toBe(false);
    });
});

describe("LFF.Journey.StateMachine.EdgeCases", () => {
    it("rejects everything when empty and honours step gates", () => {
        const empty = new JourneyStateMachine();
        empty.load([]);
        expect(empty.start()).toBe("RejectedEmpty");
        expect(empty.next()).toBe("RejectedEmpty");
        expect(empty.back()).toBe("RejectedEmpty");
        expect(empty.restart()).toBe("RejectedEmpty");
        expect(empty.getCurrentStep()).toBeUndefined();
        const steps = makeSteps(3);
        steps[0].bNextEnabled = false;
        steps[2].bBackEnabled = false;
        const gated = new JourneyStateMachine();
        gated.load(steps);
        gated.start();
        expect(gated.next()).toBe("RejectedDisabledByStep");
    });
});

describe("LFF.Journey.Rules", () => {
    it("matches the Unreal rules", () => {
        expect(R.resolveTransitionProfile("Ease", false)).toBe("Ease");
        expect(R.resolveTransitionProfile("Ease", true)).toBe("ReducedMotion");
        expect(R.resolveTransitionProfile("Instant", true)).toBe("Instant");
        expect(R.resolveTransitionProfile("ReducedMotion", false)).toBe("ReducedMotion");
        expect(R.clampDuration(0, 3, 0.5, 6)).toBe(3);
        expect(R.clampDuration(20, 3, 0.5, 6)).toBe(6);
        expect(R.clampDuration(0.1, 3, 0.5, 6)).toBe(0.5);
        expect(R.shouldShowDisclaimer("Illustrative")).toBe(true);
        expect(R.shouldShowDisclaimer("Approximate")).toBe(true);
        expect(R.shouldShowDisclaimer("Verified")).toBe(false);
        expect(R.shouldBackReturnToIntro(true, 0, false, true)).toBe(true);
        expect(R.shouldBackReturnToIntro(false, 0, false, true)).toBe(false);
        expect(R.shouldBackReturnToIntro(true, 1, false, true)).toBe(false);
        expect(R.shouldBackReturnToIntro(true, 0, true, true)).toBe(false);
        expect(R.shouldBackReturnToIntro(true, 0, false, false)).toBe(false);

        const standard = settings();
        const lessMovement = settings({ bReducedMotion: true });
        const calm = settings({ bCalmMode: true });
        const soundOn = settings({ bSoundEnabled: true });
        const calmWithSound = settings({ bCalmMode: true, bSoundEnabled: true });
        expect(R.resolveMove("Ease", standard, false)).toBe("Ease");
        expect(R.resolveMove("Ease", standard, true)).toBe("ReducedMotion");
        expect(R.resolveMove("Ease", lessMovement, false)).toBe("ReducedMotion");
        expect(R.resolveMove("Ease", calm, false)).toBe("Instant");
        expect(R.resolveMove("ReducedMotion", calm, true)).toBe("Instant");
        expect(!isSoundEffective(calmWithSound) && isSoundEffective(soundOn)).toBe(true);
        expect(isReducedMotionEffective(calm)).toBe(true);
        expect(R.shouldShowPreviewControls("None", standard)).toBe(false);
        expect(R.shouldShowPreviewControls("Sound", standard)).toBe(true);
        expect(R.shouldShowPreviewControls("Lights", calm)).toBe(false);
        expect(R.canPlayPreview("Lights", standard)).toBe(true);
        expect(R.canPlayPreview("Sound", standard)).toBe(false);
        expect(R.canPlayPreview("Sound", soundOn)).toBe(true);
        expect(R.canPlayPreview("Sound", calmWithSound)).toBe(false);
        expect(R.shouldHideLabelAt(true, new Vec2(0.3, 0.4))).toBe(false);
        expect(R.shouldHideLabelAt(true, new Vec2(0.3, 0.75))).toBe(true);
        expect(R.shouldHideLabelAt(false, new Vec2(0.5, 0.5))).toBe(true);

        const voiceOn = settings({ bSoundEnabled: true, bVoiceEnabled: true });
        const voiceNoSound = settings({ bVoiceEnabled: true });
        const voiceCalm = settings({ bSoundEnabled: true, bVoiceEnabled: true, bCalmMode: true });
        expect(isVoiceEffective(standard)).toBe(false);
        expect(R.shouldPlayVoice(voiceOn, true, false)).toBe(true);
        expect(R.shouldPlayVoice(voiceNoSound, true, false)).toBe(false);
        expect(R.shouldPlayVoice(voiceCalm, true, false)).toBe(false);
        expect(R.shouldPlayVoice(voiceOn, false, false)).toBe(false);
        expect(R.shouldPlayVoice(voiceOn, true, true)).toBe(false);
        expect(R.decibelsToGain(-8)).toBeCloseTo(0.398, 2);

        expect(isCrowdShown(standard)).toBe(true);
        expect(isCrowdShown(settings({ bCrowdEnabled: false }))).toBe(false);
        expect(isCrowdShown(calm)).toBe(false);
        expect(R.canPlayPreview("Anthem", standard)).toBe(true);
        expect(R.canPlayPreview("Anthem", calm)).toBe(false);

        expect(R.resolveCueState(false, 5, 5)).toBe("Before");
        expect(R.resolveCueState(true, 3, 5)).toBe("Before");
        expect(R.resolveCueState(true, 5, 5)).toBe("Play");
        expect(R.resolveCueState(true, 9, 5)).toBe("After");
        expect(R.resolveCueState(true, 9, NONE)).toBe("Before");
        expect(R.shouldOrbitSplash(standard)).toBe(true);
        expect(R.shouldOrbitSplash(lessMovement)).toBe(false);
        expect(R.shouldOrbitSplash(calm)).toBe(false);
    });
});

describe("LFF.Journey.StateMachine.Jump", () => {
    it("jumps to timeline marks", () => {
        const m = new JourneyStateMachine();
        m.load(makeSteps(6));
        expect(m.jumpTo(3)).toBe("RejectedNotStarted");
        m.start();
        expect(m.jumpTo(4)).toBe("Accepted");
        expect(m.getPhase()).toBe("Transitioning");
        expect(m.getIndex()).toBe(4);
        expect(m.jumpTo(1)).toBe("Accepted");
        expect(m.jumpTo(6)).toBe("RejectedAtEnd");
        expect(m.jumpTo(-1)).toBe("RejectedAtStart");
        expect(m.jumpTo(1)).not.toBe("Accepted");
        expect(m.getIndex()).toBe(1);
        m.pause();
        expect(m.jumpTo(5)).toBe("RejectedPaused");
    });
});

describe("LFF.Journey.StateMachine.Modes", () => {
    it("bookmarks the tour in Free explore and resumes it", () => {
        const firstSteps = [0, 4, 7, 10, 14];
        const m = new JourneyStateMachine();
        m.load(makeSteps(23));
        m.setTourSteps(firstSteps);
        expect(R.resolveTourStep(0, firstSteps)).toBe("Arrival");
        expect(R.resolveTourStep(4, firstSteps)).toBe("Gates");
        expect(R.resolveTourStep(6, firstSteps)).toBe("Gates");
        expect(R.resolveTourStep(8, firstSteps)).toBe("Plaza");
        expect(R.resolveTourStep(22, firstSteps)).toBe("Game");
        expect(R.resolveTourStep(12, [])).toBe("Arrival");

        m.start();
        m.jumpTo(8);
        m.notifyTransitionFinished(m.getTransitionId());
        m.next();
        const inFlight = m.getTransitionId();
        expect(m.switchToMode("FreeExplore")).toBe("Accepted");
        expect(m.getMode()).toBe("FreeExplore");
        expect(m.getBookmark().StepIndex).toBe(9);
        expect(m.getBookmark().TourStep).toBe("Plaza");
        expect(m.getBookmark().bTourStarted).toBe(true);
        expect(m.switchToMode("FreeExplore")).toBe("RejectedSameMode");
        expect(m.notifyTransitionFinished(inFlight)).toBe(false);
        expect(m.getPhase()).toBe("AtStep");
        expect(m.getMode()).toBe("FreeExplore");
        expect(m.next()).toBe("RejectedInExplore");
        expect(m.back()).toBe("RejectedInExplore");
        expect(m.jumpTo(2)).toBe("RejectedInExplore");
        expect(m.canNext()).toBe(false);
        expect(m.canBack()).toBe(false);
        expect(m.getIndex()).toBe(9);
        expect(m.getTourStep()).toBe("Plaza");

        m.pause();
        expect(m.resumeTour()).toBe("RejectedPaused");
        m.resume();
        const beforeResume = m.getTransitionId();
        expect(m.resumeTour()).toBe("Accepted");
        expect(m.getMode()).toBe("GuidedTour");
        expect(m.getIndex()).toBe(9);
        expect(m.getPhase()).toBe("Transitioning");
        expect(m.getTransitionId()).not.toBe(beforeResume);
        expect(m.getBookmark().isSet()).toBe(false);
        expect(m.resumeTour()).toBe("RejectedSameMode");
        m.notifyTransitionFinished(m.getTransitionId());
        expect(m.next()).toBe("Accepted");
        expect(m.getIndex()).toBe(10);

        m.switchToMode("FreeExplore");
        expect(m.resumeTour(14)).toBe("Accepted");
        expect(m.getIndex()).toBe(14);
        expect(m.getTourStep()).toBe("Game");
        m.switchToMode("FreeExplore");
        expect(m.resumeTour(23)).toBe("RejectedAtEnd");
        expect(m.getMode()).toBe("FreeExplore");

        m.restart();
        expect(m.getMode()).toBe("GuidedTour");
        expect(m.getBookmark().isSet()).toBe(false);
        expect(m.switchToMode("FreeExplore")).toBe("Accepted");
        expect(m.getBookmark().bTourStarted).toBe(false);
        expect(m.start()).toBe("RejectedInExplore");
        expect(m.resumeTour()).toBe("Accepted");
        expect(m.hasStarted()).toBe(true);
        expect(m.getIndex()).toBe(0);

        const empty = new JourneyStateMachine();
        empty.load([]);
        expect(empty.switchToMode("FreeExplore")).toBe("RejectedEmpty");
    });
});
