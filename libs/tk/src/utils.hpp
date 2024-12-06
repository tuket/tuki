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
    ZStrView(const char* s, size_t len) : std::string_view(s, len) {}
    ZStrView(const std::string& s) : std::string_view(s) {}

    operator const char* ()const { return data(); }
    //operator ::std::string()const { return std::string(begin(), end()); }

    const char* c_str()const { return data(); }
};

struct StackTmpAllocator
{
    typedef u32 Size;
    std::unique_ptr<u8[]> data;
    Size capacity = 0;
    Size top = -1;

    StackTmpAllocator() {}
    StackTmpAllocator(Size capacity) : data(new u8[capacity]), capacity(capacity), top(capacity) {}

    template <typename T>
    struct AutoDelete
    {
        StackTmpAllocator* stackAllocator;
        T* ptr = nullptr;

        AutoDelete() {}
        AutoDelete(StackTmpAllocator& a, u8* ptr) : stackAllocator(&a), ptr((T*)ptr) {}
        AutoDelete(const AutoDelete&) = delete;

        ~AutoDelete()
        {
            if (!ptr)
                return;

            auto& A = *stackAllocator;
            u8* aPtr = (u8*)A.data.get();
            assert(Size((u8*)ptr - aPtr) == A.top + sizeof(Size));
            const u32 frameSize = *(Size*)(aPtr + A.top);
            A.top += frameSize + sizeof(Size);
        }
    };

    template <typename T>
    static bool _allocDefaultInitFn(T* data, Size len) { return true; }

    template <typename T, typename InitFn = decltype(_allocDefaultInitFn<T>)>
    [[nodiscard]]
    AutoDelete<T> alloc(Size len, InitFn initFn = _allocDefaultInitFn<T>)
    {
        const size_t sizeT = len * sizeof(T);
        const size_t size2 = sizeT + sizeof(Size);
        T* ptr = nullptr;
        if (top == capacity) {
            if (size2 > capacity) {
                data = std::make_unique<u8[]>(size2);
                capacity = size2;
            }
        }
        else {
            if (top < size2)
                return {};
        }
        top -= size2;
        *(Size*)(data.get() + top) = sizeT;
        auto dataStart = data.get() + top + sizeof(Size);
        if (initFn((T*)dataStart, len))
            return AutoDelete<T>(*this, dataStart);
        else {
            AutoDelete<T> _autoDel(*this, dataStart);
            return {};
        }
    }
};

template <typename T>
using StackTmpAllocation = StackTmpAllocator::AutoDelete<T>;

void initStackTmpAllocator(size_t capacity);
StackTmpAllocator& getStackTmpAllocator();

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

template <typename... Ts>
auto make_refs_tuple(Ts&... v) { return std::make_tuple(std::ref(v)...); }

template <typename... Ts>
auto make_crefs_tuple(Ts&... v) { return std::make_tuple(std::cref(v)...); }

u64 hash(std::string_view s);
struct DumbHash { u64 operator()(u64 x)const { return x; } };

struct AnyStringHasher { // https://stackoverflow.com/a/71258936/1754322
    using is_transparent = void;
    size_t operator()(const char* s)const { return hash(std::string_view(s)); }
    size_t operator()(std::string_view s)const { return hash(s); }
    size_t operator()(ZStrView s)const { return hash(s); }
    size_t operator()(const std::string& s)const { return hash(s); }
};

template <typename T>
using TStrMap = std::unordered_map<std::string, T, AnyStringHasher, std::equal_to<>>;

void getWorkingDir(u32 bufferSize, char* buffer);

bool loadTextFile(std::string& str, CStr path);

struct LoadedBinaryFile {
    size_t size = 0;
    u8* data = nullptr;
};
LoadedBinaryFile loadBinaryFile(CStr path);

struct LoadedBinaryFile_tmp {
    size_t size = 0;
    StackTmpAllocation<u8> alloc;
};
LoadedBinaryFile_tmp loadBinaryFile_tmp(CStr path);

struct SaveFileResult {
    enum Code : u8 {
        ok,
        cant_open,
        cant_write
    };
    Code code;

    SaveFileResult(Code c = ok) : code(c) {}
    operator bool()const { return code == ok; }
    bool operator == (SaveFileResult::Code c)const { return code == c; }
 };
 SaveFileResult saveBinaryFile(CSpan<u8> data, ZStrView path);

 // This is basically a dictionary of string -> Int
 // It's used for caching loaded files
 // Why not just use std::unordered_map<std::string, Int>?
 // 1) Performance:
 //     - We don't compare strings, just the hashes
 //     - Use a faster hash function provided by the wyhash library
 //     - Use of std::string_view
 //     - Querying for a path would imply having to create a new std::string if we don't have one, whch is expensive since it requires allating memory
 // 2) Bidirectionality:
 //     - We can easy get the path from the entry, just by looking up the "paths" array
 template <typename Int>
