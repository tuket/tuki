#include "tg.hpp"
#include <array>
#include <stb_image.h>
#include "tvk.hpp"
#include "shader_compiler.hpp"
#include <format>

namespace tk {
namespace gfx {

static constexpr u32 MAX_SWAPCHAIN_IMAGES = vk::Swapchain::MAX_IMAGES;
static constexpr u32 STAGING_BUFFER_CAPACITY = 32u << 20u;
// static constexpr u32 MAX_STAGING_BUFFERS = 32; // TODO: we could have a maximum number of staging buffers. This would help cap the staging memory usage, and also defer some transfer work for future frames. However this would take a big effort to implement right

static const u32 MAX_DIR_LIGHTS = 4;
static const auto MAX_DIR_LIGHTS_STR = std::format("{}", MAX_DIR_LIGHTS);

struct ImageStagingProc {
	ImageId img;
	vk::Buffer stagingBuffer;
	u32 stagingBufferOffset;
	u32 dataSize : 31;
	bool generateMipChain : 1;
};

struct RenderUniverse
{
	u32 queueFamily;
	VkSurfaceKHR surface;
	vk::Device device;
	ShaderCompiler shaderCompiler;

	vk::Format depthStencilFormat;
	u32 screenW = 0, screenH = 0;
	u32 oldScreenW, oldScreenH = 0;
	vk::SwapchainOptions swapchainOptions;
	vk::SwapchainSyncHelper swapchain;
	vk::Image depthStencilImages[MAX_SWAPCHAIN_IMAGES];
	vk::ImageView depthStencilImageViews[MAX_SWAPCHAIN_IMAGES];
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];
	VkRenderPass renderPass;
	VkDescriptorSetLayout globalDescSetLayout;

	// staging buffers
	struct StagingBuffer {
		vk::Buffer buffer;
		size_t offset;
	};
	std::vector<StagingBuffer> spareStagingBuffers; // these are the staging buffers that can be picked up for future staging processes
	std::vector<StagingBuffer> bussyStagingBuffers[MAX_SWAPCHAIN_IMAGES]; // here we keep the staging buffers that are in use. After the staging process is finished, we move the stagingBuffer back to spareStagingBuffers

	// resources to destroy
	std::vector<vk::Buffer> buffersToDestroy[MAX_SWAPCHAIN_IMAGES];
	std::vector<vk::Image> imagesToDestroy[MAX_SWAPCHAIN_IMAGES];
	std::vector<vk::ImageView> imageViewsToDestroy[MAX_SWAPCHAIN_IMAGES];

	// images
	std::vector<vk::Image> images_vk;
	std::vector<u32> images_refCount; // when the entry is free, it indicates the next free entry (forming a linked list)
	std::vector<ImageInfo> images_info;
	std::vector<u32> images_defaultImageView;
	std::unordered_map<Path, ImageId> images_nameToId;
	u32 images_nextFreeEntry = u32(-1);
	std::vector<ImageStagingProc> images_stagingProcs;

	// image views
	std::vector<vk::ImageView> imageViews_vk;
	std::vector<u32> imageViews_refCount;
	std::vector<ImageRC> imageViews_image;
	u32 imageViews_nextFreeEntry = u32(-1);

	// geoms
	std::vector<GeomInfo> geoms_info;
	std::vector<u32> geoms_refCount;
	std::vector<vk::Buffer> geoms_buffer;
	u32 geoms_nextFreeEntry = u32(-1);

	// materials
	std::vector<MaterialManager> materialManagers;
	std::vector<std::vector<u32>> materials_refCount;

	// meshes
	std::vector<MeshInfo> meshes_info;
	std::vector<u32> meshes_refCount;
	u32 meshes_nextFreeEntry = u32(-1);
	
	VkCommandPool cmdPool;
	vk::CmdBuffer cmdBuffers_staging[MAX_SWAPCHAIN_IMAGES + 1]; // we have one extra buffer because we could have a staging cmd buffer "in use" but we still want to record staging cmd for future frames
	u32 cmdBuffers_staging_ind = 0; // that's why we need a separate index for it
	vk::CmdBuffer cmdBuffers_draw[MAX_SWAPCHAIN_IMAGES];

	VkSampler defaultSampler;

	// RenderWorlds
	std::vector<RenderWorld> renderWorlds;
	u32 renderWorlds_nextFreeEntry = u32(-1);

