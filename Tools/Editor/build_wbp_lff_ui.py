"""Builds the HUD widgets in /Game/LFF/UI through Unreal MCP (Docs/ManualSetup.md §5).

  WBP_LFF_Splash       welcome card, before Start
  WBP_LFF_TopChrome    prototype badge and, on steps, the one-line disclaimer; Step X of Y, Pause, Sound; Settings
  WBP_LFF_StepCaption  small lower-left caption card: step number, chip, title, body, "Preview this?" on intensity steps
  WBP_LFF_BottomRail   Back, progress pips, Next
  WBP_LFF_Settings     modal layer over a dimmed world: Settings, Paused, Start again?
  WBP_LFF_HUD          the one HUD on screen; holds the five panels above

Run from the repo root with the editor open and the Unreal MCP server running:
  python3 Tools/Editor/build_wbp_lff_ui.py            # refuses to touch widgets that already exist
  python3 Tools/Editor/build_wbp_lff_ui.py --rebuild  # clears and rebuilds their widget trees in place
  python3 Tools/Editor/build_wbp_lff_ui.py --extend   # session 3: adds only the new pieces to the existing trees
                                                      # (Voice and Crowd toggles, timeline, ticket phone,
                                                      # Learn More) and moves the caption above the timeline

Session 3 additions (EXTENSIONS below) are also part of a fresh build or --rebuild.

Widget names are the C++ contract in Docs/JOURNEY_SCHEMA.md. Labels that C++ writes get placeholder text here.
Visible wording is placeholder until the user and the Eagles Autism Foundation approve it (LEDGER F24).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from unreal_mcp import call  # noqa: E402

UMG, OBJ, ASSET = "UMGToolSet.UMGToolSet", "editor_toolset.toolsets.object.ObjectTools", "editor_toolset.toolsets.asset.AssetTools"
FOLDER = "/Game/LFF/UI"
PANEL_CLASS = "/Script/VirtualStadium.LFFJourneyPanel"
HUD_CLASS = "/Script/VirtualStadium.LFFHUDWidget"

# Linear colours. Midnight green #004C54, bone white #F2EDE4, muted bone #C9C3B8.
# Bone on midnight green is about 8.3:1; bone and muted bone on the glass tint are above 7:1 (WCAG AA needs 4.5:1).
BONE = {"r": 0.888, "g": 0.847, "b": 0.777, "a": 1}
BONE_MUTED = {"r": 0.584, "g": 0.546, "b": 0.479, "a": 1}
BONE_EDGE = {"r": 0.888, "g": 0.847, "b": 0.777, "a": 0.14}
BONE_OUTLINE = {"r": 0.888, "g": 0.847, "b": 0.777, "a": 0.4}
MIDNIGHT = {"r": 0.0, "g": 0.0723, "b": 0.0887, "a": 1}
MIDNIGHT_HOVER = {"r": 0.0, "g": 0.095, "b": 0.115, "a": 1}
MIDNIGHT_PRESSED = {"r": 0.0, "g": 0.0331, "b": 0.0395, "a": 1}
# Dark frosted glass: a background blur under a near-opaque tint, so text contrast holds over a bright sky.
GLASS = {"r": 0.004, "g": 0.009, "b": 0.011, "a": 0.86}
SECONDARY = {"r": 0.02, "g": 0.035, "b": 0.04, "a": 0.95}
SECONDARY_PRESSED = {"r": 0.008, "g": 0.014, "b": 0.016, "a": 0.95}
DISABLED = {"r": 0.03, "g": 0.03, "b": 0.03, "a": 0.55}
CHIP = {"r": 0.05, "g": 0.07, "b": 0.075, "a": 1}
SCRIM = {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.55}
NOTICE = {"r": 1.0, "g": 0.85, "b": 0.45, "a": 1}
WHITE = {"r": 1, "g": 1, "b": 1, "a": 1}

RADIUS = 16
BLUR = 18
# The stadium is the product, so the chrome stays small. Hit targets (1080p) are the project minimum of 64 px
# everywhere except Start (72). Font sizes follow ACCESSIBILITY.md at 1080p: titles 30-40, body 22, controls 20+.
TOP_H, RAIL_H, START_H, TOGGLE_H = 64, 64, 72, 64
# The bottom 72 px stay clear for the Cesium and Google credits. The timeline sits on the rail; the caption above it.
CREDITS_CLEAR = 72
TIMELINE_H = 76
TIMELINE_BOTTOM = CREDITS_CLEAR + RAIL_H + 12
CAPTION_BOTTOM = TIMELINE_BOTTOM + TIMELINE_H + 16
# Caption text width. With padding the card is about 450 px wide (under a quarter of 1920) and 180-240 px tall.
CAPTION_WRAP = 400

KEYS = [
    ("Right arrow or Space", "Next"),
    ("Left arrow", "Back"),
    ("P", "Pause"),
    ("S", "Settings"),
    ("1", "Less movement"),
    ("2", "Sound"),
    ("3", "Calm Mode"),
    ("4", "Large text"),
    ("5", "Voice"),
    ("6", "Crowd"),
    ("V", "Preview (loud or bright steps)"),
    ("R", "Start again"),
    ("Enter", "Yes"),
    ("Esc", "Close"),
]

# Names C++ reads, per widget. They are marked as variables and checked after the build.
CONTRACT = {
    "WBP_LFF_Splash": {"TitleText", "BodyText", "StartButton", "DisclaimerText", "TextScale", "StandardButton", "CalmButton"},
    "WBP_LFF_TopChrome": {"DisclaimerLineText", "StepControls", "ProgressText", "StandardButton", "CalmButton",
                          "SettingsButton"},
    "WBP_LFF_StepCaption": {"StepNumberText", "TitleText", "BodyText", "DisclaimerChip", "DisclaimerChipText",
                            "TargetMissingText", "TextScale", "PreviewRow", "PreviewButton", "PreviewLabel",
                            "PreviewNoteText", "TicketPhone", "TicketImage",
                            "FinaleActions", "FinaleStartOverButton", "FinaleLearnMoreButton"},
    "WBP_LFF_BottomRail": {"BackButton", "NextButton", "NextLabel", "ProgressPips", "TimelinePanel", "TimelineMarks",
                           "TimelineNoteText", "LearnMorePanel", "LearnMoreButton"},
    "WBP_LFF_Settings": {"Scrim", "SettingsPanel", "SettingsCloseButton", "CalmModeButton", "CalmModeLabel",
                         "ReducedMotionButton", "ReducedMotionLabel", "SoundButton", "SoundLabel", "LargeTextButton",
                         "LargeTextLabel", "HighContrastButton", "HighContrastLabel", "RestartButton", "PausePanel",
                         "ResumeButton", "RestartConfirmPanel", "RestartYesButton", "RestartNoButton",
                         "VoiceButton", "VoiceLabel", "CrowdButton", "CrowdLabel"},
    "WBP_LFF_HUD": {"Splash", "TopChrome", "StepCaption", "BottomRail", "Settings"},
}


def ref(name):
    return {"refPath": f"{FOLDER}/{name}.{name}"}


def set_props(target, values):
    if not call(OBJ, "set_properties", {"instance": target, "values": json.dumps(values)}):
        raise RuntimeError(f"set_properties failed on {target['refPath']}: {values}")


class Builder:
    def __init__(self, name):
        self.name = name
        self.bp = ref(name)

    def add(self, cls, name, parent=None, props=None, slot=None):
        widget_class = cls if cls.startswith("/") else f"/Script/UMG.{cls}"
        info = call(UMG, "AddWidget", {"widgetBlueprint": self.bp, "widgetClass": {"refPath": widget_class},
                                       "widgetDisplayName": name, "parentWidget": parent, "childIndex": -1})
        widget = info["widget"]
        if props:
            set_props(widget, props)
        if slot and info.get("slot") not in (None, "None"):
            set_props(info["slot"], slot)
        if name in CONTRACT[self.name]:
            call(UMG, "ToggleWidgetAsVariable", {"widgetBlueprint": self.bp, "widget": widget, "bIsVariable": True})
        return widget

    def text(self, name, value, parent, size, face="Regular", color=BONE, wrap=None, justify=None, slot=None):
        props = {"Text": value, "Font": font(size, face),
                 "ColorAndOpacity": {"specifiedColor": color, "colorUseRule": "UseColor_Specified"}}
        if wrap:
            props.update({"AutoWrapText": False, "WrapTextAt": wrap})
        if justify:
            props["Justification"] = justify
        return self.add("TextBlock", name, parent, props, slot)

    def glass(self, name, surface, parent, slot, padding):
        """BackgroundBlur (named `name`, the part C++ shows or hides) > Border `Surface<surface>` (the tint)."""
        blur = self.add("BackgroundBlur", name, parent, {"BlurStrength": BLUR, "bApplyAlphaToBlur": True, "Padding": pad(0),
                                                         "CornerRadius": {"x": RADIUS, "y": RADIUS, "z": RADIUS, "w": RADIUS}}, slot)
        return self.add("Border", f"Surface{surface}", blur, {"Background": rounded(WHITE, BONE_EDGE, 1), "BrushColor": GLASS,
                                                              "Padding": padding},
                        {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})

    def button(self, name, label_name, label, parent, primary=False, min_w=180, min_h=RAIL_H, size=22, slot=None):
        """SizeBox (minimum size) > Button (not focusable; C++ hands focus back to the game) > label."""
        box = self.add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": min_w,
                                                          "bOverride_MinDesiredHeight": True, "MinDesiredHeight": min_h}, slot)
        style = PRIMARY_STYLE if primary else SECONDARY_STYLE
        b = self.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": style, "BackgroundColor": WHITE},
                     {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
        self.text(label_name, label, b, size, "Bold", slot={"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
        return b

    def child(self, name):
        """An existing widget in this blueprint's tree (a parent for --extend)."""
        return {"refPath": f"{self.bp['refPath']}:WidgetTree.{name}"}

    def collapse(self, *names):
        for name in names:
            set_props({"refPath": f"{self.bp['refPath']}:WidgetTree.{name}"}, {"Visibility": "Collapsed"})


