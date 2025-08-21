#include "prefab/PathResolver.h"
#include <algorithm>

static inline bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false; for (size_t i = 0; i < a.size(); ++i) if (tolower(a[i]) != tolower(b[i])) return false; return true;
}

bool ResolvePath(const std::string& path, EntityID root, Scene& scene, ResolvedTarget& out) {
    out = {};
    if (path.rfind("@root/", 0) != 0) return false;
    std::string rest = path.substr(6); // after @root/
    size_t slash = rest.find('/');
    std::string compSel = (slash == std::string::npos ? rest : rest.substr(0, slash));
    std::string fields = (slash == std::string::npos ? std::string() : rest.substr(slash + 1));

    // Only Transform supported for now
    if (!ieq(compSel, "Transform") && !ieq(compSel, "Transform#0")) return false;
    out.Entity = root;
    out.ComponentType = "Transform";
    out.OrdinalIndex = 0;

    // Field chain: split by '.'
    out.FieldChain.clear();
    size_t pos = 0;
    while (pos < fields.size()) {
        size_t dot = fields.find('.', pos);
        std::string tok = fields.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
        if (!tok.empty()) out.FieldChain.push_back(tok);
        if (dot == std::string::npos) break; else pos = dot + 1;
    }
    return true;
}

bool ApplySet(Scene& scene, const ResolvedTarget& tgt, const nlohmann::json& value) {
    if (tgt.Entity == (EntityID)-1) return false;
    auto* d = scene.GetEntityData(tgt.Entity); if (!d) return false;
    if (tgt.ComponentType != "Transform") return false;
    // Supported: Position/Rotation/Scale and shorthand S
    if (tgt.FieldChain.empty()) return false;
    const std::string& f = tgt.FieldChain.back();
    auto readVec3 = [&](glm::vec3& v) -> bool {
        if (value.is_array() && value.size() == 3) { v.x = value[0]; v.y = value[1]; v.z = value[2]; return true; }
        return false;
    };
    bool ok = false;
    if (ieq(f, "Position")) ok = readVec3(d->Transform.Position);
    else if (ieq(f, "Rotation")) ok = readVec3(d->Transform.Rotation);
    else if (ieq(f, "Scale") || ieq(f, "S")) ok = readVec3(d->Transform.Scale);
    if (ok) { scene.MarkTransformDirty(tgt.Entity); }
    return ok;
}