	~RenderUniverse() {
		printf("~RenderUniverse()\n");
	}
};

static RenderUniverse RU;

struct PreparedStagingData {
	vk::Buffer buffer;
	u32 offset;
	u32 dataSize;
};

static vk::CmdBuffer& getCurrentStagingCmdBuffer()
{
	return RU.cmdBuffers_staging[RU.cmdBuffers_staging_ind];
}

static PreparedStagingData prepareStagingData(CSpan<CSpan<u8>> data)
{
	size_t totalSize = 0;
	for (auto d : data)
		totalSize += d.size();
	assert(totalSize <= STAGING_BUFFER_CAPACITY);

	int i;
	for (i = int(RU.spareStagingBuffers.size()) - 1; i >= 0; i--) {
		const u32 remainingSpace = STAGING_BUFFER_CAPACITY - RU.spareStagingBuffers[i].offset;
		if (data.size() <= remainingSpace)
			break;
	}
	if (i == -1) { // no spareStagingBuffer found, need to create one
		i = RU.spareStagingBuffers.size();
		const vk::Buffer stagingBuffer = RU.device.createBuffer(vk::BufferUsage::transferSrc, STAGING_BUFFER_CAPACITY, { .sequentialWrite = true });
		RU.spareStagingBuffers.push_back(RenderUniverse::StagingBuffer{
			.buffer = stagingBuffer,
			.offset = 0,
		});
	}
	auto& sb = RU.spareStagingBuffers[i];
	auto memPtr = RU.device.getBufferMemPtr(sb.buffer) + sb.offset;
	for (auto d : data) {
		memcpy(memPtr, d.data(), d.size());
		memPtr += d.size();
	}

	const size_t offset = sb.offset;
	sb.offset += totalSize;

	return { sb.buffer, u32(offset), u32(totalSize) };
}

static void stageData(vk::Buffer buffer, CSpan<CSpan<u8>> data, size_t dstOffset = 0)
{
	auto [srcBuffer, srcBufferOffset, dataSize] = prepareStagingData(data);
	auto& cmdBuffer = getCurrentStagingCmdBuffer();
	cmdBuffer.cmd_copy(RU.device.getVkHandle(srcBuffer), RU.device.getVkHandle(buffer), srcBufferOffset, dstOffset, dataSize);
}

static void stageData(vk::Buffer buffer, CSpan<u8> data, size_t dstOffset = 0)
{
	const CSpan<u8> datas[] = { data };
	stageData(buffer, datas, dstOffset);
}

static void stageDataToImage(vk::Image img, CSpan<u8> data)
{
	const CSpan<u8> datas[] = { data };
	auto [srcBuffer, srcBufferOffset, dataSize] = prepareStagingData(datas);

	const auto& imgInfo = RU.device.getInfo(img);

	auto& cmdBuffer = getCurrentStagingCmdBuffer();
	const VkBufferImageCopy copy{
		.bufferOffset = srcBufferOffset,
		.bufferRowLength = 0, .bufferImageHeight = 0, // when either of these values is 0, the buffer memory is assumed to be tightly packed according to the imageExtent
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = imgInfo.getNumLayers(),
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {imgInfo.size.width, imgInfo.size.height, imgInfo.size.depth},
	};
	cmdBuffer.cmd_copy(RU.device.getVkHandle(srcBuffer), RU.device.getVkHandle(img), { &copy, 1 });
}

// --- IMAGES ---
ImageInfo ImageId::getInfo()const { return RU.images_info[id]; }
vk::Image ImageId::getHandle()const { return RU.images_vk[id]; }

static u32 acquireImageEntry()
{
	if (RU.images_nextFreeEntry != u32(-1)) {
		// the was a free entry
		const u32 e = RU.images_nextFreeEntry;
		RU.images_nextFreeEntry = RU.images_refCount[e];
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = RU.images_vk.size();
	RU.images_vk.emplace_back();
	RU.images_refCount.emplace_back();
	RU.images_info.emplace_back();
	return e;
}

static void releaseImageEntry(u32 e)
{
	const u32 e2 = RU.images_nextFreeEntry;
	RU.images_refCount[e] = e2;
	RU.images_nextFreeEntry = e;
	// TODO: do some safety check in debug mode for detecting double-free
}

void incRefCount(ImageId id)
{
	if (id == ImageId{})
		return;
	RU.images_refCount[id.id]++;
}
void decRefCount(ImageId id)
{
	if (id == ImageId{})
		return;
	auto& c = RU.images_refCount[id.id];
	c--;
	if (c == 0) {
		RU.imagesToDestroy[RU.swapchain.imgInd].push_back(RU.images_vk[id.id]);
		releaseImageEntry(id.id);
	}
}

u8 calcNumMipsFromDimensions(u32 w, u32 h)
{
	w = glm::max(w, h);
	u8 n = 1;
	while(w > u32(1)) {
		w /= u32(2);
		n++;
	}
	return n;
}

bool nextMipLevelDown(u32& w, u32& h)
{
	w = glm::max(u32(1), w / u32(2));
	h = glm::max(u32(1), h / u32(2));
	return w == u32(1) && h == u32(1);
}

ImageRC getOrLoadImage(Path path, bool srgb, bool generateMipChain)
{
	auto it = RU.images_nameToId.find(path);
	if (it != RU.images_nameToId.end())
		return ImageRC{ it->second };

	int w, h, nc;
	u8* imgData = stbi_load(path.string().c_str(), &w, &h, &nc, 4);
	if(imgData == nullptr)
		return ImageRC(ImageId{});

	assert(w > 0 && h > 0 && imgData != nullptr);
	const u8 numMips = calcNumMipsFromDimensions(u32(w), u32(h));
	const auto format = srgb ? vk::Format::RGBA8_SRGB : vk::Format::RGBA8_UNORM;
	vk::Image imageVk = RU.device.createImage({
		.size = {
			.width = u16(w),
			.height = u16(h),
			.numMips = numMips,
		},
		.format = format,
		.dimensions = 2,
		.numSamples = 1,
		.usage = vk::ImageUsage::default_texture(),
		.layout = vk::ImageLayout::undefined,
	});

	const u32 e = acquireImageEntry();
	RU.images_info[e] = ImageInfo {
		.format = format,
		.w = u16(w), .h = u16(h),
		.numMips = numMips,
	};
	RU.images_vk[e] = imageVk;
	RU.images_refCount[e] = 0;
	ImageId imgId{ e };

	CSpan<u8> datas[] = { { imgData, size_t(w * h * 4) } };
	auto [stagingBuffer, stagingBufferOffset, dataSize] = prepareStagingData(datas);
	RU.images_stagingProcs.push_back(ImageStagingProc{
		.img = imgId,
		.stagingBuffer = stagingBuffer,
		.stagingBufferOffset = u32(stagingBufferOffset),
		.dataSize = dataSize,
		.generateMipChain = generateMipChain
	});

	return ImageRC{imgId};
}

// --- IMAGE VIEWS ---
ImageRC ImageViewId::getImage()const
{
	assert(isValid());
	return RU.imageViews_image[id];
}

vk::ImageView ImageViewId::getHandle()const
{
	assert(isValid());
	return RU.imageViews_vk[id];
}

static u32 acquireImageViewEntry()
{
	if (RU.imageViews_nextFreeEntry != u32(-1)) {
		// the was a free entry
		const u32 e = RU.imageViews_nextFreeEntry;
		RU.imageViews_nextFreeEntry = RU.imageViews_refCount[e];
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = RU.imageViews_vk.size();
	RU.imageViews_vk.emplace_back();
	RU.imageViews_refCount.emplace_back();
	RU.imageViews_image.emplace_back();
	return e;
}

static void releaseImageViewEntry(u32 e)
{
	const u32 e2 = RU.imageViews_nextFreeEntry;
	RU.imageViews_refCount[e] = e2;
	RU.imageViews_nextFreeEntry = e;
	// TODO: do some safety check in debug mode for detecting double-free
}

void incRefCount(ImageViewId id)
{
	if (id == ImageViewId{})
		return;
	RU.imageViews_refCount[id.id]++;
}
void decRefCount(ImageViewId id)
{
	if (id == ImageViewId{})
		return;
	auto& c = RU.imageViews_refCount[id.id];
	c--;
	if (c == 0) {
		RU.imageViewsToDestroy[RU.swapchain.imgInd].push_back(RU.imageViews_vk[id.id]);
		releaseImageViewEntry(id.id);
	}
}

ImageViewRC makeImageView(const MakeImageView& info)
{
	vk::ImageViewInfo infoVk = { .image = info.image.id.getHandle() };
	const u32 e = acquireImageViewEntry();
	RU.imageViews_image[e] = info.image;
	RU.imageViews_vk[e] = RU.device.createImageView(infoVk);
	RU.imageViews_refCount[e] = 0;
	return ImageViewRC{ ImageViewId{e} };
}

// --- GEOMETRY ---
static u32 acquireGeomEntry()
{
	if (RU.geoms_nextFreeEntry != u32(-1)) {
		// the was a free entry
		const u32 e = RU.geoms_nextFreeEntry;
		RU.geoms_nextFreeEntry = RU.geoms_refCount[e];
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = RU.geoms_buffer.size();
	RU.geoms_buffer.emplace_back();
	RU.geoms_refCount.emplace_back();
	RU.geoms_info.emplace_back();
	return e;
}

static void releaseGeomEntry(u32 e)
{
	const u32 e2 = RU.images_nextFreeEntry;
	RU.geoms_refCount[e] = e2;
	RU.geoms_nextFreeEntry = e;
	
	// TODO: do some safety check in debug mode for detecting double-free
}

const GeomInfo& GeomId::getInfo()const
{
	return RU.geoms_info[id];
}
vk::Buffer GeomId::getBuffer()const
{
	return RU.geoms_buffer[id];
}

void incRefCount(GeomId id)
{
	if (id.id == GeomId::invalid().id)
		return;
	RU.geoms_refCount[id.id]++;
}
void decRefCount(GeomId id)
{
	if (id.id == GeomId::invalid().id)
		return;
	auto& c = RU.geoms_refCount[id.id];
	c--;
	if (c == 0) {
		RU.buffersToDestroy[RU.swapchain.imgInd].push_back(RU.geoms_buffer[id.id]);
		releaseGeomEntry(id.id);
	}
}

GeomRC registerGeom(const GeomInfo& info, vk::Buffer buffer)
{
	const u32 id = acquireGeomEntry();
	RU.geoms_info[id] = info;
	RU.geoms_buffer[id] = buffer;
	RU.geoms_refCount[id] = 0;
	return GeomRC(GeomId{ id });
}

GeomRC createGeom(const CreateGeomInfo& info)
{
	GeomInfo geomInfo = { .attribOffset_positions = 0 };
	assert(info.positions.size());
	std::array<CSpan<u8>, 6> datas;
	u32 numDatas = 0;
	u32 offset = 0;
	auto considerAttrib = [&](u32& attribOffset, CSpan<u8> attribData) {
		if (attribData.size()) {
			attribOffset = offset;
			datas[numDatas] = attribData;
			offset += attribData.size();
			numDatas++;
		}
	};
	
	considerAttrib(geomInfo.attribOffset_positions, info.positions);
	considerAttrib(geomInfo.attribOffset_normals, info.normals);
	considerAttrib(geomInfo.attribOffset_tangents, info.tangents);
	considerAttrib(geomInfo.attribOffset_texCoords, info.texCoords);
	considerAttrib(geomInfo.attribOffset_colors, info.colors);
	considerAttrib(geomInfo.indsOffset, info.indices);
	geomInfo.numVerts = info.numVerts;
	geomInfo.numInds = info.numInds;

	auto usage = vk::BufferUsage::indexBuffer | vk::BufferUsage::vertexBuffer | vk::BufferUsage::transferDst;
	auto buffer = RU.device.createBuffer(usage, offset, vk::BufferHostAccess{});
	stageData(buffer, { datas.data(), numDatas});

	return registerGeom(geomInfo, buffer);
}

// --- MATERIAL ---

VkPipeline MaterialId::getPipeline(GeomId geomId)const
{
	auto& mgr = RU.materialManagers[manager.id];
	return mgr.getPipeline(mgr.managerPtr, *this, geomId);
}
VkPipelineLayout MaterialId::getPipelineLayout()const
{
	auto& mgr = RU.materialManagers[manager.id];
	return mgr.getPipelineLayout(mgr.managerPtr, *this);
}

VkDescriptorSet MaterialId::getDescSet()const
{
	auto& mgr = RU.materialManagers[manager.id];
	return mgr.getDescriptorSet(mgr.managerPtr, *this);
}

void incRefCount(MaterialId id)
{
	if (id.isValid()) {
		const auto& materialManagerFns = RU.materialManagers[id.manager.id];
		u32& rc = RU.materials_refCount[id.manager.id][id.id];
		rc++;
	}
}
void decRefCount(MaterialId id)
{
	if (id.isValid()) {
		const auto& materialManagerFns = RU.materialManagers[id.manager.id];
		u32& rc = RU.materials_refCount[id.manager.id][id.id];
		rc--;
		if (rc == 0)
			materialManagerFns.destroyMaterial(materialManagerFns.managerPtr, id);
	}
}

void registerMaterialManager(const MaterialManager& mgr)
{
	const u32 mgrId = RU.materialManagers.size();
	RU.materialManagers.push_back(mgr);
	RU.materials_refCount.emplace_back();
	mgr.setManagerId(mgr.managerPtr, mgrId);
}

// --- PBR MATERIAL ---

void PbrMaterialManager::init(u32 maxExpectedMaterials)
{
	assert(maxExpectedMaterials > 0);
	this->maxExpectedMaterials = maxExpectedMaterials;
	materials_info.reserve(maxExpectedMaterials);
	materials_descSet.reserve(maxExpectedMaterials);
	uniformBuffer = RU.device.createBuffer(vk::BufferUsage::uniformBuffer | vk::BufferUsage::transferDst, maxExpectedMaterials * sizeof(PbrUniforms), {});

	const VkDescriptorPoolSize descPoolSizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 3 * maxExpectedMaterials
		},
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = maxExpectedMaterials,
		}
	};
	descPool = RU.device.createDescriptorPool(maxExpectedMaterials, descPoolSizes, {});
}

static u32 acquireMaterialEntry(PbrMaterialManager& mgr)
{
	if (const u32 e = mgr.materials_nextFreeEntry;
		e != u32(-1))
	{
		// the was a free entry
		mgr.materials_nextFreeEntry = *(const u32*)(&mgr.materials_info[e]);
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = mgr.materials_info.size();
	assert(e < mgr.maxExpectedMaterials);
	mgr.materials_info.emplace_back();
	mgr.materials_descSet.emplace_back();
	RU.materials_refCount[mgr.managerId.id].emplace_back();
	return e;
}

void releaseMaterialEntry(PbrMaterialManager& mgr, u32 e)
{
	const u32 e2 = mgr.materials_nextFreeEntry;
	*(u32*)(&mgr.materials_info[e]) = e2;
	mgr.materials_nextFreeEntry = e;
}

VkDescriptorSetLayout PbrMaterialManager::getCreateDescriptorSetLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture)
{
	auto& dc = descSetLayouts[hasAlbedoTexture][hasNormalTexture][hasMetallicRoughnessTexture];
	if (!dc) {
		const vk::DescriptorSetLayoutBindingInfo bindings[] = {
			{
				.binding = 0,
				.descriptorType = vk::DescriptorType::uniformBuffer,
				.accessStages = vk::ShaderStages::fragment,
			},
			{
				.binding = 1,
				.descriptorType = vk::DescriptorType::combinedImageSampler,
				.accessStages = vk::ShaderStages::fragment,
			},
			{
				.binding = 2,
				.descriptorType = vk::DescriptorType::combinedImageSampler,
				.accessStages = vk::ShaderStages::fragment,
			},
			{
				.binding = 3,
				.descriptorType = vk::DescriptorType::combinedImageSampler,
				.accessStages = vk::ShaderStages::fragment,
			},
		};
		dc = RU.device.createDescriptorSetLayout(bindings);
	}
	return dc;
}

VkPipelineLayout PbrMaterialManager::getCreatePipelineLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture)
{
	auto& l = pipelineLayouts[hasAlbedoTexture][hasNormalTexture][hasMetallicRoughnessTexture];
	if (!l) {
		const VkDescriptorSetLayout descSetLayouts[] = {
			RU.globalDescSetLayout,
			getCreateDescriptorSetLayout(hasAlbedoTexture, hasNormalTexture, hasMetallicRoughnessTexture),
		};
		l = RU.device.createPipelineLayout(descSetLayouts, {});
	}
	return l;
}