def font(size, face):
    return {"fontObject": {"refPath": "/Engine/EngineFonts/Roboto.Roboto"}, "typefaceFontName": face, "size": size}


def pad(l, t=None, r=None, b=None):
    t = l if t is None else t
    return {"left": l, "top": t, "right": l if r is None else r, "bottom": t if b is None else b}


def rounded(fill, outline, width, half_height=False):
    return {"tintColor": {"specifiedColor": fill, "colorUseRule": "UseColor_Specified"}, "drawAs": "RoundedBox",
            "outlineSettings": {"cornerRadii": {"x": RADIUS, "y": RADIUS, "z": RADIUS, "w": RADIUS}, "width": width,
                                "roundingType": "HalfHeightRadius" if half_height else "FixedRadius",
                                "color": {"specifiedColor": outline, "colorUseRule": "UseColor_Specified"}}}


def button_style(fill, hover_fill, pressed_fill, outline):
    # Hover and press only change the outline or darken, so contrast never drops.
    return {"normal": rounded(fill, outline, 2, True), "hovered": rounded(hover_fill, BONE, 3, True),
            "pressed": rounded(pressed_fill, BONE, 3, True), "disabled": rounded(DISABLED, {"r": 0.3, "g": 0.3, "b": 0.3, "a": 0.5}, 2, True),
            "normalPadding": pad(20, 6), "pressedPadding": pad(20, 7, 20, 5)}


