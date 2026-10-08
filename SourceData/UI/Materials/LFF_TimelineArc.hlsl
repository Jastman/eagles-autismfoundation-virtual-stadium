// M_LFF_TimelineArc: Custom node "ArcHUD" (paste everything below the pin list into the node's Code field).
//
// Material: /Game/LFF/UI/Materials/M_LFF_TimelineArc
//   Material Domain  User Interface
//   Blend Mode       Translucent
//   Custom node      Output Type CMOT Float 4 (rgb = colour, a = opacity)
//   Wire             ArcHUD.rgb -> Final Color, ArcHUD.a -> Opacity (a ComponentMask R G B and a ComponentMask A)
//   No Time node and no texture: the arc is still unless C++ changes a parameter.
//
// Inputs (add them on the Custom node in this order, and feed each from a parameter of the same name;
// ULFFTimelineArc sets them all, FLFFArcShape::ApplyTo the first five):
//   UV                 TexCoord[0]                0-1 across and down the arc's Image in UMG
//   AspectRatio        Scalar  4.881 (820 / 168)  widget width / height: distances are in widget heights, so no oval
//   ArcCenterV         Scalar  0.36               V of the line at its lowest point (the middle)
//   CurveIntensity     Scalar  0.34               how far both ends rise above the middle, in widget heights; 0 = straight
//   ArcThickness       Scalar  0.024              line thickness in widget heights (4 px at the 168 px design height)
//   FadeBoundaryWidth  Scalar  0.12               width of the fade at each end, as a fraction of the width
//   BandAbove          Scalar  0.10               band extent above the line, in widget heights
//   BandBelow          Scalar  0.62               band extent below the line (the labels sit in here)
//   BandSoftness       Scalar  0.06               feather on the band's lower edge
//   BandOpacity        Scalar  0.94               1.0 in High contrast
//   BandColor          Vector3 SURFACE            linear (0.0021, 0.0232, 0.0284); black in High contrast
//   ProgressU          Scalar  0.50               U of the current node: the line is "done" up to here (-1 = none yet)
//   DoneColor          Vector3 INK                linear (0.905, 0.888, 0.838)
//   AheadColor         Vector3 INK_MUTED          linear (0.479, 0.558, 0.565)
//   AheadOpacity       Scalar  0.55               0.8 in High contrast
//
// The same maths, in C++, places the node buttons: FLFFArcShape (Source/VirtualStadium/Public/UI/LFFTimelineArc.h).
// Change one, change the other.

// 1. Circle through the arc. Half-width W and rise H give the radius: R = (W^2 + H^2) / 2H. The centre sits R above the
//    line's lowest point (far above the widget for a gentle curve), so the circle's lower edge is a smile.
float W = AspectRatio * 0.5;
float H = max(CurveIntensity, 0.001);
float R = (W * W + H * H) / (2.0 * H);

// 2. Aspect correction: measure in widget heights on both axes, so the circle stays round on a 4.9:1 widget.
float2 P = float2(UV.x * AspectRatio, UV.y);
float2 C = float2(W, ArcCenterV - R);

// 3. Signed distance to the line's centre: negative above it, positive below. Nearly flat means a straight line
//    (a huge radius would lose the line to float precision).
float S = (CurveIntensity < 0.001) ? (UV.y - ArcCenterV) : (length(P - C) - R);

// 4. One screen pixel in these units, so every edge is anti-aliased at any DPI.
float AA = max(fwidth(S), 1e-5);

// 5. The line.
float HalfT = ArcThickness * 0.5;
float Line = 1.0 - smoothstep(HalfT - AA, HalfT + AA, abs(S));

// 6. The band the labels sit on: crisp above the line, feathered at the bottom.
float Band = smoothstep(-BandAbove - AA, -BandAbove + AA, S) * (1.0 - smoothstep(BandBelow - BandSoftness, BandBelow, S));

// 7. Both ends fade into the scene over FadeBoundaryWidth: a plain left-to-right gradient mask, mirrored.
float Fade = max(FadeBoundaryWidth, 0.0001);
float Edge = smoothstep(0.0, Fade, UV.x) * smoothstep(0.0, Fade, 1.0 - UV.x);

// 8. Done up to the current node, quieter after it.
float AAu = max(fwidth(UV.x), 1e-5);
float T = smoothstep(ProgressU - AAu, ProgressU + AAu, UV.x);
float3 LineRGB = lerp(DoneColor, AheadColor, T);
float LineA = lerp(1.0, AheadOpacity, T);

// 9. Line over band, both faded at the ends.
float3 RGB = lerp(BandColor, LineRGB, Line);
float A = saturate(max(Band * BandOpacity, Line * LineA)) * Edge;
return float4(RGB, A);
