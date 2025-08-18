#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// Minimal bgfx-style shader importer for Claymore unified .shader assets
// Parses source-first files with tiny pragmas, generates varyings and stage temps,
// invokes tools/shaderc.exe, and emits meta JSON used by the renderer and inspector.

namespace cm {

struct ShaderParamDesc {
    std::string name;     // e.g., u_albedoColor
    std::string type;     // float, vec2, vec3, vec4, int
    std::string uiHint;   // Color, Range(0,1), etc. (raw decorator)
    std::string defaultValue; // e.g., "1,1,1,1" or "0.5"
};

struct ShaderSamplerDesc {
    std::string name;     // e.g., s_albedo
    int slot = -1;        // numeric slot index
    std::string tag;      // logical slot tag, e.g., albedo, normal
    bool optional = false;
};

struct ShaderMeta {
    std::string name;                   // display/base name
    std::string baseName;               // filename stem fallback if name missing
    std::unordered_map<std::string, std::string> renderState; // key->value
    std::vector<std::string> attributes;   // POSITION, NORMAL, TEXCOORD0, ...
    std::vector<ShaderParamDesc> params;   // uniform scalars/vecs
    std::vector<ShaderSamplerDesc> samplers; // sampler bindings
    bool skinned = false;                // true if skinning requested
};

struct ParsedShader {
    std::string originalPath;  // project-relative path
    std::string name;
    std::string header;        // pre-stage text (kept for error mapping)
    std::string paramsBlock;   // raw @params block text
    std::string vertexSource;  // source inside // @vertex ...
    std::string fragmentSource;// source inside // @fragment ...
    bool hasVertex = false;
    bool hasFragment = false;
    bool skinnedOn = false;
    bool skinnedAuto = false;
    std::unordered_map<std::string, std::string> renderState;
    std::vector<std::string> attributes;
    std::vector<ShaderParamDesc> params;
    std::vector<ShaderSamplerDesc> samplers;
};

struct VaryingDecl { std::string name; std::string type; }; // e.g., v_normal vec3

struct ShaderImporterContext {
    std::string projectRoot;        // absolute path to project root
    std::string toolsDir;           // absolute path to tools (for shaderc)
    std::string shadersOutRoot;     // absolute path to shaders output root (shaders)
    std::string platform;           // e.g., windows/opengl/vulkan
};

class ShaderImporter {
public:
    static bool ImportShader(const std::string& path, const ShaderImporterContext& ctx, ShaderMeta& outMeta, std::string& outError);
    // Lightweight parse without compilation: fill out meta from source pragmas/params
    static bool ExtractMetaFromSource(const std::string& path, ShaderMeta& outMeta, std::string& outError);

private:
    static bool ReadFileText(const std::string& absPath, std::string& out);
    static bool Parse(const std::string& src, const std::string& path, ParsedShader& out, std::string& err);
    static void ParseParamsBlock(ParsedShader& ps);
    static void InferVaryings(const ParsedShader& ps, std::vector<VaryingDecl>& out);
    static void InferAttributesIfMissing(const ParsedShader& ps, std::vector<std::string>& attrs);
    static std::string GenerateVaryingDef(const std::vector<VaryingDecl>& varyings);
    static std::string EmitVertexSource(const ParsedShader& ps, const std::string& varyingDef, bool skinned);
    static std::string EmitFragmentSource(const ParsedShader& ps, const std::string& varyingDef);
    static bool WriteTextFile(const std::string& absPath, const std::string& text);
    static bool RunShaderc(const ShaderImporterContext& ctx, const std::string& inPath, const std::string& outBin, const char* type, std::string& err);
    static bool WriteMetaJson(const ShaderMeta& meta, const std::string& metaPath, std::string& err);
};

} // namespace cm


