#include "ShaderImporter.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;
namespace cm {

static std::string NormalizeSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

bool ShaderImporter::ReadFileText(const std::string& absPath, std::string& out) {
    std::ifstream in(absPath, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    return s.substr(a, b - a + 1);
}

void ShaderImporter::ParseParamsBlock(ParsedShader& ps) {
    // Extract lines between // @params and // @endparams in header
    const std::string& src = ps.header;
    size_t i1 = src.find("// @params");
    size_t i2 = src.find("// @endparams");
    if (i1 == std::string::npos || i2 == std::string::npos || i2 <= i1) return;
    std::string block = src.substr(i1, i2 - i1);
    ps.paramsBlock = block;
    std::istringstream iss(block);
    std::string line;
    std::regex samplerRe(R"(^\s*SAMPLER2D\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([0-9]+)\s*\)\s*;\s*(?://\s*\[(.*)\])?.*$)");
    std::regex uniformRe(R"(^\s*uniform\s+([A-Za-z0-9_]+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;\s*(?://\s*\[(.*)\])?.*$)");
    std::smatch m;
    while (std::getline(iss, line)) {
        if (std::regex_match(line, m, samplerRe)) {
            ShaderSamplerDesc s; s.name = m[1]; s.slot = std::stoi(m[2]);
            std::string deco = Trim(m[3]);
            if (!deco.empty()) {
                // parse tag=albedo optional
                if (deco.find("slot=") != std::string::npos) {
                    size_t p = deco.find("slot="); size_t e = deco.find_first_of(", ]", p+5);
                    s.tag = Trim(deco.substr(p+5, e-(p+5)));
                }
                if (deco.find("optional") != std::string::npos) s.optional = true;
            }
            ps.samplers.push_back(s);
        } else if (std::regex_match(line, m, uniformRe)) {
            ShaderParamDesc p; p.type = m[1]; p.name = m[2]; p.uiHint = Trim(m[3]);
            // extract default=...
            if (!p.uiHint.empty()) {
                size_t d = p.uiHint.find("default=");
                if (d != std::string::npos) {
                    size_t e = p.uiHint.find_first_of("]", d+8);
                    p.defaultValue = Trim(p.uiHint.substr(d+8, e-(d+8)));
                }
            }
            ps.params.push_back(p);
        }
    }
}

bool ShaderImporter::Parse(const std::string& src, const std::string& path, ParsedShader& out, std::string& err) {
    out.originalPath = path;
    std::string s = src;
    // Split header and stages by markers
    size_t v = s.find("// @vertex");
    size_t f = s.find("// @fragment");
    size_t first = std::min(v == std::string::npos ? s.size() : v, f == std::string::npos ? s.size() : f);
    out.header = s.substr(0, first);
    // Name
    {
        std::regex nameRe(R"(//\s*Shader:\s*(.*)$)");
        std::smatch m; std::istringstream iss(out.header); std::string line;
        while (std::getline(iss, line)) {
            if (std::regex_match(line, m, nameRe)) { out.name = Trim(m[1]); break; }
        }
    }
    // Render state
    {
        std::regex rsRe(R"(^\s*#pragma\s+clay\s+render_state\s+(.*)$)");
        std::smatch m; std::istringstream iss(out.header); std::string line;
        while (std::getline(iss, line)) {
            if (std::regex_match(line, m, rsRe)) {
                std::string kvs = Trim(m[1]);
                std::istringstream ss(kvs); std::string kv;
                while (std::getline(ss, kv, ',')) {
                    size_t eq = kv.find('=');
                    if (eq != std::string::npos) {
                        std::string k = Trim(kv.substr(0, eq));
                        std::string v = Trim(kv.substr(eq+1));
                        out.renderState[k] = v;
                    }
                }
            }
        }
    }
    // Attributes
    {
        std::regex attrRe(R"(^\s*#pragma\s+clay\s+attributes\s+(.+)$)");
        std::smatch m; std::istringstream iss(out.header); std::string line;
        while (std::getline(iss, line)) {
            if (std::regex_match(line, m, attrRe)) {
                std::string list = Trim(m[1]);
                std::istringstream ss(list); std::string tok;
                while (std::getline(ss, tok, ',')) out.attributes.push_back(Trim(tok));
            }
        }
    }
    // Skinning
    {
        std::regex skinRe(R"(^\s*#pragma\s+clay\s+skinned\s*(.*)$)");
        std::smatch m; std::istringstream iss(out.header); std::string line;
        while (std::getline(iss, line)) {
            if (std::regex_match(line, m, skinRe)) {
                std::string val = Trim(m[1]);
                if (val.find("on") != std::string::npos) out.skinnedOn = true;
                else if (val.find("auto") != std::string::npos) out.skinnedAuto = true;
            }
        }
    }
    // Params block
    ParseParamsBlock(out);
    // Stages
    if (v != std::string::npos) {
        size_t vend = (f != std::string::npos && f > v) ? f : s.size();
        out.vertexSource = s.substr(v + std::string("// @vertex").size(), vend - (v + std::string("// @vertex").size()));
        out.hasVertex = true;
    }
    if (f != std::string::npos) {
        size_t fend = s.size();
        out.fragmentSource = s.substr(f + std::string("// @fragment").size(), fend - (f + std::string("// @fragment").size()));
        out.hasFragment = true;
    }
    if (!out.hasVertex || !out.hasFragment) { err = "Missing // @vertex or // @fragment block"; return false; }
    return true;
}

void ShaderImporter::InferVaryings(const ParsedShader& ps, std::vector<VaryingDecl>& out) {
    // Very simple heuristic: find v_* in VS and FS and assume vec2/vec3/vec4 based on suffix hints
    auto collect = [&](const std::string& src, std::unordered_map<std::string, std::string>& map){
        std::regex vname(R"(v_([A-Za-z0-9_]+))");
        std::sregex_iterator it(src.begin(), src.end(), vname), end;
        for (; it != end; ++it) {
            std::string n = "v_" + (*it)[1].str();
            if (map.find(n) == map.end()) map[n] = "vec3"; // default vec3
        }
    };
    std::unordered_map<std::string, std::string> names;
    collect(ps.vertexSource, names);
    collect(ps.fragmentSource, names);
    out.clear(); out.reserve(names.size());
    for (auto& kv : names) out.push_back({kv.first, kv.second});
}

void ShaderImporter::InferAttributesIfMissing(const ParsedShader& ps, std::vector<std::string>& attrs) {
    if (!ps.attributes.empty()) { attrs = ps.attributes; return; }
    std::unordered_map<std::string, bool> set;
    if (ps.vertexSource.find("a_position") != std::string::npos) set["POSITION"] = true;
    if (ps.vertexSource.find("a_normal") != std::string::npos) set["NORMAL"] = true;
    if (ps.vertexSource.find("a_tangent") != std::string::npos) set["TANGENT"] = true;
    if (ps.vertexSource.find("a_texcoord0") != std::string::npos || ps.vertexSource.find("a_texcoord") != std::string::npos) set["TEXCOORD0"] = true;
    for (auto& kv : set) attrs.push_back(kv.first);
}

std::string ShaderImporter::GenerateVaryingDef(const std::vector<VaryingDecl>& varyings) {
    std::string out;
    out += "// Auto-generated by ShaderImporter\n";
    out += "#ifndef __AUTO_VARYING_DEF__\n#define __AUTO_VARYING_DEF__\n";
    for (const auto& v : varyings) {
        out += "VARYING " + v.type + " " + v.name + ";\n";
    }
    out += "#endif\n";
    return out;
}

static std::string CommonPrologue(bool skinned) {
    std::string p;
    p += "#include <bgfx_shader.sh>\n";
    if (skinned) p += "#define CLAY_SKINNED 1\n";
    return p;
}

std::string ShaderImporter::EmitVertexSource(const ParsedShader& ps, const std::string& varyingDef, bool skinned) {
    std::string out;
    out += CommonPrologue(skinned);
    out += "\n";
    out += varyingDef;
    out += "\n";
    if (skinned) {
        out += "#include \"shaders/imgui/varying.def.sc\"\n"; // harmless include for layout macro compat
        out += "#include \"shaders/engine/skinning.sc\"\n"; // engine helpers
    }
    out += ps.vertexSource;
    return out;
}

std::string ShaderImporter::EmitFragmentSource(const ParsedShader& ps, const std::string& varyingDef) {
    std::string out;
    out += CommonPrologue(false);
    out += "\n";
    out += varyingDef;
    out += "\n";
    out += ps.fragmentSource;
    return out;
}

bool ShaderImporter::WriteTextFile(const std::string& absPath, const std::string& text) {
    fs::create_directories(fs::path(absPath).parent_path());
    std::ofstream out(absPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), (std::streamsize)text.size());
    return true;
}

bool ShaderImporter::RunShaderc(const ShaderImporterContext& ctx, const std::string& inPath, const std::string& outBin, const char* type, std::string& err) {
    std::string profile = "s_5_0"; // Windows default
    fs::path bgfxInc = ctx.projectRoot;
    for (int i = 0; i < 10 && !fs::exists(bgfxInc / "external/bgfx/src/bgfx_shader.sh"); ++i) bgfxInc = bgfxInc.parent_path();
    bgfxInc /= "external/bgfx/src";
    std::string cmd = "\"" + NormalizeSlashes((fs::path(ctx.toolsDir) / "shaderc.exe").string()) + "\"";
    cmd += " -f " + NormalizeSlashes(inPath);
    cmd += " -o " + NormalizeSlashes(outBin);
    cmd += std::string(" --type ") + type;
    cmd += " --platform windows";
    cmd += " --profile " + profile;
    cmd += " -i shaders"; // project shaders include
    cmd += " -i " + NormalizeSlashes(bgfxInc.string());
    std::cout << "[ShaderImporter] shaderc: " << cmd << std::endl;
    int rc = system(cmd.c_str());
    if (rc != 0) { err = "shaderc failed for " + inPath; return false; }
    return true;
}

bool ShaderImporter::WriteMetaJson(const ShaderMeta& meta, const std::string& metaPath, std::string& err) {
    json j;
    j["name"] = meta.name.empty() ? meta.baseName : meta.name;
    j["skinned"] = meta.skinned;
    j["renderState"] = meta.renderState;
    j["attributes"] = meta.attributes;
    j["params"] = json::array();
    for (const auto& p : meta.params) {
        json pj; pj["name"] = p.name; pj["type"] = p.type; pj["ui"] = p.uiHint; pj["default"] = p.defaultValue; j["params"].push_back(pj);
    }
    j["samplers"] = json::array();
    for (const auto& s : meta.samplers) {
        json sj; sj["name"] = s.name; sj["slot"] = s.slot; sj["tag"] = s.tag; sj["optional"] = s.optional; j["samplers"].push_back(sj);
    }
    fs::create_directories(fs::path(metaPath).parent_path());
    std::ofstream out(metaPath);
    if (!out) { err = "Failed to write meta: " + metaPath; return false; }
    out << j.dump(4);
    return true;
}

bool ShaderImporter::ImportShader(const std::string& path, const ShaderImporterContext& ctx, ShaderMeta& outMeta, std::string& outError) {
    // Read source
    std::string abs = path;
    std::string src;
    if (!ReadFileText(abs, src)) { outError = "Failed to read: " + path; return false; }

    // Parse
    ParsedShader ps;
    if (!Parse(src, path, ps, outError)) return false;
    // Infer name/base
    fs::path p(path);
    outMeta.baseName = p.stem().string();
    outMeta.name = ps.name;
    outMeta.renderState = ps.renderState;
    outMeta.params = ps.params;
    outMeta.samplers = ps.samplers;

    // Skinning decision
    bool needsSkin = ps.skinnedOn;
    if (!needsSkin && ps.skinnedAuto) {
        needsSkin = (ps.vertexSource.find("a_indices") != std::string::npos) || (ps.vertexSource.find("a_weights") != std::string::npos);
    }
    outMeta.skinned = needsSkin;

    // Varyings & attributes
    std::vector<VaryingDecl> varyings;
    InferVaryings(ps, varyings);
    InferAttributesIfMissing(ps, outMeta.attributes);
    std::string varyingDef = GenerateVaryingDef(varyings);

    // Emit temps
    fs::path tmpDir = fs::path(ctx.shadersOutRoot) / "cache" / "shaders" / "tmp" / outMeta.baseName;
    fs::path varyingPath = tmpDir / (outMeta.baseName + ".varying.def.sc");
    fs::path vsPath = tmpDir / (outMeta.baseName + ".vs.sc");
    fs::path fsPath = tmpDir / (outMeta.baseName + ".fs.sc");
    if (!WriteTextFile(varyingPath.string(), varyingDef)) { outError = "Failed to write varying.def"; return false; }
    std::string vs = EmitVertexSource(ps, varyingDef, needsSkin);
    std::string fs = EmitFragmentSource(ps, varyingDef);
    if (!WriteTextFile(vsPath.string(), vs) || !WriteTextFile(fsPath.string(), fs)) { outError = "Failed to write stage sources"; return false; }

    // Compile bins
    fs::path outDir = fs::path("shaders") / "compiled" / ctx.platform;
    fs::path vsBin = outDir / (outMeta.baseName + ".vs.bin");
    fs::path fsBin = outDir / (outMeta.baseName + ".fs.bin");
    std::string err;
    if (!RunShaderc(ctx, vsPath.string(), vsBin.string(), "vertex", err)) { outError = err; return false; }
    if (!RunShaderc(ctx, fsPath.string(), fsBin.string(), "fragment", err)) { outError = err; return false; }

    // Write meta JSON
    fs::path metaPath = fs::path("shaders") / "meta" / (outMeta.baseName + ".json");
    if (!WriteMetaJson(outMeta, metaPath.string(), outError)) return false;

    std::cout << "[ShaderImporter] Imported .shader: " << path << std::endl;
    return true;
}

bool ShaderImporter::ExtractMetaFromSource(const std::string& path, ShaderMeta& outMeta, std::string& outError) {
    std::string src;
    if (!ReadFileText(path, src)) { outError = "Failed to read: " + path; return false; }
    ParsedShader ps;
    if (!Parse(src, path, ps, outError)) return false;
    outMeta.baseName = fs::path(path).stem().string();
    outMeta.name = ps.name;
    outMeta.renderState = ps.renderState;
    outMeta.params = ps.params;
    outMeta.samplers = ps.samplers;
    InferAttributesIfMissing(ps, outMeta.attributes);
    return true;
}

} // namespace cm


