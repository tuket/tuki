#include <stdio.h>
#include <tg.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <tk.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <physfs.h>

namespace tvk = tk::vk;
using tk::CSpan;
using tk::CStr;
using tk::u8;
using tk::u32;

static const bool imguiEnable = true;

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

tk::DefaultBasicWorldSystems systems;

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

struct IconCodes {
	typedef const CStr Code;
	// https://www.cogsci.ed.ac.uk/~richard/utf-8.cgi?input=0xf114&mode=hex
	// file://assets/fontello/demo.html
	Code closedFolder = "\xEF\x84\x94";
	Code openedFolder = "\xEF\x84\x95";
	Code genericFile = "\xEE\xA0\x80";
	Code imageFile = "\xEE\xA0\x99";
	Code textFile = "\xEF\x83\xB6";
	Code geomFile = "\xEF\x88\x99";
};
static constexpr IconCodes iconCodes{};

static void initImGui(GLFWwindow* window)
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window, true);

	auto& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontDefault();
	static const ImWchar icons_ranges[] = { 0xe8000, 0xf3ff, 0 };
	ImFontConfig config;
	config.MergeMode = true;
	io.Fonts->AddFontFromFileTTF("assets/fontello/font/fontello.ttf", 18.0f, &config, icons_ranges);
	io.Fonts->Build();
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

static bool endsWith(std::string_view str, std::string_view ending)
{
	if (str.size() < ending.size())
		return false;
	for (int i = 0; i < ending.size(); i++) {
		if (ending[ending.size() - 1 - i] != str[str.size() - 1 - i])
			return false;
	}
	return true;
}


struct FilePreviews {
	struct TextPreview {
		std::string path;
		std::string contents;

		TextPreview(CStr path)
			: path(path)
		{
			const bool ok = tk::loadTextFile(contents, path);
			assert(ok);
		}

		bool draw()
		{
			bool open = true;
			ImGui::Begin(path.c_str(), &open);
			ImGui::Text(contents.c_str());
			ImGui::End();
			return !open;
		}
	};

	struct ImagePreview {
		std::string path;
		tg::ImageRC img;
		tg::ImageViewRC imgView;
		VkSampler sampler;
		VkDescriptorSet descSet;

