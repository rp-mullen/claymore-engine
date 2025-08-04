# Asset Reference System

This document describes the Unity-like asset reference system implemented in Claymore Engine.

## Overview

The asset reference system allows scenes to reference assets (meshes, textures, materials) using GUIDs instead of file paths or names. This provides several benefits:

- **Reliability**: Assets are referenced by unique GUIDs, not fragile file paths
- **Performance**: Assets are cached and shared across scenes
- **Flexibility**: Assets can be moved or renamed without breaking references
- **Consistency**: Similar to Unity's asset reference system

## Architecture

### Core Components

1. **GUID**: 128-bit unique identifier for assets
2. **AssetReference**: Contains GUID, fileID, and asset type
3. **AssetLibrary**: Manages loaded assets and provides lookup functionality
4. **AssetMetadata**: Extended to include GUID and asset reference

### Asset Types

```cpp
enum class AssetType {
    Mesh = 3,
    Texture = 2,
    Material = 21,
    Shader = 48,
    Script = 115
};
```

## Usage

### Creating Asset References

```cpp
// For imported assets (FBX, textures, etc.)
AssetReference ref(guid, fileID, static_cast<int32_t>(AssetType::Mesh));

// For primitive meshes
AssetReference cubeRef = AssetReference::CreatePrimitive("Cube");
```

### Loading Assets

```cpp
// Load mesh from asset reference
std::shared_ptr<Mesh> mesh = AssetLibrary::Instance().LoadMesh(ref);

// Load material from asset reference
std::shared_ptr<Material> material = AssetLibrary::Instance().LoadMaterial(ref);
```

### Serialization

The system automatically serializes asset references in scene files:

```json
{
  "meshReference": {
    "guid": "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6",
    "fileID": 0,
    "type": 3
  }
}
```

## Primitive Meshes

Primitive meshes (Cube, Sphere, Plane) use a special GUID and are registered automatically:

```cpp
// Special GUID for primitives
static const GUID PRIMITIVE_GUID = GUID::FromString("00000000000000000000000000000001");

// Different fileIDs for different primitives
AssetReference cubeRef(PRIMITIVE_GUID, 0, 3);   // Cube
AssetReference sphereRef(PRIMITIVE_GUID, 1, 3);  // Sphere
AssetReference planeRef(PRIMITIVE_GUID, 2, 3);   // Plane
```

## Asset Pipeline Integration

When assets are imported through the AssetPipeline:

1. A new GUID is generated for the asset
2. AssetReference is created with the GUID
3. Asset is registered in AssetLibrary
4. Metadata is saved with GUID and reference

## Migration from Name-Based System

The system maintains backward compatibility:

1. **Serialization**: Both meshName and meshReference are saved
2. **Deserialization**: Asset reference is tried first, falls back to name-based system
3. **Runtime**: Both systems work simultaneously

## Benefits

### Scene Serialization
- Scenes no longer store mesh names or file paths
- Asset references are robust and portable
- Primitive meshes are reconstructed from type, not cached data

### Asset Management
- Assets are loaded once and shared across scenes
- AssetLibrary provides centralized asset management
- GUID-based lookup is fast and reliable

### Future Extensibility
- Easy to add new asset types
- Support for asset dependencies
- Asset streaming and LOD systems

## Example Scene JSON

```json
{
  "entities": [
    {
      "id": 1,
      "name": "Cube",
      "components": {
        "Transform": {
          "position": {"x": 0, "y": 0, "z": 0},
          "rotation": {"x": 0, "y": 0, "z": 0},
          "scale": {"x": 1, "y": 1, "z": 1}
        },
        "Mesh": {
          "meshName": "Cube",
          "meshReference": {
            "guid": "00000000000000000000000000000001",
            "fileID": 0,
            "type": 3
          }
        }
      }
    }
  ]
}
```

## Implementation Notes

- GUIDs are generated using std::mt19937_64 for randomness
- AssetLibrary uses std::unordered_map for O(1) lookups
- Primitive meshes are cached in StandardMeshManager
- Asset references are serialized as JSON for human readability 