# Multi-Light System for PBR Shaders

## Overview

The PBR shaders have been updated to support multiple lights (up to 4) including both directional and point lights. This system provides proper attenuation for point lights and supports different light types simultaneously.

## Changes Made

### Shader Updates

1. **Fragment Shaders** (`fs_pbr.sc` and `fs_pbr_skinned.sc`):
   - Replaced single light uniforms with arrays supporting up to 4 lights
   - Added `CalculatePBRLighting()` function to encapsulate PBR lighting calculations
   - Implemented proper light type handling (directional vs point)
   - Added distance-based attenuation for point lights
   - Added range checking for point lights

2. **New Uniform Structure**:
   ```glsl
   uniform vec4 u_lightColors[4];     // rgb = color, a = intensity
   uniform vec4 u_lightPositions[4];  // xyz = position/direction, w = light type (0=directional, 1=point)
   uniform vec4 u_lightParams[4];     // x = range, y = constant, z = linear, w = quadratic
   ```

### Renderer Updates

1. **Renderer.h**:
   - Updated `LightData` structure to include attenuation parameters
   - Replaced single light uniforms with multi-light uniform arrays

2. **Renderer.cpp**:
   - Updated uniform initialization for multi-light system
   - Enhanced light collection to include attenuation parameters
   - Completely rewrote `UploadLightsToShader()` to support multiple lights

## Light Types Supported

### Directional Lights
- Infinite light source (no attenuation)
- Uses rotation from TransformComponent for direction
- Parameters: color, intensity, direction

### Point Lights
- Finite light source with distance-based attenuation
- Uses position from TransformComponent
- Parameters: color, intensity, position, range, attenuation coefficients

## Default Parameters

### Point Light Defaults
- Range: 50.0 units
- Constant: 1.0
- Linear: 0.09
- Quadratic: 0.032

These provide a realistic attenuation curve that works well for most scenes.

## Usage

### Creating Lights in Code
```cpp
// Create a directional light
Entity dirLight = scene.CreateLight("Main Light", LightType::Directional, 
                                   glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);

// Create a point light
Entity pointLight = scene.CreateLight("Point Light", LightType::Point,
                                     glm::vec3(1.0f, 0.5f, 0.2f), 2.0f);
```

### Scene File Format
```json
{
    "light": {
        "type": 0,  // 0 = directional, 1 = point
        "color": {"x": 1.0, "y": 1.0, "z": 1.0},
        "intensity": 1.0
    }
}
```

## Performance Considerations

- Maximum of 4 lights per shader pass
- Lights are processed in order of appearance in the scene
- Disabled lights (beyond the 4-light limit) are set to zero values
- Point lights include range checking to skip processing for distant objects

## Backward Compatibility

The system maintains backward compatibility with existing single-light scenes. The renderer automatically handles the conversion from the old single-light format to the new multi-light format.

## Testing

The updated sample scene (`assets/scenes/SampleScene.scene`) now includes:
1. Main directional light (white, intensity 1.0)
2. Point light (orange, intensity 2.0)
3. Secondary directional light (blue, intensity 0.8)

This provides a good test case for the multi-light system with different light types and colors. 