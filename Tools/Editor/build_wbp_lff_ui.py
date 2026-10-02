"""Builds the HUD widgets in /Game/LFF/UI through Unreal MCP (Docs/ManualSetup.md §5).

  WBP_LFF_Splash       welcome card, before Start
  WBP_LFF_TopChrome    prototype badge and, on steps, the one-line disclaimer; Step X of Y, Pause, Sound; Settings
  WBP_LFF_StepCaption  small lower-left caption card: step number, chip, title, body, "Preview this?" on intensity steps
  WBP_LFF_BottomRail   one dock: Back, step count and pips, day-timeline button, Next
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


# Design tokens (Docs/UI_DESIGN.md), "night field" theme: opaque deep midnight-green cards with warm off-white text,
# one kelly-green action per screen. Scene first: the stadium fills the screen and the UI keeps to the corners and one
# dock. Dark, opaque cards cut glare for light-sensitive visitors and keep contrast independent of what the camera sees.
# WCAG (Docs/UI_DESIGN.md has the table): INK on SURFACE 13.6:1, INK_MUTED 8.7:1, KELLY text 6.5:1, dark text on KELLY 6.5:1.
SURFACE_HEX, INK_HEX, INK_MUTED_HEX, KELLY_HEX = "#072A2F", "#F4F2EC", "#B8C7C8", "#69BE28"
SURFACE = srgb(SURFACE_HEX)
RAISED = srgb("#0F3B41")             # secondary buttons and Off toggles
RAISED_HOVER = srgb("#16474E")
RAISED_PRESSED = srgb("#0A3237")
INK = srgb(INK_HEX)                  # primary text
INK_MUTED = srgb(INK_MUTED_HEX)      # secondary text
EDGE = srgb(INK_HEX, 0.12)           # hairline around cards
OUTLINE = srgb(INK_HEX, 0.55)        # secondary button outline (5.5:1 against the card)
KELLY = srgb(KELLY_HEX)              # the one primary action, eyebrows, the current step, the spoken word
KELLY_HOVER = srgb("#7BCB3C")
KELLY_PRESSED = srgb("#5AA622")
ON_KELLY = SURFACE                   # text on kelly
PIP_UPCOMING = srgb("#557A7E")       # 3.2:1 against the card
DISABLED = srgb("#12353A")
SCRIM = {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.55}
NOTICE = srgb("#F2B65A")             # amber, 8.4:1
WHITE = {"r": 1, "g": 1, "b": 1, "a": 1}

UI_FONT = "/Game/LFF/UI/Fonts/F_LFF_Atkinson.F_LFF_Atkinson"  # Atkinson Hyperlegible (SIL OFL), SourceData/UI/Fonts
RADIUS = 28
BLUR = 0  # opaque panels: no background blur
# Type scale at 1080p: label 18, control 22, body 24, title 38, headline 52. Space on an 8 px grid.
T_LABEL, T_CONTROL, T_BODY, T_TITLE, T_HEAD = 18, 22, 24, 38, 52
BODY_LINE = 1.0                      # Atkinson's own line gap already gives about 1.5x the font size (WCAG 1.4.12)
EYEBROW_SPACING = 60                 # letter spacing for the short eyebrow lines, in 1/1000 em
# Hit targets (1080p) are the project minimum of 64 px everywhere except Start (76).
TOP_H, RAIL_H, START_H, TOGGLE_H = 64, 64, 76, 64
DOCK_PAD = 12
DOCK_H = RAIL_H + DOCK_PAD * 2
# The bottom 72 px stay clear for the Cesium and Google credits. Dock above them, timeline drawer above the dock,
# the caption card above the drawer (LFFJourneyPanel.cpp uses the same 296 px for the card).
CREDITS_CLEAR = 72
TIMELINE_BOTTOM = CREDITS_CLEAR + DOCK_H + 16
CAPTION_BOTTOM = 296
# Caption text width. With padding the card is about 640 px wide at 1080p.
CAPTION_WRAP = 560

KEYS = [
    ("Right arrow or Space", "Next"),
    ("Left arrow", "Back"),
    ("Tab, Shift+Tab", "Move between buttons"),
    ("Enter", "Press the ringed button, or Yes"),
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
    ("Esc", "Close"),
]

# Names C++ reads, per widget. They are marked as variables and checked after the build.
CONTRACT = {
    "WBP_LFF_Splash": {"TitleText", "BodyText", "StartButton", "DisclaimerText", "TextScale", "StandardButton", "CalmButton",
                       "StandardLabel", "CalmLabel"},
    "WBP_LFF_TopChrome": {"DisclaimerLineText", "SettingsButton", "DisclaimerPill"},
    "WBP_LFF_StepCaption": {"StepNumberText", "TitleText", "BodyText", "DisclaimerChip", "DisclaimerChipText",
                            "TargetMissingText", "TextScale", "PreviewRow", "PreviewButton", "PreviewLabel",
                            "PreviewNoteText", "TicketPhone", "TicketImage",
                            "FinaleActions", "FinaleStartOverButton", "FinaleLearnMoreButton"},
    "WBP_LFF_BottomRail": {"BackButton", "NextButton", "NextLabel", "TimelinePanel", "TimelineMarks",
                           "TimelineNoteText", "LearnMorePanel", "LearnMoreButton", "ProgressText", "TimelineToggleButton",
                           "TimelineToggleLabel", "ProgressPips"},
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

    def text(self, name, value, parent, size, face="Regular", color=INK, wrap=None, justify=None, slot=None, spacing=0,
             line=None):
        props = {"Text": value, "Font": font(size, face, spacing),
                 "ColorAndOpacity": {"specifiedColor": color, "colorUseRule": "UseColor_Specified"}}
        if wrap:
            props.update({"AutoWrapText": False, "WrapTextAt": wrap})
        if justify:
            props["Justification"] = justify
        if line:
            props["LineHeightPercentage"] = line
        return self.add("TextBlock", name, parent, props, slot)

    def eyebrow(self, name, value, parent, slot=None, color=KELLY):
        """The short kelly line above a title: bold, slightly spaced. Sentence case (all caps reads slower)."""
        return self.text(name, value, parent, T_LABEL, "Bold", color=color, slot=slot, spacing=EYEBROW_SPACING)

    def glass(self, name, surface, parent, slot, padding, radius=None):
        """BackgroundBlur (named `name`, the part C++ shows or hides) > Border `Surface<surface>` (the opaque card)."""
        r = radius or RADIUS
        blur = self.add("BackgroundBlur", name, parent, {"BlurStrength": BLUR, "bApplyAlphaToBlur": True, "Padding": pad(0),
                                                         "CornerRadius": {"x": r, "y": r, "z": r, "w": r}}, slot)
        return self.add("Border", f"Surface{surface}", blur, {"Background": rounded(WHITE, EDGE, 1, radius=r), "BrushColor": SURFACE,
                                                              "Padding": padding},
                        {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})

    def button(self, name, label_name, label, parent, primary=False, min_w=180, min_h=RAIL_H, size=T_CONTROL, slot=None,
               style=None):
        """SizeBox (minimum size) > Button (not focusable; the HUD runs its own focus ring) > label."""
        box = self.add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": min_w,
                                                          "bOverride_MinDesiredHeight": True, "MinDesiredHeight": min_h}, slot)
        b = self.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": style or (PRIMARY_STYLE if primary else SECONDARY_STYLE),
                                           "BackgroundColor": WHITE},
                     {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
        self.text(label_name, label, b, size, "Bold", color=ON_KELLY if primary else INK,
                  slot={"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
        return b

    def toggle_row(self, name, label_name, label, parent, slot=None, min_w=440):
        """A Settings row: the name on the left, its On / Off word on the right (C++ fills both and the row's fill)."""
        box = self.add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": min_w,
                                                          "bOverride_MinDesiredHeight": True, "MinDesiredHeight": TOGGLE_H}, slot)
        b = self.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": ROW_STYLE, "BackgroundColor": WHITE},
                     {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
        row = self.add("HorizontalBox", f"{name}Content", b, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Center"})
        self.text(label_name, label, row, T_CONTROL, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
        self.text(label_name.replace("Label", "State"), "Off", row, T_CONTROL, slot={"VerticalAlignment": "VAlign_Center",
                                                                                    "Padding": pad(24, 0, 0, 0)})
        return b

    def child(self, name):
        """An existing widget in this blueprint's tree (a parent for --extend)."""
        return {"refPath": f"{self.bp['refPath']}:WidgetTree.{name}"}

    def collapse(self, *names):
        for name in names:
            set_props({"refPath": f"{self.bp['refPath']}:WidgetTree.{name}"}, {"Visibility": "Collapsed"})


def font(size, face, spacing=0):
    return {"fontObject": {"refPath": UI_FONT}, "typefaceFontName": face, "size": size, "letterSpacing": spacing}


def pad(l, t=None, r=None, b=None):
    t = l if t is None else t
    return {"left": l, "top": t, "right": l if r is None else r, "bottom": t if b is None else b}


def rounded(fill, outline, width, half_height=False, radius=None):
    r = radius or RADIUS
    return {"tintColor": {"specifiedColor": fill, "colorUseRule": "UseColor_Specified"}, "drawAs": "RoundedBox",
            "outlineSettings": {"cornerRadii": {"x": r, "y": r, "z": r, "w": r}, "width": width,
                                "roundingType": "HalfHeightRadius" if half_height else "FixedRadius",
                                "color": {"specifiedColor": outline, "colorUseRule": "UseColor_Specified"}}}


def button_style(fill, hover_fill, pressed_fill, outline, hover_outline, half_height=True, radius=None, padding=(24, 6),
                 disabled_fill=None):
    # Hover and press only brighten the outline or shift the fill a step, so contrast never drops.
    return {"normal": rounded(fill, outline, 2, half_height, radius), "hovered": rounded(hover_fill, hover_outline, 3, half_height, radius),
            "pressed": rounded(pressed_fill, hover_outline, 3, half_height, radius),
            "disabled": rounded(disabled_fill or DISABLED, srgb(INK_HEX, 0.35), 2, half_height, radius),  # still outlined, so it keeps its place
            "normalPadding": pad(*padding), "pressedPadding": pad(padding[0], padding[1] + 1, padding[0], padding[1] - 1)}


# A disabled primary button keeps its dark label, so its fill goes grey-light rather than dark (or the word would vanish).
PRIMARY_STYLE = button_style(KELLY, KELLY_HOVER, KELLY_PRESSED, KELLY, INK, disabled_fill=srgb("#8FA3A2"))
SECONDARY_STYLE = button_style(RAISED, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK)
# Settings rows and the Standard / Calm cards: rounded rectangles, content left-aligned.
ROW_STYLE = button_style(RAISED, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK, half_height=False, radius=16, padding=(24, 8))
MODE_STYLE = button_style(RAISED, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK, half_height=False, radius=20, padding=(24, 16))


def gap(px):
    return {"Padding": pad(0, 0, 0, px)}


def fill_slot():
    return {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"}


EAF_LOGO = "/Game/LFF/UI/Icons/T_LFF_Logo_EAF_White.T_LFF_Logo_EAF_White"  # Assets/Logos/EAF-logo_white.png, cropped: 1024 x 357
GEAR_ICON = "/Game/LFF/UI/Icons/T_LFF_Icon_Settings.T_LFF_Icon_Settings"  # SourceData/UI, white on transparent, 128 px


def mode_card(w, name, label_name, title, note, parent, slot):
    """One of the two Standard / Calm cards: name and a two-line description, together on one button."""
    box = w.add("SizeBox", f"{name}Size", parent, {"bOverride_WidthOverride": True, "WidthOverride": 360,
                                                   "bOverride_MinDesiredHeight": True, "MinDesiredHeight": 120}, slot)
    b = w.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": MODE_STYLE, "BackgroundColor": WHITE}, fill_slot())
    stack = w.add("VerticalBox", f"{name}Content", b, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Center"})
    w.text(label_name, title, stack, 26, "Bold", slot=gap(6))
    w.text(label_name.replace("Label", "Note"), note, stack, T_LABEL, wrap=300, line=BODY_LINE)


def build_splash(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center", "Padding": pad(0, 40, 0, 0)})
    card = w.glass("SplashCard", "SplashCard", scale, None, pad(64, 56), radius=32)
    # Left-aligned throughout: one straight left edge is easier to read than centred lines.
    stack = w.add("VerticalBox", "SplashStack", card)
    w.eyebrow("WelcomeText", "Your game day guide", stack, gap(12))
    w.text("TitleText", "Lincoln Financial Field", stack, T_HEAD, "Bold", slot=gap(16))
    w.text("BodyText", "See each part of the day before you go.\nYou choose when to move on.", stack, T_BODY, color=INK_MUTED,
           wrap=740, line=BODY_LINE, slot=gap(40))
    w.text("ModePromptText", "How would you like your day?", stack, T_CONTROL, "Bold", slot=gap(16))
    modes = w.add("HorizontalBox", "ModeSegment", stack, None, gap(40))
    mode_card(w, "StandardButton", "StandardLabel", "Standard", "Voice, music, crowd and the big moments.", modes,
              {"Padding": pad(0, 0, 16, 0)})
    mode_card(w, "CalmButton", "CalmLabel", "Calm", "Quiet and still. No sound, no spectacle.", modes, None)
    w.button("StartButton", "StartLabel", "Start your day", stack, primary=True, min_w=360, min_h=START_H, size=28,
             slot={"HorizontalAlignment": "HAlign_Left", **gap(32)})
    w.text("DisclaimerText", "Illustration only. This is not a navigation instruction for the real stadium.", stack, T_LABEL,
           color=INK_MUTED, wrap=740, line=BODY_LINE)


def build_top_chrome(w):
    root = w.add("Overlay", "Root")
    # Two small floating pieces and nothing between them, so the stadium has the middle of the screen. Left, the logo chip
    # and, on journey steps, the disclaimer pill. Right, the Settings pill.
    left = w.add("HorizontalBox", "TopLeft", root, None, {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Top",
                                                          "Padding": pad(32, 28, 0, 0)})
    chip = w.add("Border", "LogoChip", left, {"Background": rounded(WHITE, EDGE, 1, True), "BrushColor": SURFACE, "Padding": pad(26, 14)},
                 {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 12, 0)})
    w.add("Image", "EAFLogo", chip, {"Brush": {"resourceObject": {"refPath": EAF_LOGO}, "imageSize": {"x": 152, "y": 53}},
                                     "ToolTipText": "Eagles Autism Foundation"}, {"VerticalAlignment": "VAlign_Center"})
    # The one persistent disclaimer line on journey steps (C++ shows pill and text after Start; the splash has the full sentence).
    pill = w.add("Border", "DisclaimerPill", left, {"Background": rounded(WHITE, EDGE, 1, True), "BrushColor": SURFACE,
                                                     "Padding": pad(24, 14)}, {"VerticalAlignment": "VAlign_Center"})
    w.add("TextBlock", "DisclaimerLineText", pill, {"Text": "Illustration. Not directions for the real stadium.", "Font": font(T_LABEL, "Regular"),
          "ColorAndOpacity": {"specifiedColor": INK_MUTED, "colorUseRule": "UseColor_Specified"}}, {"VerticalAlignment": "VAlign_Center"})
    w.collapse("DisclaimerPill")

    # Settings: icon plus the word, never the icon alone.
    box = w.add("SizeBox", "SettingsButtonSize", root, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": 180,
                                                        "bOverride_MinDesiredHeight": True, "MinDesiredHeight": TOP_H},
                {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Top", "Padding": pad(0, 28, 32, 0)})
    gear = w.add("Button", "SettingsButton", box, {"IsFocusable": False, "WidgetStyle": SECONDARY_STYLE_ON_SCENE, "BackgroundColor": WHITE,
                                                   "ToolTipText": "Settings (S)"},
                 {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    inner = w.add("HorizontalBox", "SettingsContent", gear, None, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.add("Image", "SettingsIcon", inner, {"Brush": {"resourceObject": {"refPath": GEAR_ICON}, "imageSize": {"x": 26, "y": 26}},
                                           "ColorAndOpacity": INK}, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 12, 0)})
    w.text("SettingsLabel", "Settings", inner, T_CONTROL, "Bold", slot={"VerticalAlignment": "VAlign_Center"})


# On the open scene (no card behind it) a button uses the card colour, so it stands out from a bright sky.
SECONDARY_STYLE_ON_SCENE = button_style(SURFACE, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK)


def build_step_caption(w):
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(48, 0, 0, CAPTION_BOTTOM)})
    card = w.glass("Caption", "Caption", scale, None, pad(40, 32), radius=RADIUS)
    stack = w.add("VerticalBox", "CaptionStack", card)
    # Header: when (kelly eyebrow), how sure (chip), then the voice button that C++ adds at the end.
    header = w.add("HorizontalBox", "CaptionHeader", stack, None, gap(18))
    w.eyebrow("StepNumberText", "Step 1", header, {"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    chip = w.add("Border", "DisclaimerChip", header, {"Background": rounded(WHITE, OUTLINE, 1.5, True), "BrushColor": SURFACE,
                                                       "Padding": pad(14, 5)},
                 {"VerticalAlignment": "VAlign_Center", "HorizontalAlignment": "HAlign_Right", "Padding": pad(16, 0, 0, 0)})
    w.text("DisclaimerChipText", "Illustration", chip, T_LABEL, "Bold", color=INK_MUTED)
    w.text("TitleText", "Title", stack, T_TITLE, "Bold", wrap=CAPTION_WRAP, line=0.92, slot=gap(14))
    w.text("BodyText", "Body", stack, T_BODY, wrap=CAPTION_WRAP, line=BODY_LINE)
    # Loud or bright steps only. Nothing plays until the visitor picks Soft; Calm Mode hides the row (C++).
    preview = w.add("VerticalBox", "PreviewRow", stack, None, {"Padding": pad(0, 20, 0, 0)})
    w.button("PreviewButton", "PreviewLabel", "Preview this? Off", preview, min_w=280, min_h=TOGGLE_H, size=T_CONTROL - 2,
             slot={"HorizontalAlignment": "HAlign_Left"})
    w.text("PreviewNoteText", "", preview, T_LABEL, color=INK_MUTED, wrap=CAPTION_WRAP, line=BODY_LINE, slot={"Padding": pad(0, 10, 0, 0)})
    w.text("TargetMissingText", "This step is not available yet.", stack, T_LABEL, "Bold", color=NOTICE, wrap=CAPTION_WRAP,
           slot={"Padding": pad(0, 10, 0, 0)})
    w.collapse("TargetMissingText", "PreviewRow")


def build_bottom_rail(w):
    root = w.add("Overlay", "Root")
    rail = w.add("Overlay", "Rail", root, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Bottom",
                                                 "Padding": pad(32, 0, 32, CREDITS_CLEAR)})
    # One dock, centred, the same on every step: Back, where you are (words and one pip per step), the timeline, Next.
    dock = w.glass("PipsCard", "Pips", rail, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom"},
                   pad(DOCK_PAD), radius=DOCK_H // 2)
    row = w.add("HorizontalBox", "DockRow", dock)
    mid = {"VerticalAlignment": "VAlign_Center"}
    w.button("BackButton", "BackLabel", "Back", row, min_w=150, size=T_BODY, slot={**mid, "Padding": pad(0, 0, 28, 0)})
    block = w.add("VerticalBox", "ProgressBlock", row, None, {**mid, "Padding": pad(0, 0, 28, 0)})
    w.text("ProgressText", "Step 1 of 23", block, T_CONTROL, "Bold", slot={"HorizontalAlignment": "HAlign_Center", "Padding": pad(0, 0, 0, 10)})
    w.add("HorizontalBox", "ProgressPips", block, None, {"HorizontalAlignment": "HAlign_Center"})
    # Opens the day timeline (closed by default; C++ shows it only once the journey has started).
    w.button("TimelineToggleButton", "TimelineToggleLabel", "Show day timeline", row, min_w=240, size=T_LABEL + 2,
             slot={**mid, "Padding": pad(0, 0, 12, 0)})
    w.button("NextButton", "NextLabel", "Next", row, primary=True, min_w=170, size=T_BODY, slot=mid)
    w.collapse("TimelineToggleButton")


def build_settings(w):
    root = w.add("Overlay", "Root")
    w.add("Border", "Scrim", root, {"BrushColor": SCRIM, "Visibility": "Collapsed"}, fill_slot())
    centre = {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}

    card = w.glass("SettingsPanel", "Settings", root, centre, pad(56, 40), radius=32)
    stack = w.add("VerticalBox", "SettingsStack", card)
    header = w.add("HorizontalBox", "SettingsHeader", stack, None, gap(8))
    w.text("SettingsTitle", "Settings", header, 44, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    w.button("SettingsCloseButton", "SettingsCloseLabel", "Close", header, min_w=140, min_h=TOP_H, size=T_CONTROL,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.text("SettingsHint", "You can change these at any time. Calm Mode turns sound and voice off and uses less movement.", stack, T_LABEL,
           color=INK_MUTED, wrap=820, line=BODY_LINE, slot=gap(24))
    columns = w.add("HorizontalBox", "SettingsColumns", stack)
    toggles = w.add("VerticalBox", "ToggleColumn", columns, None, {"Padding": pad(0, 0, 56, 0)})
    # Two groups with a plain heading each, so seven toggles don't read as one flat list.
    groups = (("Comfort", (("CalmModeButton", "CalmModeLabel", "Calm Mode"),
                           ("ReducedMotionButton", "ReducedMotionLabel", "Less movement"),
                           ("LargeTextButton", "LargeTextLabel", "Large text"),
                           ("HighContrastButton", "HighContrastLabel", "High contrast"))),
              ("Sound and scene", (("SoundButton", "SoundLabel", "Sound"),
                                   ("VoiceButton", "VoiceLabel", "Voice"),
                                   ("CrowdButton", "CrowdLabel", "Crowd"))))
    for index, (heading, items) in enumerate(groups):
        w.eyebrow(f"Group{index}Title", heading, toggles, {"Padding": pad(0, 0 if index == 0 else 20, 0, 10)})
        for name, label_name, label in items:
            w.toggle_row(name, label_name, label, toggles, slot=gap(8))

    keys = w.add("VerticalBox", "KeysColumn", columns)
    w.eyebrow("KeysTitle", "Keyboard", keys, {"Padding": pad(0, 0, 0, 10)})
    table = w.add("HorizontalBox", "KeysTable", keys, None, gap(24))
    w.text("KeysKeys", "\n".join(k for k, _ in KEYS), table, T_LABEL, "Bold", line=0.95, slot={"Padding": pad(0, 0, 24, 0)})
    w.text("KeysActions", "\n".join(a for _, a in KEYS), table, T_LABEL, color=INK_MUTED, line=0.95)
    w.button("RestartButton", "RestartLabel", "Start again", keys, min_w=220, min_h=TOGGLE_H, size=T_CONTROL,
             slot={"HorizontalAlignment": "HAlign_Left"})

    pause = w.glass("PausePanel", "Pause", root, centre, pad(64, 52), radius=32)
    pause_stack = w.add("VerticalBox", "PauseStack", pause)
    w.eyebrow("PauseEyebrow", "Take your time", pause_stack, gap(10))
    w.text("PauseTitle", "Paused", pause_stack, T_HEAD, "Bold", slot=gap(10))
    w.text("PauseBody", "Nothing will move until you resume.", pause_stack, T_BODY, color=INK_MUTED, wrap=560, slot=gap(28))
    w.button("ResumeButton", "ResumeLabel", "Resume", pause_stack, primary=True, min_w=220, size=T_BODY,
             slot={"HorizontalAlignment": "HAlign_Left"})

    confirm = w.glass("RestartConfirmPanel", "Confirm", root, centre, pad(64, 52), radius=32)
    confirm_stack = w.add("VerticalBox", "RestartStack", confirm)
    w.text("RestartTitle", "Start again?", confirm_stack, T_HEAD, "Bold", slot=gap(10))
    w.text("RestartBody", "You will go back to the beginning.", confirm_stack, T_BODY, color=INK_MUTED, wrap=560, slot=gap(28))
    choices = w.add("HorizontalBox", "RestartChoices", confirm_stack, None, {"HorizontalAlignment": "HAlign_Left"})
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
    """The day timeline drawer above the dock (C++ adds one mark per timed step) and Learn More on the last step."""
    root = w.child("Root")
    card = w.glass("TimelinePanel", "Timeline", root, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom",
                                                       "Padding": pad(0, 0, 0, TIMELINE_BOTTOM)}, pad(20, 10), radius=40)
    row = w.add("HorizontalBox", "TimelineRow", card)
    w.text("TimelineNoteText", "Approximate times\nExample 1:00 PM game", row, T_LABEL, color=INK_MUTED,
           slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(8, 0, 20, 0)})
    w.add("HorizontalBox", "TimelineMarks", row, None, {"VerticalAlignment": "VAlign_Center"})
    box = w.add("SizeBox", "LearnMorePanel", w.child("Rail"), None,
                {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 186, 0)})
    w.button("LearnMoreButton", "LearnMoreLabel", "Learn More", box, min_w=200, size=T_CONTROL)
    w.collapse("TimelinePanel", "LearnMorePanel")


def extend_settings(w):
    """Voice (off by default; needs Sound) and Crowd (Low or Off) under the other toggles, and the new keys."""
    existing = {x["widgetName"] for x in call(UMG, "GetWidgets", {"widgetBlueprint": w.bp})["widgets"] if x["widget"] != "None"}
    if "VoiceButton" not in existing:
        toggles = w.child("ToggleColumn")
        for name, label_name, label in (("VoiceButton", "VoiceLabel", "Voice"), ("CrowdButton", "CrowdLabel", "Crowd")):
            w.toggle_row(name, label_name, label, toggles, slot=gap(8))
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
                {"HorizontalAlignment": "HAlign_Left", "Padding": pad(0, 20, 0, 0)})
    w.button("FinaleStartOverButton", "FinaleStartOverLabel", "Start Over", row, min_w=200, size=T_CONTROL, slot={"Padding": pad(0, 0, 12, 0)})
    w.button("FinaleLearnMoreButton", "FinaleLearnMoreLabel", "Learn More", row, primary=True, min_w=200, size=T_CONTROL)
    w.collapse("FinaleActions")


# Slice A (session 6): widget -> the function that adds its pieces, on a fresh build or --rebuild.
SLICE_A = {"WBP_LFF_StepCaption": add_finale_actions}


def build_hud(w):
    root = w.add("Overlay", "RootOverlay")
    # Later children draw on top: the modal layer covers everything else. C++ adds the focus ring last of all.
    for instance, asset in (("TopChrome", "WBP_LFF_TopChrome"), ("Splash", "WBP_LFF_Splash"), ("StepCaption", "WBP_LFF_StepCaption"),
                            ("BottomRail", "WBP_LFF_BottomRail"), ("Settings", "WBP_LFF_Settings")):
        w.add(f"{FOLDER}/{asset}.{asset}_C", instance, root, None, fill_slot())


# ULFFJourneyPanel style properties set on the generated class defaults. Done pips are muted ink, upcoming are a dim teal
# (3.2:1), and the current pip is a longer kelly pill, so position never depends on colour alone.
DEFAULTS = {
    "WBP_LFF_BottomRail": {"PipCurrentColor": KELLY, "PipDoneColor": INK_MUTED, "PipUpcomingColor": PIP_UPCOMING},
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