PRIMARY_STYLE = button_style(MIDNIGHT, MIDNIGHT_HOVER, MIDNIGHT_PRESSED, BONE_OUTLINE)
SECONDARY_STYLE = button_style(SECONDARY, SECONDARY, SECONDARY_PRESSED, BONE_OUTLINE)
# Icon-only buttons: the same look, with padding that leaves room for a 32 px icon in a 64 px circle.
ICON_STYLE = {**SECONDARY_STYLE, "normalPadding": pad(16), "pressedPadding": pad(16, 17, 16, 15)}


def gap(px):
    return {"Padding": pad(0, 0, 0, px)}


def fill_slot():
    return {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"}


EAF_LOGO = "/Game/LFF/UI/Icons/T_LFF_Logo_EAF_White.T_LFF_Logo_EAF_White"  # Assets/Logos/EAF-logo_white.png, cropped: 1024 x 357
GEAR_ICON = "/Game/LFF/UI/Icons/T_LFF_Icon_Settings.T_LFF_Icon_Settings"  # SourceData/UI, white on transparent, 128 px


def mode_segment(w, parent, slot):
    """Standard | Calm: two halves of one row. C++ dims the side that isn't chosen."""
    row = w.add("HorizontalBox", "ModeSegment", parent, None, slot)
    w.button("StandardButton", "StandardLabel", "Standard", row, min_w=140, min_h=TOP_H, size=20, slot={"Padding": pad(0, 0, 4, 0)})
    w.button("CalmButton", "CalmLabel", "Calm", row, min_w=140, min_h=TOP_H, size=20)


def build_splash(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(0, 0, 0, 110)})
    card = w.glass("SplashCard", "SplashCard", scale, None, pad(40, 28))
    stack = w.add("VerticalBox", "SplashStack", card)
    centre = {"HorizontalAlignment": "HAlign_Center"}
    w.text("WelcomeText", "Welcome to", stack, 20, color=BONE_MUTED, slot={**centre, **gap(2)})
    w.text("TitleText", "Lincoln Financial Field", stack, 38, "Bold", justify="Center", slot={**centre, **gap(10)})
    w.text("BodyText", "A calm preview of game day.\nYou choose when to move on.", stack, 22, wrap=560, justify="Center",
           slot={**centre, **gap(20)})
    # Standard or Calm before Start (the same choice as the top bar's segmented control).
    w.text("ModePromptText", "How would you like your day?", stack, 20, color=BONE_MUTED, slot={**centre, **gap(8)})
    mode_segment(w, stack, {**centre, **gap(6)})
    w.text("ModeNoteText", "Standard: voice, music, crowd and the big moments. Calm: quiet, still, no spectacle.", stack, 16,
           color=BONE_MUTED, wrap=560, justify="Center", slot={**centre, **gap(18)})
    w.button("StartButton", "StartLabel", "Start Your Day", stack, primary=True, min_w=320, min_h=START_H, size=26,
             slot={**centre, **gap(18)})
    w.text("DisclaimerText", "Illustration only. This is not a navigation instruction for the real stadium.", stack, 16,
           color=BONE_MUTED, wrap=560, justify="Center", slot=centre)


