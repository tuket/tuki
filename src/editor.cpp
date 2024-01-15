#include <stdio.h>
#include <tg.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <tk.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace tvk = tk::vk;
using tk::CSpan;
using tk::u8;
using tk::u32;

static u32 cubeInds[6*6] = {
	0, 1, 3, 0, 3, 2,
	4+0, 4+1, 4+3, 4+0, 4+3, 4+2,
};
static const glm::vec3 cubeVerts_positions[] = {
	// -X
	{-1, -1, -1},
	{-1, -1, +1},
	{-1, +1, -1},
	{-1, +1, +1},
	// +X
	{+1, -1, +1},
	{+1, -1, -1},
	{+1, +1, +1},
	{+1, +1, -1},
	// -Y
	{-1, -1, -1},
	{+1, -1, -1},
	{-1, -1, +1},
	{+1, -1, +1},
	// +Y
	{-1, +1, +1},
	{+1, +1, +1},
	{-1, +1, -1},
	{+1, +1, -1},
	// -Z
	{+1, -1, -1},
	{-1, -1, -1},
	{+1, +1, -1},
	{-1, +1, -1},
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
	{0.3f, 0, 0, 1},
	{0.3f, 0, 0, 1},
	{0.3f, 0, 0, 1},
	{0.3f, 0, 0, 1},
	// +X
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	{1.f, 0, 0, 1},
	// -Y
	{0, 0.3f, 0, 1},
	{0, 0.3f, 0, 1},
	{0, 0.3f, 0, 1},
	{0, 0.3f, 0, 1},
	// +Y
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	{0, 1.f, 0, 1},
	// -Z
	{0, 0, 0.3f, 1},
	{0, 0, 0.3f, 1},
	{0, 0, 0.3f, 1},
	{0, 0, 0.3f, 1},
	// +Z
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
	{0, 0, 1.f, 1},
};

static const glm::vec2 cubeVerts_texCoords[] = {
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
	{0, 0}, {1, 0}, {0, 1}, {1, 1},
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

static void initImGui(GLFWwindow* window)
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window, true);
}

static tk::WorldId mainWorld;
static std::vector<bool> entitiesSelected;

static void deleteSelectedEntities(tk::WorldId mainWorld)
{
	tk::EntityId e = mainWorld->getRootEntity().firstChild();
	u32 i = 0;
	std::vector<u32> toDelete;
	while (e.valid()) {
		const auto nextE = e.nextSibling();
		if (entitiesSelected[e.ind]) {
			toDelete.push_back(e.ind);
		}
		i++;
		e = nextE;
	}
	mainWorld->addEntitiesToDelete(toDelete);
	entitiesSelected.resize(entitiesSelected.size() - toDelete.size());
	std::fill(entitiesSelected.begin(), entitiesSelected.end(), false);
}

static void imgui_hierarchy(tk::WorldId mainWorld)
{
	ImGui::Begin("scene");
	tk::EntityId e = mainWorld->getRootEntity().firstChild();
	int i = 0;
	tk::EntityId clicked = {};
	while (e.valid()) {
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		entitiesSelected.resize(glm::max(entitiesSelected.size(), size_t(e.ind+1)));
		if (entitiesSelected[e.ind])
			flags |= ImGuiTreeNodeFlags_Selected;
		
		ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%d", i);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			clicked = e;

		i++;
		e = e.nextSibling();
	}

	if (clicked.valid()) {
		if (ImGui::GetIO().KeyCtrl)
			entitiesSelected[clicked.ind].flip();
		else {
			std::fill(entitiesSelected.begin(), entitiesSelected.end(), false);
			entitiesSelected[clicked.ind] = true;
		}
	}

	ImGui::End();
}

