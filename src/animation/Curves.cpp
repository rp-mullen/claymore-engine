#include "animation/Curves.h"

#include <algorithm>

namespace cm {
namespace animation {

namespace {
template <typename KeyT>
static int findSegment(const std::vector<KeyT>& keys, float t, int hint, bool loop, float length)
{
    const int n = static_cast<int>(keys.size());
    if (n <= 1) return 0;
    if (loop && length > 0.0f) {
        // wrap t to [0, length)
        t = std::fmod(std::fmod(t, length) + length, length);
    }
    // fast-forward/backtrack from hint
    int i = std::clamp(hint, 0, n - 2);
    if (keys[i].t <= t && t <= keys[i+1].t) return i;
    // binary search
    int lo = 0, hi = n - 2;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (keys[mid].t <= t) {
            if (t <= keys[mid + 1].t) return mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return std::clamp(lo, 0, n - 2);
}

template <typename T>
static T lerp(const T& a, const T& b, float t) { return a * (1.0f - t) + b * t; }
}

float CurveFloat::Sample(float t, bool loop, float length) const
{
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1) return keys[0].v;
    int seg = findSegment(keys, t, cache.last, loop, length);
    cache.last = seg;
    const auto& k0 = keys[seg];
    const auto& k1 = keys[seg + 1];
    const float dt = (k1.t - k0.t);
    if (dt <= 1e-6f) return k1.v;
    const float a = (t - k0.t) / dt; // linear for now
    return k0.v * (1.0f - a) + k1.v * a;
}

glm::vec2 CurveVec2::Sample(float t, bool loop, float length) const
{
    if (keys.empty()) return glm::vec2(0.0f);
    if (keys.size() == 1) return keys[0].v;
    int seg = findSegment(keys, t, cache.last, loop, length);
    cache.last = seg;
    const auto& k0 = keys[seg];
    const auto& k1 = keys[seg + 1];
    const float dt = (k1.t - k0.t);
    if (dt <= 1e-6f) return k1.v;
    const float a = (t - k0.t) / dt;
    return lerp(k0.v, k1.v, a);
}

glm::vec3 CurveVec3::Sample(float t, bool loop, float length) const
{
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys[0].v;
    int seg = findSegment(keys, t, cache.last, loop, length);
    cache.last = seg;
    const auto& k0 = keys[seg];
    const auto& k1 = keys[seg + 1];
    const float dt = (k1.t - k0.t);
    if (dt <= 1e-6f) return k1.v;
    const float a = (t - k0.t) / dt;
    return lerp(k0.v, k1.v, a);
}

glm::quat CurveQuat::Sample(float t, bool loop, float length) const
{
    if (keys.empty()) return glm::quat(1,0,0,0);
    if (keys.size() == 1) return keys[0].v;
    int seg = findSegment(keys, t, cache.last, loop, length);
    cache.last = seg;
    const auto& k0 = keys[seg];
    const auto& k1 = keys[seg + 1];
    const float dt = (k1.t - k0.t);
    if (dt <= 1e-6f) return k1.v;
    const float a = (t - k0.t) / dt;
    return glm::slerp(k0.v, k1.v, a);
}

glm::vec4 CurveColor::Sample(float t, bool loop, float length) const
{
    if (keys.empty()) return glm::vec4(1.0f);
    if (keys.size() == 1) return keys[0].v;
    int seg = findSegment(keys, t, cache.last, loop, length);
    cache.last = seg;
    const auto& k0 = keys[seg];
    const auto& k1 = keys[seg + 1];
    const float dt = (k1.t - k0.t);
    if (dt <= 1e-6f) return k1.v;
    const float a = (t - k0.t) / dt;
    return lerp(k0.v, k1.v, a);
}

} // namespace animation
} // namespace cm