def build_top_chrome(w):
    root = w.add("Overlay", "Root")
    # The white Eagles Autism Foundation logo straight on the world, no card behind it.
    badge = w.add("VerticalBox", "Badge", root, None, {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Top",
                                                       "Padding": pad(32, 24, 0, 0)})
    w.add("Image", "EAFLogo", badge, {"Brush": {"resourceObject": {"refPath": EAF_LOGO}, "imageSize": {"x": 220, "y": 77}},
                                      "ToolTipText": "Eagles Autism Foundation"}, {"HorizontalAlignment": "HAlign_Left"})
    # The one persistent disclaimer line on journey steps (C++ shows it after Start; the splash has the full sentence).
    # No card either, so a soft shadow keeps it legible over a bright sky.
    w.add("TextBlock", "DisclaimerLineText", badge, {"Text": "Illustration. Not directions for the real stadium.", "Font": font(16, "Regular"),
          "ColorAndOpacity": {"specifiedColor": BONE, "colorUseRule": "UseColor_Specified"}, "ShadowOffset": {"x": 1, "y": 1},
          "ShadowColorAndOpacity": {"r": 0, "g": 0, "b": 0, "a": 0.85}}, {"Padding": pad(0, 6, 0, 0)})
    w.collapse("DisclaimerLineText")

    bar = w.glass("TopBar", "TopBar", root, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Top",
                                             "Padding": pad(0, 20, 32, 0)}, pad(6))
    row = w.add("HorizontalBox", "Bar", bar)
    controls = w.add("HorizontalBox", "StepControls", row, None, {"VerticalAlignment": "VAlign_Center"})
    w.text("ProgressText", "Step 1 of 16", controls, 20, "Bold",
           slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(14, 0, 16, 0)})
    # Pause and Sound stay on the keyboard (P, 2); the bar shows the Standard | Calm choice instead.
    mode_segment(w, controls, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 6, 0)})
    # Settings: a 64 px gear button (the label is the tooltip).
    box = w.add("SizeBox", "SettingsButtonSize", row, {"bOverride_WidthOverride": True, "WidthOverride": TOP_H,
                                                       "bOverride_HeightOverride": True, "HeightOverride": TOP_H},
                {"VerticalAlignment": "VAlign_Center"})
    gear = w.add("Button", "SettingsButton", box, {"IsFocusable": False, "WidgetStyle": ICON_STYLE, "BackgroundColor": WHITE,
                                                   "ToolTipText": "Settings"},
                 {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    w.add("Image", "SettingsIcon", gear, {"Brush": {"resourceObject": {"refPath": GEAR_ICON}, "imageSize": {"x": 32, "y": 32}},
                                          "ColorAndOpacity": BONE},
          {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})


def build_step_caption(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(48, 0, 0, CAPTION_BOTTOM)})
    card = w.glass("Caption", "Caption", scale, None, pad(24, 18))
    stack = w.add("VerticalBox", "CaptionStack", card)
    header = w.add("HorizontalBox", "CaptionHeader", stack, None, gap(6))
    w.text("StepNumberText", "Step 1", header, 17, "Bold", color=BONE_MUTED,
           slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    chip = w.add("Border", "DisclaimerChip", header, {"Background": rounded(WHITE, BONE_OUTLINE, 1.5, True), "BrushColor": CHIP,
                                                       "Padding": pad(10, 2)},
                 {"VerticalAlignment": "VAlign_Center", "HorizontalAlignment": "HAlign_Right", "Padding": pad(16, 0, 0, 0)})
    w.text("DisclaimerChipText", "Illustration", chip, 16)
    w.text("TitleText", "Title", stack, 30, "Bold", wrap=CAPTION_WRAP, slot=gap(4))
    w.text("BodyText", "Body", stack, 22, wrap=CAPTION_WRAP)
    # Loud or bright steps only. Nothing plays until the visitor picks Soft; Calm Mode hides the row (C++).
    preview = w.add("VerticalBox", "PreviewRow", stack, None, {"Padding": pad(0, 12, 0, 0)})
    w.button("PreviewButton", "PreviewLabel", "Preview this? Off", preview, min_w=260, min_h=TOGGLE_H, size=20,
             slot={"HorizontalAlignment": "HAlign_Left"})
    w.text("PreviewNoteText", "", preview, 16, color=BONE_MUTED, wrap=CAPTION_WRAP, slot={"Padding": pad(0, 6, 0, 0)})
    w.text("TargetMissingText", "This step is not available yet.", stack, 18, "Bold", color=NOTICE, wrap=CAPTION_WRAP,
           slot={"Padding": pad(0, 8, 0, 0)})
    w.collapse("TargetMissingText", "PreviewRow")


def build_bottom_rail(w):
    root = w.add("Overlay", "Root")
    rail = w.add("Overlay", "Rail", root, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Bottom",
                                                 "Padding": pad(32, 0, 32, CREDITS_CLEAR)})
    w.button("BackButton", "BackLabel", "Back", rail, min_w=170, size=24,
             slot={"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Center"})
    pips = w.glass("PipsCard", "Pips", rail, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}, pad(14, 10))
    w.add("HorizontalBox", "ProgressPips", pips)
    w.button("NextButton", "NextLabel", "Next", rail, primary=True, min_w=170, size=24,
             slot={"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center"})


