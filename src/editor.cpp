#include <stdio.h>
#include <tg.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tvk = tk::vk;
using tk::CSpan;
using tk::u8;
using tk::u32;

static u32 cubeInds[6*6] = {
	0, 2, 1, 1, 2, 3,
	4+0, 4+1, 4+2, 4+1, 4+3, 4+2,
};
static const glm::vec3 cubeVerts_positions[] = {
	// -X
	{-1, -1, -1},
	{-1, -1, +1},
	{-1, +1, -1},
	{-1, +1, +1},
	// +X
	{+1, -1, -1},
	{+1, -1, +1},
	{+1, +1, -1},
	{+1, +1, +1},
	// -Y
	{-1, -1, -1},
	{-1, -1, +1},
	{+1, -1, -1},
	{+1, -1, +1},
	// +Y
	{-1, +1, -1},
	{-1, +1, +1},
	{+1, +1, -1},
	{+1, +1, +1},
	// -Z
	{-1, -1, -1},
	{+1, -1, -1},
	{-1, +1, -1},
	{+1, +1, -1},
	// +Z
	{-1, -1, +1},
	{+1, -1, +1},
	{-1, +1, +1},
	{+1, +1, +1},
};
static const glm::vec3 cubeVerts_normals[] = {
	// -X
	{-1, 0, 0},
	{-1, 0, 0},
	{-1, 0, 0},
	{-1, 0, 0},
	// +X
	{+1, 0, 0},
	{+1, 0, 0},
	{+1, 0, 0},
	{+1, 0, 0},
	// -Y
	{0, -1, 0},
	{0, -1, 0},
	{0, -1, 0},
	{0, -1, 0},
	// +Y
	{0, +1, 0},
	{0, +1, 0},
	{0, +1, 0},
	{0, +1, 0},
	// -Z
	{0, 0, -1},
	{0, 0, -1},
	{0, 0, -1},
	{0, 0, -1},
	// +Z
	{0, 0, +1},
	{0, 0, +1},
	{0, 0, +1},
	{0, 0, +1},
};
static const glm::vec4 cubeVerts_colors[] = {
	// -X
	{0.5f, 0, 0, 1},
	{0.5f, 0, 0, 1},
	{0.5f, 0, 0, 1},
	{0.5f, 0, 0, 1},
	// +X
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	// -Y
	{0, 0.5f, 0, 1},
	{0, 0.5f, 0, 1},
	{0, 0.5f, 0, 1},
	{0, 0.5f, 0, 1},
	// +Y
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	// -Z
	{0, 0, 0.5f, 1},
	{0, 0, 0.5f, 1},
	{0, 0, 0.5f, 1},
	{0, 0, 0.5f, 1},
	// +Z
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
};

static void completeCubeInds()
{
	for (u32 d = 1; d < 3; d++) {
		for (u32 i = 0; i < 12; i++) {
			cubeInds[12*d + i] = cubeInds[i] + 8*d;
		}
	}
}

struct Camera {
	tk::FpsCamera fps;
	tk::OrbitCamera orbit;
	tk::PerspectiveCamera perspective;
	bool orbitMode = true;

	glm::mat4 viewMtx()const {
		if (orbitMode)
			return orbit.viewMtx();
		else
			return fps.viewMtx();
	}

	void rotate(float screenDeltaX, float screenDeltaY) {
		if (orbitMode)
			orbit.rotate(screenDeltaX, screenDeltaY);
		else
			fps.rotate(screenDeltaX, screenDeltaY);
	}

	void flipMode() {
		if (orbitMode) {
			fps = tk::orbitToFps(orbit);
			orbitMode = false;
		}
		else {
			orbit = tk::fpsToOrbit(fps);
			orbitMode = true;
		}
	}
};
static const tk::OrbitCamera initialOrbitCam{ .distance = 6, .heading = glm::radians(0.f*45.f), .pitch = glm::radians(0.f*45.f) };
static Camera camera = {
	.fps = tk::orbitToFps(initialOrbitCam),
	.orbit = initialOrbitCam,
};

