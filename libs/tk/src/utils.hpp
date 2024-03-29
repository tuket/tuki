#pragma once

#include <span>
#include <vector>
#include <initializer_list>
#include <type_traits>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <filesystem>
#include <utility>
#include <unordered_map>

namespace tk
{

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef const char* CStr;
typedef const CStr ConstStr;
template <typename T> using CSpan = std::span<const T>;
typedef std::string_view StrView;
typedef std::filesystem::path Path;

struct IndRange { u32 first, count; };

struct ZStrView : public std::string_view {
    ZStrView() {}
    ZStrView(std::string_view s) = delete;
    ZStrView(const char* s) : std::string_view(s) {}
    ZStrView(const std::string& s) : std::string_view(s) {}

    operator const char* ()const { return data(); }
    operator ::std::string()const { return std::string(begin(), end()); }

    const char* c_str()const { return data(); }
};

struct AABB {
    glm::vec3 min = { 0, 0, 0 }, max = { 0, 0, 0 };

    glm::vec3 center()const { return 0.5f * (min + max); }
    glm::vec3 size()const { return max - min; }
};
inline AABB pointCloudToAABB(CSpan<glm::vec3> positions) {
    AABB b{};
    if (positions.size()) {
        b.min = b.max = positions[0];
        for (size_t i = 0; i < positions.size(); i++) {
            b.min = glm::min(b.min, positions[i]);
            b.max = glm::max(b.max, positions[i]);
        }
    }
    return b;
}

struct FpsCamera {
    glm::vec3 position = { 0, 0, 0 };
    float heading = 0, pitch = 0; // in radians

    void init_lookScene(const AABB& sceneAABB, glm::vec2 hFovXY);
    void move(glm::vec3 delta_XYZ);
    void rotate(float deltaScreenX, float deltaScreenY);
    glm::mat4 rotMtx()const;
    glm::mat4 mtx()const;
    glm::mat4 viewMtx()const;
};
struct OrbitCamera {
    glm::vec3 pivot = { 0, 0, 0 };
    float distance = 1;
    float heading = 0, pitch = 0; // in radians

    void zoom(float factor);
    void pan(glm::vec2 deltaScreenXY, bool lockedY = true);
    void rotate(float deltaScreenX, float deltaScreenY);
    glm::mat4 mtx()const;
    glm::mat4 viewMtx()const;
};
FpsCamera orbitToFps(const OrbitCamera& c);
OrbitCamera fpsToOrbit(const FpsCamera& c);

struct PerspectiveCamera {
    float hFovY = glm::radians(30.f);
    float near = 0.1f, far = 1000.f;

    glm::mat4 projMtx_vk(float aspectRatio)const;
};

inline glm::mat4 buildMtx(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) {
    glm::mat4 m = glm::toMat4(rotation);
    m[3] = glm::vec4(position, 1);
    m = glm::scale(m, scale);
    return m;
}

template <typename T>
std::span<u8> asBytesSpan(std::span<T> v) { return { (u8*)v.data(), v.size_bytes() }; }
template <typename T>
CSpan<u8> asBytesSpan(CSpan<T> v) { return { (const u8*)v.data(), v.size_bytes() }; }
template <typename T>
CSpan<u8> asBytesSpan(const T& v) { return { (const u8*)&v, sizeof(T)}; }
template <typename T, size_t N>
std::span<u8> asBytesSpan(T (&v)[N]) { return {(u8*)v, sizeof(T) * N}; }
template <typename T, size_t N>
CSpan<u8> asBytesSpan(const T (&v)[N]) { return { (const u8*)v, sizeof(T) * N }; }

template <typename Container, typename Element>
bool has(const Container& v, const Element& e) {
    for (const auto& x : v)
        if (x == e)
            return true;
    return false;
}

template <typename T, typename... Ts>
auto make_vector(const T& t, const Ts&... ts) {
    std::vector<std::remove_const_t<std::remove_reference_t<decltype(t[0])>>> res;
    const size_t size = std::size(t) + (std::size(ts) + ...);
    res.reserve(size);
    res.insert(res.end(), std::begin(t), std::end(t));
    (res.insert(res.end(), std::begin(ts), std::end(ts)), ...);
    return res;
}

struct DumbHash { u64 operator()(u64 x)const { return x; } };

bool loadTextFile(std::string& str, CStr path);

struct LoadedBinaryFile {
    size_t size = 0;
    u8* data = nullptr;
};
 LoadedBinaryFile loadBinaryFile(CStr path);

struct PathBag
{
    std::vector<std::string> paths;
    std::unordered_map<u64, u32, DumbHash> hashToEntry;

