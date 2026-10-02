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

def srgb(hex_value, alpha=1.0):
    """#RRGGBB to the linear colour UMG stores."""
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (int(hex_value[i:i + 2], 16) for i in (1, 3, 5))
    return {"r": lin(r), "g": lin(g), "b": lin(b), "a": alpha}


# Design tokens (Docs/UI_AUDIT.md). Opaque surfaces, so text contrast never depends on what the camera sees.
# Contrast (WCAG): TEXT on SURFACE about 15:1, SILVER on SURFACE about 7.4:1, TEXT on BRAND about 8.8:1.
SURFACE_HEX, RAISED_HEX, TEXT_HEX, SILVER_HEX, BRAND_HEX, ACCENT_HEX = "#0B1D1F", "#12292C", "#F4F1EA", "#A5ACAF", "#004C54", "#69BE28"
SURFACE = srgb(SURFACE_HEX)
RAISED = srgb(RAISED_HEX)
RAISED_PRESSED = srgb("#0E2124")
BONE = srgb(TEXT_HEX)               # primary text (name kept: C++ and DEFAULTS refer to it)
BONE_MUTED = srgb(SILVER_HEX)       # secondary text, Eagles silver
BONE_EDGE = srgb(SILVER_HEX, 0.30)  # hairline around cards
BONE_OUTLINE = srgb(SILVER_HEX, 0.65)
MIDNIGHT = srgb(BRAND_HEX)          # primary button fill, Eagles midnight green
MIDNIGHT_HOVER = srgb("#00606A")
MIDNIGHT_PRESSED = srgb("#003A40")
ACCENT = srgb(ACCENT_HEX)           # kelly green: hover and focus ring only
GLASS = SURFACE                     # kept as a name; no longer translucent
SECONDARY = RAISED
SECONDARY_PRESSED = RAISED_PRESSED
DISABLED = srgb("#0B1D1F", 0.6)
CHIP = RAISED
SCRIM = {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.55}
NOTICE = srgb("#FFD973")
WHITE = {"r": 1, "g": 1, "b": 1, "a": 1}

RADIUS = 12
BLUR = 0  # opaque panels: no background blur
# Type scale at 1080p: label 18, body 22, title 32, headline 40. Space on an 8 px grid.
T_LABEL, T_BODY, T_TITLE, T_HEAD = 18, 22, 32, 40
# The stadium is the product, so the chrome stays small. Hit targets (1080p) are the project minimum of 64 px
# everywhere except Start (72). Font sizes follow ACCESSIBILITY.md at 1080p: titles 30-40, body 22, controls 20+.
TOP_H, RAIL_H, START_H, TOGGLE_H = 64, 64, 72, 64
HEADER_H = 96
# The bottom 72 px stay clear for the Cesium and Google credits. The timeline sits on the rail; the caption above it.
CREDITS_CLEAR = 72
TIMELINE_H = 76
TIMELINE_BOTTOM = CREDITS_CLEAR + RAIL_H + 12
CAPTION_BOTTOM = TIMELINE_BOTTOM + TIMELINE_H + 16
# Caption text width. With padding the card is about 450 px wide (under a quarter of 1920) and 180-240 px tall.
CAPTION_WRAP = 440

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
    "WBP_LFF_Splash": {"TitleText", "BodyText", "StartButton", "DisclaimerText", "TextScale", "StandardButton", "CalmButton",
                       "StandardLabel", "CalmLabel"},
    "WBP_LFF_TopChrome": {"DisclaimerLineText", "SettingsButton"},
    "WBP_LFF_StepCaption": {"StepNumberText", "TitleText", "BodyText", "DisclaimerChip", "DisclaimerChipText",
                            "TargetMissingText", "TextScale", "PreviewRow", "PreviewButton", "PreviewLabel",
                            "PreviewNoteText", "TicketPhone", "TicketImage",
                            "FinaleActions", "FinaleStartOverButton", "FinaleLearnMoreButton"},
    "WBP_LFF_BottomRail": {"BackButton", "NextButton", "NextLabel", "ProgressPips", "TimelinePanel", "TimelineMarks",
                           "TimelineNoteText", "LearnMorePanel", "LearnMoreButton", "ProgressText", "TimelineToggleButton",
                           "TimelineToggleLabel"},
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
    return {"normal": rounded(fill, outline, 2, True), "hovered": rounded(hover_fill, ACCENT, 3, True),
            "pressed": rounded(pressed_fill, ACCENT, 3, True), "disabled": rounded(DISABLED, srgb(SILVER_HEX, 0.25), 2, True),
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
    w.button("StandardButton", "StandardLabel", "Standard", row, min_w=160, min_h=TOP_H, size=T_BODY, slot={"Padding": pad(0, 0, 8, 0)})
    w.button("CalmButton", "CalmLabel", "Calm", row, min_w=160, min_h=TOP_H, size=T_BODY)


def build_splash(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(0, 0, 0, 120)})
    card = w.glass("SplashCard", "SplashCard", scale, None, pad(56, 40))
    stack = w.add("VerticalBox", "SplashStack", card)
    centre = {"HorizontalAlignment": "HAlign_Center"}
    w.text("WelcomeText", "Welcome to", stack, T_LABEL, color=BONE_MUTED, slot={**centre, **gap(4)})
    w.text("TitleText", "Lincoln Financial Field", stack, T_HEAD, "Bold", justify="Center", slot={**centre, **gap(16)})
    w.text("BodyText", "A calm preview of game day.\nYou choose when to move on.", stack, T_BODY, wrap=560, justify="Center",
           slot={**centre, **gap(32)})
    # Standard or Calm before Start (the same choice is in Settings).
    w.text("ModePromptText", "How would you like your day?", stack, T_LABEL, "Bold", color=BONE_MUTED, slot={**centre, **gap(12)})
    mode_segment(w, stack, {**centre, **gap(12)})
    w.text("ModeNoteText", "Standard: voice, music, crowd and the big moments.\nCalm: quiet, still, no spectacle.", stack, T_LABEL,
           color=BONE_MUTED, wrap=560, justify="Center", slot={**centre, **gap(32)})
    w.button("StartButton", "StartLabel", "Start Your Day", stack, primary=True, min_w=320, min_h=START_H, size=26,
             slot={**centre, **gap(24)})
    w.text("DisclaimerText", "Illustration only. This is not a navigation instruction for the real stadium.", stack, T_LABEL,
           color=BONE_MUTED, wrap=560, justify="Center", slot=centre)


