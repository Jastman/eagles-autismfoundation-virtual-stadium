"""Builds the scene-first corner HUD (Docs/UI_DESIGN.md) in /Game/LFF/UI through Unreal MCP:

  DT_LFF_Amenities        the map's markers, imported from SourceData/Journey/LFF_Amenities.json
  M_LFF_LevelHighlight    Free explore's level highlight, a post-process material (build_m_lff_level_highlight.py)
  WBP_LFF_Welcome         stadium view, lower-left green fade, partner logos, title and mode controls
  WBP_LFF_CalmToggle      cheering fans / empty seats and reversible UMG thumb animation
  WBP_LFF_SidebarHeader   identity card, top left; legacy asset name retained
  WBP_LFF_Narrative       caption card below identity, with optional day timeline, previews, ticket and finale
  WBP_LFF_ExploreMap      right-hand places panel with category controls, expandable levels and picked-place detail
  WBP_LFF_Callouts        places on the stadium; full-screen layer below opaque corner groups
  WBP_LFF_TimelineArc     five compact chapter chips in the control dock; legacy asset name retained
  WBP_LFF_StageChrome     mode, Settings, Voice, Calm Mode and Crowd controls, top right
  WBP_LFF_TourNav         Back and Next below chapter chips
  WBP_LFF_ExploreNav      My seat, Resume tour and flight-key guidance
  WBP_LFF_HUD             responsive InformationSize and ControlSize groups, Welcome and Settings

WBP_LFF_Settings is reused. Old sidebar, arc-material and bottom-rail presentations remain on disk, unused.

Run from the repo root with the editor open (a headless one is fine) and the Unreal MCP server on 127.0.0.1:8000:
  python3 Tools/Editor/build_wbp_lff_modes.py
Safe to rerun: existing widgets have their trees cleared and rebuilt in place, so every reference to them stays valid.
Widget names are the C++ contract (LFFHUDWidget.h, LFFJourneyPanel.h, LFFMapPanel.h, LFFTimelineArc.h).
"""
import json
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import build_wbp_lff_ui as ui  # noqa: E402
from build_wbp_lff_ui import Builder, gap, pad, fill_slot, srgb  # noqa: E402
from unreal_mcp import call, run_python  # noqa: E402

UMG, ASSET = ui.UMG, ui.ASSET
FOLDER = ui.FOLDER
PANEL_CLASS, HUD_CLASS = ui.PANEL_CLASS, ui.HUD_CLASS
MAP_CLASS = "/Script/VirtualStadium.LFFMapPanel"
ARC_CLASS = "/Script/VirtualStadium.LFFTimelineArc"
CALLOUT_CLASS = "/Script/VirtualStadium.LFFCalloutLayer"
CALM_TOGGLE_CLASS = "/Script/VirtualStadium.LFFCalmToggle"
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

INK, INK_MUTED, KELLY, SURFACE, RAISED = ui.INK, ui.INK_MUTED, ui.KELLY, ui.SURFACE, ui.RAISED
MIDNIGHT = srgb("#004C54")
T_LABEL, T_CONTROL, T_BODY, T_TITLE, T_HEAD = ui.T_LABEL, ui.T_CONTROL, ui.T_BODY, ui.T_TITLE, ui.T_HEAD
INFO_W, CONTROL_W = 480, 540
COLUMN_PAD, CARD_PAD = 24, 20
CAPTION_W = INFO_W - CARD_PAD * 2
TOUR_TITLE = 32
NAV_H = 72
FIVE_BELOW_LOGO = "/Game/LFF/UI/Icons/T_LFF_Logo_FiveBelow_UI.T_LFF_Logo_FiveBelow_UI"
WELCOME_FADE = "/Game/LFF/UI/Icons/T_LFF_WelcomeFade.T_LFF_WelcomeFade"
WELCOME_TEXTURES = ("T_LFF_Logo_FiveBelow_UI", "T_LFF_WelcomeFade", "T_LFF_CalmActive_UI", "T_LFF_CalmQuiet_UI")
WELCOME_BUTTON_STYLE = ui.button_style(srgb("#286B70"), srgb("#327C81"), MIDNIGHT, ui.WHITE, ui.WHITE,
                                       half_height=False, radius=16, padding=(24, 8))
WELCOME_CALM_STYLE = ui.button_style(MIDNIGHT, srgb("#0B5E66"), srgb("#003C43"), ui.WHITE, ui.WHITE,
                                     half_height=True, padding=(20, 8))
WELCOME_TOGGLE_STYLE = ui.button_style(MIDNIGHT, MIDNIGHT, MIDNIGHT,
                                       ui.WHITE, ui.WHITE, half_height=True, padding=(0, 0))
# Level tabs: the row style with less side padding, so the longest level name stays on one line.
TAB_STYLE = ui.button_style(ui.RAISED, ui.RAISED_HOVER, ui.RAISED_PRESSED, ui.OUTLINE, INK, half_height=False, radius=8, padding=(12, 8))
ACTION_STYLE = ui.button_style(ui.KELLY, ui.KELLY_HOVER, ui.KELLY_PRESSED, ui.KELLY, INK, half_height=False, radius=8, padding=(12, 8))

