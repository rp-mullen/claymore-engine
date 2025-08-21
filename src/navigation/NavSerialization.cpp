#include "navigation/NavSerialization.h"
#include "navigation/NavMesh.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

using namespace nav;
using namespace nav::io;

namespace {
    // Lightweight CRC32 (IEEE 802.3) implementation to avoid zlib dependency here
    static uint32_t crc32_table[256];
    static bool crc32_init = false;
    static void init_crc32() {
        if (crc32_init) return;
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            crc32_table[i] = c;
        }
        crc32_init = true;
    }
    static uint32_t crc32_buf(const uint8_t* data, size_t len) {
        init_crc32();
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }
}

// Layout:
// [magic u32][version u32]
// INFO chunk: 'INFO'[size u32]{ cell/bake defaults + counts + bounds }
// VERT chunk: 'VERT'[size u32]{ float3[] }
// POLY chunk: 'POLY'[size u32]{ u32 i0,i1,i2; u16 area; u32 flags }[]
// LINK chunk: 'LINK'[size u32]{ float3 a,b; float radius; u32 flags; u8 bidir }[]
// BVTX chunk: 'BVTX'[size u32]{ reserved for future BVH }
// HASH chunk: 'HASH'[size u32]{ u64 bakeHash }
// FOOTER: 'CRCC'[size u32=4]{ crc32 of all previous bytes }

struct ChunkHeader { uint32_t id; uint32_t size; };

static void write_u32(std::vector<uint8_t>& buf, uint32_t v){ size_t o = buf.size(); buf.resize(o+4); memcpy(buf.data()+o,&v,4);} 
static void write_u64(std::vector<uint8_t>& buf, uint64_t v){ size_t o = buf.size(); buf.resize(o+8); memcpy(buf.data()+o,&v,8);} 
static void write_f32(std::vector<uint8_t>& buf, float v){ size_t o = buf.size(); buf.resize(o+4); memcpy(buf.data()+o,&v,4);} 
static void write_vec3(std::vector<uint8_t>& buf, const glm::vec3& v){ write_f32(buf,v.x); write_f32(buf,v.y); write_f32(buf,v.z);} 

bool nav::io::WriteNavbin(const NavMeshRuntime& rt, uint64_t bakeHash, const std::string& filePath)
{
    std::vector<uint8_t> buf;
    write_u32(buf, NAVBIN_MAGIC);
    write_u32(buf, NAVBIN_VERSION);

    // INFO
    {
        ChunkHeader hdr{ 'OFNI', 0 };
        size_t at = buf.size(); write_u32(buf, hdr.id); write_u32(buf, hdr.size);
        // cell defaults unknown at runtime; write sentinel + counts + bounds
        write_f32(buf, 0.0f); // cellSize
        write_f32(buf, 0.0f); // cellHeight
        write_f32(buf, 0.0f); write_f32(buf, 0.0f); // agentRadius/Height
        write_f32(buf, 0.0f); write_f32(buf, 0.0f); // climb/slope
        // bounds
        write_vec3(buf, rt.m_Bounds.min); write_vec3(buf, rt.m_Bounds.max);
        // counts
        write_u32(buf, (uint32_t)rt.m_Vertices.size());
        write_u32(buf, (uint32_t)rt.m_Polys.size());
        write_u32(buf, (uint32_t)rt.m_Links.size());
        // finalize size
        uint32_t sz = (uint32_t)(buf.size() - at - 8);
        memcpy(buf.data() + at + 4, &sz, 4);
    }

    // VERT
    {
        ChunkHeader hdr{ 'TREV', 0 };
        size_t at = buf.size(); write_u32(buf, hdr.id); write_u32(buf, hdr.size);
        for (auto& v : rt.m_Vertices) write_vec3(buf, v);
        uint32_t sz = (uint32_t)(buf.size() - at - 8); memcpy(buf.data()+at+4,&sz,4);
    }

    // POLY
    {
        ChunkHeader hdr{ 'YLOP', 0 };
        size_t at = buf.size(); write_u32(buf, hdr.id); write_u32(buf, hdr.size);
        for (auto& p : rt.m_Polys) {
            write_u32(buf, p.i0); write_u32(buf, p.i1); write_u32(buf, p.i2);
            uint32_t af = (uint32_t)p.area | (p.flags << 16);
            write_u32(buf, af);
        }
        uint32_t sz = (uint32_t)(buf.size() - at - 8); memcpy(buf.data()+at+4,&sz,4);
    }

    // LINK
    {
        ChunkHeader hdr{ 'KNIL', 0 };
        size_t at = buf.size(); write_u32(buf, hdr.id); write_u32(buf, hdr.size);
        for (auto& l : rt.m_Links) {
            write_vec3(buf, l.a); write_vec3(buf, l.b);
            write_f32(buf, l.radius); write_u32(buf, l.flags);
            uint32_t bid = (uint32_t)l.bidir; write_u32(buf, bid);
        }
        uint32_t sz = (uint32_t)(buf.size() - at - 8); memcpy(buf.data()+at+4,&sz,4);
    }

    // HASH
    {
        ChunkHeader hdr{ 'HSAH', 8 };
        write_u32(buf, hdr.id); write_u32(buf, hdr.size); write_u64(buf, bakeHash);
    }

    // Footer CRC
    {
        ChunkHeader hdr{ 'CRCC', 4 };
        write_u32(buf, hdr.id); write_u32(buf, hdr.size);
        uint32_t crc = crc32_buf(buf.data(), buf.size()); write_u32(buf, crc);
    }

    std::ofstream f(filePath, std::ios::binary); if (!f) return false; f.write((const char*)buf.data(), buf.size()); return (bool)f;
}