VkPipeline PbrMaterialManager::getCreatePipeline(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture,
	HasVertexNormalsOrTangents hasVertexNormalsOrTangents, bool hasTexCoords, bool hasVertexColors, bool doubleSided)
{
	VkPipeline& p = pipelines[hasAlbedoTexture][hasNormalTexture][hasMetallicRoughnessTexture]
		[int(hasVertexNormalsOrTangents)][hasTexCoords][hasVertexColors][doubleSided];
	if (!p) {
		const tk::PreprocDefine defines[] = {
			{"MAX_DIR_LIGHTS", MAX_DIR_LIGHTS_STR.c_str()},
			{"HAS_ALBEDO_TEX", hasTexCoords && hasAlbedoTexture ? "1" : "0"},
			{"HAS_NORMAL_TEX", hasTexCoords && hasNormalTexture ? "1" : "0"},
			{"HAS_METALLIC_ROUGHNESS_TEX", hasTexCoords && hasMetallicRoughnessTexture ? "1" : "0"},
			{"HAS_NORMAL", hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::no ? "0" : "1"},
			{"HAS_TANGENT", hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::normalsAndTangents ? "1" : "0"},
			{"HAS_TEXCOORD_0", hasTexCoords ? "1" : "0"},
			{"HAS_VERTCOLOR_0", hasVertexColors ? "1" : "0"},
		};

		ZStrView vertShadPath = "shaders/pbr.vert.glsl";
		ZStrView fragShadPath = "shaders/pbr.frag.glsl";

		const auto vertShad_compileResult = RU.shaderCompiler.glslToSpv(vertShadPath, defines);
		if (!vertShad_compileResult.ok()) {
			printf("Error compiling VERTEX shader (%s):\n%s\n", vertShadPath.c_str(), vertShad_compileResult.getErrorMsgs().c_str());
			assert(false);
			exit(1);
		}
		auto vertShad = RU.device.createVertShader(vertShad_compileResult.getSpirvSrc());

		const auto fragShad_compileResult = RU.shaderCompiler.glslToSpv(fragShadPath, defines);
		if (!fragShad_compileResult.ok()) {
			printf("Error compiling FRAGMENT shader (%s):\n%s", fragShadPath.c_str(), fragShad_compileResult.getErrorMsgs().c_str());
			assert(false);
			exit(1);
		}
		auto fragShad = RU.device.createFragShader(fragShad_compileResult.getSpirvSrc());

		u32 numBindings = 1;
		std::array<vk::VertexInputBindingInfo, 6> bindings;
		bindings[0] = { // instancing data
				.binding = 0,
				.stride = sizeof(RenderWorld::ObjectMatrices),
				.perInstance = true,
		};
		bindings[numBindings++] = { // a_position
			.binding = numBindings,
			.stride = sizeof(glm::vec3)
		};
		u32 bindingLocation = numBindings;
		// a_normal
		if (hasVertexNormalsOrTangents != HasVertexNormalsOrTangents::no) {
			bindings[numBindings++] = {
				.binding = bindingLocation,
				.stride = sizeof(glm::vec3),
			};
		}
		bindingLocation++;
		// a_tangent
		if (hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::normalsAndTangents) {
			bindings[numBindings++] = {
				.binding = bindingLocation,
				.stride = sizeof(glm::vec3),
			};
		}
		bindingLocation++;
		// a_texCoord_0
		if (hasTexCoords) {
			bindings[numBindings++] = {
				.binding = bindingLocation,
				.stride = sizeof(glm::vec2),
			};
		}
		bindingLocation++;
		// a_color_0
		if (hasVertexColors) {
			bindings[numBindings++] = {
				.binding = bindingLocation,
				.stride = sizeof(glm::vec4),
			};
		}
		//bindingLocation++;

		std::array<vk::VertexInputAttribInfo, 20> attribs;
		u32 numAttribs = 0;
		auto addAttribMat4 = [&]() {
			for (u32 colI = 0; colI < 4; colI++) {
				attribs[numAttribs++] = {
					.location = numAttribs,
					.binding = 0,
					.format = vk::Format::RGBA32_SFLOAT,
					.offset = numAttribs * u32(sizeof(glm::vec4)),
				};
			}
		};
		auto addAttribMat3 = [&]() {
			for (u32 colI = 0; colI < 3; colI++) {
				attribs[numAttribs++] = {
					.location = numAttribs,
					.binding = 0,
					.format = vk::Format::RGB32_SFLOAT,
					.offset = numAttribs * u32(sizeof(glm::vec3)),
				};
			}
		};

		// instance data matrices
		addAttribMat4(); // u_model
		addAttribMat4(); // u_modelViewProj
		addAttribMat3(); // u_invTransModel

		{
			u32 location = numAttribs;
			u32 bindingI = 1;
			auto addAttribFmt = [&](vk::Format format) {
				attribs[numAttribs++] = {
					.location = location,
					.binding = bindingI,
					.format = format,
				};
			};

			// a_position
			addAttribFmt(vk::Format::RGB32_SFLOAT);
			location++;
			bindingI++;
			// a_normal
			if (hasVertexNormalsOrTangents != HasVertexNormalsOrTangents::no)
				addAttribFmt(vk::Format::RGB32_SFLOAT);
			location++;
			bindingI++;
			// a_tangent
			if (hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::normalsAndTangents)
				addAttribFmt(vk::Format::RGB32_SFLOAT);
			location++;
			bindingI++;
			// a_texCoord
			if (hasTexCoords)
				addAttribFmt(vk::Format::RG32_SFLOAT);
			location++;
			bindingI++;
			// a_color_0
			if (hasVertexColors)
				addAttribFmt(vk::Format::RGBA32_SFLOAT);
			//location++;
			//bindingI++;
		}

		const vk::ColorBlendAttachment colorBlendAttachments[] = {
			{} // no blending for now
		};

		const vk::GraphicsPipelineInfo info = {
			.shaderStages = {
				.vertex = {.shader = vertShad},
				.fragment = {.shader = fragShad},
			},
			.vertexInputBindings = {&bindings[0], numBindings},
			.vertexInputAttribs = {&attribs[0], numAttribs},
			.cull_back = !doubleSided,
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.colorBlendAttachments = colorBlendAttachments,
			.layout = getCreatePipelineLayout(hasAlbedoTexture, hasNormalTexture, hasMetallicRoughnessTexture),
			.renderPass = RU.renderPass,
		};
		vk::ASSERT_VKRES(RU.device.createGraphicsPipelines({ &p, 1 }, { &info, 1 }, nullptr));
	}
	return p;
}

