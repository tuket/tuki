#include "utils.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <wyhash.h>
#include <array>
#include <physfs.h>

namespace tk
{

// --- FpsCamera ---
void FpsCamera::init_lookScene(const AABB& sceneAABB, glm::vec2 hFovXY)
{
	const glm::vec3 c = sceneAABB.center();
	const glm::vec3 hSize = 0.5f * sceneAABB.size();
	position.x = c.x;
	position.y = c.y;
	const float z_x = hSize.x / tanf(hFovXY.x);
	const float z_y = hSize.y / tanf(hFovXY.y);
	const float z = glm::max(z_x, z_y);
	position.z = hSize.z + z;
	heading = 0; // glm::radians(180.f);
	pitch = 0;
}

void FpsCamera::move(glm::vec3 delta_XYZ)
{
	const auto R = glm::mat3(rotMtx());
	position += R * delta_XYZ;
}

static void handleHeadinPitchBounds(float& heading, float& pitch) {
	heading = glm::mod(heading, glm::two_pi<float>());
	pitch = glm::clamp(pitch, -0.9f * glm::half_pi<float>(), +0.9f * glm::half_pi<float>());
}

void FpsCamera::rotate(float deltaScreenX, float deltaScreenY)
{
	heading -= glm::pi<float>() * deltaScreenX;
	pitch -= glm::pi<float>() * deltaScreenY;
	handleHeadinPitchBounds(heading, pitch);
}

glm::mat4 FpsCamera::rotMtx()const
{
	return glm::eulerAngleYX(heading, pitch);
}

glm::mat4 FpsCamera::mtx()const
{
	auto M = glm::translate(position);
	M = M * rotMtx();
	//auto M = glm::eulerAngleXY(pitch, heading);
	//M[3] = glm::vec4(position, 1);
	return M;
}

glm::mat4 FpsCamera::viewMtx()const
{
	return glm::affineInverse(mtx());
}

// --- OrbitCamera ---

void OrbitCamera::zoom(float factor)
{

}

void OrbitCamera::pan(glm::vec2 deltaScreenXY, bool lockedY)
{

}

void OrbitCamera::rotate(float deltaScreenX, float deltaScreenY)
{
	heading -= glm::pi<float>() * deltaScreenX;
	pitch -= glm::pi<float>() * deltaScreenY;
	handleHeadinPitchBounds(heading, pitch);
}

glm::mat4 OrbitCamera::mtx()const
{
	glm::mat4 m = glm::translate(glm::vec3(0, 0, distance));
	m = glm::rotate(pitch, glm::vec3(1, 0, 0)) * m;
	m = glm::rotate(heading, glm::vec3(0, 1, 0)) * m;
	return m;
}

glm::mat4 OrbitCamera::viewMtx()const
{
	return glm::affineInverse(mtx());
}

// --- convert cameras ---
FpsCamera orbitToFps(const OrbitCamera& c)
{
	FpsCamera fps{};
	fps.position = glm::vec3(c.mtx()[3]);
	fps.heading = c.heading;
	fps.pitch = c.pitch;
	return fps;
}

OrbitCamera fpsToOrbit(const FpsCamera& c)
{
	OrbitCamera orbit{};
	orbit.pivot = glm::vec3(0);
	orbit.distance = glm::length(c.position);
	orbit.heading = glm::atan2(c.position.x, c.position.z);
	orbit.pitch = glm::atan2(-c.position.y, glm::length(glm::vec2(c.position.x, c.position.z)));
	return orbit;
}

// --- PerspectiveCamera ---
glm::mat4 PerspectiveCamera::projMtx_vk(float aspectRatio)const
{
	auto M = glm::perspectiveZO(2 * hFovY, aspectRatio, near, far);
	M[1][1] *= -1;
	return M;
}

bool loadTextFile(std::string& str, CStr path)
{
	if (0) { // using the C API
		FILE* file = fopen(path, "r");
		if (!file)
			return false;
		fseek(file, 0, SEEK_END);
		auto n = ftell(file);
		fseek(file, 0, SEEK_SET);
		str.resize(n);
		fread(str.data(), 1, n, file);
	}
	else { // using PhysicsFS
		auto file = PHYSFS_openRead(path);
		if (!file)
			return false;
		auto len = PHYSFS_fileLength(file);
		str.resize(len);
		PHYSFS_readBytes(file, str.data(), len);
	}
	return true;
}

LoadedBinaryFile loadBinaryFile(CStr path)
{
	auto* file = PHYSFS_openRead(path);
	if (!file)
		return { 0, nullptr };
	defer(PHYSFS_close(file););
	const auto fileLen = PHYSFS_fileLength(file);
	u8* fileData = new u8[fileLen];
	if (PHYSFS_readBytes(file, fileData, fileLen) < fileLen) {
		delete[] fileData;
		return { 0, nullptr };
	}
	return { size_t(fileLen), fileData };
}

static std::array<u64, 4> k_hashingSecret = []() {
	std::array<u64, 4> sec;
	make_secret(42382348, sec.data());
	return sec;
}();

static u64 hash(std::string_view s) {
	return wyhash(s.data(), s.size(), 524354325, k_hashingSecret.data());
}

// -- PathBag --

u32 PathBag::getEntry(std::string_view path)const
{
	const u64 h = hash(path);
	if (auto it = hashToEntry.find(h); it == hashToEntry.end())
		return u32(-1);
	else {
		assert(path == paths[it->second]);
		return it->second;
	}
}
void PathBag::addPath(std::string_view path, u32 entry)
{
	assert(!path.empty());
	assert(getEntry(path) == u32(-1));
	const size_t newSize = glm::max<size_t>(paths.size(), entry + 1);
	paths.resize(newSize);
	paths[entry] = path;
	const u64 h = hash(path);
	hashToEntry[h] = entry;
}
void PathBag::deleteEntry(u32 entry)
{
	assert(!paths[entry].empty());
	const u64 h = hash(paths[entry]);
	paths[entry] = {};
	hashToEntry.erase(h);
}

// StackTmpAllocator
static StackTmpAllocator s_stackTmpAllocator;

void initStackTmpAllocator(size_t capacity)
{
	s_stackTmpAllocator = StackTmpAllocator(capacity);
}
StackTmpAllocator& getStackTmpAllocator()
{
	return s_stackTmpAllocator;
}

}