bool nav::io::ReadNavbin(const std::string& filePath, std::shared_ptr<NavMeshRuntime>& out, uint64_t& outHash)
{
    std::ifstream f(filePath, std::ios::binary | std::ios::ate); if (!f) return false;
    size_t sz = (size_t)f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf(sz); f.read((char*)buf.data(), sz);
    if (sz < 8) return false;
    uint32_t magic = *(uint32_t*)&buf[0]; uint32_t ver = *(uint32_t*)&buf[4];
    if (magic != NAVBIN_MAGIC || ver != NAVBIN_VERSION) return false;
    // Verify CRC footer
    size_t off = sz; // parse backwards for footer
    if (sz < 8) return false;
    uint32_t crcStored = *(uint32_t*)&buf[sz - 4];
    uint32_t crcCalc = crc32_buf(buf.data(), (size_t)(sz - 8)); // exclude footer header+payload
    if (crcStored != crcCalc) return false;

    auto rt = std::make_shared<NavMeshRuntime>();
    outHash = 0;
    // Iterate chunks after header (8 bytes) until footer
    size_t p = 8;
    while (p + 8 <= sz) {
        uint32_t id = *(uint32_t*)&buf[p]; uint32_t csz = *(uint32_t*)&buf[p+4]; p += 8;
        if (id == 'CRCC') break;
        if (p + csz > sz) return false;
        const uint8_t* data = buf.data() + p;
        switch (id) {
            case 'OFNI': {
                size_t q = 0;
                q += 4*6; // skip unknowns
                glm::vec3 bmin = *(const glm::vec3*)(data + q); q += sizeof(glm::vec3);
                glm::vec3 bmax = *(const glm::vec3*)(data + q); q += sizeof(glm::vec3);
                rt->m_Bounds.min = bmin; rt->m_Bounds.max = bmax;
                // counts are not needed here; used to reserve if desired
                break; }
            case 'TREV': {
                size_t n = csz / (sizeof(float)*3);
                rt->m_Vertices.resize(n);
                memcpy(rt->m_Vertices.data(), data, csz);
                break; }
            case 'YLOP': {
                size_t stride = 16; // 3*4 + 4
                size_t n = csz / stride;
                rt->m_Polys.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    const uint8_t* rec = data + i * stride;
                    NavMeshRuntime::Poly poly{};
                    poly.i0 = *(const uint32_t*)(rec + 0);
                    poly.i1 = *(const uint32_t*)(rec + 4);
                    poly.i2 = *(const uint32_t*)(rec + 8);
                    uint32_t af = *(const uint32_t*)(rec + 12);
                    poly.area = (uint16_t)(af & 0xFFFF);
                    poly.flags = (af >> 16);
                    rt->m_Polys[i] = poly;
                }
                break; }
            case 'KNIL': {
                size_t stride = sizeof(float)*3*2 + 4 + 4 + 4;
                size_t n = csz / stride;
                rt->m_Links.resize(n);
                const uint8_t* r = data;
                for (size_t i = 0; i < n; ++i) {
                    OffMeshLink l{};
                    l.a = *(const glm::vec3*)r; r += sizeof(glm::vec3);
                    l.b = *(const glm::vec3*)r; r += sizeof(glm::vec3);
                    l.radius = *(const float*)r; r += 4;
                    l.flags = *(const uint32_t*)r; r += 4;
                    l.bidir = (uint8_t)*(const uint32_t*)r; r += 4;
                    rt->m_Links[i] = l;
                }
                break; }
            case 'HSAH': {
                outHash = *(const uint64_t*)data; break; }
            default: break;
        }
        p += csz;
    }
    rt->RebuildBVH();
    out = std::move(rt);
    return true;
}

bool nav::io::LoadNavMeshFromFile(const std::string& path, std::shared_ptr<NavMeshRuntime>& out, uint64_t& outHash)
{
    return ReadNavbin(path, out, outHash);
}