PbrMaterialRC PbrMaterialManager::createMaterial(const PbrMaterialInfo& params)
{
	const bool hasAlbedoTexture = params.albedoImageView.id.isValid();
	const bool hasNormalTexture = params.normalImageView.id.isValid();
	const bool hasMetallicRoughnessTexture = params.metallicRoughnessImageView.id.isValid();
	const auto descSetLayout = getCreateDescriptorSetLayout(hasAlbedoTexture, hasNormalTexture, hasMetallicRoughnessTexture);
	VkDescriptorSet descSet;
	vk::ASSERT_VKRES(RU.device.allocDescriptorSets(descPool, descSetLayout, { &descSet, 1 }));
	const u32 entry = acquireMaterialEntry(*this);
	materials_info[entry] = params;
	materials_descSet[entry] = descSet;

	const size_t bufferOffset = sizeof(PbrUniforms) * entry;
	const PbrUniforms values = {
		.albedo = params.albedo,
		.metallic = params.metallic,
		.roughness = params.roughness
	};
	stageData(uniformBuffer, tk::asBytesSpan(values), bufferOffset);

	vk::DescriptorSetWrite descSetWrites[4] = {
		{	.descSet = descSet,
			.binding = 0,
			.type = vk::DescriptorType::uniformBuffer,
			.bufferInfo = {
				.buffer = RU.device.getVkHandle(uniformBuffer),
				.offset = bufferOffset,
				.range = sizeof(PbrUniforms),
			}
		}
	};
	u32 numWrites = 1;
	auto addWriteImgIfNeeded = [&](const ImageViewRC& img, u32 binding) {
		if (img.id.isValid()) {
			descSetWrites[numWrites] = {
				.descSet = descSet,
				.binding = numWrites,
				.type = vk::DescriptorType::combinedImageSampler,
				.imageInfo = {
					.sampler = RU.defaultSampler,
					.imageView = RU.device.getVkHandle(img.id.getHandle()),
					.imageLayout = vk::ImageLayout::shaderReadOnly,
				}
			};
			numWrites++;
		}
	};
	addWriteImgIfNeeded(params.albedoImageView, 1);
	addWriteImgIfNeeded(params.normalImageView, 2);
	addWriteImgIfNeeded(params.metallicRoughnessImageView, 3);

	RU.device.writeDescriptorSets({ descSetWrites, numWrites });
	return PbrMaterialRC(managerId, entry);
}

void PbrMaterialManager::destroyMaterial(MaterialId id)
{
	materials_info[id.id] = {};
	RU.device.freeDescriptorSets(descPool, { &materials_descSet[id.id], 1});
	releaseMaterialEntry(*this, id.id);
}

VkPipeline PbrMaterialManager::getPipeline(MaterialId materialId, GeomId geomId)
{
	const auto& materialInfo = materials_info[materialId.id];
	const bool hasAlbedoTexture = materialInfo.albedoImageView.id.isValid();
	const bool hasNormalTexture = materialInfo.normalImageView.id.isValid();
	const bool hasMetallicRoughnessTexture = materialInfo.metallicRoughnessImageView.id.isValid();

	const auto& geomInfo = geomId.getInfo();
	const HasVertexNormalsOrTangents hasVertexNormalsOrTangents =
		geomInfo.attribOffset_tangents != u32(-1) ? HasVertexNormalsOrTangents::normalsAndTangents :
		geomInfo.attribOffset_normals != u32(-1) ? HasVertexNormalsOrTangents::normals : HasVertexNormalsOrTangents::no;
	const bool hasTexCoords = geomInfo.attribOffset_texCoords != u32(-1);
	const bool hasVertexColors = geomInfo.attribOffset_colors != u32(-1);

	return getCreatePipeline(hasAlbedoTexture, hasNormalTexture, hasMetallicRoughnessTexture,
		hasVertexNormalsOrTangents, hasTexCoords, hasVertexColors, materialInfo.doubleSided);
}

VkPipelineLayout PbrMaterialManager::getPipelineLayout(MaterialId materialId)
{
	const auto& materialInfo = materials_info[materialId.id];
	const bool hasAlbedoTexture = materialInfo.albedoImageView.id.isValid();
	const bool hasNormalTexture = materialInfo.normalImageView.id.isValid();
	const bool hasMetallicRoughnessTexture = materialInfo.metallicRoughnessImageView.id.isValid();
	return pipelineLayouts[hasAlbedoTexture][hasNormalTexture][hasMetallicRoughnessTexture];
}

// --- MESHES ---

static u32 acquireMeshEntry()
{
	if (RU.meshes_nextFreeEntry != u32(-1)) {
		// there was a free entry
		const u32 e = RU.images_nextFreeEntry;
		RU.meshes_nextFreeEntry = RU.meshes_refCount[e];
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = RU.meshes_info.size();
	RU.meshes_info.emplace_back();
	RU.meshes_refCount.emplace_back();
	return e;
}

static void releaseMeshEntry(u32 e)
{
	const u32 e2 = RU.meshes_nextFreeEntry;
	RU.meshes_refCount[e] = e2;
	RU.meshes_nextFreeEntry = e;
}

const MeshInfo MeshId::getInfo()const
{
	return RU.meshes_info[id];
}
VkPipeline MeshId::getPipeline()const
{
	const auto& info = RU.meshes_info[id];
	return info.material.id.getPipeline(info.geom.id);
}

void incRefCount(MeshId id)
{
	if (id.isValid()) {
		RU.meshes_refCount[id.id]++;
	}
}
void decRefCount(MeshId id)
{
	if (id.isValid()) {
		u32& rc = RU.meshes_refCount[id.id];
		rc--;
		if (rc == 0) {
			RU.meshes_info[id.id] = MeshInfo{};
			releaseMeshEntry(id.id);
		}
	}
}

MeshRC makeMesh(MeshInfo&& info)
{
	const u32 e = acquireMeshEntry();
	RU.meshes_info[e] = std::move(info);
	RU.meshes_refCount[e] = 0;
	return MeshRC({ e });
}

// --- OBJECTS ---
ObjectInfo ObjectId::getInfo()const
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	return RW.objects_info[e];
}