		ImagePreview(CStr path)
			: path(path)
		{
			img = tg::getOrLoadImage(path, true, true);
			imgView = tg::makeImageView({.image = img});
			sampler = tg::getAnisotropicFilteringSampler(1.f);
			descSet = ImGui_ImplVulkan_AddTexture(sampler, (VkImageView)imgView.id.getInternalHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		~ImagePreview() {
			ImGui_ImplVulkan_RemoveTexture(descSet);
		}

		bool draw()
		{
			bool open = true;
			ImGui::Begin(path.c_str(), &open);
			const glm::vec2 imgSize = ImGui::GetContentRegionAvail();
			ImGui::Image(descSet, imgSize);
			ImGui::End();
			return !open;
		}
	};

	struct GeomPreview {
		static constexpr glm::uvec2 k_initWindowSize = {256, 256};

		std::string path;
		tg::RenderTargetId renderTarget;
		VkDescriptorSet descSets[tvk::Swapchain::MAX_IMAGES] = {};
		tk::WorldId world; // yes, each preview has it's own world!
		tk::DefaultBasicWorldSystems systems;
		tg::GeomRC geom;
		tg::MeshRC mesh;
		tk::EntityId entity;
		tk::OrbitCamera orbitCamera;
		tk::AABB aabb;
		glm::uvec2 windowSize = { 0, 0 };
		glm::vec2 prevMousePos = {0,0};
		std::vector<VkDescriptorSet> toDelete_descSets[tvk::Swapchain::MAX_IMAGES];

		//std::vector<glm::vec3> verts_positions;
		//std::vector<glm::vec2> 

		GeomPreview(CStr path)
			: path(path)
		{
			world = tk::createWorld();
			systems = world->createDefaultBasicSystems();
			
			geom = tg::geom_getOrLoadFromFile(path, &aabb);
			mesh = tg::makeMesh({ .geom = geom, .material = geomMaterial() });
			auto& factory = *systems.system_render->factory_renderable3d; // world->getEntityFactory<tk::EntityFactory_Renderable3d>();
			entity = factory.create({ .mesh = mesh });

			orbitCamera.pivot = aabb.center();
			orbitCamera.heading = orbitCamera.pitch = 0;//glm::quarter_pi<float>();
			orbitCamera.distance = 5;
		}

		auto getRenderWorld() { return systems.system_render->RW; };

		~GeomPreview()
		{
			tg::destroyRenderTarget(renderTarget);
			tk::destroyWorld(world);
		}

		void update(float dt)
		{
			world->update(dt);
			systems.update(dt);
		}

		void recreateDescSets()
		{
			if (windowSize.x == 0 || windowSize.y == 0)
				return;

			const u32 numImages = tg::getNumSwapchainImages();
			for (u32 scImgInd = 0; scImgInd < numImages; scImgInd++)
			{
				if (descSets[scImgInd])
					toDelete_descSets[scImgInd].push_back(descSets[scImgInd]);
				
				const VkSampler sampler = tg::getNearestSampler();
				const VkImageView imgView = renderTarget.getTextureImageViewVk(scImgInd);
				const VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descSets[scImgInd] = ImGui_ImplVulkan_AddTexture(sampler, imgView, layout);
			}
		}

		void processDeferredDeletes()
		{
			const u32 scImgInd = tg::getCurrentSwapchainImageInd();
			for (auto& ds : toDelete_descSets[scImgInd]) {
				ImGui_ImplVulkan_RemoveTexture(ds);
				ds = VK_NULL_HANDLE;
			}
			toDelete_descSets[scImgInd].clear();
		}

		bool draw()
		{
			processDeferredDeletes();
			const glm::vec2 mousePos = ImGui::GetMousePos();

			bool open = true;
			ImGui::SetNextWindowSize(glm::vec2(k_initWindowSize), ImGuiCond_Appearing);
			ImGui::Begin(path.c_str(), &open);

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const glm::vec2 imgSize = ImGui::GetContentRegionAvail();
			const glm::uvec2 newWindowSize = imgSize;

			const bool firstTime = !renderTarget.isValid();
			const bool resized = newWindowSize != windowSize;
			if (firstTime || resized) {
				windowSize = newWindowSize;
				if(firstTime)
					renderTarget = tg::createRenderTarget({ .w = newWindowSize.x, .h = newWindowSize.y });
				else
					renderTarget.resize(newWindowSize.x, newWindowSize.y);
				recreateDescSets();
			}

			const u32 scImgInd = tg::getCurrentSwapchainImageInd();
			const glm::vec2 pos = window->DC.CursorPos;
			const ImRect bb = { pos, pos + imgSize };
			const ImGuiID id = window->GetID("img");
			ImGui::Image(descSets[scImgInd], imgSize);
			ImGui::ItemHoverable(bb, id, ImGuiItemFlags_None);
			if (ImGui::ItemAdd(bb, id)) {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
					glm::vec2 mouseDelta = mousePos - prevMousePos;
					mouseDelta /= imgSize;
					orbitCamera.rotate(mouseDelta.x, mouseDelta.y);
				}
			}
			ImGui::End();

			prevMousePos = mousePos;
			
			return !open;
		}
	};

	std::vector<TextPreview> textPreviews;
	std::vector<ImagePreview> imagePreviews;
	std::vector<GeomPreview> geomPreviews;

	static tg::MaterialRC geomMaterialRC;
	static tg::MaterialRC& geomMaterial()
	{
		if (!geomMaterialRC.id.isValid()) {
			geomMaterialRC = systems.system_render->pbrMaterialManager.createMaterial({
				.albedo = {243.f / 255.f, 237.f / 255.f, 120.f / 255.f, 1.f}
			});
		}
		return geomMaterialRC;
	}
	
	void openFilePreview(CStr path)
	{
		printf("open file preview: %s\n", path);
		if (endsWith(path, ".txt")) {
			textPreviews.emplace_back(path);
		}
		else if (endsWith(path, ".png")) {
			imagePreviews.emplace_back(path);
		}
		else if (endsWith(path, ".geom")) {
			geomPreviews.emplace_back(path);
		}
		else {
			printf("this file can't be previewed\n");
		}
	}

	void update(float dt)
	{
		auto updatePreviews = [dt](auto& previews) {
			for (auto& p : previews)
				p.update(dt);
		};

		updatePreviews(geomPreviews);
	}

	void draw()
	{
		auto drawPreviews = [](auto& previews) {
			for (size_t i = 0; i < previews.size(); ) {
				const bool toClose = previews[i].draw();
				if (toClose) {
					if (i != previews.size() - 1)
						previews[i] = std::move(previews.back());
					previews.pop_back();
				}
				else
					i++;
			}
		};

		drawPreviews(textPreviews);
		drawPreviews(imagePreviews);
		drawPreviews(geomPreviews);
	}