def build_settings(w):
    root = w.add("Overlay", "Root")
    w.add("Border", "Scrim", root, {"BrushColor": SCRIM, "Visibility": "Collapsed"}, fill_slot())
    centre = {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}

    card = w.glass("SettingsPanel", "Settings", root, centre, pad(36, 28))
    stack = w.add("VerticalBox", "SettingsStack", card)
    header = w.add("HorizontalBox", "SettingsHeader", stack, None, gap(4))
    w.text("SettingsTitle", "Settings", header, 34, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    w.button("SettingsCloseButton", "SettingsCloseLabel", "Close", header, min_w=140, min_h=TOP_H, size=20,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.text("SettingsHint", "You can change these at any time. Calm Mode turns sound off and uses less movement.", stack, 18,
           color=BONE_MUTED, wrap=760, slot=gap(18))
    columns = w.add("HorizontalBox", "SettingsColumns", stack)
    toggles = w.add("VerticalBox", "ToggleColumn", columns, None, {"Padding": pad(0, 0, 40, 0)})
    for name, label_name, label in (("CalmModeButton", "CalmModeLabel", "Calm Mode: Off"),
                                    ("ReducedMotionButton", "ReducedMotionLabel", "Less movement: Off"),
                                    ("SoundButton", "SoundLabel", "Sound: Off"),
                                    ("LargeTextButton", "LargeTextLabel", "Large text: Off"),
                                    ("HighContrastButton", "HighContrastLabel", "High contrast: Off")):
        w.button(name, label_name, label, toggles, min_w=340, min_h=TOGGLE_H, size=20, slot=gap(8))

    keys = w.add("VerticalBox", "KeysColumn", columns)
    w.text("KeysTitle", "Keyboard", keys, 20, "Bold", slot=gap(8))
    table = w.add("HorizontalBox", "KeysTable", keys, None, gap(18))
    w.text("KeysKeys", "\n".join(k for k, _ in KEYS), table, 17, "Bold", slot={"Padding": pad(0, 0, 20, 0)})
    w.text("KeysActions", "\n".join(a for _, a in KEYS), table, 17, color=BONE_MUTED)
    w.button("RestartButton", "RestartLabel", "Start again", keys, min_w=220, min_h=TOGGLE_H, size=20)

    pause = w.glass("PausePanel", "Pause", root, centre, pad(44, 32))
    pause_stack = w.add("VerticalBox", "PauseStack", pause)
    w.text("PauseTitle", "Paused", pause_stack, 34, "Bold", slot={"HorizontalAlignment": "HAlign_Center", **gap(8)})
    w.text("PauseBody", "Nothing will move until you resume.", pause_stack, 22, wrap=560, slot={"HorizontalAlignment": "HAlign_Center", **gap(20)})
    w.button("ResumeButton", "ResumeLabel", "Resume", pause_stack, primary=True, min_w=220, slot={"HorizontalAlignment": "HAlign_Center"})

    confirm = w.glass("RestartConfirmPanel", "Confirm", root, centre, pad(44, 32))
    confirm_stack = w.add("VerticalBox", "RestartStack", confirm)
    w.text("RestartTitle", "Start again?", confirm_stack, 34, "Bold", slot={"HorizontalAlignment": "HAlign_Center", **gap(8)})
    w.text("RestartBody", "You will go back to the beginning.", confirm_stack, 22, wrap=560,
           slot={"HorizontalAlignment": "HAlign_Center", **gap(20)})
    choices = w.add("HorizontalBox", "RestartChoices", confirm_stack, None, {"HorizontalAlignment": "HAlign_Center"})
    w.button("RestartNoButton", "RestartNoLabel", "No, keep going", choices, min_w=220, slot={"Padding": pad(0, 0, 16, 0)})
    w.button("RestartYesButton", "RestartYesLabel", "Yes, start again", choices, primary=True, min_w=220)

    w.collapse("SettingsPanel", "PausePanel", "RestartConfirmPanel")


def extend_step_caption(w):
    """The phone that rises on the ticket step: right of centre, above the timeline. C++ animates and fills it."""
    root = w.child("Root")
    phone = w.add("SizeBox", "TicketPhone", root, {"bOverride_WidthOverride": True, "WidthOverride": 250,
                                                    "bOverride_HeightOverride": True, "HeightOverride": 500},
                  {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Bottom",
                   "Padding": pad(0, 0, 220, CAPTION_BOTTOM)})
    w.add("Image", "TicketImage", phone, None, fill_slot())
    w.collapse("TicketPhone")


def extend_bottom_rail(w):
    """The thin timeline above Back/Next (C++ adds one mark per timed step) and Learn More on the last step."""
    root = w.child("Root")
    card = w.glass("TimelinePanel", "Timeline", root, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom",
                                                       "Padding": pad(0, 0, 0, TIMELINE_BOTTOM)}, pad(12, 6))
    row = w.add("HorizontalBox", "TimelineRow", card)
    w.text("TimelineNoteText", "Approximate times\nExample 1:00 PM game", row, 16, color=BONE_MUTED,
           slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(4, 0, 16, 0)})
    w.add("HorizontalBox", "TimelineMarks", row, None, {"VerticalAlignment": "VAlign_Center"})
    box = w.add("SizeBox", "LearnMorePanel", w.child("Rail"), None,
                {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 186, 0)})
    w.button("LearnMoreButton", "LearnMoreLabel", "Learn More", box, min_w=200, size=22)
    w.collapse("TimelinePanel", "LearnMorePanel")