int main()
{
	completeCubeInds();
	printf("tuki editor!\n");
	glfwInit();
	defer(glfwTerminate);
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	int screenW = 800, screenH = 600;
	auto glfwWindow = glfwCreateWindow(screenW, screenH, "tuki editor", nullptr, nullptr);

	u32 numRequiredExtensions;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&numRequiredExtensions);

	const tvk::AppInfo info = { .apiVersion = {1, 3, 0}, .appName = "tuki editor" };
	VkInstance instance = tvk::createInstance(info, {}, { requiredExtensions, numRequiredExtensions });

	VkSurfaceKHR surface;
	tvk::ASSERT_VKRES(glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface));

	tg::initRenderUniverse({ .instance = instance, .surface = surface, .screenW = u32(screenW), .screenH = u32(screenH) });

	tg::PbrMaterialManager pbrMgr = {};
	pbrMgr.init();
	tg::registerMaterialManager(pbrMgr.getRegisterCallbacks());

	auto RW = tg::createRenderWorld();
	defer(tg::destroyRenderWorld(RW));

	CSpan<u8> cubeGeomData[] = { tk::asBytesSpan(cubeInds), tk::asBytesSpan(cubeVerts_positions), tk::asBytesSpan(cubeVerts_normals), tk::asBytesSpan(cubeVerts_colors) };
	auto cubeGeom = tg::createGeom({
		.positions = tk::asBytesSpan(cubeVerts_positions),
		.normals = tk::asBytesSpan(cubeVerts_normals),
		.colors = tk::asBytesSpan(cubeVerts_colors),
		.indices = tk::asBytesSpan(cubeInds),
		.numVerts = std::size(cubeVerts_positions),
		.numInds = std::size(cubeInds)
	});

	auto cubeMaterial = pbrMgr.createMaterial({});
	auto cubeMesh = tg::makeMesh({ .geom = cubeGeom, .material = cubeMaterial });
	auto cubeObject = RW.createObject(cubeMesh);
	cubeObject.setModelMatrix(glm::mat4(1));

	glm::dvec2 prevMousePos;
	glfwGetCursorPos(glfwWindow, &prevMousePos.x, &prevMousePos.y);

	float prevTime = glfwGetTime();

	glfwSetKeyCallback(glfwWindow, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
			camera.flipMode();
		}
	});

	while (!glfwWindowShouldClose(glfwWindow))
	{
		glfwPollEvents();

		const float time = glfwGetTime();
		const float dt = time - prevTime;
		prevTime = time;

		glfwGetFramebufferSize(glfwWindow, &screenW, &screenH);
		const float aspectRatio = float(screenW) / float(screenH);

		// camera rotation
		glm::dvec2 mousePos;
		glfwGetCursorPos(glfwWindow, &mousePos.x, &mousePos.y);
		if (glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT)) {
			const glm::dvec2 mouseDelta = mousePos - prevMousePos;
			const glm::dvec2 mouseScreenDelta = -mouseDelta / glm::dvec2(screenW, screenH);
			camera.rotate(mouseScreenDelta.x, mouseScreenDelta.y);
		}
		prevMousePos = mousePos;

		// camera translation
		glm::vec3 translation(0);
		if (glfwGetKey(glfwWindow, GLFW_KEY_W))
			translation.z -= 1;
		if (glfwGetKey(glfwWindow, GLFW_KEY_S))
			translation.z += 1;
		if (glfwGetKey(glfwWindow, GLFW_KEY_A))
			translation.x -= 1;
		if (glfwGetKey(glfwWindow, GLFW_KEY_D))
			translation.x += 1;
		if (translation.x != 0 || translation.y != 0 || translation.z != 0)
			translation = glm::normalize(translation);
		const float camSpeed = 3;
		translation *= camSpeed * dt;
		camera.fps.move(translation);
		
		auto projMtx = glm::perspectiveZO(glm::radians(60.f), aspectRatio, 0.1f, 1000.f);
		projMtx[1][1] *= -1;
		const tg::RenderWorldViewport viewports[] = {
			{
				.renderWorld = RW,
				.viewMtx = camera.viewMtx(),
				.projMtx = camera.perspective.projMtx_vk(aspectRatio),
				.viewport = {0, 0, float(screenW), float(screenH)},
				.scissor = {0,0, u32(screenW), u32(screenH)}
			}
		};
		tg::draw(viewports);
	}

	return 0;
}