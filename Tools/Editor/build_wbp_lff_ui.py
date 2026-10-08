"""Builds the HUD widgets in /Game/LFF/UI through Unreal MCP (Docs/ManualSetup.md §5).

  WBP_LFF_Splash       title screen: headline, Standard / Calm, Start, on a soft field at the left (Docs/UI_DESIGN.md)
  WBP_LFF_TopChrome    header: wordmark left, Voice and Settings right
  WBP_LFF_StepCaption  the step as a caption on the scene, lower left: chapter eyebrow, title, body, Preview row
  WBP_LFF_BottomRail   the chapter bar (Back, one chip per chapter, Next) and the key hints
  WBP_LFF_Settings     Settings as a right-hand sheet; Paused and Start again? in the middle of a dimmed scene
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
# Soft backgrounds (Tools/Pipeline/prepare_ui_fog.py, Tools/Editor/import_ui_fog.py).
FOG = "/Game/LFF/UI/Icons/T_LFF_Fog.T_LFF_Fog"
FADE_TOP = "/Game/LFF/UI/Icons/T_LFF_FadeTop.T_LFF_FadeTop"
FADE_BOTTOM = "/Game/LFF/UI/Icons/T_LFF_FadeBottom.T_LFF_FadeBottom"
FADE_SIDE = "/Game/LFF/UI/Icons/T_LFF_FadeSide.T_LFF_FadeSide"
# Drawn at 352 so the feathered edge is 132 px wide at 1080p: wide enough to read as light, not as a box.
FOG_BRUSH = {"resourceObject": {"refPath": FOG}, "drawAs": "Box", "imageSize": {"x": 352, "y": 352},
             "margin": {"left": 0.375, "top": 0.375, "right": 0.375, "bottom": 0.375},
             "tintColor": {"specifiedColor": {"r": 1, "g": 1, "b": 1, "a": 1}, "colorUseRule": "UseColor_Specified"}}
RADIUS = 28
BLUR = 0  # opaque panels: no background blur
# Type scale at 1080p: label 18, control 22, body 24, title 38, headline 52. Space on an 8 px grid.
T_LABEL, T_CONTROL, T_BODY, T_TITLE, T_HEAD = 18, 22, 26, 44, 64
BODY_LINE = 1.0                      # Atkinson's own line gap already gives about 1.5x the font size (WCAG 1.4.12)
EYEBROW_SPACING = 60                 # letter spacing for the short eyebrow lines, in 1/1000 em
# Hit targets (1080p) are the project minimum of 64 px everywhere except Start (76).
TOP_H, RAIL_H, START_H, TOGGLE_H = 64, 64, 76, 64
EDGE_PAD = 40                        # distance from the screen edge to the corner pieces
# The bottom 72 px stay clear for the Cesium and Google credits (bottom left); the key hints sit in that band on the right.
# The chapter bar sits just above the band, and the caption above the bar (LFFJourneyPanel::CaptionRestPadding is set
# from CAPTION_BOTTOM in DEFAULTS).
CREDITS_CLEAR = 72
BAR_BOTTOM = CREDITS_CLEAR + 12
CAPTION_BOTTOM = 124                 # the field's soft lower edge runs behind the chapter bar
# Caption text width; the fog around it adds FOG_PAD on each side.
CAPTION_WRAP = 600
FOG_PAD = 120                        # text starts where the field is at full strength (the feather is 121 px)
# Text that sits on the fog gets a soft shadow, so its edges stay crisp over a busy scene.
SHADOW = {"ShadowOffset": {"x": 0, "y": 1}, "ShadowColorAndOpacity": {"r": 0, "g": 0, "b": 0, "a": 0.55}}

# Names C++ reads, per widget. They are marked as variables and checked after the build.
CONTRACT = {
    "WBP_LFF_Splash": {"TitleText", "BodyText", "StartButton", "TextScale", "StandardButton", "CalmButton",
                       "StandardLabel", "CalmLabel"},
    "WBP_LFF_TopChrome": {"SettingsButton", "SettingsLabel", "VoiceSlot"},
    "WBP_LFF_StepCaption": {"StepNumberText", "TitleText", "BodyText", "TargetMissingText", "TextScale", "PreviewRow", "PreviewButton", "PreviewLabel",
                            "PreviewNoteText", "TicketPhone", "TicketImage",
                            "FinaleActions", "FinaleStartOverButton", "FinaleLearnMoreButton"},
    "WBP_LFF_BottomRail": {"BackButton", "NextButton", "NextLabel", "ChapterMarks"},
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
             line=None, shadow=False):
        props = {"Text": value, "Font": font(size, face, spacing),
                 "ColorAndOpacity": {"specifiedColor": color, "colorUseRule": "UseColor_Specified"}}
        if shadow:
            props.update(SHADOW)
        if wrap:
            props.update({"AutoWrapText": False, "WrapTextAt": wrap})
        if justify:
            props["Justification"] = justify
        if line:
            props["LineHeightPercentage"] = line
        return self.add("TextBlock", name, parent, props, slot)

    def eyebrow(self, name, value, parent, slot=None, color=KELLY, size=T_LABEL):
        """The short kelly line above a title: bold, slightly spaced. Sentence case (all caps reads slower)."""
        return self.text(name, value, parent, size, "Bold", color=color, slot=slot, spacing=EYEBROW_SPACING, shadow=True)

    def fog(self, name, parent, slot, padding, alpha=0.9):
        """Border `Surface<name>`: a soft dark field with no hard edge (T_LFF_Fog as a 9-slice), for text on the scene.
        The middle is solid enough for 7:1 text whatever is behind it; High contrast makes it opaque black."""
        return self.add("Border", f"Surface{name}", parent, {"Background": FOG_BRUSH, "BrushColor": {**SURFACE, "a": alpha},
                                                             "Padding": padding}, slot)

    def fade(self, name, texture, parent, slot, alpha=0.75, height=200):
        """A full-width gradient behind a screen edge (header, bottom bar). Never takes clicks."""
        return self.add("Image", name, parent, {"Brush": {"resourceObject": {"refPath": texture}, "drawAs": "Image",
                                                          "imageSize": {"x": 16, "y": height}},
                                                "ColorAndOpacity": {**SURFACE, "a": alpha}, "Visibility": "HitTestInvisible"}, slot)

    def keycap(self, name, key, parent, slot=None, label=None):
        """A key drawn as a small keycap, with what it does next to it: [S] Settings."""
        row = self.add("HorizontalBox", f"{name}Hint", parent, None, slot)
        cap = self.add("Border", f"{name}Cap", row, {"Background": rounded(WHITE, OUTLINE, 1.5, radius=8), "BrushColor": RAISED,
                                                     "Padding": pad(10, 2)}, {"VerticalAlignment": "VAlign_Center"})
        self.text(f"{name}Key", key, cap, T_LABEL, "Bold", slot={"HorizontalAlignment": "HAlign_Center"})
        if label:
            self.text(f"{name}Action", label, row, T_LABEL, color=INK_MUTED, shadow=True,
                      slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(10, 0, 0, 0)})
        return row

    def glass(self, name, surface, parent, slot, padding, radius=None):
        """BackgroundBlur (named `name`, the part C++ shows or hides) > Border `Surface<surface>` (the opaque card)."""
        r = radius or RADIUS
        blur = self.add("BackgroundBlur", name, parent, {"BlurStrength": BLUR, "bApplyAlphaToBlur": True, "Padding": pad(0),
                                                         "CornerRadius": {"x": r, "y": r, "z": r, "w": r}}, slot)
        return self.add("Border", f"Surface{surface}", blur, {"Background": rounded(WHITE, EDGE, 1, radius=r), "BrushColor": SURFACE,
                                                              "Padding": padding},
                        {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})

    def button(self, name, label_name, label, parent, primary=False, min_w=180, min_h=RAIL_H, size=T_CONTROL, slot=None,
               style=None, wrap=False):
        """SizeBox (minimum size) > Button (not focusable; the HUD runs its own focus ring) > label."""
        box = self.add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": min_w,
                                                          "bOverride_MinDesiredHeight": True, "MinDesiredHeight": min_h}, slot)
        b = self.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": style or (PRIMARY_STYLE if primary else SECONDARY_STYLE),
                                           "BackgroundColor": WHITE},
                     {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
        text = self.text(label_name, label, b, size, "Bold", color=ON_KELLY if primary else INK,
                         justify="Center" if wrap else None,
                         slot={"HorizontalAlignment": "HAlign_Fill" if wrap else "HAlign_Center",
                               "VerticalAlignment": "VAlign_Center"})
        if wrap:
            set_props(text, {"AutoWrapText": True})
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
    """One of the two Standard / Calm choices: name and a one-line description, together on one button."""
    box = w.add("SizeBox", f"{name}Size", parent, {"bOverride_WidthOverride": True, "WidthOverride": 330,
                                                   "bOverride_MinDesiredHeight": True, "MinDesiredHeight": 112}, slot)
    b = w.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": MODE_STYLE, "BackgroundColor": WHITE}, fill_slot())
    stack = w.add("VerticalBox", f"{name}Content", b, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Center"})
    w.text(label_name, title, stack, 26, "Bold", slot=gap(6))
    w.text(label_name.replace("Label", "Note"), note, stack, T_LABEL, wrap=280, line=BODY_LINE)


def build_splash(w):
    """The opening screen as a title sequence: a big headline on the left over a soft field, the stadium on the right."""
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Center", "Padding": pad(-40, 80, 0, 0)})
    field = w.fog("Splash", scale, None, pad(FOG_PAD + 16, FOG_PAD, FOG_PAD, FOG_PAD), alpha=0.92)
    stack = w.add("VerticalBox", "SplashStack", field)
    w.eyebrow("WelcomeText", "Your game day guide", stack, gap(16), size=20)
    w.text("TitleText", "Lincoln Financial Field", stack, T_HEAD, "Bold", wrap=760, line=0.92, shadow=True, slot=gap(20))
    w.text("BodyText", "See each part of the day before you go.\nYou choose when to move on.", stack, T_BODY, color=INK_MUTED,
           wrap=700, line=BODY_LINE, shadow=True, slot=gap(44))
    w.text("ModePromptText", "How would you like your day?", stack, T_CONTROL, "Bold", shadow=True, slot=gap(14))
    modes = w.add("HorizontalBox", "ModeSegment", stack, None, gap(36))
    mode_card(w, "StandardButton", "StandardLabel", "Standard", "Voice, music, crowd and the big moments.", modes,
              {"Padding": pad(0, 0, 16, 0)})
    mode_card(w, "CalmButton", "CalmLabel", "Calm", "Quiet and still. No sound, no spectacle.", modes, None)
    start = w.add("HorizontalBox", "StartRow", stack, None, gap(28))
    w.button("StartButton", "StartLabel", "Start your day", start, primary=True, min_w=340, min_h=START_H, size=28,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.keycap("StartKey", "Enter", start, {"VerticalAlignment": "VAlign_Center", "Padding": pad(24, 0, 0, 0)}, "to start")



def build_top_chrome(w):
    """The header, like a film's title strip: wordmark left, labelled controls right, the middle left to the stadium."""
    root = w.add("Overlay", "Root")
    w.fade("TopFade", FADE_TOP, root, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Top"}, height=190)

    # Wordmark: the EAF logo, a hairline, then what this is and where.
    left = w.add("HorizontalBox", "Wordmark", root, None, {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Top",
                                                           "Padding": pad(EDGE_PAD, 30, 0, 0)})
    w.add("Image", "EAFLogo", left, {"Brush": {"resourceObject": {"refPath": EAF_LOGO}, "imageSize": {"x": 150, "y": 52}},
                                     "ToolTipText": "Eagles Autism Foundation"}, {"VerticalAlignment": "VAlign_Center"})
    rule = w.add("SizeBox", "WordmarkRuleSize", left, {"bOverride_WidthOverride": True, "WidthOverride": 2,
                                                      "bOverride_HeightOverride": True, "HeightOverride": 44},
                 {"VerticalAlignment": "VAlign_Center", "Padding": pad(20, 0)})
    w.add("Border", "WordmarkRule", rule, {"BrushColor": srgb(INK_HEX, 0.45)}, fill_slot())
    names = w.add("VerticalBox", "WordmarkText", left, None, {"VerticalAlignment": "VAlign_Center"})
    w.text("WordmarkTitle", "Game day guide", names, 20, "Bold", spacing=EYEBROW_SPACING, shadow=True)
    w.text("WordmarkPlace", "Lincoln Financial Field", names, T_LABEL, color=INK_MUTED, shadow=True)

    # Controls, right: C++ puts Voice at the front of VoiceSlot; Settings always. Icon and word, never the icon alone.
    cluster = w.add("HorizontalBox", "VoiceSlot", root, None, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Top",
                                                               "Padding": pad(0, 28, EDGE_PAD, 0)})
    box = w.add("SizeBox", "SettingsButtonSize", cluster, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": 170,
                                                           "bOverride_MinDesiredHeight": True, "MinDesiredHeight": TOP_H},
                {"VerticalAlignment": "VAlign_Center"})
    gear = w.add("Button", "SettingsButton", box, {"IsFocusable": False, "WidgetStyle": SECONDARY_STYLE_ON_SCENE, "BackgroundColor": WHITE,
                                                   "ToolTipText": "Settings (S)"}, fill_slot())
    inner = w.add("HorizontalBox", "SettingsContent", gear, None, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.add("Image", "SettingsIcon", inner, {"Brush": {"resourceObject": {"refPath": GEAR_ICON}, "imageSize": {"x": 24, "y": 24}},
                                           "ColorAndOpacity": INK}, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 10, 0)})
    w.text("SettingsLabel", "Settings", inner, 20, "Bold", slot={"VerticalAlignment": "VAlign_Center"})