CONTRACT = {
    "WBP_LFF_Welcome": {"TourModeButton", "TourModeLabel", "ExploreModeButton", "ExploreModeLabel", "CalmToggle",
                        "SettingsButton", "SettingsLabel", "WelcomeWidth", "WelcomeActions", "WelcomeHeadline"},
    "WBP_LFF_CalmToggle": {"CalmModeButton", "CalmModeLabel", "CalmModeState", "CalmModeInfo", "CalmModeThumb",
                           "CalmModeActiveScene", "CalmModeQuietScene"},
    "WBP_LFF_SidebarHeader": set(),
    "WBP_LFF_Narrative": {"TimelineMarks", "TimelinePanel", "TimelineToggleButton", "TimelineToggleLabel",
                          "CaptionStack", "StepNumberText", "TitleText", "BodyText", "TimelineNoteText",
                          "TargetMissingText", "TextScale", "PreviewRow", "PreviewButton", "PreviewLabel", "PreviewNoteText",
                          "TicketPhone", "TicketImage", "FinaleActions", "FinaleStartOverButton", "FinaleLearnMoreButton"},
    "WBP_LFF_ExploreMap": {"ViewingText", "LevelsGroup", "LevelsToggleButton", "LevelsToggleLabel",
                           "UpperDeckTab", "UpperDeckTabLabel", "SuitesTab", "SuitesTabLabel", "ClubTab", "ClubTabLabel",
                           "LowerConcourseTab", "LowerConcourseTabLabel", "FieldTab", "FieldTabLabel",
                           "SensoryToggle", "SensoryLabel", "QuietToggle", "QuietLabel", "GuestToggle", "GuestLabel",
                           "RestroomToggle", "RestroomLabel", "FoodToggle", "FoodLabel", "HideAllButton",
                           "PoiCard", "PoiTitle", "PoiNote", "ShowMeButton", "PickHint"},
    "WBP_LFF_Callouts": {"CalloutCanvas"},
    "WBP_LFF_TimelineArc": {"ChapterMarks"},
    "WBP_LFF_StageChrome": {"TourModeButton", "TourModeLabel", "ExploreModeButton", "ExploreModeLabel",
                             "VoiceSlot", "CrowdButton", "CrowdLabel", "CalmModeButton", "CalmModeLabel", "SettingsButton", "SettingsLabel"},
    "WBP_LFF_TourNav": {"BackButton", "NextButton", "NextLabel"},
    "WBP_LFF_ExploreNav": {"MySeatButton", "ResumeTourButton", "ResumeTourLabel"},
    "WBP_LFF_HUD": {"InformationSize", "ControlSize", "Callouts", "Welcome", "SidebarHeader", "Narrative", "ExploreMap",
                    "TimelineArc", "StageChrome", "NavSwitcher", "TourNav", "ExploreNav", "Settings"},
}
# Builder.add marks these names as variables (BindWidget needs them) and the build checks they all exist.
ui.CONTRACT.update(CONTRACT)
# The tour card keeps its size on step 1 too (the smaller step-1 card was for the old caption on the scene).
ui.DEFAULTS["WBP_LFF_Narrative"] = {"FirstStepCardScale": 1.0}
ui.DEFAULTS["WBP_LFF_TimelineArc"] = {"CompactChapterChips": True, "ShowDrop": 0.0, "ShowSeconds": 0.18}
ui.DEFAULTS["WBP_LFF_Welcome"] = {"ModeMutedText": ui.WHITE}


def wordmark(w, parent, slot):
    """EAF logo, a hairline, then what this is and where (the same wordmark the old header had)."""
    row = w.add("HorizontalBox", "Wordmark", parent, None, slot)
    w.add("Image", "EAFLogo", row, {"Brush": {"resourceObject": {"refPath": ui.EAF_LOGO}, "imageSize": {"x": 150, "y": 52}},
                                    "ToolTipText": "Eagles Autism Foundation"}, {"VerticalAlignment": "VAlign_Center"})
    rule = w.add("SizeBox", "WordmarkRuleSize", row, {"bOverride_WidthOverride": True, "WidthOverride": 2,
                                                     "bOverride_HeightOverride": True, "HeightOverride": 44},
                 {"VerticalAlignment": "VAlign_Center", "Padding": pad(20, 0)})
    w.add("Border", "WordmarkRule", rule, {"BrushColor": srgb(ui.INK_HEX, 0.45)}, fill_slot())
    names = w.add("VerticalBox", "WordmarkText", row, None, {"VerticalAlignment": "VAlign_Center"})
    w.text("WordmarkTitle", "Game day guide", names, 20, "Bold", spacing=ui.EYEBROW_SPACING)
    w.text("WordmarkPlace", "Lincoln Financial Field", names, T_LABEL, color=INK_MUTED)
    return row