def extend_settings(w):
    """Voice (off by default; needs Sound) and Crowd (Low or Off) under the other toggles, and the new keys."""
    toggles = w.child("ToggleColumn")
    for name, label_name, label in (("VoiceButton", "VoiceLabel", "Voice: Off"), ("CrowdButton", "CrowdLabel", "Crowd: Low")):
        w.button(name, label_name, label, toggles, min_w=340, min_h=TOGGLE_H, size=20, slot=gap(8))
    set_props(w.child("KeysKeys"), {"Text": "\n".join(k for k, _ in KEYS)})
    set_props(w.child("KeysActions"), {"Text": "\n".join(a for _, a in KEYS)})
    set_props(w.child("SettingsHint"), {"Text": "You can change these at any time. Calm Mode turns sound and voice off and uses less movement."})


def move_caption_above_timeline(w):
    """The caption card sits above the timeline (session 3), so the two never overlap."""
    info = call(UMG, "GetWidgets", {"widgetBlueprint": w.bp})
    scale = next(x for x in info["widgets"] if x["widgetName"] == "TextScale" and x["widget"] != "None")
    if scale.get("slot") not in (None, "None"):
        set_props(scale["slot"], {"Padding": pad(48, 0, 0, CAPTION_BOTTOM)})


# Session 3 additions: widget -> (the names they add, the function that adds them).
EXTENSIONS = {
    "WBP_LFF_StepCaption": ({"TicketPhone"}, extend_step_caption),
    "WBP_LFF_BottomRail": ({"TimelinePanel", "LearnMorePanel"}, extend_bottom_rail),
    "WBP_LFF_Settings": ({"VoiceButton", "CrowdButton"}, extend_settings),
}