def build_top_chrome(w):
    root = w.add("Overlay", "Root")
    # One solid header band: logo and the disclaimer on the left, progress and Settings on the right. Nothing sits
    # straight on the 3D scene any more, so the logo and the "Illustration" line are always legible.
    bar = w.glass("TopBar", "TopBar", root, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Top"}, pad(32, 16))
    row = w.add("HorizontalBox", "Bar", bar)
    w.add("Image", "EAFLogo", row, {"Brush": {"resourceObject": {"refPath": EAF_LOGO}, "imageSize": {"x": 176, "y": 62}},
                                    "ToolTipText": "Eagles Autism Foundation"},
          {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 24, 0)})
    # The one persistent disclaimer line on journey steps (C++ shows it after Start; the splash has the full sentence).
    w.add("TextBlock", "DisclaimerLineText", row, {"Text": "Illustration. Not directions for the real stadium.", "Font": font(T_LABEL, "Regular"),
          "ColorAndOpacity": {"specifiedColor": BONE_MUTED, "colorUseRule": "UseColor_Specified"}},
          {"VerticalAlignment": "VAlign_Center"})
    w.collapse("DisclaimerLineText")
    # Fill slot keeps Settings right-aligned on the splash, where the disclaimer line is hidden.
    w.add("Spacer", "HeaderSpacer", row, None, {"Size": {"value": 1, "sizeRule": "Fill"}})
    # Settings: icon plus the word, never the icon alone.
    box = w.add("SizeBox", "SettingsButtonSize", row, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": 180,
                                                       "bOverride_MinDesiredHeight": True, "MinDesiredHeight": TOP_H},
                {"VerticalAlignment": "VAlign_Center"})
    gear = w.add("Button", "SettingsButton", box, {"IsFocusable": False, "WidgetStyle": SECONDARY_STYLE, "BackgroundColor": WHITE,
                                                   "ToolTipText": "Settings (S)"},
                 {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    inner = w.add("HorizontalBox", "SettingsContent", gear, None, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.add("Image", "SettingsIcon", inner, {"Brush": {"resourceObject": {"refPath": GEAR_ICON}, "imageSize": {"x": 28, "y": 28}},
                                           "ColorAndOpacity": BONE}, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 10, 0)})
    w.text("SettingsLabel", "Settings", inner, T_BODY, "Bold", slot={"VerticalAlignment": "VAlign_Center"})


def build_step_caption(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(48, 0, 0, CAPTION_BOTTOM)})
    card = w.glass("Caption", "Caption", scale, None, pad(32, 24))
    stack = w.add("VerticalBox", "CaptionStack", card)
    header = w.add("HorizontalBox", "CaptionHeader", stack, None, gap(6))
    w.text("StepNumberText", "Step 1", header, T_LABEL, "Bold", color=BONE_MUTED,
           slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    chip = w.add("Border", "DisclaimerChip", header, {"Background": rounded(WHITE, BONE_OUTLINE, 1.5, True), "BrushColor": CHIP,
                                                       "Padding": pad(10, 2)},
                 {"VerticalAlignment": "VAlign_Center", "HorizontalAlignment": "HAlign_Right", "Padding": pad(16, 0, 0, 0)})
    w.text("DisclaimerChipText", "Illustration", chip, T_LABEL)
    w.text("TitleText", "Title", stack, T_TITLE, "Bold", wrap=CAPTION_WRAP, slot=gap(12))
    w.text("BodyText", "Body", stack, T_BODY, wrap=CAPTION_WRAP)
    # Loud or bright steps only. Nothing plays until the visitor picks Soft; Calm Mode hides the row (C++).
    preview = w.add("VerticalBox", "PreviewRow", stack, None, {"Padding": pad(0, 12, 0, 0)})
    w.button("PreviewButton", "PreviewLabel", "Preview this? Off", preview, min_w=260, min_h=TOGGLE_H, size=20,
             slot={"HorizontalAlignment": "HAlign_Left"})
    w.text("PreviewNoteText", "", preview, T_LABEL, color=BONE_MUTED, wrap=CAPTION_WRAP, slot={"Padding": pad(0, 6, 0, 0)})
    w.text("TargetMissingText", "This step is not available yet.", stack, 18, "Bold", color=NOTICE, wrap=CAPTION_WRAP,
           slot={"Padding": pad(0, 8, 0, 0)})
    w.collapse("TargetMissingText", "PreviewRow")


def build_bottom_rail(w):
    root = w.add("Overlay", "Root")
    rail = w.add("Overlay", "Rail", root, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Bottom",
                                                 "Padding": pad(32, 0, 32, CREDITS_CLEAR)})
    w.button("BackButton", "BackLabel", "Back", rail, min_w=170, size=24,
             slot={"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Center"})
    # The one progress indicator: "Step 2 of 23" over a slim segmented bar.
    pips = w.glass("PipsCard", "Pips", rail, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}, pad(24, 12))
    block = w.add("VerticalBox", "ProgressBlock", pips)
    w.text("ProgressText", "Step 1 of 16", block, T_BODY, "Bold", slot={"HorizontalAlignment": "HAlign_Center", "Padding": pad(0, 0, 0, 8)})
    w.add("HorizontalBox", "ProgressPips", block, None, {"HorizontalAlignment": "HAlign_Center"})
    w.button("NextButton", "NextLabel", "Next", rail, primary=True, min_w=170, size=24,
             slot={"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center"})
    # Opens the day timeline (closed by default; C++ shows it only once the journey has started).
    toggle = w.add("SizeBox", "TimelineToggleSize", rail, None, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center",
                                                                "Padding": pad(0, 0, 194, 0)})
    w.button("TimelineToggleButton", "TimelineToggleLabel", "Show day timeline", toggle, min_w=260, size=T_BODY)
    w.collapse("TimelineToggleButton")