void ObjectId::setModelMatrix(const glm::mat4& m, u32 instanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	assert(instanceInd < RW.objects_info[e].numInstances);
	RW.modelMatrices[RW.objects_firstModelMtx[e] + instanceInd] = m;
}

void ObjectId::setModelMatrices(CSpan<glm::mat4> matrices, u32 firstInstanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	assert(firstInstanceInd + matrices.size() <= RW.objects_info[e].numInstances);
	for (size_t i = 0; i < matrices.size(); i++)
		RW.modelMatrices[RW.objects_firstModelMtx[e] + firstInstanceInd + i] = matrices[i];
}

bool ObjectId::addInstances(u32 n)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	auto& info = RW.objects_info[e];
	if (info.numInstances + n <= info.maxInstances) {
		info.numInstances += n;
	}
	else {
		return false;
	}
}

void ObjectId::destroyInstance(u32 instanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	auto& info = RW.objects_info[e];
	assert(instanceInd < info.numInstances);
	const u32 fmm = RW.objects_firstModelMtx[e];
	RW.modelMatrices[fmm + instanceInd] = RW.modelMatrices[fmm + info.numInstances - 1];
	info.numInstances--;
}

static void begingStagingCmdRecordingForNextFrame()
{
	// begin staging cmd recording for the next frame
	RU.cmdBuffers_staging_ind = (RU.cmdBuffers_staging_ind + 1) % (RU.swapchain.numImages + 1);
	getCurrentStagingCmdBuffer().begin(vk::CmdBufferUsageFlags{ .oneTimeSubmit = true });
}

// *** INIT RENDER UNIVERSE ***
void initRenderUniverse(const InitRenderUniverseParams& params)
{
	RU.oldScreenW = RU.screenW = params.screenW;
	RU.oldScreenH = RU.screenH = params.screenH;
	RU.surface = params.surface;
	{ // create device
		std::vector<vk::PhysicalDeviceInfo> physicalDeviceInfos;
		vk::getPhysicalDeviceInfos(params.instance, physicalDeviceInfos, params.surface);
		const u32 bestPhysicalDeviceInd = vk::chooseBestPhysicalDevice(physicalDeviceInfos, {});
		const auto& bestPhysicalDeviceInfo = physicalDeviceInfos[bestPhysicalDeviceInd];

		RU.queueFamily = bestPhysicalDeviceInfo.findGraphicsAndPresentQueueFamily();
		assert(RU.queueFamily < bestPhysicalDeviceInfo.queueFamiliesProps.size());

		const float queuePriorities[] = { 1.f };
		const vk::QueuesCreateInfo queuesInfos[] = { {
			.queueFamily = RU.queueFamily,
			.queuePriorities = queuePriorities,
		} };
		vk::ASSERT_VKRES(vk::createDevice(RU.device, params.instance, bestPhysicalDeviceInfo, queuesInfos));
	}

	const VkQueue mainQueue = RU.device.queues[0][0]; // TODO: more sofisticated queue handling, detect compute queues, transfer queues, etc

	// https://developer.nvidia.com/blog/vulkan-dos-donts/
	// - Prefer using 24-bit depth formats for optimal performance
	// - Prefer using packed depth/stencil formats. This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
	const vk::Format bufferDepthStencilFormats[] = {
		vk::Format::D24_UNORM_S8_UINT,
		vk::Format::D16_UNORM_S8_UINT,
		vk::Format::D32_SFLOAT_S8_UINT,
		vk::Format::X8_D24_UNORM,
		vk::Format::D32_SFLOAT,
		vk::Format::D16_UNORM,
	};
	RU.depthStencilFormat = RU.device.getSupportedDepthStencilFormat_firstAmong(bufferDepthStencilFormats);

	// create swapchain
	RU.swapchainOptions = {};
	vk::ASSERT_VKRES(vk::createSwapchainSyncHelper(RU.swapchain, RU.surface, RU.device, RU.swapchainOptions));

	RU.renderPass = [&]() {
		vk::FbAttachmentInfo attachments[] = {
			{.format = RU.swapchain.format.format},
			{.format = RU.depthStencilFormat}
		};
		const vk::AttachmentOps attachmentOps[] = {
			{ // color
				.loadOp = vk::Attachment_LoadOp::clear,
				.expectedLayout = vk::ImageLayout::undefined,
				.finalLayout = vk::ImageLayout::presentSrc,
			},
			{ // depth
				.loadOp = vk::Attachment_LoadOp::clear,
				.storeOp = vk::Attachment_StoreOp::dontCare,
				.expectedLayout = vk::ImageLayout::undefined,
				.finalLayout = vk::ImageLayout::depthStencilAttachment, // we don't care about the contents of the depth buffer after the pass
			}
		};
		static_assert(std::size(attachments) == std::size(attachmentOps));
		const u32 colorAttachments[] = { 0 };
		return RU.device.createRenderPass_simple({
			.attachments = attachments,
			.attachmentOps = attachmentOps,
			.colorAttachments = colorAttachments,
			.depthStencilAttachment = 1,
		});
	}();

	RU.shaderCompiler.init();

	vk::ASSERT_VKRES(vk::createSwapchainSyncHelper(RU.swapchain, RU.surface, RU.device, RU.swapchainOptions));

	RU.cmdPool = RU.device.createCmdPool(RU.queueFamily, { .transientCmdBuffers = true, .reseteableCmdBuffers = true });
	RU.device.allocCmdBuffers(RU.cmdPool, RU.cmdBuffers_staging, false);
	RU.device.allocCmdBuffers(RU.cmdPool, RU.cmdBuffers_draw, false);
	RU.cmdBuffers_staging_ind = 0;

	{ // global descset layout
		const vk::DescriptorSetLayoutBindingInfo bindings[] = {
			{
				.binding = DESCSET_GLOBAL,
				.descriptorType = vk::DescriptorType::uniformBuffer,
				.accessStages = vk::ShaderStages::fragment,
			}
		};
		RU.globalDescSetLayout = RU.device.createDescriptorSetLayout(bindings);
	}

	RU.defaultSampler = RU.device.createSampler(vk::Filter::linear, vk::Filter::linear, vk::SamplerMipmapMode::linear, vk::SamplerAddressMode::clampToEdge);

	begingStagingCmdRecordingForNextFrame();
}

void onWindowResized(u32 w, u32 h)
{
	RU.oldScreenW = RU.screenW;
	RU.oldScreenH = RU.screenH;
	RU.screenW = w;
	RU.screenH = h;
}

ShaderCompiler& getShaderCompiler()
{
	return RU.shaderCompiler;
}

// *** RENDER WORLD ***
static u32 acquireRenderWorldEntry()
{
	if (RU.renderWorlds_nextFreeEntry != u32(-1)) {
		// there was a free entry
		const u32 e = RU.renderWorlds_nextFreeEntry;
		RU.renderWorlds_nextFreeEntry = *(const u32*)&RU.renderWorlds[e];
		return e;
	}

	// there wasn't a free entry; need to create one
	const u32 e = RU.renderWorlds.size();
	RU.renderWorlds.emplace_back();
	return e;
}

