# Serialization System and Enhanced Inspector

This document describes the new serialization system and enhanced inspector features that have been integrated into the engine.

## Features Overview

### 1. Scene Serialization System
- **JSON-based serialization** for scenes and prefabs
- **Complete entity serialization** including all components and scripts
- **Parent-child relationship preservation** during serialization/deserialization
- **Automatic file management** with proper directory creation

### 2. Prefab System
- **Reusable entity templates** that can be serialized and instantiated
- **Drag-and-drop prefab creation** from scene hierarchy to project panel
- **Component and script preservation** in prefabs
- **Easy prefab instantiation** in scenes

### 3. Enhanced Project Panel
- **Double-click scene loading**: Double-click any `.scene` file to load it as the active scene
- **Drag-to-create prefabs**: Drag entities from the scene hierarchy to the project panel to create prefabs
- **File type recognition**: Automatic detection of scene and prefab files
- **Real-time file tree updates** when new files are created

### 4. Unity-like Inspector Panel
- **Add Component button**: Easily add native and script components to entities
- **Component management**: Remove components with individual remove buttons
- **Script component support**: Full integration with managed script components
- **Reflection-based property editing**: Edit script properties directly in the inspector

### 5. Script Reflection System
- **Property type support**: int, float, bool, string, and Vector3 properties
- **Automatic UI generation**: Properties automatically appear in the inspector
- **Real-time editing**: Changes to script properties are applied immediately
- **Extensible architecture**: Easy to add new property types

## Usage Guide

### Loading Scenes
1. **Via Project Panel**: Double-click any `.scene` file in the project panel
2. **Via Menu**: Use File → Load Scene... (currently loads SampleScene.scene)
3. **Programmatically**: Use `Serializer::LoadSceneFromFile(path, scene)`

### Saving Scenes
1. **Via Menu**: Use File → Save Scene As... (currently saves to CurrentScene.scene)
2. **Programmatically**: Use `Serializer::SaveSceneToFile(scene, path)`

### Creating Prefabs
1. **Drag and Drop**: Drag any entity from the Scene Hierarchy panel to the Project Panel
2. **Automatic Naming**: Prefabs are automatically named (NewPrefab.prefab, NewPrefab1.prefab, etc.)
3. **Component Preservation**: All components and scripts are preserved in the prefab

### Adding Components
1. **Select Entity**: Click on an entity in the Scene Hierarchy
2. **Add Component**: Click the "Add Component" button at the bottom of the Inspector
3. **Choose Component**: Select from native components (Mesh, Light, Collider) or script components
4. **Script Components**: All registered managed scripts appear in the list

### Script Properties
1. **Automatic Detection**: Script properties are automatically detected via the reflection system
2. **Property Types**: Supports int, float, bool, string, and Vector3 properties
3. **Real-time Editing**: Changes are applied immediately to the script instance
4. **Visual Feedback**: Properties appear with appropriate UI controls (sliders, checkboxes, text fields, etc.)

## File Formats

### Scene File Format (.scene)
```json
{
    "version": "1.0",
    "entities": [
        {
            "id": 1,
            "name": "EntityName",
            "layer": 0,
            "tag": "TagName",
            "parent": -1,
            "children": [],
            "transform": { /* transform data */ },
            "mesh": { /* mesh component data */ },
            "light": { /* light component data */ },
            "collider": { /* collider component data */ },
            "scripts": [
                { "className": "ScriptName" }
            ]
        }
    ]
}
```

### Prefab File Format (.prefab)
```json
{
    "version": "1.0",
    "type": "prefab",
    "entity": {
        "name": "PrefabName",
        /* entity data structure same as scene entities */
    }
}
```

## Architecture

### Core Classes
- **`Serializer`**: Main serialization class with static methods for scenes and prefabs
- **`ScriptReflection`**: Manages script property registration and reflection
- **`ProjectPanel`**: Enhanced with scene loading and prefab creation
- **`InspectorPanel`**: Enhanced with component addition and script property editing
- **`ComponentDrawerRegistry`**: Extended to support all component types

### Integration Points
- **UILayer**: Initializes component drawers and script reflection
- **MenuBarPanel**: Provides scene save/load menu options
- **SceneHierarchyPanel**: Supports drag-and-drop for prefab creation
- **DotNetHost**: Integrates with managed script reflection (future enhancement)

## Sample Content

### Sample Scene
- **Location**: `assets/scenes/SampleScene.scene`
- **Contents**: Player entity with PlayerController script, Main Light, and Rotating Platform
- **Demonstrates**: Full scene serialization with multiple entity types and scripts

### Sample Prefab
- **Location**: `assets/prefabs/EnemyPrefab.prefab`
- **Contents**: Enemy entity with EnemyAI script and collider
- **Demonstrates**: Prefab creation and script component preservation

### Sample Script Properties
The system includes sample script reflection for demonstration:
- **PlayerController**: Speed, JumpHeight, CanDoubleJump properties
- **EnemyAI**: PatrolRadius, DetectionRange, PatrolPoints properties
- **TransformRotator**: RotationSpeed (Vector3), RotateInLocalSpace properties
- **UITextDisplay**: DisplayText, UpdateInterval properties

## Future Enhancements

1. **File Dialogs**: Proper file open/save dialogs instead of hardcoded paths
2. **Prefab Instantiation**: Drag prefabs from project panel to scene to instantiate
3. **Advanced Reflection**: Support for more complex property types (arrays, custom classes)
4. **Undo/Redo**: Integration with an undo system for property changes
5. **Property Validation**: Min/max ranges, validation rules for properties
6. **Asset References**: Proper serialization of mesh, texture, and material references
7. **Nested Prefabs**: Support for prefabs containing other prefabs
8. **Property Serialization**: Save script property values in scene/prefab files

## Technical Notes

- **JSON Library**: Uses nlohmann/json for serialization
- **Error Handling**: Comprehensive error handling with console logging
- **Memory Management**: Proper cleanup of dynamically allocated components
- **Performance**: Efficient serialization suitable for real-time editing
- **Extensibility**: Easy to add new component types and property types