def build_settings(w):
    root = w.add("Overlay", "Root")
    w.add("Border", "Scrim", root, {"BrushColor": SCRIM, "Visibility": "Collapsed"}, fill_slot())
    centre = {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}

    card = w.glass("SettingsPanel", "Settings", root, centre, pad(48, 36))
    stack = w.add("VerticalBox", "SettingsStack", card)
    header = w.add("HorizontalBox", "SettingsHeader", stack, None, gap(8))
    w.text("SettingsTitle", "Settings", header, T_HEAD, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    w.button("SettingsCloseButton", "SettingsCloseLabel", "Close", header, min_w=140, min_h=TOP_H, size=T_BODY,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.text("SettingsHint", "You can change these at any time. Calm Mode turns sound and voice off and uses less movement.", stack, T_LABEL,
           color=BONE_MUTED, wrap=760, slot=gap(24))
    columns = w.add("HorizontalBox", "SettingsColumns", stack)
    toggles = w.add("VerticalBox", "ToggleColumn", columns, None, {"Padding": pad(0, 0, 56, 0)})
    # Two groups with a plain heading each, so seven toggles don't read as one flat list.
    groups = (("Comfort", (("CalmModeButton", "CalmModeLabel", "Calm Mode: Off"),
                           ("ReducedMotionButton", "ReducedMotionLabel", "Less movement: Off"),
                           ("LargeTextButton", "LargeTextLabel", "Large text: Off"),
                           ("HighContrastButton", "HighContrastLabel", "High contrast: Off"))),
              ("Sound and scene", (("SoundButton", "SoundLabel", "Sound: Off"),
                                   ("VoiceButton", "VoiceLabel", "Voice: Off"),
                                   ("CrowdButton", "CrowdLabel", "Crowd: Low"))))
    for index, (heading, items) in enumerate(groups):
        w.text(f"Group{index}Title", heading, toggles, T_LABEL, "Bold", color=BONE_MUTED,
               slot={"Padding": pad(0, 0 if index == 0 else 16, 0, 8)})
        for name, label_name, label in items:
            w.button(name, label_name, label, toggles, min_w=360, min_h=TOGGLE_H, size=T_BODY, slot=gap(8))

    keys = w.add("VerticalBox", "KeysColumn", columns)
    w.text("KeysTitle", "Keyboard", keys, T_LABEL, "Bold", color=BONE_MUTED, slot=gap(8))
    table = w.add("HorizontalBox", "KeysTable", keys, None, gap(18))
    w.text("KeysKeys", "\n".join(k for k, _ in KEYS), table, T_LABEL, "Bold", slot={"Padding": pad(0, 0, 20, 0)})
    w.text("KeysActions", "\n".join(a for _, a in KEYS), table, T_LABEL, color=BONE_MUTED)
    w.button("RestartButton", "RestartLabel", "Start again", keys, min_w=220, min_h=TOGGLE_H, size=T_BODY)

    pause = w.glass("PausePanel", "Pause", root, centre, pad(56, 40))
    pause_stack = w.add("VerticalBox", "PauseStack", pause)
    w.text("PauseTitle", "Paused", pause_stack, T_HEAD, "Bold", slot={"HorizontalAlignment": "HAlign_Center", **gap(8)})
    w.text("PauseBody", "Nothing will move until you resume.", pause_stack, 22, wrap=560, slot={"HorizontalAlignment": "HAlign_Center", **gap(20)})
    w.button("ResumeButton", "ResumeLabel", "Resume", pause_stack, primary=True, min_w=220, slot={"HorizontalAlignment": "HAlign_Center"})

    confirm = w.glass("RestartConfirmPanel", "Confirm", root, centre, pad(56, 40))
    confirm_stack = w.add("VerticalBox", "RestartStack", confirm)
    w.text("RestartTitle", "Start again?", confirm_stack, T_HEAD, "Bold", slot={"HorizontalAlignment": "HAlign_Center", **gap(8)})
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
    w.text("TimelineNoteText", "Approximate times\nExample 1:00 PM game", row, T_LABEL, color=BONE_MUTED,
           slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(4, 0, 16, 0)})
    w.add("HorizontalBox", "TimelineMarks", row, None, {"VerticalAlignment": "VAlign_Center"})
    box = w.add("SizeBox", "LearnMorePanel", w.child("Rail"), None,
                {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 186, 0)})
    w.button("LearnMoreButton", "LearnMoreLabel", "Learn More", box, min_w=200, size=22)
    w.collapse("TimelinePanel", "LearnMorePanel")


def extend_settings(w):
    """Voice (off by default; needs Sound) and Crowd (Low or Off) under the other toggles, and the new keys."""
    existing = {x["widgetName"] for x in call(UMG, "GetWidgets", {"widgetBlueprint": w.bp})["widgets"] if x["widget"] != "None"}
    if "VoiceButton" not in existing:
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
    "WBP_LFF_BottomRail": {"PipCurrentColor": BONE, "PipDoneColor": BONE_MUTED, "PipUpcomingColor": srgb("#3A4A4C")},
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