def add_finale_actions(w):
    """Start Over and Learn More on the caption card. C++ shows them and centres the card on the last step."""
    row = w.add("HorizontalBox", "FinaleActions", w.child("CaptionStack"), None,
                {"HorizontalAlignment": "HAlign_Center", "Padding": pad(0, 16, 0, 0)})
    w.button("FinaleStartOverButton", "FinaleStartOverLabel", "Start Over", row, min_w=200, size=22, slot={"Padding": pad(0, 0, 8, 0)})
    w.button("FinaleLearnMoreButton", "FinaleLearnMoreLabel", "Learn More", row, primary=True, min_w=200, size=22)
    w.collapse("FinaleActions")


# Slice A (session 6): widget -> the function that adds its pieces, on a fresh build or --rebuild.
SLICE_A = {"WBP_LFF_StepCaption": add_finale_actions}


def build_hud(w):
    root = w.add("Overlay", "RootOverlay")
    # Later children draw on top: the modal layer covers everything else.
    for instance, asset in (("TopChrome", "WBP_LFF_TopChrome"), ("Splash", "WBP_LFF_Splash"), ("StepCaption", "WBP_LFF_StepCaption"),
                            ("BottomRail", "WBP_LFF_BottomRail"), ("Settings", "WBP_LFF_Settings")):
        w.add(f"{FOLDER}/{asset}.{asset}_C", instance, root, None, fill_slot())


# ULFFJourneyPanel style properties set on the generated class defaults. Done pips are muted bone, upcoming are grey,
# and the current pip is a wider bone pill, so position never depends on colour alone.
DEFAULTS = {
    "WBP_LFF_BottomRail": {"PipCurrentColor": BONE, "PipDoneColor": BONE_MUTED, "PipUpcomingColor": {"r": 0.2, "g": 0.2, "b": 0.2, "a": 0.9}},
}