    u32 getEntry(std::string_view path)const;
    void addPath(std::string_view path, u32 entry);
    void deleteEntry(u32 entry);
};

template <typename Int, typename FirstVector, typename... Vectors>
Int acquireReusableEntry(Int& nextFreeEntry, FirstVector& firstVector, size_t firstVectorOffset, Vectors&... vectors)
{
    assert(firstVectorOffset + sizeof(Int) <= sizeof(FirstVector::value_type));
    if (nextFreeEntry != Int(-1)) {
        // there was a free entry
        const Int e = nextFreeEntry;
        u8* ptr = (u8*)&firstVector[e];
        ptr += firstVectorOffset;
        nextFreeEntry = *(Int*)ptr;
        return e;
    }

    const Int e = firstVector.size();
    firstVector.emplace_back();
    (vectors.emplace_back(), ...);
    return e;
}

template <typename Int, typename FirstVector, typename... Vectors>
void releaseReusableEntry(Int& nextFreeEntry, FirstVector& firstVector, size_t firstVectorOffset, u32 entryToRelase)
{
    assert(firstVectorOffset + sizeof(Int) <= sizeof(FirstVector::value_type));
    u8* ptr = (u8*)&firstVector[entryToRelase];
    ptr += firstVectorOffset;
    *(Int*)ptr = nextFreeEntry;
    nextFreeEntry = entryToRelase;
    // TODO: do some safety check in debug mode for detecting double-free
}

template <typename T, size_t offset = 0>
struct EntriesArray
{
    std::vector<T> entries;
    u32 nextFreeEntry = u32(-1);
    static_assert(sizeof(T) >= sizeof(u32));

    u32 alloc()
    {
        const u32 e = acquireReusableEntry(nextFreeEntry, entries, offset);
        return e;
    }

    void free(u32 id)
    {
        assert(id < entries.size());
        releaseReusableEntry(nextFreeEntry, entries, offset, id);
    }

    T& operator[](u32 id)
    {
        return entries[id];
    }
    const T& operator[](u32 id)const
    {
        return entries[id];
    }
};

// computes the next power of two, unless it's already a power of two
template <typename T>
static constexpr T nextPowerOf2(T x) noexcept
{
    x--;
    x |= x >> T(1);
    x |= x >> T(2);
    x |= x >> T(4);
    if constexpr (sizeof(T) >= 2)
        x |= x >> T(8);
    if constexpr (sizeof(T) >= 4)
        x |= x >> T(16);
    if constexpr (sizeof(T) >= 8)
        x |= x >> T(32);
    if constexpr (sizeof(T) >= 16)
        x |= x >> T(64);
    x++;
    return x;
}

template <typename F>
struct _Defer {
    F f;
    _Defer(F f) : f(f) {}
    ~_Defer() { f(); }
};

template <typename F>
_Defer<F> _defer_func(F f) {
    return _Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = tk::_defer_func([&](){code;})

#define DEFINE_ENUM_CLASS_LOGIC_OP_1(E, op) static inline E operator op(E a) { return E(op std::underlying_type_t<E>(a)); }; 
#define DEFINE_ENUM_CLASS_LOGIC_OP_2(E, op) static inline E operator op(E a, E b) { return E(std::underlying_type_t<E>(a) op std::underlying_type_t<E>(b)); }
#define DEFINE_ENUM_CLASS_LOGIC_OP_ASSIGN(E, op) static inline E& operator op(E& a, E b) { (std::underlying_type_t<E>&)(a) op std::underlying_type_t<E>(b); return a; }
#define DEFINE_ENUM_CLASS_LOGIC_OPS(E) \
	DEFINE_ENUM_CLASS_LOGIC_OP_1(E, ~) \
	DEFINE_ENUM_CLASS_LOGIC_OP_2(E, &) \
	DEFINE_ENUM_CLASS_LOGIC_OP_2(E, |) \
	DEFINE_ENUM_CLASS_LOGIC_OP_2(E, ^) \
    DEFINE_ENUM_CLASS_LOGIC_OP_ASSIGN(E, &=) \
    DEFINE_ENUM_CLASS_LOGIC_OP_ASSIGN(E, |=) \
    DEFINE_ENUM_CLASS_LOGIC_OP_ASSIGN(E, ^=)

}