	void getRenderTargetsViewports(std::vector<tg::RenderTargetWorldViewports>& rtViewports)
	{
		for (auto& gp : geomPreviews) {
			const u32 w = gp.windowSize.x;
			const u32 h = gp.windowSize.y;
			const float fW = w;
			const float fH = h;
			const float aspectRatio = fW / fH;
			const tg::RenderWorldViewport viewport = {
				.renderWorld = gp.getRenderWorld(),
				.viewMtx = gp.orbitCamera.viewMtx(),
				.projMtx = tk::PerspectiveCamera{}.projMtx_vk(aspectRatio),
				.viewport = {0, 0, fW, fW},
				.scissor = {0, 0, w, h},
			};
			rtViewports.push_back(tg::RenderTargetWorldViewports{
				.renderTarget = gp.renderTarget,
				.viewports = {viewport}
			});
		}
	}
};
static FilePreviews filePreviews;
tg::MaterialRC FilePreviews::geomMaterialRC = tg::MaterialRC{};

static bool fileTreeNode(CStr label, CStr path, bool folder)
{
	auto& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;

	const ImGuiID id = window->GetID(label);
	const ImGuiID icon_id = ImHashStr("icon", 0, id);
	const glm::vec2 pos = window->DC.CursorPos;

	const bool opened = folder && ImGui::TreeNodeBehaviorIsOpen(id);

	CStr iconStr = "";
	if (folder) {
		iconStr = opened ? iconCodes.openedFolder : iconCodes.closedFolder;
	}
	else {
		if(endsWith(label, ".png"))
			iconStr = iconCodes.imageFile;
		else if (endsWith(label, ".txt"))
			iconStr = iconCodes.textFile;
		else if (endsWith(label, ".geom"))
			iconStr = iconCodes.geomFile;
		else
			iconStr = iconCodes.genericFile;
	}
	const float buttonSize = g.FontSize + g.Style.FramePadding.y * 2;

	ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
	const ImRect icon_bb = { pos, pos + glm::vec2(buttonSize) };
	const ImRect label_bb = { pos + glm::vec2(buttonSize, 0), bb.Max };

	// icon button
	bool icon_hovered, icon_held;
	if (folder) {
		if (ImGui::ButtonBehavior(icon_bb, icon_id, &icon_hovered, &icon_held)) {
			window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
		}
	}
	else {
		if (ImGui::ButtonBehavior(icon_bb, icon_id, &icon_hovered, &icon_held, ImGuiButtonFlags_PressedOnDoubleClick)) {
			// open preview
			filePreviews.openFilePreview(path);
		}
	}

	bool label_hovered = false, label_held = false;
	if (ImGui::ButtonBehavior(label_bb, id, &label_hovered, &label_held)) {
		window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
	}

	if (icon_hovered || label_hovered || icon_held || label_held) {
		const auto eColor = icon_held || label_held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered;
		window->DrawList->AddRectFilled(bb.Min, bb.Max, ImColor(ImGui::GetStyle().Colors[eColor]));
	}

	// draw icon
	ImGui::RenderText(glm::vec2(pos.x, pos.y + 2*g.Style.FramePadding.y), iconStr);
	// draw text
	ImGui::RenderText(ImVec2(pos.x + buttonSize + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

	ImGui::ItemSize(icon_bb, g.Style.FramePadding.y);
	ImGui::ItemAdd(icon_bb, icon_id);
	ImGui::SameLine();
	ImGui::ItemSize(label_bb, g.Style.FramePadding.y);
	ImGui::ItemAdd(label_bb, id);

	if (opened)
		ImGui::TreePush(label);

	return opened;
}


struct ProjectExplorer
{
	char tmpPath[512];
	u32 tmpPathLen;

	void drawDirectory()
	{
		char** childFiles = PHYSFS_enumerateFiles(tmpPath);
		const u32 prevPathLen = tmpPathLen;
		for (size_t i = 0; childFiles[i]; i++) {
			const char* child = childFiles[i];
			if (tmpPathLen) { // the initial slash is not needed, and in fact messes up with our resource managers because of 'aliasing'
				tmpPath[tmpPathLen] = '/';
				tmpPathLen++;
			}
			for (const char* c = child; *c; c++) {
				tmpPath[tmpPathLen] = *c;
				tmpPathLen++;
			}
			tmpPath[tmpPathLen] = '\0';
			PHYSFS_Stat stat;
			if (PHYSFS_stat(tmpPath, &stat)) {
				if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
					ImGuiStorage* storage = ImGui::GetStateStorage();
					if (fileTreeNode(child, tmpPath, true)) {
						drawDirectory();
						ImGui::TreePop();
					}
				}
				else {
					//ImGui::TreeNodeEx(child, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
					fileTreeNode(child, tmpPath, false);
				}
			}
			else {
				// TODO?
			}
			tmpPath[prevPathLen] = '\0';
			tmpPathLen = prevPathLen;
		}
	}

	void draw()
	{
		ImGui::Begin("project");

		tmpPath[0] = '\0';
		u32 pathLen = 0;
		drawDirectory();

		ImGui::End();
	}
};

static ProjectExplorer projectExplorer;

int main(int argc, char** argv)
{
	completeCubeInds();
	printf("tuki editor!\n");
	glfwInit();
	defer(glfwTerminate());
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
	int screenW = 1320, screenH = 1000;
	auto glfwWindow = glfwCreateWindow(screenW, screenH, "tuki editor", nullptr, nullptr);

	u32 numRequiredExtensions;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&numRequiredExtensions);

	const tvk::AppInfo info = { .apiVersion = {1, 3, 0}, .appName = "tuki editor" };
	VkInstance instance = tvk::createInstance(info, {}, { requiredExtensions, numRequiredExtensions });

	VkSurfaceKHR surface;
	tvk::ASSERT_VKRES(glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface));
	
