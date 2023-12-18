#include "utils.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>

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
	heading += glm::pi<float>() * deltaScreenX;
	pitch += glm::pi<float>() * deltaScreenY;
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
	heading += glm::pi<float>() * deltaScreenX;
	pitch += glm::pi<float>() * deltaScreenY;
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

std::string loadTextFile(CStr path)
{
	FILE* file = fopen(path, "r");
	if (!file)
		return "";
	fseek(file, 0, SEEK_END);
	auto n = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::string res(n, ' ');
	fread(res.data(), 1, n, file);
	return res;
}

}