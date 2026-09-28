"""Builds /Game/LFF/UI/WBP_LFF_HUD (Docs/ManualSetup.md §5) through Unreal MCP.

Run from the repo root with the editor open and the Unreal MCP server running:
  python3 Tools/Editor/build_wbp_lff_hud.py

Refuses to overwrite an existing widget, so hand edits are safe. Delete the asset first to rebuild.
Widget names are the C++ contract in Docs/JOURNEY_SCHEMA.md. Labels that C++ writes (NextLabel etc.) get
placeholder text here; the key hint sits in a separate text block so the C++ text never drops it.
Visible wording is placeholder until the user and the Eagles Autism Foundation approve it (LEDGER F24).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from unreal_mcp import call  # noqa: E402

UMG, OBJ, ASSET = "UMGToolSet.UMGToolSet", "editor_toolset.toolsets.object.ObjectTools", "editor_toolset.toolsets.asset.AssetTools"
FOLDER, NAME = "/Game/LFF/UI", "WBP_LFF_HUD"
BP = {"refPath": f"{FOLDER}/{NAME}.{NAME}"}

# Linear colours. Midnight green #004C54 with white text is about 9.7:1 (ACCESSIBILITY.md asks for 7:1).
WHITE = {"r": 1, "g": 1, "b": 1, "a": 1}
MIDNIGHT = {"r": 0.0, "g": 0.0723, "b": 0.0887, "a": 1}
MIDNIGHT_PRESSED = {"r": 0.0, "g": 0.0331, "b": 0.0395, "a": 1}
DISABLED = {"r": 0.03, "g": 0.03, "b": 0.03, "a": 0.6}
PANEL = {"r": 0.01, "g": 0.012, "b": 0.016, "a": 0.92}
NOTICE = {"r": 1.0, "g": 0.85, "b": 0.45, "a": 1}

TITLE_PT, BODY_PT, SMALL_PT = 48, 32, 28
MIN_BUTTON = 120
# Contract widgets C++ binds by name; they're also marked as variables.
BOUND = {
    "DisclaimerText", "PrototypeBadgeText", "TitleText", "BodyText", "ProgressText", "TargetMissingText",
    "NextLabel", "PauseLabel", "ReducedMotionLabel", "SoundLabel", "CalmModeLabel", "LargeTextLabel",
    "DisclaimerPanel", "IntroPanel", "PausePanel", "SettingsPanel", "RestartConfirmPanel",
    "NextButton", "BackButton", "PauseButton", "ResumeButton", "SettingsButton", "SettingsCloseButton",
    "RestartButton", "RestartYesButton", "RestartNoButton", "ReducedMotionButton", "SoundButton",
    "CalmModeButton", "LargeTextButton",
}


def set_props(ref, values):
    if not call(OBJ, "set_properties", {"instance": ref, "values": json.dumps(values)}):
        raise RuntimeError(f"set_properties failed on {ref['refPath']}: {values}")


def add(cls, name, parent=None, props=None, slot=None):
    info = call(UMG, "AddWidget", {"widgetBlueprint": BP, "widgetClass": {"refPath": f"/Script/UMG.{cls}"},
                                   "widgetDisplayName": name, "parentWidget": parent, "childIndex": -1})
    widget = info["widget"]
    if props:
        set_props(widget, props)
    if slot and info.get("slot") not in (None, "None"):
        set_props(info["slot"], slot)
    if name in BOUND:
        call(UMG, "ToggleWidgetAsVariable", {"widgetBlueprint": BP, "widget": widget, "bIsVariable": True})
    return widget


def font(size, face="Regular"):
    return {"fontObject": {"refPath": "/Engine/EngineFonts/Roboto.Roboto"}, "typefaceFontName": face, "size": size}


def text(name, value, parent, size=BODY_PT, face="Regular", wrap=None, color=WHITE, slot=None):
    props = {"Text": value, "Font": font(size, face), "ColorAndOpacity": {"specifiedColor": color, "colorUseRule": "UseColor_Specified"}}
    if wrap:
        props.update({"AutoWrapText": False, "WrapTextAt": wrap})
    return add("TextBlock", name, parent, props, slot)


def canvas(anchor, align, pos, z=0):
    return {"LayoutData": {"offsets": {"left": pos[0], "top": pos[1], "right": 0, "bottom": 0},
                           "anchors": {"minimum": {"x": anchor[0], "y": anchor[1]}, "maximum": {"x": anchor[0], "y": anchor[1]}},
                           "alignment": {"x": align[0], "y": align[1]}},
            "bAutoSize": True, "ZOrder": z}


def pad(l, t=None, r=None, b=None):
    t = l if t is None else t
    return {"left": l, "top": t, "right": l if r is None else r, "bottom": t if b is None else b}


def brush(fill, outline, width):
    return {"tintColor": {"specifiedColor": fill, "colorUseRule": "UseColor_Specified"}, "drawAs": "RoundedBox",
            "outlineSettings": {"cornerRadii": {"x": 8, "y": 8, "z": 8, "w": 8}, "width": width, "roundingType": "FixedRadius",
                                "color": {"specifiedColor": outline, "colorUseRule": "UseColor_Specified"}}}


BUTTON_STYLE = {
    "normal": brush(MIDNIGHT, {"r": 0.75, "g": 0.75, "b": 0.75, "a": 1}, 2),
    # Hover and press change the outline or darken; they never lighten the fill below 7:1.
    "hovered": brush(MIDNIGHT, WHITE, 4),
    "pressed": brush(MIDNIGHT_PRESSED, WHITE, 4),
    "disabled": brush(DISABLED, {"r": 0.3, "g": 0.3, "b": 0.3, "a": 0.6}, 2),
    "normalPadding": pad(24, 12), "pressedPadding": pad(24, 14, 24, 10),
}


def button(name, label_name, label, key_hint, parent, min_width=MIN_BUTTON, slot=None):
    """SizeBox (min 120x120) > Button (not focusable) > [label][key hint]."""
    box = add("SizeBox", f"{name}Size", parent, {"bOverride_MinDesiredWidth": True, "MinDesiredWidth": min_width,
                                                 "bOverride_MinDesiredHeight": True, "MinDesiredHeight": MIN_BUTTON}, slot)
    b = add("Button", name, box, {"IsFocusable": False, "WidgetStyle": BUTTON_STYLE, "BackgroundColor": WHITE},
            {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    row = add("HorizontalBox", f"{name}Row", b, None, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    text(label_name, label, row, SMALL_PT, "Bold", slot={"VerticalAlignment": "VAlign_Center"})
    text(f"{name}KeyHint", f"  {key_hint}", row, SMALL_PT, slot={"VerticalAlignment": "VAlign_Center"})
    return b


def panel(name, parent, slot, padding=40):
    border = add("Border", name, parent, {"BrushColor": PANEL, "Padding": pad(padding)}, slot)
    return add("VerticalBox", f"{name}Stack", border, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})


def gap(px):
    return {"Padding": pad(0, 0, 0, px)}


def build():
    if call(ASSET, "exists", {"path": f"{FOLDER}/{NAME}"}):
        raise SystemExit(f"{FOLDER}/{NAME} already exists; delete it first to rebuild")
    call(UMG, "CreateWidgetBlueprint", {"folderPath": FOLDER, "assetName": NAME,
                                        "parentClass": {"refPath": "/Script/VirtualStadium.LFFHUDWidget"}})
    root = add("CanvasPanel", "RootCanvas")

    # Left card: step (or intro) text. C++ fills all four.
    card = panel("StepCard", root, canvas((0, 0.5), (0, 0.5), (40, 20)))
    text("ProgressText", "Step 1 of 4", card, SMALL_PT, slot=gap(8))
    text("TitleText", "Title", card, TITLE_PT, "Bold", wrap=760, slot=gap(16))
    text("BodyText", "Body", card, BODY_PT, wrap=760, slot=gap(8))
    text("TargetMissingText", "This step is not available yet.", card, BODY_PT, "Bold", wrap=760, color=NOTICE)

    # Intro choices (right side). The two one-click toggles from LEDGER D13.
    intro = panel("IntroPanel", root, canvas((1, 0.5), (1, 0.5), (-40, 20)))
    text("IntroHeading", "Before you start", intro, BODY_PT, "Bold", slot=gap(16))
    button("ReducedMotionButton", "ReducedMotionLabel", "Less movement: Off", "(1)", intro, 520, gap(16))
    button("CalmModeButton", "CalmModeLabel", "Calm Mode: Off", "(3)", intro, 520, gap(16))
    text("IntroHint", "You can change these at any time.\nPress S for settings.", intro, SMALL_PT, wrap=520)

    # Bottom row. Fixed positions, never hidden (C++ disables instead). Bottom 80 px stays clear for credits.
    button("BackButton", "BackLabel", "Back", "(←)", root, 300, canvas((0, 1), (0, 1), (40, -80)))
    button("RestartButton", "RestartLabel", "Start again?", "(R)", root, 360, canvas((0, 1), (0, 1), (380, -80)))
    button("NextButton", "NextLabel", "Next", "(→)", root, 300, canvas((1, 1), (1, 1), (-40, -80)))

    # Top right.
    top = add("HorizontalBox", "TopBar", root, None, canvas((1, 0), (1, 0), (-40, 24)))
    button("PauseButton", "PauseLabel", "Pause", "(P)", top, 220, {"Padding": pad(0, 0, 16, 0)})
    button("SoundButton", "SoundLabel", "Sound: Off", "(2)", top, 280, {"Padding": pad(0, 0, 16, 0)})
    button("SettingsButton", "SettingsLabel", "Settings", "(S)", top, 240)

    # Modal overlays, centred. Collapsed by default; C++ shows them.
    centre = canvas((0.5, 0.5), (0.5, 0.5), (0, 0), z=10)
    pause = panel("PausePanel", root, centre, 56)
    text("PauseTitle", "Paused", pause, TITLE_PT, "Bold", slot=gap(16))
    text("PauseBody", "Nothing will move until you resume.", pause, BODY_PT, wrap=760, slot=gap(24))
    button("ResumeButton", "ResumeLabel", "Resume", "(P)", pause, 320)

    settings = panel("SettingsPanel", root, centre, 56)
    text("SettingsTitle", "Settings", settings, TITLE_PT, "Bold", slot=gap(16))
    button("LargeTextButton", "LargeTextLabel", "Large text: Off", "(4)", settings, 520, gap(24))
    text("SettingsKeys", "Keys: 1 Less movement · 2 Sound · 3 Calm Mode · 4 Large text", settings, SMALL_PT, wrap=760, slot=gap(24))
    button("SettingsCloseButton", "SettingsCloseLabel", "Close", "(S)", settings, 320)

    restart = panel("RestartConfirmPanel", root, centre, 56)
    text("RestartTitle", "Start again?", restart, TITLE_PT, "Bold", slot=gap(16))
    text("RestartBody", "You will go back to the beginning.", restart, BODY_PT, wrap=760, slot=gap(24))
    choices = add("HorizontalBox", "RestartChoices", restart)
    button("RestartYesButton", "RestartYesLabel", "Yes, start again", "(Enter)", choices, 380, {"Padding": pad(0, 0, 24, 0)})
    button("RestartNoButton", "RestartNoLabel", "No, keep going", "(Esc)", choices, 380)
    for name in ("PausePanel", "SettingsPanel", "RestartConfirmPanel"):
        set_props({"refPath": f"{BP['refPath']}:WidgetTree.{name}"}, {"Visibility": "Collapsed"})

    # Always on top: the prototype badge and the disclaimer (C++ re-shows both on every update).
    badge = add("Border", "PrototypeBadge", root, {"BrushColor": PANEL, "Padding": pad(20, 12)},
                canvas((0, 0), (0, 0), (40, 24), z=20))
    text("PrototypeBadgeText", "Prototype · Illustrative · Example ticket", badge, SMALL_PT, "Bold")
    disclaimer = add("Border", "DisclaimerPanel", root, {"BrushColor": PANEL, "Padding": pad(24, 14)},
                     canvas((0.5, 0), (0.5, 0), (0, 168), z=20))
    text("DisclaimerText", "This is an illustration to help you prepare. It is not directions for the real stadium.",
         disclaimer, SMALL_PT, wrap=1300)

    if not call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": BP}):
        raise RuntimeError("WBP_LFF_HUD failed to compile")
    call(ASSET, "save_assets", {"asset_paths": [f"{FOLDER}/{NAME}"]})
    # GetWidgets also lists the parent's unbound BindWidget names as placeholders with widget == "None".
    names = {w["widgetName"] for w in call(UMG, "GetWidgets", {"widgetBlueprint": BP})["widgets"] if w["widget"] != "None"}
    missing = sorted(BOUND - names)
    if missing:
        raise RuntimeError(f"missing contract widgets: {missing}")
    print(f"built {FOLDER}/{NAME}: {len(names)} widgets, all {len(BOUND)} contract names present, compiled and saved")


if __name__ == "__main__":
    build()
