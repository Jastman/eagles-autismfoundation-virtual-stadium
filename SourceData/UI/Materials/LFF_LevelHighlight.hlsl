// M_LFF_LevelHighlight: Custom node "LevelHighlight" (Output Type CMOT Float 3), the whole graph of a post-process material.
//
// Material: /Game/LFF/UI/Materials/M_LFF_LevelHighlight (Tools/Editor/build_m_lff_level_highlight.py)
//   Material Domain     Post Process
//   Blendable Location  Scene Color After Tonemapping (so the tint is the colour you see)
//   Wire                LevelHighlight -> Emissive Color
// Free explore puts it on the camera when a level is picked (ULFFJourneySubsystem::ApplyLevelHighlight): the level's
// height band on the stadium shows through a semi-transparent kelly tint, and the rest of the stadium recedes a little.
// It works from depth alone, so it needs nothing from the stadium's own (Nanite) materials.
//
// Inputs:
//   SceneColor        SceneTexture PostProcessInput0, Color
//   WorldPos          Absolute World Position (the pixel's, from depth; the world origin is at the stadium, so floats hold it)
//   SiteOrigin        Vector  LFF_SiteRoot's location
//   SiteAxisX/Y/Z     Vector  its unit axes (+X across the field to the east stand, +Y to the south end, +Z up)
//   BandMin, BandMax  Scalar  the level's band, cm above the turf (ALFFGameMode::LevelViews)
//   FootprintX/Y      Scalar  the stadium's half-widths, cm: the city at the same height stays untouched
//   HighlightColor    Vector  KELLY, linear (converted to display values here)
//   HighlightOpacity  Scalar  0.35
//   DimOutside        Scalar  0.3: how much the rest of the stadium recedes

// The pixel in the site frame.
float3 D = WorldPos - SiteOrigin;
float3 L = float3(dot(D, SiteAxisX), dot(D, SiteAxisY), dot(D, SiteAxisZ));

// Inside the stadium's footprint (an ellipse), feathered over its last 8%. Sky pixels are far away, so they're outside.
float E = length(float2(L.x / max(FootprintX, 1.0), L.y / max(FootprintY, 1.0)));
float Inside = 1.0 - smoothstep(0.92, 1.0, E);

// The picked level's band, feathered over 40 cm at each end.
float Band = smoothstep(BandMin - 40.0, BandMin + 40.0, L.z) * (1.0 - smoothstep(BandMax - 40.0, BandMax + 40.0, L.z));
float InLevel = Inside * Band;
float OutLevel = Inside * (1.0 - Band);

float3 C = SceneColor.rgb;
// The rest of the stadium recedes: a little darker and greyer, so the level reads first.
float Grey = dot(C, float3(0.3, 0.59, 0.11));
C = lerp(C, lerp(float3(Grey, Grey, Grey), C, 0.5) * 0.7, DimOutside * OutLevel);
// The level shows through a semi-transparent tint, lifted a little so dark floors show it too. Static: nothing pulses.
float3 Tint = pow(saturate(HighlightColor), 1.0 / 2.2);
C = lerp(C, saturate(C * 0.6 + Tint * 0.9), HighlightOpacity * InLevel);
return C;
