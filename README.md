# claymore-engine
A 3D game engine developed in C++ with .NET 8.0 native interop; built on bgfx, assimp, and Jolt Physics

## Features
- **Cross-platform**: Runs on Windows, Linux.
- **Graphics**: Uses bgfx for rendering, supporting multiple backends.
- **Physics**: Integrates Jolt Physics for realistic simulations.
- **Asset Management**: Utilizes Assimp for importing various 3D model formats.
- **Scripting**: Supports C# scripting with .NET 8.0 native interop.
- **Modular Design**: Engine components are designed to be modular and extensible.
- **Open Source**: Licensed under the MIT License, allowing for free use and modification.

## Getting Started
### Prerequisites
- **C++ Compiler**: A modern C++ compiler (e.g., GCC, Clang, MSVC).
- **.NET SDK**: .NET 8.0 SDK for C# scripting.
- **Dependencies**: Ensure you have the following libraries installed:
  - bgfx
  - Jolt Physics
  - Assimp

## Example Script
```csharp
using ClaymoreEngine;
using System.Numerics;

public class TestScript : ScriptComponent
{
    public override void OnCreate()
    {
        
    }
    public override void Update(float dT)
    {
        transform.position += new Vector3(0, 0, 1) * dT; // Move forward
    }
}
```