RenderWorldId createRenderWorld()
{
	const u32 entry = acquireRenderWorldEntry();
	auto& RW = RU.renderWorlds[entry];
	RW.id = { entry };
	const u32 numExpectedObjects = 1 << 10;
	RW.objects_id_to_entry.reserve(numExpectedObjects);
	RW.objects_info.reserve(numExpectedObjects);
	RW.objects_firstModelMtx.reserve(numExpectedObjects);
	RW.modelMatrices.reserve(numExpectedObjects);
	RW.objects_matricesTmp.reserve(numExpectedObjects);
	RW.objects_instancesCursorsTmp.reserve(numExpectedObjects);
	const u32 numScImages = RU.swapchain.numImages;
	RW.instancingBuffers.resize(numScImages);
	
	RW.global_uniformBuffers.resize(numScImages);
	{
		const auto usage = vk::BufferUsage::uniformBuffer;
		const size_t size = sizeof(GlobalUniforms_Header) + MAX_DIR_LIGHTS * sizeof(GlobalUniforms_DirLight);
		for (auto& b : RW.global_uniformBuffers) {
			b = RU.device.createBuffer(usage, size, {.sequentialWrite = true});
		}
	}
	RW.global_descSets.resize(numScImages);
	{
		const VkDescriptorPoolSize sizes[] = { { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = RU.swapchain.numImages } };
		RW.global_descPool = RU.device.createDescriptorPool(RU.swapchain.numImages, sizes, {});

		vk::ASSERT_VKRES(RU.device.allocDescriptorSets(RW.global_descPool, RU.globalDescSetLayout, RW.global_descSets));

		vk::DescriptorSetWrite writes[MAX_SWAPCHAIN_IMAGES];
		for (u32 i = 0; i < numScImages; i++) {
			writes[i] = {
				.descSet = RW.global_descSets[i],
				.binding = 0,
				.type = vk::DescriptorType::uniformBuffer,
				.bufferInfo = { .buffer = RU.device.getVkHandle(RW.global_uniformBuffers[i]), },
			};		
		}
		RU.device.writeDescriptorSets({writes, numScImages});
	}

	return RW.id;
}

void destroyRenderWorld(RenderWorldId id)
{
	RU.renderWorlds[id.id] = {};
}

static u32 acquireObjectEntry(RenderWorld& RW)
{

	// there wasn't a free entry; need to create one
	const u32 e = RW.objects_info.size();
	RW.objects_entry_to_id.emplace_back();
	RW.objects_info.emplace_back();
	RW.objects_firstModelMtx.emplace_back();
	return e;
}

static u32 acquireObjectId(RenderWorld& RW)
{
	if (RW.objects_nextFreeId != u32(-1)) {
		// there was a free id
		const u32 id = RW.objects_nextFreeId;
		RW.objects_nextFreeId = RW.objects_id_to_entry[id];
		return id;
	}

	const u32 id = RW.objects_id_to_entry.size();
	RW.objects_id_to_entry.emplace_back();
	return id;
}

static void releaseObjectId(RenderWorld& RW, u32 id)
{
	RW.objects_id_to_entry[id] = RW.objects_nextFreeId;
	RW.objects_nextFreeId = id;
}

ObjectId RenderWorldId::createObject(MeshRC mesh, const glm::mat4& modelMtx, u32 maxInstances)
{
	return RU.renderWorlds[id].createObject(std::move(mesh), modelMtx, maxInstances);
}
ObjectId RenderWorldId::createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances)
{
	return RU.renderWorlds[id].createObjectWithInstancing(std::move(mesh), instancesMatrices, maxInstances);
}
ObjectId RenderWorldId::createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances)
{
	return RU.renderWorlds[id].createObjectWithInstancing(std::move(mesh), numInstances, maxInstances);
}
void RenderWorldId::destroyObject(ObjectId oid)
{
	RU.renderWorlds[id].destroyObject(oid);
}

template<bool PROVIDE_DATA>
static ObjectId _createObjectWithInstancing(RenderWorld& RW, MeshRC mesh, u32 numInstances, const glm::mat4* instancesMatrices, u32 maxInstances)
{
	maxInstances = glm::max(maxInstances, numInstances);
	const u32 e = acquireObjectEntry(RW);
	const u32 oid = acquireObjectId(RW);
	RW.objects_entry_to_id[e] = oid;
	RW.objects_id_to_entry[oid] = e;
	RW.objects_info[e] = { mesh, numInstances, maxInstances };
	RW.objects_firstModelMtx[e] = u32(RW.modelMatrices.size());
	if constexpr (PROVIDE_DATA) {
		for (u32 i = 0; i < numInstances; i++) {
			const auto& m = instancesMatrices[i];
			RW.modelMatrices.push_back(m);
		}
		RW.modelMatrices.resize(RW.modelMatrices.size() + maxInstances - numInstances);
	}
	else {
		RW.modelMatrices.resize(RW.modelMatrices.size() + size_t(maxInstances));
	}
	return ObjectId(RW.id, { oid });
}

ObjectId RenderWorld::createObject(MeshRC mesh, const glm::mat4& modelMtx, u32 maxInstances)
{
	return _createObjectWithInstancing<true>(*this, std::move(mesh), 1, &modelMtx, maxInstances);
}

ObjectId RenderWorld::createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances)
{
	return _createObjectWithInstancing<true>(*this, std::move(mesh), instancesMatrices.size(), instancesMatrices.data(), maxInstances);
}

ObjectId RenderWorld::createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances)
{
	return _createObjectWithInstancing<false>(*this, std::move(mesh), numInstances, nullptr, maxInstances);
}

void RenderWorld::destroyObject(ObjectId oid)
{
	const u32 e = objects_id_to_entry[oid.id];
	objects_info[e].mesh = MeshRC{};
	//releaseObjectEntry(*this, e);
	releaseObjectId(*this, oid.id);
	needDefragmentObjects = true;
}

void RenderWorld::_defragmentObjects()
{
	if (!needDefragmentObjects)
		return;

	u32 mtxI = 0;
	u32 numObjs = objects_info.size();
	u32 objI;
	for (objI = 0; objI < numObjs; objI++) {
		if (objects_info[objI].mesh.id.isValid())
			mtxI += objects_info[objI].maxInstances;
		else
			break;
	}

	for (u32 objJ = objI + 1; objJ < numObjs; objJ++) {
		auto& info = objects_info[objJ];
		if (info.mesh.id.isValid()) {
			const u32 firstModelMtx = objects_firstModelMtx[objJ];
			const u32 numInstances = info.numInstances;
			objects_info[objI] = std::move(info);
			objects_firstModelMtx[objI] = mtxI;
			const u32 oid = objects_entry_to_id[objJ];
			objects_entry_to_id[objI] = oid;
			objects_id_to_entry[oid] = objI;
			const u32 nextObjMtxI = mtxI + info.maxInstances;
			for (u32 i = 0; i < numInstances; i++) {
				modelMatrices[mtxI] = modelMatrices[firstModelMtx + i];
				mtxI++;
			}
			mtxI = nextObjMtxI;

			objI++;
		}
	}

	objects_info.resize(objI);
	objects_firstModelMtx.resize(objI);
	objects_entry_to_id.resize(objI);
	modelMatrices.resize(mtxI);
	needDefragmentObjects = false;
}