WIDGETS = [
    ("WBP_LFF_Splash", PANEL_CLASS, build_splash),
    ("WBP_LFF_TopChrome", PANEL_CLASS, build_top_chrome),
    ("WBP_LFF_StepCaption", PANEL_CLASS, build_step_caption),
    ("WBP_LFF_BottomRail", PANEL_CLASS, build_bottom_rail),
    ("WBP_LFF_Settings", PANEL_CLASS, build_settings),
    ("WBP_LFF_HUD", HUD_CLASS, build_hud),  # last: it instances the five above
]


def prepare(name, parent_class, rebuild):
    """Creates the widget, or clears its tree when rebuilding. The asset itself is kept so references stay valid."""
    if not call(ASSET, "exists", {"path": f"{FOLDER}/{name}"}):
        call(UMG, "CreateWidgetBlueprint", {"folderPath": FOLDER, "assetName": name, "parentClass": {"refPath": parent_class}})
        return
    if not rebuild:
        raise SystemExit(f"{FOLDER}/{name} already exists; pass --rebuild to clear and rebuild it")
    info = call(UMG, "GetWidgets", {"widgetBlueprint": ref(name)})
    roots = [x for x in info["widgets"] if x["widget"] != "None" and x["parent"] == "None"]
    for root in roots:
        call(UMG, "RemoveWidget", {"widgetBlueprint": ref(name), "widget": root["widget"]})


def finish(name):
    if not call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": ref(name)}):
        raise RuntimeError(f"{name} failed to compile")
    if name in DEFAULTS:
        set_props({"refPath": f"{FOLDER}/{name}.Default__{name}_C"}, DEFAULTS[name])
    call(ASSET, "save_assets", {"asset_paths": [f"{FOLDER}/{name}"]})
    names = {x["widgetName"] for x in call(UMG, "GetWidgets", {"widgetBlueprint": ref(name)})["widgets"] if x["widget"] != "None"}
    missing = sorted(CONTRACT[name] - names)
    if missing:
        raise RuntimeError(f"{name}: missing contract widgets {missing}")
    return names


def extend():
    """Adds the session 3 pieces to widgets that exist already. Skips any whose names are already there."""
    for name, (marker_names, extension) in EXTENSIONS.items():
        existing = {x["widgetName"] for x in call(UMG, "GetWidgets", {"widgetBlueprint": ref(name)})["widgets"] if x["widget"] != "None"}
        w = Builder(name)
        if marker_names & existing:
            print(f"{name}: session 3 widgets already present; left as is")
        else:
            extension(w)
        if name == "WBP_LFF_StepCaption":
            move_caption_above_timeline(w)
        names = finish(name)
        print(f"extended {FOLDER}/{name}: {len(names)} widgets, all {len(CONTRACT[name])} contract names present, compiled and saved")


def build(rebuild):
    for name, parent_class, builder in WIDGETS:
        prepare(name, parent_class, rebuild)
        builder(Builder(name))
        if name in EXTENSIONS:
            EXTENSIONS[name][1](Builder(name))
        if name in SLICE_A:
            SLICE_A[name](Builder(name))
        if not call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": ref(name)}):
            raise RuntimeError(f"{name} failed to compile")
        if name in DEFAULTS:
            set_props({"refPath": f"{FOLDER}/{name}.Default__{name}_C"}, DEFAULTS[name])
        call(ASSET, "save_assets", {"asset_paths": [f"{FOLDER}/{name}"]})
        # GetWidgets also lists the parent's unbound BindWidget names as placeholders with widget == "None".
        names = {x["widgetName"] for x in call(UMG, "GetWidgets", {"widgetBlueprint": ref(name)})["widgets"] if x["widget"] != "None"}
        missing = sorted(CONTRACT[name] - names)
        if missing:
            raise RuntimeError(f"{name}: missing contract widgets {missing}")
        print(f"built {FOLDER}/{name}: {len(names)} widgets, all {len(CONTRACT[name])} contract names present, compiled and saved")


if __name__ == "__main__":
    if "--extend" in sys.argv[1:]:
        extend()
    else:
        build("--rebuild" in sys.argv[1:])
