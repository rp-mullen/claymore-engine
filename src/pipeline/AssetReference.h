#pragma once
#include <string>
#include <cstdint>
#include <random>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

// GUID structure similar to Unity's asset reference system
struct ClaymoreGUID {
    uint64_t high;
    uint64_t low;
    
    ClaymoreGUID() : high(0), low(0) {}
    ClaymoreGUID(uint64_t h, uint64_t l) : high(h), low(l) {}
    
    bool operator==(const ClaymoreGUID& other) const {
        return high == other.high && low == other.low;
    }
    
    bool operator!=(const ClaymoreGUID& other) const {
        return !(*this == other);
    }
    
    bool operator<(const ClaymoreGUID& other) const {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }
    
    // Convert to string representation
    std::string ToString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') 
           << std::setw(16) << high 
           << std::setw(16) << low;
        return ss.str();
    }
    
    // Generate a new random GUID
    static ClaymoreGUID Generate() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        
        return ClaymoreGUID(dis(gen), dis(gen));
    }
    
    // Create GUID from string (for loading from serialized data)
    static ClaymoreGUID FromString(const std::string& str) {
        if (str.length() != 32) return ClaymoreGUID();
        
        uint64_t h = 0, l = 0;
        std::stringstream ss(str.substr(0, 16));
        ss >> std::hex >> h;
        ss.clear();
        ss.str(str.substr(16, 16));
        ss >> std::hex >> l;
        
        return ClaymoreGUID(h, l);
    }
};

// Asset reference structure similar to Unity's m_Mesh reference
struct AssetReference {
    ClaymoreGUID guid;
    int32_t fileID;  // Specific object within the asset file
    int32_t type;    // Asset type (3 = Mesh, 2 = Texture, etc.)
    
    AssetReference() : fileID(0), type(0) {}
    AssetReference(const ClaymoreGUID& g, int32_t fid = 0, int32_t t = 0) 
        : guid(g), fileID(fid), type(t) {}
    
    bool IsValid() const {
        return guid.high != 0 || guid.low != 0;
    }
    
    // For primitive types (cubes, spheres, etc.)
    static AssetReference CreatePrimitive(const std::string& primitiveType) {
        // Use a special GUID for primitives
        static const ClaymoreGUID PRIMITIVE_GUID = ClaymoreGUID::FromString("00000000000000000000000000000001");
        return AssetReference(PRIMITIVE_GUID, 0, 3); // type 3 = Mesh
    }
};

// JSON serialization for ClaymoreGUID and AssetReference
inline void to_json(nlohmann::json& j, const ClaymoreGUID& guid) {
    j = guid.ToString();
}

inline void from_json(const nlohmann::json& j, ClaymoreGUID& guid) {
    guid = ClaymoreGUID::FromString(j.get<std::string>());
}

inline void to_json(nlohmann::json& j, const AssetReference& ref) {
    j = {
        {"guid", ref.guid},
        {"fileID", ref.fileID},
        {"type", ref.type}
    };
}

inline void from_json(const nlohmann::json& j, AssetReference& ref) {
    j.at("guid").get_to(ref.guid);
    j.at("fileID").get_to(ref.fileID);
    j.at("type").get_to(ref.type);
}

// Hash function for ClaymoreGUID to work with std::unordered_map
namespace std {
    template<>
    struct hash<ClaymoreGUID> {
        std::size_t operator()(const ClaymoreGUID& guid) const {
            // Combine high and low parts for hashing
            return std::hash<uint64_t>{}(guid.high) ^ (std::hash<uint64_t>{}(guid.low) << 1);
        }
    };
} 