// *** DRAW ***
static void draw_renderWorld(RenderWorldId renderWorldId, const RenderWorldViewport& rwViewport, u32 viewportInd)
{
	auto& RW = RU.renderWorlds[renderWorldId.id];

	// update global uniform buffer
	const u32 scImgInd = RU.swapchain.imgInd;
	auto& globalUnifBuffer = RW.global_uniformBuffers[scImgInd];
	{
		u8* data = RU.device.getBufferMemPtr(globalUnifBuffer);
		const GlobalUniforms_Header header = {
			.ambientLight = glm::vec3(0.5f),
			.numDirLights = 0,
		};
		memcpy(data, &header, sizeof(header));
		RU.device.flushBuffer(globalUnifBuffer);
	}

	RW._defragmentObjects();

	const size_t numObjects = RW.objects_info.size();
	u32 totalInstances = 0;
	for (size_t objectI = 0; objectI < numObjects; objectI++) {
		totalInstances += RW.objects_info[objectI].numInstances;
	}
	RW.objects_matricesTmp.resize(totalInstances);

	const glm::mat4 viewProj = rwViewport.projMtx * rwViewport.viewMtx;
	u32 instancesCursor_src = 0;
	u32 instancesCursor_dst = 0;
	RW.objects_instancesCursorsTmp.resize(numObjects);
	for (size_t objectI = 0; objectI < numObjects; objectI++) {
		auto& objInfo = RW.objects_info[objectI];
		for (size_t instanceI = 0; instanceI < objInfo.numInstances; instanceI++) {
			const glm::mat4& modelMtx = RW.modelMatrices[instancesCursor_src + instanceI];
			auto& Ms = RW.objects_matricesTmp[instancesCursor_dst + instanceI];
			Ms.modelMtx = modelMtx;
			Ms.modelViewProj = viewProj * modelMtx;
			Ms.invTransModelView = glm::inverse(glm::transpose(modelMtx));
		}
		RW.objects_instancesCursorsTmp[objectI] = instancesCursor_dst;
		instancesCursor_src += RW.objects_info[objectI].maxInstances;
		instancesCursor_dst += RW.objects_info[objectI].numInstances;
	}

	const size_t instancingBufferRequiredSize = 3 * sizeof(glm::mat4) * size_t(totalInstances);
	const size_t instancingBufferRequiredExtendedSize = 4 * sizeof(glm::mat4) * size_t(totalInstances); // 33% more that the minimum required size
	// NOTE: notice we don't use a staging buffer here
	// From what I've read, since we are going to use it only once, it should be akoy to use a host-visible buffer for instancing
	auto& instancingBuffers = RW.instancingBuffers[RU.swapchain.imgInd];
	assert(viewportInd <= instancingBuffers.size());
	if (instancingBuffers.size() == viewportInd) {
		vk::Buffer b = RU.device.createBuffer(vk::BufferUsage::vertexBuffer, instancingBufferRequiredExtendedSize, { .sequentialWrite = true });
		instancingBuffers.push_back(b);
	}
	else {
		auto& instancingBuffer = instancingBuffers[viewportInd];
		const size_t instancingBufferSize = RU.device.getBufferSize(instancingBuffer);
		if (instancingBufferSize < instancingBufferRequiredSize) {
			// need a bigger buffer
			RU.device.destroyBuffer(instancingBuffer);
			instancingBuffer = RU.device.createBuffer(vk::BufferUsage::vertexBuffer, instancingBufferRequiredExtendedSize, { .sequentialWrite = true });
		}
	}

	auto& instancingBuffer = instancingBuffers[viewportInd];
	u8* bufferMem = RU.device.getBufferMemPtr(instancingBuffer);
	memcpy(bufferMem, RW.objects_matricesTmp.data(), instancingBufferRequiredSize);
	RU.device.flushBuffer(instancingBuffer);

	auto& cmdBuffer_draw = RU.cmdBuffers_draw[RU.swapchain.imgInd];
	cmdBuffer_draw.cmd_viewport(rwViewport.viewport);
	cmdBuffer_draw.cmd_scissor(rwViewport.scissor);

	for (size_t objectI = 0; objectI < numObjects; objectI++) {
		auto& objInfo = RW.objects_info[objectI];
		auto meshId = objInfo.mesh.id;
		auto meshInfo = meshId.getInfo();
		auto materialId = meshInfo.material.id;
		
		cmdBuffer_draw.cmd_bindGraphicsPipeline(meshId.getPipeline());
		
		cmdBuffer_draw.cmd_bindDescriptorSet(vk::PipelineBindPoint::graphics, materialId.getPipelineLayout(), DESCSET_GLOBAL, RW.global_descSets[scImgInd]); // TODO: move outside of the loop
		cmdBuffer_draw.cmd_bindDescriptorSet(vk::PipelineBindPoint::graphics, materialId.getPipelineLayout(), DESCSET_MATERIAL, materialId.getDescSet());
		
		const auto geomId = meshInfo.geom.id;
		const auto& geomInfo = geomId.getInfo();
		const auto geomBuffer = RU.device.getVkHandle(geomId.getBuffer());

		// instancing buffer
		cmdBuffer_draw.cmd_bindVertexBuffer(0, RU.device.getVkHandle(instancingBuffer), sizeof(RenderWorld::ObjectMatrices) * RW.objects_instancesCursorsTmp[objectI]);
		// positions
		cmdBuffer_draw.cmd_bindVertexBuffer(1, geomBuffer, geomInfo.attribOffset_positions);
		// normals
		if(geomInfo.attribOffset_normals != u32(-1))
			cmdBuffer_draw.cmd_bindVertexBuffer(2, geomBuffer, geomInfo.attribOffset_normals);
		// tangents
		if (geomInfo.attribOffset_tangents != u32(-1))
			cmdBuffer_draw.cmd_bindVertexBuffer(3, geomBuffer, geomInfo.attribOffset_tangents);
		// texCoords
		if (geomInfo.attribOffset_texCoords != u32(-1))
			cmdBuffer_draw.cmd_bindVertexBuffer(4, geomBuffer, geomInfo.attribOffset_texCoords);
		// colors
		if (geomInfo.attribOffset_colors != u32(-1))
			cmdBuffer_draw.cmd_bindVertexBuffer(5, geomBuffer, geomInfo.attribOffset_colors);

		// index buffer
		if (geomInfo.indsOffset == u32(-1)) {
			cmdBuffer_draw.cmd_draw(geomInfo.numVerts, objInfo.numInstances, 0, 0);
		}
		else {
			cmdBuffer_draw.cmd_bindIndexBuffer(geomBuffer, VK_INDEX_TYPE_UINT32, geomInfo.indsOffset);
			cmdBuffer_draw.cmd_drawIndexed(geomInfo.numInds, objInfo.numInstances);
		}
	}
}