def sized_button(w, name, parent, slot, width, height, style, min_width=False):
    """SizeBox (fixed or minimum size) > Button (not focusable; the HUD runs its own focus ring)."""
    size = {"bOverride_HeightOverride": True, "HeightOverride": height}
    size.update({"bOverride_MinDesiredWidth": True, "MinDesiredWidth": width} if min_width
                else {"bOverride_WidthOverride": True, "WidthOverride": width})
    box = w.add("SizeBox", f"{name}Size", parent, size, slot)
    return w.add("Button", name, box, {"IsFocusable": False, "WidgetStyle": style, "BackgroundColor": ui.WHITE}, fill_slot())


def toggle_cell(w, name, label_name, label, parent, slot):
    """Name and explicit state on two lines, including the truthful unavailable-data state."""
    b = w.add("Button", name, parent, {"IsFocusable": False, "WidgetStyle": TAB_STYLE, "BackgroundColor": ui.WHITE}, slot)
    row = w.add("VerticalBox", f"{name}Content", b, None, {"HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Center"})
    title = w.text(label_name, label, row, T_LABEL, "Bold")
    state = w.text(label_name.replace("Label", "State"), "Off", row, T_LABEL)
    ui.set_props(title, {"AutoWrapText": True})
    ui.set_props(state, {"AutoWrapText": True})
    return b


def key_hint(w, name, key, parent, slot):
    """A keycap drawn as an outline only, so its letter takes the button's text colour and reads on the light chosen chip too."""
    cap = w.add("Border", f"{name}Cap", parent, {"Background": ui.rounded(ui.WHITE, ui.OUTLINE, 1.5, radius=8), "BrushColor": {**SURFACE, "a": 0},
                                                 "Padding": pad(10, 2)}, slot)
    w.text(f"{name}Key", key, cap, T_LABEL, "Bold", slot={"HorizontalAlignment": "HAlign_Center"})
    return cap


def settings_button(w, parent, slot):
    """Settings: icon and word, never the icon alone."""
    gear = sized_button(w, "SettingsButton", parent, slot, 170, ui.TOP_H, ui.SECONDARY_STYLE_ON_SCENE, min_width=True)
    inner = w.add("HorizontalBox", "SettingsContent", gear, None, {"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.add("Image", "SettingsIcon", inner, {"Brush": {"resourceObject": {"refPath": ui.GEAR_ICON}, "imageSize": {"x": 24, "y": 24}},
                                           "ColorAndOpacity": INK}, {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 10, 0)})
    w.text("SettingsLabel", "Settings", inner, 20, "Bold", slot={"VerticalAlignment": "VAlign_Center"})
    return gear


