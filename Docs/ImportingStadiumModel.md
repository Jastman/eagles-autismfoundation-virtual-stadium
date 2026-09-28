# Importing a purchased stadium model

Use this workflow when you license or purchase a model that can legally be used in the project.

## Prepare the source files

1. Confirm the license covers Unreal Engine use and redistribution limits for your team.
2. Export or collect the model in FBX, glTF, or Datasmith format.
3. Gather textures into a single import folder before bringing them into Unreal.

## Import into Unreal

1. Create `/Game/Models/Stadium`.
2. Import the mesh into that folder.
3. If the source package includes multiple pieces, keep the folder split by area:
   - `/Game/Models/Stadium/Exterior`
   - `/Game/Models/Stadium/Concourse`
   - `/Game/Models/Stadium/Seating`
4. Fix scale and pivot issues before wiring gameplay logic.

## Align with Cesium geospatial context

1. Use `CesiumGeoreference` as the world anchor.
2. Place the stadium exterior at the correct real-world latitude/longitude.
3. If the model is not georeferenced, align it manually against Cesium terrain or imagery using a local origin near the venue.
4. Re-check the `WorldLocation` values in `DA_LFF_ScheduleStops` after alignment.

## Replace placeholders incrementally

1. Keep `StadiumTourCamera` and `StadiumScheduleDataAsset` as the gameplay source of truth.
2. Replace billboard markers with imported geometry one stop at a time.
3. Update only the affected stop's preview image, transform, and placeholder mesh.
4. Leave simple markers in place for any area that still lacks interior coverage.

## Recommended import checkpoints

- Exterior aligned with Cesium imagery
- Main entrance and security route verified
- Concourse navigation readable at player height
- Sensory room and restrooms clearly marked
- Seating stop framed from the correct section viewpoint