struct StringIds
{
    std::vector<std::string> paths;
    std::unordered_map<u64, Int, DumbHash> hashToEntry;

    Int getEntry(std::string_view path)const
    {
        const u64 h = hash(path);
        if (auto it = hashToEntry.find(h); it == hashToEntry.end())
            return Int(-1);
        else {
            assert(path == paths[it->second]);
            return it->second;
        }
    }
    void addPath(std::string_view path, Int entry)
    {
        assert(!path.empty());
        assert(getEntry(path) == u32(-1));
        const size_t newSize = glm::max<size_t>(paths.size(), entry + 1);
        paths.resize(newSize);
        paths[entry] = path;
        const u64 h = tk::hash(path);
        hashToEntry[h] = entry;
    }
    void deleteEntry(Int entry)
    {
        assert(!paths[entry].empty());
        const u64 h = tk::hash(paths[entry]);
        paths[entry] = {};
        hashToEntry.erase(h);
    }
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

template <typename T, typename DestroyFn, T nullVal = T{}>
struct UniqueResource
{
    T handle;

    UniqueResource(T handle = nullVal) : handle(handle) {}
    UniqueResource(const UniqueResource& o) = delete;
    UniqueResource(UniqueResource&& o) noexcept
        : handle(o.handle)
    {
        o.handle = {};
    }
    UniqueResource& operator=(UniqueResource&& o) noexcept
    {
        if (handle != nullVal)
            DestroyFn{}(handle);
        handle = o.handle;
        o.handle = nullptr;
        return *this;
    }
    ~UniqueResource()
    {
        if (handle != nullVal)
            DestroyFn{}(handle);
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

template <typename Int>
static void appendBits(size_t& y, Int x, size_t numBits) {
    using UInt = std::make_unsigned_t<Int>;
    auto bits = size_t(*(UInt*)&x);
    size_t mask = (size_t(1) << numBits) - size_t(1);
    y = (y << numBits) | (bits & mask);
}

template <typename Fn, size_t... INDS>
void _comptime_for(Fn fn, std::index_sequence<INDS...>) {
    (fn.template operator()<INDS>(), ...);
}
template <size_t N, typename Fn>
void comptime_for(Fn fn) {
    _comptime_for<Fn>(fn, std::make_index_sequence<N>{});
}

template <std::unsigned_integral UInt, std::same_as<bool> ...Bool>
UInt packBools(Bool... bools) {
    UInt res = 0;
    ((res = (res << 1) | UInt(bools ? 1 : 0)), ...);
    return res;
}

template<typename T> concept EnumConcept = std::is_enum_v<T>;

template <typename IntT, EnumConcept EnumT>
struct FlagSet
{
    IntT bits = 0;

    constexpr FlagSet() {}
    constexpr explicit FlagSet(IntT bits) : bits(bits) {}
    constexpr FlagSet(std::initializer_list<EnumT> enums) {
        for (EnumT e : enums) {
            assert(IntT(e) < 8 * sizeof(IntT));
            bits |= IntT(1) << IntT(e);
        }
    }

    FlagSet& operator=(std::initializer_list<EnumT> enums) { bits = FlagSet({ enums }).bits; };
    
    FlagSet operator|(FlagSet o) const { return FlagSet(bits | o.bits); }
    FlagSet operator&(FlagSet o) const { return FlagSet(bits & o.bits); }
    FlagSet operator^(FlagSet o) const { return FlagSet(bits ^ o.bits); }
    FlagSet operator~() const { return FlagSet(~bits); }
    FlagSet operator|(EnumT e) const { return *this | FlagSet{e}; }
    FlagSet operator&(EnumT e) const { return *this & FlagSet{e}; }
    FlagSet operator^(EnumT e) const { return *this ^ FlagSet{e}; }

    FlagSet& operator=(FlagSet o) { bits = o.bits; return *this; }
    FlagSet& operator|=(FlagSet o) { bits |= o.bits; return *this; }
    FlagSet& operator&=(FlagSet o) { bits &= o.bits; return *this; }
    FlagSet& operator^=(FlagSet o) { bits ^= o.bits; return *this; }

    FlagSet& operator|=(EnumT e) { bits |= FlagSet({e}).bits; return *this; }
    FlagSet& operator&=(EnumT e) { bits &= FlagSet({e}).bits; return *this; }
    FlagSet& operator^=(EnumT e) { bits ^= FlagSet({e}).bits; return *this; }

    operator bool()const { return bits != IntT(0); };
    bool has(EnumT e)const { return *this & FlagSet{ e }; }
    FlagSet& set(EnumT e, bool b = true) {
        if (b)
            bits |= IntT(1) << IntT(e);
        else
            bits &= ~(IntT(1) << IntT(e));
        return *this;
    }

    auto operator<=>(FlagSet o)const { return bits <=> o.bits; }
};

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