void draw(CSpan<RenderWorldViewport> viewports)
{
	if (RU.screenW != RU.oldScreenW || RU.screenH != RU.oldScreenH) {
		// detect window resize -> recreate the swapchain
		RU.device.waitIdle();
		vk::createSwapchainSyncHelper(RU.swapchain, RU.surface, RU.device, RU.swapchainOptions);
		for (u32 i = 0; i < RU.swapchain.numImages; i++) {
			RU.device.destroyImage(RU.depthStencilImages[i]);
			RU.device.destroyImageView(RU.depthStencilImageViews[i]);
			RU.device.destroyFramebuffer(RU.framebuffers[i]);
			RU.framebuffers[i] = VK_NULL_HANDLE;
		}
		RU.oldScreenW = RU.screenW;
		RU.oldScreenH = RU.screenH;
	}

	RU.swapchain.acquireNextImage(RU.device);

	const auto mainQueue = RU.device.queues[0][0];

	RU.swapchain.waitCanStartFrame(RU.device);
	const u32 scImgInd = RU.swapchain.imgInd;

	// destroy resources that had been scheduled
	for (auto& x : RU.buffersToDestroy[scImgInd])
		RU.device.destroyBuffer(x);
	RU.buffersToDestroy[scImgInd].resize(0);

	for (auto& x : RU.imageViewsToDestroy[scImgInd])
		RU.device.destroyImageView(x);
	RU.imageViewsToDestroy[scImgInd].resize(0);

	for (auto& x : RU.imagesToDestroy[scImgInd])
		RU.device.destroyImage(x);
	RU.imagesToDestroy[scImgInd].resize(0);

	auto& framebuffer = RU.framebuffers[scImgInd];
	if (!framebuffer) {
		RU.depthStencilImages[scImgInd] = RU.device.createImage({
			.size = {u16(RU.screenW), u16(RU.screenH)},
			.format = RU.depthStencilFormat,
			.usage = vk::ImageUsage::default_framebuffer(false, false),
		});
		RU.depthStencilImageViews[scImgInd] = RU.device.createImageView({ .image = RU.depthStencilImages[scImgInd]	});

		const vk::ImageView attachments[] = {
			RU.swapchain.imageViews[scImgInd],
			RU.depthStencilImageViews[scImgInd]
		};
		framebuffer = RU.device.createFramebuffer({
			.renderPass = RU.renderPass,
			.attachments = attachments,
			.width = u32(RU.screenW), .height = u32(RU.screenH)
		});
	}

	// flush staging buffers
	for (const auto& sb : RU.spareStagingBuffers) {
		if (sb.offset)
			RU.device.flushBuffer(sb.buffer);
	}

	auto& cmdBuffer_staging = getCurrentStagingCmdBuffer();
	auto& cmdBuffer_draw = RU.cmdBuffers_draw[scImgInd];
	cmdBuffer_draw.begin(vk::CmdBufferUsageFlags{ .oneTimeSubmit = true });

	// staging to buffers
	const vk::MemoryBarrier stagingMemoryBarriers[] = { {
		.srcAccess = vk::AccessFlags::transferWrite,
		.dstAccess = vk::AccessFlags::vertexAttributeRead | vk::AccessFlags::indexRead | vk::AccessFlags::shaderRead,
	} };
	cmdBuffer_staging.cmd_pipelineBarrier({
		.srcStages = vk::PipelineStages::transfer,
		.dstStages = vk::PipelineStages::allCmds, // TODO: keep trach of what kind of destination resources have been staged and build a proper mask. Also, if there hasn't been any staging operation, we can avoid putting the barrier
		.dependencyFlags = vk::DependencyFlags::none,
		.memoryBarriers = stagingMemoryBarriers,
		.bufferBarriers = {},
		.imageBarriers = {}, // TODO
	});

	// staging to images
	if (const size_t N = RU.images_stagingProcs.size(); N != 0) {
		// - images to transferDst layout
		std::vector<vk::Image> images(N);
		for (size_t i = 0; i < N; i++) {
			const auto& proc = RU.images_stagingProcs[i];
			images[i] = proc.img.getHandle();
		}
		cmdBuffer_staging.cmd_pipelineBarrier_imagesToTransferDstLayout(RU.device, images);

		// copy data to images
		for (size_t i = 0; i < N; i++) {
			const auto& proc = RU.images_stagingProcs[i];
			cmdBuffer_staging.cmd_copy(proc.stagingBuffer, proc.img.getHandle(), RU.device, proc.stagingBufferOffset);
		}

		u8 maxLevels = 1;
		u32 numGenerateMipChains = 0;
		for (const auto& proc : RU.images_stagingProcs) {
			if (proc.generateMipChain) {
				maxLevels = glm::max(proc.img.getInfo().numMips, maxLevels);
				numGenerateMipChains++;
			}
		}

		// generate the mipchain with blit operations (and the necessary layout transitions)
		// in order to minimize the number of barriers, we place one shared(among different images) barrier per lvl. This should also allow the blit of separate images to run in parallel
		std::vector<vk::ImageBarrier> imgBarriers;
		imgBarriers.reserve(numGenerateMipChains);
		for (u8 invLevel = maxLevels-1; invLevel; invLevel--) {
			// transition to transferRead layout
			imgBarriers.resize(0);
			for (size_t i = 0; i < RU.images_stagingProcs.size(); i++) {
				const auto& proc = RU.images_stagingProcs[i];
				const auto& imgInfo = proc.img.getInfo();
				const u8 numMips = imgInfo.numMips;
				if (proc.generateMipChain && invLevel <= numMips) {
					const u8 lvl = numMips - invLevel - 1;
					const vk::Image imgVk = proc.img.getHandle();

					imgBarriers.push_back(vk::ImageBarrier{
						.srcAccess = vk::AccessFlags::transferWrite,
						.dstAccess = vk::AccessFlags::transferRead,
						.srcLayout = vk::ImageLayout::transferDst,
						.dstLayout = vk::ImageLayout::transferSrc,
						.image = RU.device.getVkHandle(imgVk),
						.subresourceRange = {
							.baseMip = lvl,
							.numLayers = imgInfo.getNumLayers(),
						},
					});
				}
			}
			const vk::PipelineBarrier pipelineBarrier = {
				.srcStages = vk::PipelineStages::transfer,
				.dstStages = vk::PipelineStages::transfer,
				.imageBarriers = imgBarriers
			};
			cmdBuffer_staging.cmd_pipelineBarrier(pipelineBarrier);

			// blit!
			for (size_t i = 0; i < RU.images_stagingProcs.size(); i++) {
				const auto& proc = RU.images_stagingProcs[i];
				const auto& imgInfo = proc.img.getInfo();
				const u8 numMips = imgInfo.numMips;
				if (proc.generateMipChain && invLevel <= numMips) {
					const u8 lvl = numMips - invLevel - 1;
					const vk::Image imgVk = proc.img.getHandle();

					cmdBuffer_staging.cmd_blitToNextMip(RU.device, imgVk, lvl);
				}
			}
		}

		// transtion to shaderRead layout
		imgBarriers.resize(0);
		for (size_t i = 0; i < RU.images_stagingProcs.size(); i++) {
			const auto& proc = RU.images_stagingProcs[i];
			if (proc.generateMipChain) {
				const auto& imgInfo = proc.img.getInfo();
				const u8 numMips = imgInfo.numMips;
				const vk::Image imgVk = proc.img.getHandle();

				// most levels are in the transferSrc layout...
				if (numMips > 1) {
					imgBarriers.push_back(vk::ImageBarrier{
						.srcAccess = vk::AccessFlags::transferRead,
						.dstAccess = vk::AccessFlags::shaderRead,
						.srcLayout = vk::ImageLayout::transferSrc,
						.dstLayout = vk::ImageLayout::shaderReadOnly,
						.image = RU.device.getVkHandle(imgVk),
						.subresourceRange = {
							.baseMip = 0,
							.numMips = numMips - 1u,
							.numLayers = imgInfo.getNumLayers(),
						},
					});
				}

				// ...but the smallest one is in the transferDst layout
				imgBarriers.push_back(vk::ImageBarrier{
					.srcAccess = vk::AccessFlags::transferWrite,
					.dstAccess = vk::AccessFlags::shaderRead,
					.srcLayout = vk::ImageLayout::transferDst,
					.dstLayout = vk::ImageLayout::shaderReadOnly,
					.image = RU.device.getVkHandle(imgVk),
					.subresourceRange = {
						.baseMip = numMips - 1u,
						.numMips = 1,
						.numLayers = imgInfo.getNumLayers(),
					},
				});
			}
		}
		if (imgBarriers.size()) {
			cmdBuffer_staging.cmd_pipelineBarrier({
				.srcStages = vk::PipelineStages::transfer,
				.dstStages = vk::PipelineStages::fragmentShader, // assuming that images are just needed in the frament shader
				.memoryBarriers = {},
				.bufferBarriers = {},
				.imageBarriers = imgBarriers,
			});
		}

		RU.images_stagingProcs.resize(0);
	}

	cmdBuffer_staging.end();
	
	// begin renderPass
	const VkClearValue clearValues[] = {
		{.color = {.float32 = {0.1f, 0.1f, 0.1f, 0.f}}},
		{.depthStencil = {.depth = 1.f, .stencil = 0}}
	};
	cmdBuffer_draw.cmd_beginRenderPass({
		.renderPass = RU.renderPass,
		.framebuffer = framebuffer,
		.renderArea = {0,0, u32(RU.screenW), u32(RU.screenH)},
		.clearValues = clearValues,
	});
	
	// draw the viewports
	for (u32 viewportI = 0; viewportI < u32(viewports.size()); viewportI++) {
		const auto& viewport = viewports[viewportI];
		const RenderWorldId renderWorldId = viewport.renderWorld;
		RenderWorld& RW = RU.renderWorlds[renderWorldId.id];
		draw_renderWorld(renderWorldId, viewport, viewportI);
	}
	
	// end render pass
	cmdBuffer_draw.cmd_endRenderPass();

	// submit everything
	cmdBuffer_draw.end();
	{
		const std::tuple<VkSemaphore, vk::PipelineStages> waitSemaphores[] = {
			{
				RU.swapchain.semaphore_imageAvailable[RU.swapchain.frameInd],
				vk::PipelineStages::colorAttachmentOutput
			}
		};
		const vk::CmdBuffer cmdBuffers[] = { cmdBuffer_staging, cmdBuffer_draw };
		RU.device.submit(mainQueue, {
			vk::SubmitInfo {
				.waitSemaphores = waitSemaphores,
				.cmdBuffers = cmdBuffers,
				.signalSemaphores = {&RU.swapchain.semaphore_drawFinished[scImgInd], 1},
			},
		}, RU.swapchain.fence_drawFinished[scImgInd]);
	}

	// move staging buffers around: spareStagingBuffers <--> bussyStagingBuffers
	{
		// TODO: optimize?
		static std::vector<RenderUniverse::StagingBuffer> newBussy;
		newBussy.resize(0);
		for (int i = int(RU.spareStagingBuffers.size()) - 1; i >= 0; i--) {
			if (RU.spareStagingBuffers[i].offset) {
				newBussy.push_back(RU.spareStagingBuffers[i]);
				RU.spareStagingBuffers.erase(RU.spareStagingBuffers.begin() + i);
			}
		}

		auto& bussy = RU.bussyStagingBuffers[scImgInd];
		for (const auto& b : bussy)
			RU.spareStagingBuffers.push_back(b);
		std::swap(bussy, newBussy);
	}

	// present
	RU.swapchain.present(mainQueue);

	begingStagingCmdRecordingForNextFrame();
}

}
}