int main()
{
	completeCubeInds();
	printf("tuki editor!\n");
	glfwInit();
	defer(glfwTerminate());
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	int screenW = 1000, screenH = 800;
	auto glfwWindow = glfwCreateWindow(screenW, screenH, "tuki editor", nullptr, nullptr);

	u32 numRequiredExtensions;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&numRequiredExtensions);

	const tvk::AppInfo info = { .apiVersion = {1, 3, 0}, .appName = "tuki editor" };
	VkInstance instance = tvk::createInstance(info, {}, { requiredExtensions, numRequiredExtensions });

	VkSurfaceKHR surface;
	tvk::ASSERT_VKRES(glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface));
	
	initImGui(glfwWindow);

	tg::initRenderUniverse({ .instance = instance, .surface = surface, .screenW = u32(screenW), .screenH = u32(screenH), .enableImgui = true });

	//tg::PbrMaterialManager pbrMgr = {};
	//pbrMgr.init();
	//tg::registerMaterialManager(pbrMgr.getRegisterCallbacks());

	mainWorld = tk::createWorld();
	//defer(tk::destroyWorld());

	auto systems = mainWorld->createDefaultBasicSystems();
	auto& pbrMgr = systems.system_render.pbrMaterialManager;
	auto& RW = systems.system_render.RW;
	auto& factory_renderable3d = systems.system_render.factory_renderable3d;

	CSpan<u8> cubeGeomData[] = { tk::asBytesSpan(cubeInds), tk::asBytesSpan(cubeVerts_positions), tk::asBytesSpan(cubeVerts_normals), tk::asBytesSpan(cubeVerts_colors) };
	auto cubeGeom = tg::createGeom({
		.positions = tk::asBytesSpan(cubeVerts_positions),
		.normals = tk::asBytesSpan(cubeVerts_normals),
		.colors = tk::asBytesSpan(cubeVerts_colors),
		.indices = tk::asBytesSpan(cubeInds),
		.numVerts = std::size(cubeVerts_positions),
		.numInds = std::size(cubeInds)
	});
	auto cubeMaterial = pbrMgr.createMaterial({.doubleSided = false});
	auto cubeMesh = tg::makeMesh({ .geom = cubeGeom, .material = cubeMaterial });

	auto crateGeom = tg::createGeom({
		.positions = tk::asBytesSpan(cubeVerts_positions),
		.normals = tk::asBytesSpan(cubeVerts_normals),
		.texCoords = tk::asBytesSpan(cubeVerts_texCoords),
		.indices = tk::asBytesSpan(cubeInds),
		.numVerts = std::size(cubeVerts_positions),
		.numInds = std::size(cubeInds)
	});
	auto crateImg = tg::getOrLoadImage("data/crate.png", true);
	auto crateImgView = tg::makeImageView({ .image = crateImg });
	auto crateMaterial = pbrMgr.createMaterial({ .albedoImageView = crateImgView });
	auto crateMesh = tg::makeMesh({ .geom = crateGeom, .material = crateMaterial });
	const glm::mat4 crateInstanceMatrices[] = {
		glm::translate(glm::vec3(0, 0, -2)),
		glm::translate(glm::vec3(0, 0, +2)),
	};

	const int n = 2;
	for (int iy = 0; iy < n; iy++) {
		for (int ix = 0; ix < n; ix++) {
			const bool cubeType = (iy + ix) % 2 == 0;
			factory_renderable3d->create({
				.position = glm::vec3(6.f/n * (ix - n/2 + 0.5f), 6.f/n * (iy - n/2 + 0.5f), 0),
				.scale = glm::vec3(2.0/n),
				.separateMaterial = false,
				.mesh = cubeType ? cubeMesh : crateMesh,
			});
		}
	}

	glm::dvec2 prevMousePos;
	glfwGetCursorPos(glfwWindow, &prevMousePos.x, &prevMousePos.y);

	float prevTime = glfwGetTime();

	glfwSetKeyCallback(glfwWindow, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_DELETE)
				deleteSelectedEntities(mainWorld);
			if (key == GLFW_KEY_F1)
				camera.flipMode();
		}
	});

	glfwSetFramebufferSizeCallback(glfwWindow, [](GLFWwindow* w, int screenW, int screenH) {
		tg::onWindowResized(screenW, screenH);
	});

	float dtAverage = 0;

	while (!glfwWindowShouldClose(glfwWindow))
	{
		glfwPollEvents();

		const float time = glfwGetTime();
		const float dt = time - prevTime;
		prevTime = time;
		dtAverage = glm::mix(dtAverage, dt, 0.1f);
		char windowTitle[32];
		snprintf(windowTitle, sizeof(windowTitle), "tuki editor (%.1f fps)", 1.f / dtAverage);
		glfwSetWindowTitle(glfwWindow, windowTitle);

		glfwGetFramebufferSize(glfwWindow, &screenW, &screenH);
		const float aspectRatio = float(screenW) / float(screenH);

		tg::imgui_newFrame();

		auto& imgui_io = ImGui::GetIO();

		// camera rotation
		glm::dvec2 mousePos;
		glfwGetCursorPos(glfwWindow, &mousePos.x, &mousePos.y);
		if (!imgui_io.WantCaptureMouse && glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT)) {
			const glm::dvec2 mouseDelta = mousePos - prevMousePos;
			const glm::dvec2 mouseScreenDelta = -mouseDelta / glm::dvec2(screenW, screenH);
			camera.rotate(mouseScreenDelta.x, mouseScreenDelta.y);
		}
		prevMousePos = mousePos;

		// camera translation
		if (!imgui_io.WantCaptureKeyboard) {
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
		}
		
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
		mainWorld->update(dt);
		systems.update(dt);

		imgui_hierarchy(mainWorld);
		ImGui::ShowDemoWindow();

		tg::draw(viewports);
	}

	return 0;
}