# On the open scene (no field behind it) a button uses the card colour, so it stands out from a bright sky.
SECONDARY_STYLE_ON_SCENE = button_style({**SURFACE, "a": 0.92}, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK)


def build_step_caption(w):
    """The step as an editorial caption on the scene: eyebrow, big title, body. No card: a soft field keeps it readable."""
    root = w.add("Overlay", "Root")
    scale = w.add("ScaleBox", "TextScale", root, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Bottom", "Padding": pad(8, 0, 0, CAPTION_BOTTOM)})
    field = w.fog("Caption", scale, None, pad(FOG_PAD - 16, FOG_PAD, FOG_PAD, FOG_PAD))
    stack = w.add("VerticalBox", "CaptionStack", field)
    # "02 / Gates open · 10:00 AM". (Named CaptionMeta, not CaptionHeader: Voice lives in the header.)
    meta = w.add("HorizontalBox", "CaptionMeta", stack, None, gap(14))
    w.eyebrow("StepNumberText", "01 / Getting there", meta, {"VerticalAlignment": "VAlign_Center"})
    w.text("TitleText", "Title", stack, T_TITLE, "Bold", wrap=CAPTION_WRAP, line=0.92, shadow=True, slot=gap(16))
    w.text("BodyText", "Body", stack, T_BODY, wrap=CAPTION_WRAP, line=BODY_LINE, shadow=True)
    # Loud or bright steps only. Nothing plays until the visitor picks Soft; Calm Mode hides the row (C++).
    preview = w.add("VerticalBox", "PreviewRow", stack, None, {"Padding": pad(0, 22, 0, 0)})
    w.button("PreviewButton", "PreviewLabel", "Preview this? Off", preview, min_w=280, min_h=TOGGLE_H, size=T_CONTROL - 2,
             slot={"HorizontalAlignment": "HAlign_Left"})
    w.text("PreviewNoteText", "", preview, T_LABEL, color=INK_MUTED, wrap=CAPTION_WRAP, line=BODY_LINE, shadow=True,
           slot={"Padding": pad(0, 10, 0, 0)})
    w.text("TargetMissingText", "This step is not available yet.", stack, T_LABEL, "Bold", color=NOTICE, wrap=CAPTION_WRAP,
           slot={"Padding": pad(0, 10, 0, 0)})
    w.collapse("TargetMissingText", "PreviewRow")


def build_bottom_rail(w):
    """One chapter bar: the whole day at a glance, where you are in it, Back and Next at its ends. Key hints under it."""
    root = w.add("Overlay", "Root")
    w.fade("BottomFade", FADE_BOTTOM, root, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Bottom"}, alpha=0.7, height=240)
    bar = w.glass("ChapterBar", "Bar", root, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Bottom",
                                              "Padding": pad(0, 0, 0, BAR_BOTTOM)}, pad(10), radius=30)
    row = w.add("HorizontalBox", "BarRow", bar)
    fill_v = {"VerticalAlignment": "VAlign_Fill"}
    w.button("BackButton", "BackLabel", "Back", row, min_w=130, size=T_CONTROL, slot={**fill_v, "Padding": pad(0, 0, 12, 0)},
             style=BAR_BUTTON_STYLE)
    # C++ fills this with one chip per chapter (LFFJourneyPanel::Chapters).
    w.add("HorizontalBox", "ChapterMarks", row, None, fill_v)
    w.button("NextButton", "NextLabel", "Next", row, primary=True, min_w=150, size=T_BODY, slot={**fill_v, "Padding": pad(12, 0, 0, 0)},
             style=BAR_PRIMARY_STYLE)

    # The keys, in the credits band on the right (the Cesium and Google credits keep the left).
    hints = w.add("HorizontalBox", "KeyHints", root, None, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Bottom",
                                                            "Padding": pad(0, 0, EDGE_PAD, 22)})
    for index, (name, key, label) in enumerate((("Back", "←", "Back"), ("Next", "→", "Next"), ("Move", "Tab", "Move"),
                                                ("Settings", "S", "Settings"), ("Pause", "P", "Pause"))):
        w.keycap(f"Hint{name}", key, hints, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0 if index == 0 else 22, 0, 0, 0)}, label)