	initImGui(glfwWindow);

	tk::init(argv[0]);

	tg::initRenderUniverse({ .instance = instance, .surface = surface, .screenW = u32(screenW), .screenH = u32(screenH), .enableImgui = imguiEnable });

	mainWorld = tk::createWorld();
	//defer(tk::destroyWorld());

	systems = mainWorld->createDefaultBasicSystems();
	auto& pbrMgr = systems.system_render->pbrMaterialManager;
	auto& RW = systems.system_render->RW;
	auto& factory_renderable3d = systems.system_render->factory_renderable3d;

	CSpan<u8> cubeGeomData[] = { tk::asBytesSpan(cubeInds), tk::asBytesSpan(cubeVerts_positions), tk::asBytesSpan(cubeVerts_normals), tk::asBytesSpan(cubeVerts_colors) };
	const tg::CreateGeomInfo cubeGeomInfo = {
		.positions = tk::asBytesSpan(cubeVerts_positions),
		.normals = tk::asBytesSpan(cubeVerts_normals),
		.colors = tk::asBytesSpan(cubeVerts_colors),
		.indices = tk::asBytesSpan(cubeInds),
		.numVerts = std::size(cubeVerts_positions),
		.numInds = std::size(cubeInds)
	};
	CStr cubeGeomPath = "cube.geom";
	if (!tg::geom_serializeToFile(cubeGeomInfo, cubeGeomPath)) {
		printf("could not write file %s\n", cubeGeomPath);
		exit(-1);
	}
	auto cubeGeom = tg::geom_getOrLoadFromFile(cubeGeomPath);
	auto cubeMaterial = pbrMgr.createMaterial({.doubleSided = false});
	auto cubeMesh = tg::makeMesh({ .geom = cubeGeom, .material = cubeMaterial });

	auto crateGeom = tg::geom_createFromInfo({
		.positions = tk::asBytesSpan(cubeVerts_positions),
		.normals = tk::asBytesSpan(cubeVerts_normals),
		.texCoords = tk::asBytesSpan(cubeVerts_texCoords),
		.indices = tk::asBytesSpan(cubeInds),
		.numVerts = std::size(cubeVerts_positions),
		.numInds = std::size(cubeInds)
	});
	auto crateImg = tg::getOrLoadImage("crate.png", true);
	auto crateImgView = tg::makeImageView({ .image = crateImg });
	auto crateMaterial = pbrMgr.createMaterial({ .albedoImageView = crateImgView, .anisotropicFiltering = 16.f, });
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

		tg::prepareDraw();

		tg::imgui_newFrame();

		auto& imgui_io = ImGui::GetIO();

		// camera rotation
		glm::dvec2 mousePos;
		glfwGetCursorPos(glfwWindow, &mousePos.x, &mousePos.y);
		if (!imgui_io.WantCaptureMouse && glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT)) {
			const glm::dvec2 mouseDelta = mousePos - prevMousePos;
			const glm::dvec2 mouseScreenDelta = mouseDelta / glm::dvec2(screenW, screenH);
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
		
		auto projMtx = tk::PerspectiveCamera{}.projMtx_vk(aspectRatio);

		filePreviews.update(dt);
		mainWorld->update(dt);
		systems.update(dt);

		if (imguiEnable) {
			imgui_hierarchy(mainWorld);
			ImGui::ShowDemoWindow();
			projectExplorer.draw();
			filePreviews.draw();
		}

		const tg::RenderWorldViewport mainViewports[] = {
			{
				.renderWorld = RW,
				.viewMtx = camera.viewMtx(),
				.projMtx = camera.perspective.projMtx_vk(aspectRatio),
				.viewport = {0, 0, float(screenW), float(screenH)},
				.scissor = {0,0, u32(screenW), u32(screenH)}
			}
		};
		std::vector<tg::RenderTargetWorldViewports> renderTargetsViewports;
		filePreviews.getRenderTargetsViewports(renderTargetsViewports);

		tg::draw(mainViewports, renderTargetsViewports);
	}

	return 0;
}