def build_welcome(w):
    """Partner-led lower-left composition; keep the stadium and attribution unobstructed."""
    root = w.add("Border", "SurfaceWelcome", None, {"BrushColor": {**MIDNIGHT, "a": 0}, "Padding": pad(0)})
    over = w.add("Overlay", "WelcomeRoot", root, None, fill_slot())
    w.add("Image", "WelcomeCornerFade", over, {"Brush": {"resourceObject": {"refPath": WELCOME_FADE}, "drawAs": "Image"},
                                               "ColorAndOpacity": MIDNIGHT, "Visibility": "HitTestInvisible"}, fill_slot())
    width = w.add("SizeBox", "WelcomeWidth", over, {"bOverride_WidthOverride": True, "WidthOverride": 1060},
                  {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Bottom",
                   "Padding": pad(48, 24, 48, 112)})
    stack = w.add("VerticalBox", "WelcomeStack", width)
    logos = w.add("HorizontalBox", "WelcomePartners", stack, None, gap(28))
    for name, texture, x, y in (("WelcomeEAFLogo", ui.EAF_LOGO, 220, 77),
                                ("WelcomeFiveBelowLogo", FIVE_BELOW_LOGO, 270, 72)):
        w.add("Image", name, logos, {"Brush": {"resourceObject": {"refPath": texture}, "imageSize": {"x": x, "y": y}},
                                    "Visibility": "HitTestInvisible"},
              {"VerticalAlignment": "VAlign_Center", "Padding": pad(0, 0, 40, 0)})
    w.text("WelcomeHeadline", "GAMEDAY VISUAL\nSCHEDULE", stack, 72, "Bold", color=ui.WHITE,
           wrap=1060, line=0.95, shadow=True, slot=gap(24))
    actions = w.add("WrapBox", "WelcomeActions", stack, {"bExplicitWrapSize": True, "WrapSize": 1060,
                                                        "InnerSlotPadding": {"x": 24, "y": 16}})
    for name, label, title in (("TourModeButton", "TourModeLabel", "Guided Tour"),
                                ("ExploreModeButton", "ExploreModeLabel", "Explore")):
        button = sized_button(w, name, actions, {"VerticalAlignment": "VAlign_Top"}, 300, 96, WELCOME_BUTTON_STYLE)
        w.text(label, title, button, 32, "Bold", color=ui.WHITE,
               slot={"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.add(f"{FOLDER}/WBP_LFF_CalmToggle.WBP_LFF_CalmToggle_C", "CalmToggle", actions,
          None, {"VerticalAlignment": "VAlign_Top"})
    settings = settings_button(w, over, {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Bottom",
                                         "Padding": pad(0, 0, 48, 112)})
    ui.set_props(settings, {"WidgetStyle": WELCOME_CALM_STYLE})


def build_calm_toggle(w):
    calm_size = w.add("SizeBox", "WelcomeCalmSize", None,
                      {"bOverride_WidthOverride": True, "WidthOverride": 268,
                       "bOverride_HeightOverride": True, "HeightOverride": 138})
    calm_overlay = w.add("Overlay", "WelcomeCalm", calm_size, None, fill_slot())
    stack_calm = w.add("VerticalBox", "CalmModeContent", calm_overlay)
    toggle_size = w.add("SizeBox", "CalmModeButtonSize", stack_calm,
                        {"bOverride_HeightOverride": True, "HeightOverride": 96})
    calm = w.add("Button", "CalmModeButton", toggle_size,
                 {"IsFocusable": False, "WidgetStyle": WELCOME_TOGGLE_STYLE, "BackgroundColor": ui.WHITE,
                  "bOverrideAccessibleDefaults": True, "AccessibleBehavior": "Custom", "AccessibleText": "Calm Mode",
                  "bCanChildrenBeAccessible": False}, fill_slot())
    track = w.add("Overlay", "CalmModeTrack", calm, None, fill_slot())
    for name, texture in (("CalmModeActiveScene", "T_LFF_CalmActive_UI"),
                           ("CalmModeQuietScene", "T_LFF_CalmQuiet_UI")):
        w.add("Image", name, track, {"Brush": {"resourceObject": {"refPath": f"/Game/LFF/UI/Icons/{texture}.{texture}"},
                                                "imageSize": {"x": 268, "y": 96}},
                                    "Visibility": "HitTestInvisible",
                                    "RenderOpacity": 0 if "Quiet" in name else 1}, {**fill_slot(), "Padding": pad(2)})
    thumb_size = w.add("SizeBox", "CalmModeThumbSize", track,
                       {"bOverride_WidthOverride": True, "WidthOverride": 80,
                        "bOverride_HeightOverride": True, "HeightOverride": 80},
                       {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Center", "Padding": pad(8, 0, 0, 0)})
    thumb = w.add("Border", "CalmModeThumb", thumb_size,
                  {"Background": ui.rounded(srgb("#EDF4E9"), ui.WHITE, 1, half_height=True),
                   "BrushColor": ui.WHITE, "Padding": pad(0), "Visibility": "HitTestInvisible"})
    w.text("CalmModeState", "Off", thumb, 22, "Bold", color=srgb("#15393B"),
           slot={"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})
    w.text("CalmModeLabel", "Calm Mode", stack_calm, 24, "Bold", color=ui.WHITE,
           slot={"HorizontalAlignment": "HAlign_Center", "Padding": pad(0, 10, 0, 0)})
    # Canvas children never contribute to desired size, so revealing the hint cannot stretch the action row.
    hints = w.add("CanvasPanel", "CalmModeHintLayer", calm_overlay, {"Visibility": "HitTestInvisible"}, fill_slot())
    info = w.add("Border", "CalmModeInfo", hints, {"Background": ui.rounded(SURFACE, ui.WHITE, 1, radius=12),
                                                 "Padding": pad(16), "Visibility": "Collapsed"},
                 {"LayoutData": {"offsets": {"left": 0, "top": -12, "right": 0, "bottom": 0},
                                  "alignment": {"x": 0, "y": 1}},
                  "bAutoSize": True})
    w.text("CalmModeInfoText", "Limits sounds, animations, and movement throughout the experience.",
           info, 20, color=ui.WHITE, wrap=280, line=1.0)


def build_calm_animation():
    print(run_python(f"""import unreal
path = "{FOLDER}/WBP_LFF_CalmToggle"
name = "CalmTransition"
service = unreal.WidgetService
if any(str(animation.animation_name) == name for animation in service.list_animations(path)):
    if not service.remove_animation(path, name):
        raise RuntimeError("Could not replace CalmTransition")
if not service.create_animation(path, name, 0.24):
    raise RuntimeError("Could not create CalmTransition")
for widget, prop in (("CalmModeThumb", "RenderOpacity"), ("CalmModeQuietScene", "RenderOpacity")):
    if not service.add_animation_track(path, name, widget, prop):
        raise RuntimeError("Could not bind animation: " + widget)
for time, value in ((0.0, "0"), (0.24, "1")):
    key = unreal.WidgetAnimKeyframe(time=time, value=value, interpolation="Cubic")
    if not service.add_keyframe(path, name, "CalmModeQuietScene", "RenderOpacity", key):
        raise RuntimeError("Could not keyframe empty seats")
blueprint = unreal.load_asset(path)
blueprint.modify()
animation = next(a for a in unreal.ObjectIterator(unreal.WidgetAnimation)
                 if a.get_outer() == blueprint and a.get_name() == name)
binding = next(b for b in animation.get_bindings() if str(b.get_name()) == "CalmModeThumb")
# The service supplies the UMG widget binding; use a proper 2D transform track so Slate is invalidated correctly.
for track in binding.get_tracks():
    binding.remove_track(track)
track = binding.add_track(unreal.MovieScene2DTransformTrack)
track.set_property_name_and_path("RenderTransform", "RenderTransform")
section = track.add_section()
rate = animation.get_tick_resolution()
ticks_per_second = rate.numerator / rate.denominator
end_frame = round(0.24 * ticks_per_second)
section.set_range_seconds(0, (end_frame + 1) / ticks_per_second)
channel = next(c for c in section.get_all_channels() if str(c.channel_name) == "Translation.X")
for frame, value in ((0, 0.0), (end_frame, 172.0)):
    channel.add_key(unreal.FrameNumber(frame), value, time_unit=unreal.MovieSceneTimeUnit.TICK_RESOLUTION,
                    interpolation=unreal.MovieSceneKeyInterpolation.AUTO)
for binding in animation.get_bindings():
    for track in binding.get_tracks():
        for section in track.get_sections():
            for channel in section.get_all_channels():
                for key in channel.get_keys():
                    key.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_USER)
                    key.set_arrive_tangent(0.0)
                    key.set_leave_tangent(0.0)
print("Authored 240 ms reversible UMG thumb slide and empty-seat crossfade.")
""").strip())


def import_welcome_textures():
    for name in WELCOME_TEXTURES:
        source = os.path.join(REPO, "SourceData", "UI", name + ".png")
        if not os.path.isfile(source):
            raise RuntimeError("Run python3 Tools/Pipeline/prepare_welcome_ui.py first")
    print(run_python(f"""import unreal
tasks = []
for name in {WELCOME_TEXTURES!r}:
    task = unreal.AssetImportTask()
    task.filename = r'''{REPO}/SourceData/UI/''' + name + ".png"
    task.destination_path = "/Game/LFF/UI/Icons"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    texture = unreal.load_asset(task.destination_path + "/" + task.destination_name)
    if texture is None:
        raise RuntimeError("Welcome texture import failed: " + task.destination_name)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError("Welcome texture save failed: " + task.destination_name)
print("Imported and saved welcome textures.")
""").strip())


def build_sidebar_header(w):
    """Compact identity at upper left; every interactive mode control lives in the upper-right dock."""
    card = w.add("Border", "SurfaceHeader", None, {"Background": ui.rounded(ui.WHITE, ui.EDGE, 1, radius=10),
                                                   "BrushColor": SURFACE, "Padding": pad(16)})
    wordmark(w, card, fill_slot())


def build_narrative(w):
    """One readable caption, not a permanent schedule/sidebar. The day timeline is disclosed on demand."""
    card = w.add("Border", "SurfaceCaption", None, {"Background": ui.rounded(ui.WHITE, ui.EDGE, 1, radius=10),
                                                    "BrushColor": SURFACE, "Padding": pad(CARD_PAD)})
    scroll = w.add("ScrollBox", "CaptionScroll", card)
    scale = w.add("ScaleBox", "TextScale", scroll, {"Stretch": "UserSpecified", "UserSpecifiedScale": 1.0}, {"HorizontalAlignment": "HAlign_Left"})
    width = w.add("SizeBox", "CaptionWidth", scale, {"bOverride_WidthOverride": True, "WidthOverride": CAPTION_W})
    stack = w.add("VerticalBox", "CaptionStack", width)
    step = w.text("StepNumberText", "Step 1", stack, T_LABEL, color=INK_MUTED, slot=gap(12))
    ui.set_props(step, {"AutoWrapText": True})
    w.text("TitleText", "Title", stack, TOUR_TITLE, "Bold", wrap=CAPTION_W, line=0.95, slot=gap(12))
    w.text("BodyText", "Body", stack, 24, wrap=CAPTION_W, line=ui.BODY_LINE)
    preview = w.add("VerticalBox", "PreviewRow", stack, None, {"Padding": pad(0, 20, 0, 0)})
    w.button("PreviewButton", "PreviewLabel", "Preview this? Off", preview, min_w=280, min_h=ui.TOGGLE_H, size=T_CONTROL - 2,
             slot={"HorizontalAlignment": "HAlign_Left"})
    w.text("PreviewNoteText", "", preview, T_LABEL, color=INK_MUTED, wrap=CAPTION_W, line=ui.BODY_LINE, slot={"Padding": pad(0, 10, 0, 0)})
    w.text("TargetMissingText", "This step is not available yet.", stack, T_LABEL, "Bold", color=ui.NOTICE, wrap=CAPTION_W,
           slot={"Padding": pad(0, 10, 0, 0)})
    phone = w.add("SizeBox", "TicketPhone", stack, {"bOverride_WidthOverride": True, "WidthOverride": 180,
                                                   "bOverride_HeightOverride": True, "HeightOverride": 360},
                  {"HorizontalAlignment": "HAlign_Center", "Padding": pad(0, 20, 0, 0)})
    w.add("Image", "TicketImage", phone, None, fill_slot())
    w.button("TimelineToggleButton", "TimelineToggleLabel", "Show day timeline", stack, min_w=280, min_h=64,
             size=20, style=TAB_STYLE, slot={"Padding": pad(0, 20, 0, 0)})
    drawer = w.add("VerticalBox", "TimelinePanel", stack, None, {"Padding": pad(0, 12, 0, 0)})
    w.text("TimelineNoteText", "Approximate times", drawer, T_LABEL, color=INK_MUTED, wrap=CAPTION_W, slot=gap(12))
    w.add("VerticalBox", "TimelineMarks", drawer)
    finale = w.add("VerticalBox", "FinaleActions", stack, None, {"Padding": pad(0, 20, 0, 0)})
    w.button("FinaleStartOverButton", "FinaleStartOverLabel", "Start Over", finale, min_w=280, size=22, style=TAB_STYLE, slot=gap(8))
    w.button("FinaleLearnMoreButton", "FinaleLearnMoreLabel", "Learn More", finale, primary=True, min_w=280, size=22, style=ACTION_STYLE)
    w.collapse("TimelinePanel")
    w.collapse("TargetMissingText", "PreviewRow", "TicketPhone", "FinaleActions")


def build_explore_map(w):
    """The levels, the places and the picked place's card. The map itself is the stadium in the 3D view."""
    root = w.add("Border", "SurfacePlaces", None, {"Background": ui.rounded(ui.WHITE, ui.EDGE, 1, radius=10),
                                                   "BrushColor": SURFACE, "Padding": pad(16)})
    stack = w.add("VerticalBox", "MapStackV", root)
    w.text("PlacesTitle", "Places", stack, 24, "Bold", slot=gap(4))
    w.text("PlacesScope", "Across all levels", stack, T_LABEL, color=INK_MUTED, slot=gap(16))
    w.text("ViewingText", "Viewing: Lower concourse", stack, 20, "Bold", slot=gap(8))
    w.button("LevelsToggleButton", "LevelsToggleLabel", "Change level", stack, min_w=280, min_h=64, size=20, style=TAB_STYLE, slot=gap(12))
    level_group = w.add("VerticalBox", "LevelsGroup", stack, None, gap(12))
    levels = w.add("UniformGridPanel", "LevelGrid", level_group, {"SlotPadding": pad(4), "MinDesiredSlotHeight": ui.TOGGLE_H})
    for name, label, r, c in (("UpperDeckTab", "Upper deck", 0, 0), ("SuitesTab", "Suites", 0, 1), ("ClubTab", "Club level", 1, 0),
                              ("LowerConcourseTab", "Lower concourse", 1, 1), ("FieldTab", "Field", 2, 0)):
        b = w.add("Button", name, levels, {"IsFocusable": False, "WidgetStyle": TAB_STYLE, "BackgroundColor": ui.WHITE},
                  {"Row": r, "Column": c, "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
        w.text(f"{name}Label", label, b, T_LABEL, "Bold", slot={"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Center"})

    grid = w.add("UniformGridPanel", "AmenityGrid", stack, {"SlotPadding": pad(4), "MinDesiredSlotHeight": ui.TOGGLE_H}, gap(16))
    for name, label_name, label, r, c in (("SensoryToggle", "SensoryLabel", "Sensory rooms", 0, 0), ("QuietToggle", "QuietLabel", "Quiet zones", 0, 1),
                                          ("GuestToggle", "GuestLabel", "Guest Services", 1, 0), ("RestroomToggle", "RestroomLabel", "Restrooms", 1, 1),
                                          ("FoodToggle", "FoodLabel", "Food", 2, 0)):
        toggle_cell(w, name, label_name, label, grid, {"Row": r, "Column": c, "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    hide = w.add("Button", "HideAllButton", grid, {"IsFocusable": False, "WidgetStyle": ui.ROW_STYLE, "BackgroundColor": ui.WHITE},
                 {"Row": 2, "Column": 1, "HorizontalAlignment": "HAlign_Fill", "VerticalAlignment": "VAlign_Fill"})
    w.text("HideAllLabel", "Hide all", hide, T_CONTROL, "Bold", slot={"HorizontalAlignment": "HAlign_Center", "VerticalAlignment": "VAlign_Center"})

    card = w.add("SizeBox", "PoiCard", stack, None, gap(16))
    surface = w.add("VerticalBox", "PoiContent", card, None, fill_slot())
    inner = w.add("VerticalBox", "PoiRow", surface)
    words = w.add("VerticalBox", "PoiWords", inner, None, gap(12))
    w.text("PoiTitle", "Sensory room", words, T_CONTROL, "Bold", slot=gap(4))
    w.text("PoiNote", "", words, T_LABEL, color=INK_MUTED, wrap=CONTROL_W - 96, line=ui.BODY_LINE)
    w.button("ShowMeButton", "ShowMeLabel", "Focus place", inner, primary=True, min_w=220, min_h=64, size=20, style=ACTION_STYLE)
    w.text("PickHint", "Pick a marker to see the place.", stack, T_LABEL, color=INK_MUTED, wrap=CONTROL_W - 48, line=ui.BODY_LINE)
    w.collapse("PoiCard", "LevelsGroup")


def build_callouts(w):
    """Free explore's places on the stadium. C++ adds a head and a name chip per place and draws the stems."""
    w.add("CanvasPanel", "CalloutCanvas", None, {"Visibility": "SelfHitTestInvisible"})


def build_timeline_arc(w):
    """Five compact chapter controls, in the dock rather than across the stadium."""
    card = w.add("Border", "SurfaceChapters", None, {"Background": ui.rounded(ui.WHITE, ui.EDGE, 1, radius=10),
                                                     "BrushColor": SURFACE, "Padding": pad(12)})
    w.add("UniformGridPanel", "ChapterMarks", card, {"SlotPadding": pad(4, 2)})


def build_stage_chrome(w):
    """One upper-right control dock; labels remain explicit, with 64-unit targets."""
    card = w.add("Border", "SurfaceControls", None, {"Background": ui.rounded(ui.WHITE, ui.EDGE, 1, radius=10),
                                                     "BrushColor": SURFACE, "Padding": pad(16)})
    stack = w.add("VerticalBox", "ControlsStack", card)
    modes = w.add("HorizontalBox", "ModeRow", stack, None, gap(8))
    for index, (name, label, text) in enumerate((("TourModeButton", "TourModeLabel", "Guided tour"),
                                                ("ExploreModeButton", "ExploreModeLabel", "Explore"),
                                                ("SettingsButton", "SettingsLabel", "Settings"))):
        w.button(name, label, text, modes, min_w=64, min_h=64, size=18, style=TAB_STYLE, wrap=True,
                 slot={"Size": {"value": 1, "sizeRule": "Fill"}, "Padding": pad(0 if index == 0 else 8, 0, 0, 0)})
    options = w.add("HorizontalBox", "OptionsRow", stack)
    w.add("HorizontalBox", "VoiceSlot", options, None, {"Size": {"value": 1, "sizeRule": "Fill"}})
    for name, label, text in (("CalmModeButton", "CalmModeLabel", "Calm Mode"), ("CrowdButton", "CrowdLabel", "Crowd")):
        box = w.add("SizeBox", f"{name}Size", options, {"bOverride_MinDesiredHeight": True, "MinDesiredHeight": 64},
                    {"Size": {"value": 1, "sizeRule": "Fill"}, "Padding": pad(8, 0, 0, 0)})
        toggle_cell(w, name, label, text, box, fill_slot())


def build_tour_nav(w):
    """Primary tour navigation directly below the chapter dock."""
    row = w.add("HorizontalBox", "NavRow")
    w.button("BackButton", "BackLabel", "Back", row, min_w=64, min_h=NAV_H, size=22, style=TAB_STYLE,
             slot={"Size": {"value": 1, "sizeRule": "Fill"}})
    w.button("NextButton", "NextLabel", "Next", row, primary=True, min_w=64, min_h=NAV_H, size=22, style=ACTION_STYLE,
             slot={"Size": {"value": 1, "sizeRule": "Fill"}, "Padding": pad(8, 0, 0, 0)})


def build_explore_nav(w):
    """Seat and bookmark actions remain discoverable without a screen-wide bottom rail."""
    stack = w.add("VerticalBox", "ExploreNavStack")
    row = w.add("HorizontalBox", "NavRow", stack, None, gap(8))
    w.button("MySeatButton", "MySeatLabel", "My seat", row, min_w=64, min_h=NAV_H, size=22, style=ACTION_STYLE, primary=True,
             slot={"Size": {"value": 1, "sizeRule": "Fill"}})
    w.button("ResumeTourButton", "ResumeTourLabel", "Resume tour", row, min_w=64, min_h=NAV_H, size=18, style=TAB_STYLE,
             slot={"Size": {"value": 1, "sizeRule": "Fill"}, "Padding": pad(8, 0, 0, 0)})
    ui.set_props(w.child("ResumeTourLabel"), {"AutoWrapText": True})
    strip = w.add("Border", "SurfaceFlyKeys", stack, {"BrushColor": SURFACE, "Padding": pad(12, 8)})
    help_text = w.text("FlyHelp", "WASD move  ·  E/Q up/down  ·  Drag look  ·  Shift faster", strip, T_LABEL, color=INK_MUTED,
                       line=1.1)
    ui.set_props(help_text, {"AutoWrapText": True})


def panel(w, asset, instance, parent, slot):
    # Nested widget archetypes retain old overrides even after their class defaults change.
    return w.add(f"{FOLDER}/{asset}.{asset}_C", instance, parent, ui.DEFAULTS.get(asset), slot)


def build_hud(w):
    """Scene-first corner composition. No permanent sidebar, central arc, or separated bottom navigation."""
    root = w.add("Overlay", "RootOverlay", None, {"Visibility": "SelfHitTestInvisible"})
    panel(w, "WBP_LFF_Callouts", "Callouts", root, fill_slot())
    info = w.add("SizeBox", "InformationSize", root, {"bOverride_WidthOverride": True, "WidthOverride": INFO_W,
                                                      "bOverride_MaxDesiredHeight": True, "MaxDesiredHeight": 968},
                 {"HorizontalAlignment": "HAlign_Left", "VerticalAlignment": "VAlign_Top", "Padding": pad(COLUMN_PAD, COLUMN_PAD, 0, 0)})
    info_stack = w.add("VerticalBox", "InformationStack", info)
    panel(w, "WBP_LFF_SidebarHeader", "SidebarHeader", info_stack, gap(12))
    narrative_limit = w.add("SizeBox", "NarrativeLimit", info_stack, {"bOverride_MaxDesiredHeight": True, "MaxDesiredHeight": 840})
    panel(w, "WBP_LFF_Narrative", "Narrative", narrative_limit, fill_slot())

    controls = w.add("SizeBox", "ControlSize", root, {"bOverride_WidthOverride": True, "WidthOverride": CONTROL_W,
                                                     "bOverride_MaxDesiredHeight": True, "MaxDesiredHeight": 968},
                     {"HorizontalAlignment": "HAlign_Right", "VerticalAlignment": "VAlign_Top", "Padding": pad(0, COLUMN_PAD, COLUMN_PAD, 0)})
    scroll = w.add("ScrollBox", "ControlsScroll", controls)
    stack = w.add("VerticalBox", "ControlStack", scroll)
    panel(w, "WBP_LFF_StageChrome", "StageChrome", stack, gap(12))
    panel(w, "WBP_LFF_TimelineArc", "TimelineArc", stack, gap(12))
    nav = w.add("WidgetSwitcher", "NavSwitcher", stack, {"ActiveWidgetIndex": 0, "Visibility": "SelfHitTestInvisible"}, gap(12))
    panel(w, "WBP_LFF_TourNav", "TourNav", nav, fill_slot())
    panel(w, "WBP_LFF_ExploreNav", "ExploreNav", nav, fill_slot())
    panel(w, "WBP_LFF_ExploreMap", "ExploreMap", stack, None)
    panel(w, "WBP_LFF_Welcome", "Welcome", root, fill_slot())
    panel(w, "WBP_LFF_Settings", "Settings", root, fill_slot())


WIDGETS = [
    ("WBP_LFF_CalmToggle", CALM_TOGGLE_CLASS, build_calm_toggle),
    ("WBP_LFF_Welcome", PANEL_CLASS, build_welcome),
    ("WBP_LFF_SidebarHeader", PANEL_CLASS, build_sidebar_header),
    ("WBP_LFF_Narrative", PANEL_CLASS, build_narrative),
    ("WBP_LFF_ExploreMap", MAP_CLASS, build_explore_map),
    ("WBP_LFF_TimelineArc", ARC_CLASS, build_timeline_arc),
    ("WBP_LFF_StageChrome", PANEL_CLASS, build_stage_chrome),
    ("WBP_LFF_TourNav", PANEL_CLASS, build_tour_nav),
    ("WBP_LFF_ExploreNav", PANEL_CLASS, build_explore_nav),
    ("WBP_LFF_Callouts", CALLOUT_CLASS, build_callouts),
    ("WBP_LFF_HUD", HUD_CLASS, build_hud),  # last: it instances the panels above
]


def build_material():
    for name in ("build_m_lff_level_highlight.py",):
        script = os.path.join(REPO, "Tools", "Editor", name)
        out = run_python(f"import runpy\nrunpy.run_path(r'''{script}''')\n")
        print(out.strip()[-600:])


def import_amenities():
    source = os.path.join(REPO, "SourceData", "Journey", "LFF_Amenities.json")
    out = run_python(f"""import unreal
# In UE 5.8 only the reimport subclass registers the .json format; it imports new tables too (import_lff_content.py).
factory = unreal.ReimportDataTableFactory()
settings = factory.get_editor_property("automated_import_settings")
settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
settings.set_editor_property("import_row_struct", unreal.load_object(None, "/Script/VirtualStadium.LFFAmenityMarker"))
task = unreal.AssetImportTask()
for prop, value in (("filename", r'''{source}'''), ("destination_path", "/Game/LFF/Data"), ("destination_name", "DT_LFF_Amenities"),
                    ("automated", True), ("save", True), ("replace_existing", True), ("factory", factory)):
    task.set_editor_property(prop, value)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
table = unreal.load_asset("/Game/LFF/Data/DT_LFF_Amenities")
if not table:
    raise RuntimeError("DT_LFF_Amenities import failed")
print("DT_LFF_Amenities rows:", [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)])
""")
    print(out.strip()[-600:])


def build_widgets(welcome_only=False):
    for name, parent_class, builder in WIDGETS:
        if welcome_only and name not in ("WBP_LFF_CalmToggle", "WBP_LFF_Welcome"):
            continue
        ui.prepare(name, parent_class, rebuild=True)
        builder(Builder(name))
        if name == "WBP_LFF_CalmToggle":
            build_calm_animation()
        names = ui.finish(name)
        print(f"built {FOLDER}/{name}: {len(names)} widgets, all {len(CONTRACT[name])} contract names present, compiled and saved")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--welcome-only", action="store_true", help="Rebuild the welcome screen, animated Calm Mode widget and textures.")
    args = parser.parse_args()
    if call("EditorToolset.EditorAppToolset", "IsPIERunning"):
        raise SystemExit("Stop Play-in-Editor before rebuilding UI assets.")
    import_welcome_textures()
    if not args.welcome_only:
        build_material()
        import_amenities()
    build_widgets(welcome_only=args.welcome_only)