# The bar's ends: rounded like the chips (not pills), so the bar reads as one piece.
BAR_BUTTON_STYLE = button_style(RAISED, RAISED_HOVER, RAISED_PRESSED, OUTLINE, INK, half_height=False, radius=20)
BAR_PRIMARY_STYLE = button_style(KELLY, KELLY_HOVER, KELLY_PRESSED, KELLY, INK, half_height=False, radius=20,
                                 disabled_fill=srgb("#8FA3A2"))


def toggle_row(w, name, label_name, label, key, parent, slot=None):
    """A Settings row: name, its shortcut as a keycap, then On / Off at the right end (C++ fills the words and the fill)."""
    box = w.add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": 500,
                                                   "bOverride_MinDesiredHeight": True, "MinDesiredHeight": TOGGLE_H}, slot)
    b = w.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": ROW_STYLE, "BackgroundColor": WHITE}, fill_slot())
    row = w.add("HorizontalBox", f"{name}Content", b, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Center"})
    w.text(label_name, label, row, T_CONTROL, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    if key:
        cap = w.add("Border", f"{name}Cap", row, {"Background": rounded(WHITE, OUTLINE, 1.5, radius=8), "BrushColor": {**SURFACE, "a": 0},
                                                  "Padding": pad(10, 1)}, {"VerticalAlignment": "VAlign_Center", "Padding": pad(16, 0, 0, 0)})
        w.text(f"{name}Key", key, cap, T_LABEL, "Bold")
    w.text(label_name.replace("Label", "State"), "Off", row, T_CONTROL, slot={"VerticalAlignment": "VAlign_Center", "Padding": pad(20, 0, 0, 0)})
    return b


def build_settings(w):
    """Settings is a sheet on the right, over a light dim: the scene and where you were stay in view on the left."""
    root = w.add("Overlay", "Root")
    w.add("Border", "Scrim", root, {"BrushColor": {"r": 0, "g": 0, "b": 0, "a": 0.4}, "Visibility": "Collapsed"}, fill_slot())
    centre = {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"}

    sheet = w.add("HorizontalBox", "SettingsPanel", root, None, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Fill"})
    edge = w.add("SizeBox", "SheetEdgeSize", sheet, {"bOverride_WidthOverride": True, "WidthOverride": 120}, {"VerticalAlignment": "VAlign_Fill"})
    w.add("Image", "SheetEdge", edge, {"Brush": {"resourceObject": {"refPath": FADE_SIDE}, "drawAs": "Image"},
                                       "ColorAndOpacity": SURFACE, "Visibility": "HitTestInvisible"}, fill_slot())
    body = w.add("Border", "SurfaceSettings", sheet, {"BrushColor": SURFACE, "Padding": pad(16, 44, EDGE_PAD + 16, 40)},
                 {"VerticalAlignment": "VAlign_Fill"})
    stack = w.add("VerticalBox", "SettingsStack", body)
    header = w.add("HorizontalBox", "SettingsHeader", stack, None, gap(10))
    w.text("SettingsTitle", "Settings", header, 44, "Bold", slot={"Size": {"value": 1, "sizeRule": "Fill"}, "VerticalAlignment": "VAlign_Center"})
    w.button("SettingsCloseButton", "SettingsCloseLabel", "Close", header, min_w=130, min_h=TOP_H, size=T_CONTROL,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.text("SettingsHint", "Change these at any time. Calm Mode turns sound and voice off and uses less movement.", stack, T_LABEL,
           color=INK_MUTED, wrap=500, line=BODY_LINE, slot=gap(24))
    # Two groups with a plain heading each, so seven toggles don't read as one flat list. Each row shows its key.
    groups = (("Comfort", (("CalmModeButton", "CalmModeLabel", "Calm Mode", "3"),
                           ("ReducedMotionButton", "ReducedMotionLabel", "Less movement", "1"),
                           ("LargeTextButton", "LargeTextLabel", "Large text", "4"),
                           ("HighContrastButton", "HighContrastLabel", "High contrast", None))),
              ("Sound and scene", (("SoundButton", "SoundLabel", "Sound", "2"),
                                   ("VoiceButton", "VoiceLabel", "Voice", "5"),
                                   ("CrowdButton", "CrowdLabel", "Crowd", "6"))))
    for index, (heading, items) in enumerate(groups):
        w.eyebrow(f"Group{index}Title", heading, stack, {"Padding": pad(0, 0 if index == 0 else 18, 0, 10)})
        for name, label_name, label, key in items:
            toggle_row(w, name, label_name, label, key, stack, slot=gap(8))
    w.add("Spacer", "SettingsFill", stack, None, {"Size": {"value": 1, "sizeRule": "Fill"}})
    foot = w.add("HorizontalBox", "SettingsFoot", stack, None, {"Padding": pad(0, 20, 0, 0)})
    w.button("RestartButton", "RestartLabel", "Start again", foot, min_w=200, min_h=TOGGLE_H, size=T_CONTROL,
             slot={"VerticalAlignment": "VAlign_Center"})
    w.keycap("RestartKey", "R", foot, {"VerticalAlignment": "VAlign_Center", "Padding": pad(16, 0, 0, 0)})
    w.text("SettingsKeys", "Tab moves. Enter presses. Esc closes.", stack, T_LABEL, color=INK_MUTED,
           wrap=500, slot={"Padding": pad(0, 18, 0, 0)})

    # Paused and Start again?: words in the middle of a dimmed scene, on the same soft field.
    # The named box is what C++ shows and hides; the field inside is a Surface (High contrast).
    pause_box = w.add("SizeBox", "PausePanel", root, None, centre)
    pause = w.fog("Pause", pause_box, fill_slot(), pad(FOG_PAD, FOG_PAD - 16), alpha=0.94)
    pause_stack = w.add("VerticalBox", "PauseStack", pause)
    w.eyebrow("PauseEyebrow", "Take your time", pause_stack, gap(12))
    w.text("PauseTitle", "Paused", pause_stack, T_HEAD, "Bold", shadow=True, slot=gap(10))
    w.text("PauseBody", "Nothing will move until you resume.", pause_stack, T_BODY, color=INK_MUTED, wrap=560, shadow=True, slot=gap(30))
    resume = w.add("HorizontalBox", "ResumeRow", pause_stack)
    w.button("ResumeButton", "ResumeLabel", "Resume", resume, primary=True, min_w=220, size=T_BODY, slot={"VerticalAlignment": "VAlign_Center"})
    w.keycap("ResumeKey", "P", resume, {"VerticalAlignment": "VAlign_Center", "Padding": pad(20, 0, 0, 0)}, "to resume")

    confirm_box = w.add("SizeBox", "RestartConfirmPanel", root, None, centre)
    confirm = w.fog("Confirm", confirm_box, fill_slot(), pad(FOG_PAD, FOG_PAD - 16), alpha=0.94)
    confirm_stack = w.add("VerticalBox", "RestartStack", confirm)
    w.text("RestartTitle", "Start again?", confirm_stack, T_HEAD, "Bold", shadow=True, slot=gap(10))
    w.text("RestartBody", "You will go back to the beginning.", confirm_stack, T_BODY, color=INK_MUTED, wrap=560, shadow=True, slot=gap(30))
    choices = w.add("HorizontalBox", "RestartChoices", confirm_stack)
    w.button("RestartNoButton", "RestartNoLabel", "No, keep going", choices, min_w=220, slot={"Padding": pad(0, 0, 16, 0)})
    w.button("RestartYesButton", "RestartYesLabel", "Yes, start again", choices, primary=True, min_w=220)

    w.collapse("SettingsPanel", "PausePanel", "RestartConfirmPanel")


def extend_step_caption(w):
    """The phone that rises on the ticket step: right of centre, above the chapter bar. C++ animates and fills it."""
    root = w.child("Root")
    phone = w.add("SizeBox", "TicketPhone", root, {"bOverride_WidthOverride": True, "WidthOverride": 250,
                                                    "bOverride_HeightOverride": True, "HeightOverride": 500},
                  {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Bottom",
                   "Padding": pad(0, 0, 220, CAPTION_BOTTOM)})
    w.add("Image", "TicketImage", phone, None, fill_slot())
    w.collapse("TicketPhone")


def move_caption_above_timeline(w):
    """Puts the caption back at its resting place (lower left, above the chapter bar)."""
    info = call(UMG, "GetWidgets", {"widgetBlueprint": w.bp})
    scale = next(x for x in info["widgets"] if x["widgetName"] == "TextScale" and x["widget"] != "None")
    if scale.get("slot") not in (None, "None"):
        set_props(scale["slot"], {"Padding": pad(8, 0, 0, CAPTION_BOTTOM)})


# Session 3 additions: widget -> (the names they add, the function that adds them).
EXTENSIONS = {
    "WBP_LFF_StepCaption": ({"TicketPhone"}, extend_step_caption),
}


def add_finale_actions(w):
    """Start Over and Learn More under the finale text. C++ shows them and centres the caption on the last step."""
    row = w.add("HorizontalBox", "FinaleActions", w.child("CaptionStack"), None,
                {"HorizontalAlignment": "HAlign_Left", "Padding": pad(0, 24, 0, 0)})
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


# ULFFJourneyPanel style properties set on the generated class defaults.
DEFAULTS = {
    "WBP_LFF_StepCaption": {"CaptionRestPadding": pad(8, 0, 0, CAPTION_BOTTOM)},
    "WBP_LFF_BottomRail": {"ChapterAccent": KELLY, "ChapterText": INK, "ChapterMutedText": INK_MUTED},
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
    # The two-mode HUD (Tools/Editor/build_wbp_lff_modes.py) replaced this one: WBP_LFF_HUD now binds Welcome, the sidebar
    # panels and the switchers, so rebuilding it here would leave a HUD that can't compile. Only WBP_LFF_Settings is still
    # built by this script's code (and reused as it is).
    if "--legacy" not in sys.argv[1:]:
        raise SystemExit("The HUD is built by Tools/Editor/build_wbp_lff_modes.py now. Pass --legacy to run this old builder anyway.")
    if "--extend" in sys.argv[1:]:
        extend()
    else:
        build("--rebuild" in sys.argv[1:])
