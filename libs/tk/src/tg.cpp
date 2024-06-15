#include "tg.hpp"
#include <array>
#include <stb_image.h>
#include "tvk.hpp"
#include "shader_compiler.hpp"
#include <format>
#include <algorithm>
#include <physfs.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <Tracy.hpp>

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

struct RenderTarget {
	vk::Image colorBuffer[MAX_SWAPCHAIN_IMAGES];
	vk::Image depthBuffer[MAX_SWAPCHAIN_IMAGES];
	vk::ImageView colorBufferView[MAX_SWAPCHAIN_IMAGES];
	vk::ImageView depthBufferView[MAX_SWAPCHAIN_IMAGES];
	VkFramebuffer framebuffer[MAX_SWAPCHAIN_IMAGES];
	u32 w, h;
	bool autoRedraw; // we can avoid redrawing everyframe if there are no changes
	u8 needRedraw; // if !autoRedraw, "needRedraw" tells us if we need to redraw because it has been requested.
		// Since there are use multiple images, we use an integer istead of a boolean.
		// So this is how many frames we need to redraw. For example, if we are using triple-buffering,
		// requesting to redraw would set "needRedraw" to 3, and each frame we would draw if(needRedraw > 0), and decrease needRedraw.
		// We can also set needRedraw to -1 and... when the renderer sees -1 it will change it to the number of swapchain images
};

static constexpr u32 k_anisotropicFractionResolution = 4;
static constexpr u32 k_maxAnisotropicFiltering = 16; // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxSamplerAnisotropy&platform=all
static constexpr u32 k_anisotropicFilteringNumDiscreteValues = (k_maxAnisotropicFiltering - 1) * k_anisotropicFractionResolution + 1;

struct DescPool_DescSet { DescPoolId descPool; VkDescriptorSet descSet; };

struct RenderUniverse
{
	u32 queueFamily;
	VkSurfaceKHR surface;
	vk::Device device;
	ShaderCompiler shaderCompiler;

	vk::Format depthStencilFormat;
	u32 screenW = 0, screenH = 0;
	u32 oldScreenW, oldScreenH = 0;
	u8 msaa = 1;
	vk::SwapchainOptions swapchainOptions;
	vk::SwapchainSyncHelper swapchain;
	vk::Image depthStencilImages[MAX_SWAPCHAIN_IMAGES];
	vk::ImageView depthStencilImageViews[MAX_SWAPCHAIN_IMAGES];
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];
	VkRenderPass renderPass;
	VkRenderPass renderPassOffscreen;
	VkDescriptorSetLayout globalDescSetLayout;

	// staging buffers
	struct StagingBuffer {
		vk::Buffer buffer;
		size_t offset;
	};
	std::vector<StagingBuffer> spareStagingBuffers; // these are the staging buffers that can be picked up for future staging processes
	std::vector<StagingBuffer> bussyStagingBuffers[MAX_SWAPCHAIN_IMAGES]; // here we keep the staging buffers that are in use. After the staging process is finished, we move the stagingBuffer back to spareStagingBuffers

	// resources to destroy
	struct ToDestroy {
		// for each frame-in-flight we store the resources that need to be destroyed
		// after a complete cycle,w e can be sure that the resources not in use anymore
		std::vector<vk::Buffer> buffers[MAX_SWAPCHAIN_IMAGES];
		std::vector<vk::Image> images[MAX_SWAPCHAIN_IMAGES];
		std::vector<vk::ImageView> imageViews[MAX_SWAPCHAIN_IMAGES];
		std::vector<VkFramebuffer> framebuffers[MAX_SWAPCHAIN_IMAGES];
		std::vector<std::array<std::vector<VkDescriptorSet>, MAX_SWAPCHAIN_IMAGES>> descSets; // [descPool][scImgInd][descSet]

		// here we temporarily queue the resources that need to be destroyed. Later we will transfer these resources to queues above
		std::vector<vk::Buffer> buffersTmp;
		std::vector<vk::Image> imagesTmp;
		std::vector<vk::ImageView> imageViewsTmp;
		std::vector<VkFramebuffer> framebuffersTmp;
		std::vector<std::vector<VkDescriptorSet>> descSetsTmp; // [descPool][descSet]
		bool pushToTmp = true;
	} toDestroy;

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

	// descriptor sets
	std::vector<VkDescriptorPool> descPools;
	std::vector<std::vector<VkDescriptorSet>> descSets;
	u32 descPools_nextFreeEntry = u32(-1);
	std::vector<u32> descSets_nextFreeEntry;

	// geoms
	std::vector<GeomInfo> geoms_info;
	std::vector<u32> geoms_refCount;
	std::vector<vk::Buffer> geoms_buffer;
	u32 geoms_nextFreeEntry = u32(-1);
	PathBag geoms_pathBag;

	// materials
	std::vector<MaterialManager> materialManagers;
	std::vector<std::vector<u32>> materials_refCount;

	// meshes
	std::vector<MeshInfo> meshes_info;
	std::vector<u32> meshes_refCount;
	u32 meshes_nextFreeEntry = u32(-1);

	// render targets
	tk::EntriesArray<RenderTarget> renderTargets;
	
	VkCommandPool cmdPool;
	vk::CmdBuffer cmdBuffers_staging[MAX_SWAPCHAIN_IMAGES + 1]; // we have one extra buffer because we could have a staging cmd buffer "in use" but we still want to record staging cmd for future frames
	u32 cmdBuffers_staging_ind = 0; // that's why we need a separate index for it
	vk::CmdBuffer cmdBuffers_draw[MAX_SWAPCHAIN_IMAGES];

	struct DefaultSamplers {
		// anisotropic can be float, but we will use discrete values [0]=1.0, [1]=1.25, ..., [4]=2.0, [8]=3.0, [15*4]=16.0
		VkSampler nearest;
		VkSampler mipmap_anisotropic[k_anisotropicFilteringNumDiscreteValues] = {VK_NULL_HANDLE};
	} defaultSamplers = {};

	// RenderWorlds
	std::vector<RenderWorld> renderWorlds;
	u32 renderWorlds_nextFreeEntry = u32(-1);

	// imgui
	struct ImGui {
		bool enabled = false;
		DescPoolId descPool = {};
	} imgui;

	~RenderUniverse() {
		device.waitIdle();
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
void* ImageId::getInternalHandle()const { return RU.device.getVkHandle(getHandle()); }

static u32 acquireImageEntry()
{
	const u32 e = acquireReusableEntry(RU.images_nextFreeEntry,
		RU.images_refCount, 0, RU.images_vk, RU.images_info);
	return e;
}

static void releaseImageEntry(u32 e)
{
	releaseReusableEntry(RU.images_nextFreeEntry, RU.images_refCount, 0, e);
}

static void deferredDestroy(auto& frames, auto& tmp, auto id)
{
	if (RU.toDestroy.pushToTmp)
		tmp.push_back(id);
	else
		frames[RU.swapchain.imgInd].push_back(id);
}

static void deferredDestroy_image(vk::Image id)
{
	deferredDestroy(RU.toDestroy.images, RU.toDestroy.imagesTmp, id);
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
		deferredDestroy_image(RU.images_vk[id.id]);
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

	auto fileData = tk::loadBinaryFile(path.string().c_str());
	if(fileData.data == nullptr)
		return ImageRC({});

	int w, h, nc;
	u8* imgData = stbi_load_from_memory(fileData.data, fileData.size, &w, &h, &nc, 4);
	if(imgData == nullptr)
		return ImageRC({});

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

	RU.images_nameToId[path] = imgId;
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

void* ImageViewId::getInternalHandle()const
{
	return RU.device.getVkHandle(getHandle());
}

static u32 acquireImageViewEntry()
{
	const u32 e = acquireReusableEntry(RU.imageViews_nextFreeEntry,
		RU.imageViews_refCount, 0, RU.imageViews_vk, RU.imageViews_image);
	return e;
}

static void releaseImageViewEntry(u32 e)
{
	releaseReusableEntry(RU.imageViews_nextFreeEntry, RU.imageViews_refCount, 0, e);
}

static void deferredDestroy_imageView(vk::ImageView id)
{
	deferredDestroy(RU.toDestroy.imageViews, RU.toDestroy.imageViewsTmp, id);
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
		deferredDestroy_imageView(RU.imageViews_vk[id.id]);
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

// --- FRAMEBUFFERS ---

static void deferredDestroy_framebuffer(VkFramebuffer id)
{
	deferredDestroy(RU.toDestroy.framebuffers, RU.toDestroy.framebuffersTmp, id);
}

#if 0
FramebufferId makeFramebuffer(const MakeFramebuffer& info)
{
	std::array<vk::ImageView, 2> attachments;
	u32 numAttachments = 0;
	u32 w = 0, h = 0;
	tg::ImageInfo colorImageInfo;
	tg::ImageInfo depthImageInfo;
	if (info.colorBuffer.id.isValid()) {
		colorImageInfo = info.colorBuffer.id.getImage().id.getInfo();
		w = colorImageInfo.w;
		h = colorImageInfo.h;
	}
	if (info.depthBuffer.id.isValid()) {
		depthImageInfo = info.depthBuffer.id.getImage().id.getInfo();
		w = depthImageInfo.w;
		h = depthImageInfo.h;
	}
	const VkFramebuffer fb = RU.device.createFramebuffer({
		.renderPass = RU.renderPass,
		.attachments = {attachments.data(), numAttachments},
		.width = w, .height = h
	});
}
#endif

// --- DESCRIPTOR SETS ---
static u32 acquireDescPoolEntry()
{
	return acquireReusableEntry(RU.descPools_nextFreeEntry, RU.descPools, 0, RU.descSets, RU.toDestroy.descSets, RU.toDestroy.descSetsTmp);
}
static void releaseDescPoolEntry(u32 entryToRelease)
{
	releaseReusableEntry(RU.descPools_nextFreeEntry, RU.descPools, 0, entryToRelease);
}

VkDescriptorPool DescPoolId::getHandleVk()const
{
	return RU.descPools[id];
}

DescPoolId makeDescPool(const MakeDescPool& info)
{
	const u32 e = acquireDescPoolEntry();
	std::array<VkDescriptorPoolSize, 2> sizesPerType;
	u32 sizesPerType_n = 0;
	auto addTypeSize = [&](VkDescriptorType type, u32 count) {
		if (count) {
			sizesPerType[sizesPerType_n] = { type, count };
			sizesPerType_n++;
		}
	};
	addTypeSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info.maxPerType.uniformBuffers);
	addTypeSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.maxPerType.combinedImageSamplers);
	RU.descPools[e] = RU.device.createDescriptorPool(info.maxSets, { sizesPerType.data(), sizesPerType_n }, info.options);
	return { e };
}

void releaseDescSets(DescPoolId descPool, CSpan<VkDescriptorSet> toRelease)
{
	auto& descSets = RU.toDestroy.descSets[descPool.id];
	auto& descSetsTmp = RU.toDestroy.descSetsTmp[descPool.id];
	for (auto descSet : toRelease)
		deferredDestroy(descSets, descSetsTmp, descSet);
}

void releaseImguiDescSet(VkDescriptorSet descSet)
{
	releaseDescSet(RU.imgui.descPool, descSet);
}

// ---

static u32 discretizeAnisotropicFiltering(float a)
{
	a -= 1.f;
	a *= k_anisotropicFractionResolution;
	a += 0.5f;
	int x = a;
	x = glm::clamp(x, 0, int(k_anisotropicFilteringNumDiscreteValues));
	return x;
}

VkSampler getNearestSampler()
{
	VkSampler& sampler = RU.defaultSamplers.nearest;
	if(sampler == VK_NULL_HANDLE)
		sampler = RU.device.createSampler(vk::Filter::nearest, vk::Filter::nearest, vk::SamplerMipmapMode::nearest, vk::SamplerAddressMode::clampToEdge);
	return sampler;
}

VkSampler getAnisotropicFilteringSampler(float maxAniso)
{
	const u32 e = discretizeAnisotropicFiltering(maxAniso);
	VkSampler& sampler = RU.defaultSamplers.mipmap_anisotropic[e];
	if(sampler == VK_NULL_HANDLE)
		sampler = RU.device.createSampler(vk::Filter::linear, vk::Filter::linear, vk::SamplerMipmapMode::linear, vk::SamplerAddressMode::clampToEdge, maxAniso);
	return sampler;
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

static void deferredDestroy_buffer(vk::Buffer id)
{
	deferredDestroy(RU.toDestroy.buffers, RU.toDestroy.buffersTmp, id);
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
		deferredDestroy_buffer(RU.geoms_buffer[id.id]);
		releaseGeomEntry(id.id);
	}
}

GeomRC geom_create()
{
	const u32 e = acquireGeomEntry();
	RU.geoms_info[e] = GeomInfo{};
	RU.geoms_buffer[e] = vk::Buffer{};
	RU.geoms_refCount[e] = 0;
	return GeomRC(GeomId{ e });
}

void geom_resetFromBuffer(const GeomRC& h, const GeomInfo& info, vk::Buffer buffer)
{
	const u32 e = h.id.id;
	if (RU.geoms_buffer[e].id) {
		RU.device.destroyBuffer(RU.geoms_buffer[e]);
	}

	RU.geoms_info[e] = info;
	RU.geoms_buffer[e] = buffer;
	RU.geoms_refCount[e] = 0;
}

void geom_resetFromInfo(const GeomRC& h, const CreateGeomInfo& info, AABB* aabb)
{
	GeomInfo geomInfo = {  };
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
	stageData(buffer, { datas.data(), numDatas });

	geom_resetFromBuffer(h, geomInfo, buffer);

	if (aabb) {
		CSpan<glm::vec3> positions((const glm::vec3*)info.positions.data(), info.numVerts);
		*aabb = tk::pointCloudToAABB(positions);
	}
}

bool geom_resetFromFile(const GeomRC& h, CStr filePath, AABB* aabb)
{
	auto file = PHYSFS_openRead(filePath);
	if (!file)
		return false;
	auto len = PHYSFS_fileLength(file);
	std::vector<u8> data(len);
	PHYSFS_readBytes(file, data.data(), len);
	return geom_resetFromMemFile(h, data, aabb);
}

bool geom_resetFromMemFile(const GeomRC& h, CSpan<u8> mem, AABB* aabb)
{
	if (mem.size() < sizeof(GeomInfo))
		return false;
	const GeomInfo& info = *(const GeomInfo*)mem.data(); // only works for little-endian
	const auto data = CSpan<u8>(mem).subspan(sizeof(GeomInfo));
	auto attribSubspan = [&data](u32 offset, u32 size) -> CSpan<u8> {
		if (offset == u32(-1))
			return {};
		return data.subspan(offset, size);
	};
	const CreateGeomInfo createInfo = {
		.positions = attribSubspan(info.attribOffset_positions, info.numVerts * sizeof(glm::vec3)),
		.normals = attribSubspan(info.attribOffset_normals, info.numVerts * sizeof(glm::vec3)),
		.tangents = attribSubspan(info.attribOffset_tangents, info.numVerts * sizeof(glm::vec3)),
		.texCoords = attribSubspan(info.attribOffset_texCoords, info.numVerts * sizeof(glm::vec2)),
		.colors = attribSubspan(info.attribOffset_colors, info.numVerts * sizeof(glm::vec4)),
		.indices = attribSubspan(info.indsOffset, info.numInds * sizeof(u32)),
		.numVerts = info.numVerts,
		.numInds = info.numInds,
	};
	geom_resetFromInfo(h, createInfo, aabb);
	return true;
}

GeomRC geom_getOrLoadFromFile(CStr path, AABB* aabb)
{
	if (u32 e = RU.geoms_pathBag.getEntry(path); e != u32(-1)) {
		return GeomRC(GeomId{ e });
	}
	GeomRC h = geom_create();
	if (!geom_resetFromFile(h, path, aabb))
		return GeomRC{};
	return h;
}

size_t geom_serializeToMem(const CreateGeomInfo& geomInfo, std::span<u8> data)
{
	const CSpan<u8> attribsData[] = {
		geomInfo.positions,
		geomInfo.normals,
		geomInfo.tangents,
		geomInfo.texCoords,
		geomInfo.colors,
		geomInfo.indices,
	};
	constexpr size_t numAttribs = std::size(attribsData);
	static_assert(numAttribs == size_t(AttribSemantic::COUNT) + 1);

	size_t reqMemSpace = sizeof(GeomInfo);
	for (CSpan<u8> attribData : attribsData) {
		reqMemSpace += attribData.size();
	}
	if (reqMemSpace > data.size())
		return reqMemSpace;

	u32 offset = 0;
	std::span<u32> offsets((u32*)data.data(), numAttribs);
	for (size_t i = 0; i < numAttribs; i++) {
		const auto& AD = attribsData[i];
		if (AD.size()) {
			memcpy(data.data() + sizeof(GeomInfo) + offset, AD.data(), AD.size());
			offsets[i] = offset;
			offset += AD.size();
		}
		else
			offsets[i] = u32(-1);
	}
	u32& numVerts = *((u32*)data.data() + numAttribs);
	numVerts = geomInfo.numVerts;
	u32& numInds = *((u32*)data.data() + numAttribs + 1);
	numInds = geomInfo.numInds;
	return reqMemSpace;
}

bool geom_serializeToFile(const CreateGeomInfo& geomInfo, CStr dstPath)
{
	auto file = PHYSFS_openWrite(dstPath);
	if (!file) {
		printf("could not open file for writing (%s): %s\n", dstPath, PHYSFS_getLastError());
		return false;
	}
	defer(PHYSFS_close(file));

	const size_t memSize = geom_serializeToMem(geomInfo, {});
	auto mem = std::make_unique_for_overwrite<u8[]>(memSize);
	geom_serializeToMem(geomInfo, { mem.get(), memSize});

	const size_t bytesWritten = PHYSFS_writeBytes(file, mem.get(), memSize);
	return bytesWritten == memSize;
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

u32 registerMaterialManager(const MaterialManager& mgr)
{
	const u32 mgrId = RU.materialManagers.size();
	RU.materialManagers.push_back(mgr);
	RU.materials_refCount.emplace_back();
	mgr.setManagerId(mgr.managerPtr, mgrId);
	return mgrId;
}

template <typename MgrT>
u32 registerMaterialManagerT(MgrT* mgr)
{
	return registerMaterialManager({
		.managerPtr = mgr,
		.setManagerId = [](void* self, u32 id) {
			((MgrT*)self)->managerId = { id };
		},
		.destroyMaterial = [](void* self, MaterialId id) {
			((MgrT*)self)->destroyMaterial(id);
		},
		.getPipeline = [](void* self, MaterialId materialId, GeomId geomId) {
			return ((MgrT*)self)->getPipeline(materialId, geomId);
		},
		.getPipelineLayout = [](void* self, MaterialId materialId) {
			return ((MgrT*)self)->getPipelineLayout(materialId);
		},
		.getDescriptorSet = [](void* self, MaterialId materialId) {
			return ((MgrT*)self)->getDescriptorSet(materialId);
		},
	});
}

// --- PBR MATERIAL ---

template<typename Mgr>
static u32 acquireMaterialEntry(Mgr& mgr)
{
	const u32 e = acquireReusableEntry(mgr.materials_nextFreeEntry,
		mgr.materials_info, 0, mgr.materials_descSet, RU.materials_refCount[mgr.managerId.id]);
	assert(e < mgr.maxExpectedMaterials);
	return e;
}

template<typename Mgr>
void releaseMaterialEntry(Mgr& mgr, u32 e)
{
	releaseReusableEntry(mgr.materials_nextFreeEntry, mgr.materials_info, 0, e);
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

struct CompiledVertFragShaders { vk::VertShader vertShad; vk::FragShader fragShad; };
static CompiledVertFragShaders compileVertFragShaders(ZStrView vertShadPath, ZStrView fragShadPath, CSpan<tk::PreprocDefine> defines)
{
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

	return { vertShad, fragShad };
}

VkPipeline PbrMaterialManager::getCreatePipeline(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture,
	HasVertexNormalsOrTangents hasVertexNormalsOrTangents, bool hasTexCoords, bool hasVertexColors, bool doubleSided)
{
	VkPipeline& p = pipelines[hasAlbedoTexture][hasNormalTexture][hasMetallicRoughnessTexture]
		[int(hasVertexNormalsOrTangents)][hasTexCoords][hasVertexColors][doubleSided];
	if (!p) {
		const tk::PreprocDefine defines[] = {
			{"MAX_DIR_LIGHTS", MAX_DIR_LIGHTS_STR.c_str()},
			{"USE_MODEL_MTX", "1"},
			{"USE_INV_TRANS_MODEL_MTX", "1"},
			{"USES_POSITION_VARYING", "1"},
			{"HAS_ALBEDO_TEX", hasTexCoords && hasAlbedoTexture ? "1" : "0"},
			{"HAS_NORMAL_TEX", hasTexCoords && hasNormalTexture ? "1" : "0"},
			{"HAS_METALLIC_ROUGHNESS_TEX", hasTexCoords && hasMetallicRoughnessTexture ? "1" : "0"},
			{"HAS_NORMAL", hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::no ? "0" : "1"},
			{"HAS_TANGENT", hasVertexNormalsOrTangents == HasVertexNormalsOrTangents::normalsAndTangents ? "1" : "0"},
			{"HAS_TEXCOORD_0", hasTexCoords ? "1" : "0"},
			{"HAS_VERTCOLOR_0", hasVertexColors ? "1" : "0"},
		};

		ZStrView vertShadPath = "shaders/generic.vert.glsl";
		ZStrView fragShadPath = "shaders/pbr.frag.glsl";
		auto [vertShad, fragShad] = compileVertFragShaders(vertShadPath, fragShadPath, defines);

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
		u32 offset = 0;
		u32 numAttribs = 0;
		auto addAttribMat4 = [&]() {
			for (u32 colI = 0; colI < 4; colI++) {
				attribs[numAttribs++] = {
					.location = numAttribs,
					.binding = 0,
					.format = vk::Format::RGBA32_SFLOAT,
					.offset = offset,
				};
				offset += u32(sizeof(glm::vec4));
			}
		};
		auto addAttribMat3 = [&]() {
			for (u32 colI = 0; colI < 3; colI++) {
				attribs[numAttribs++] = {
					.location = numAttribs,
					.binding = 0,
					.format = vk::Format::RGB32_SFLOAT,
					.offset = offset,
				};
				offset += u32(sizeof(glm::vec3));
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
			//.depthBias = {.constantFactor = 1, .slopeFactor = 1, .clamp = 1},
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
	vk::ASSERT_VKRES(RU.device.allocDescriptorSets(descPool.getHandleVk(), descSetLayout, {&descSet, 1}));
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
					.sampler = getAnisotropicFilteringSampler(params.anisotropicFiltering),
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
	releaseDescSet(descPool, materials_descSet[id.id]);
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

PbrMaterialManager* PbrMaterialManager::s_getOrCreate(u32 maxExpectedMaterials)
{
	static PbrMaterialManager* mgr = nullptr;
	if (mgr)
		return mgr;

	mgr = new PbrMaterialManager(maxExpectedMaterials);
	registerMaterialManagerT(mgr);

	return mgr;
}

PbrMaterialManager::PbrMaterialManager(u32 maxExpectedMaterials)
	: maxExpectedMaterials(maxExpectedMaterials)
{
	materials_info.reserve(maxExpectedMaterials);
	materials_descSet.reserve(maxExpectedMaterials);
	uniformBuffer = RU.device.createBuffer(vk::BufferUsage::uniformBuffer | vk::BufferUsage::transferDst, maxExpectedMaterials * sizeof(PbrUniforms), {});

	descPool = makeDescPool({
		.maxSets = maxExpectedMaterials,
		.maxPerType = {
			.uniformBuffers = maxExpectedMaterials,
			.combinedImageSamplers = 3 * maxExpectedMaterials,
		},
		.options = {.allowFreeIndividualSets = true}
	});
}

// --- WIREFRAME MATERIAL --

WireframeMaterialManager::WireframeMaterialManager(u32 maxExpectedMaterials)
	: maxExpectedMaterials(maxExpectedMaterials)
{
	// create descSetLayout
	{
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
		descSetLayout = RU.device.createDescriptorSetLayout(bindings);
	}

	// create pipelineLayout
	{
		const VkDescriptorSetLayout descSetLayouts[] = {
			RU.globalDescSetLayout,
			descSetLayout,
		};
		pipelineLayout = RU.device.createPipelineLayout(descSetLayouts, {});
	}
	
	// create pipeline
	{
		const bool hasVertexColors = false; // TODO
		const tk::PreprocDefine defines[] = {
			{"HAS_VERTCOLOR_0", hasVertexColors ? "1" : "0"},
		};

		ZStrView vertShadPath = "shaders/wireframe.vert.glsl";
		ZStrView fragShadPath = "shaders/wireframe.frag.glsl";
		auto [vertShad, fragShad] = compileVertFragShaders(vertShadPath, fragShadPath, defines);

		u32 numBindings = 1;
		std::array<vk::VertexInputBindingInfo, 2> bindings;
		bindings[0] = { // instancing data
				.binding = 0,
				.stride = sizeof(RenderWorld::ObjectMatrices),
				.perInstance = true,
		};
		bindings[numBindings++] = { // a_position
			.binding = numBindings,
			.stride = sizeof(glm::vec3)
		};

		std::array<vk::VertexInputAttribInfo, 5> attribs;
		u32 numAttribs = 0;
		u32 location = 4;
		// instance data matrices
		u32 offset = offsetof(RenderWorld::ObjectMatrices, modelViewProj);
		for (u32 colI = 0; colI < 4; colI++) {
			attribs[numAttribs++] = {
				.location = location++,
				.binding = 0,
				.format = vk::Format::RGBA32_SFLOAT,
				.offset = offset,
			};
			offset += u32(sizeof(glm::vec4));
		}

		{
			location = 11;
			u32 bindingI = 1;
			auto addAttribFmt = [&](vk::Format format) {
				attribs[numAttribs++] = {
					.location = location++,
					.binding = bindingI,
					.format = format,
				};
			};

			addAttribFmt(vk::Format::RGB32_SFLOAT);
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
			.polygonMode = vk::PolygonMode::line,
			.cull_back = false,
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthCompareOp = vk::CompareOp::less_or_equal, // so we can draw wireframes over "solid" objects
			.colorBlendAttachments = colorBlendAttachments,
			.depthBias = {-1, -1, -2},
			.layout = pipelineLayout,
			.renderPass = RU.renderPass,
		};
		vk::ASSERT_VKRES(RU.device.createGraphicsPipelines({ &pipeline, 1 }, { &info, 1 }, nullptr));
	}

	// resources
	materials_info.reserve(maxExpectedMaterials);
	materials_descSet.reserve(maxExpectedMaterials);
	uniformBuffer = RU.device.createBuffer(vk::BufferUsage::uniformBuffer | vk::BufferUsage::transferDst, maxExpectedMaterials * sizeof(WireframeUniforms), {});

	descPool = makeDescPool({
		.maxSets = maxExpectedMaterials,
		.maxPerType = {
			.uniformBuffers = maxExpectedMaterials,
		},
		.options = {.allowFreeIndividualSets = true}
	});
}

WireframeMaterialRC WireframeMaterialManager::createMaterial(const WireframeMaterialInfo& params)
{
	VkDescriptorSet descSet;
	vk::ASSERT_VKRES(RU.device.allocDescriptorSets(descPool.getHandleVk(), descSetLayout, { &descSet, 1 }));
	const u32 entry = acquireMaterialEntry(*this);
	materials_info[entry] = params;
	materials_descSet[entry] = descSet;

	const size_t bufferOffset = sizeof(WireframeUniforms) * entry;
	const WireframeUniforms values = { .color = params.color };
	stageData(uniformBuffer, tk::asBytesSpan(values), bufferOffset);

	vk::DescriptorSetWrite descSetWrite = {
		.descSet = descSet,
		.binding = 0,
		.type = vk::DescriptorType::uniformBuffer,
		.bufferInfo = {
			.buffer = RU.device.getVkHandle(uniformBuffer),
			.offset = bufferOffset,
			.range = sizeof(WireframeUniforms),
		}
	};

	RU.device.writeDescriptorSets({ &descSetWrite, 1});
	return WireframeMaterialRC(managerId, entry);
}
void WireframeMaterialManager::destroyMaterial(MaterialId id)
{
	materials_info[id.id] = {};
	releaseDescSet(descPool, materials_descSet[id.id]);
	releaseMaterialEntry(*this, id.id);
}

WireframeMaterialManager* WireframeMaterialManager::s_getOrCreate(u32 maxExpectedMaterials)
{
	static WireframeMaterialManager* mgr = nullptr;
	if (mgr)
		return mgr;

	mgr = new WireframeMaterialManager(maxExpectedMaterials);
	registerMaterialManagerT(mgr);

	return mgr;
}

// --- MESHES ---

static u32 acquireMeshEntry()
{
	const u32 e = acquireReusableEntry(RU.meshes_nextFreeEntry, RU.meshes_refCount, 0, RU.meshes_info);
	return e;
}

static void releaseMeshEntry(u32 e)
{
	releaseReusableEntry(RU.meshes_nextFreeEntry, RU.meshes_refCount, 0, e);
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
const MeshRC& ObjectId::getMesh()const
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	return RW.objects_mesh[0][e];
}

u32 ObjectId::getNumInstances()const
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	return RW.objects_numInstances[0][e];
}

u32 ObjectId::getNumAllocInstances()const
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	return RW.objects_numAllocInstances[0][e];
}

WorldLayer& ObjectId::layer()
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	return RW.objects_layer[0][e];
}

void ObjectId::setModelMatrix(const glm::mat4& m, u32 instanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	assert(instanceInd < RW.objects_numInstances[0][e]);
	RW.modelMatrices[0][RW.objects_firstModelMtx[0][e] + instanceInd] = m;
}

void ObjectId::setModelMatrices(CSpan<glm::mat4> matrices, u32 firstInstanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	assert(firstInstanceInd + matrices.size() <= RW.objects_numInstances[0][e]);
	for (size_t i = 0; i < matrices.size(); i++)
		RW.modelMatrices[0][RW.objects_firstModelMtx[0][e] + firstInstanceInd + i] = matrices[i];
}

bool ObjectId::addInstances(u32 n)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	auto& numInstances = RW.objects_numInstances[0][e];
	auto& numAllocInstances = RW.objects_numAllocInstances[0][e];
	if (numInstances + n <= numAllocInstances) {
		numInstances += n;
		return true;
	}
	return false;
}

bool ObjectId::changeNumInstances(u32 n)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	auto& numInstances = RW.objects_numInstances[0][e];
	if (n <= numInstances) {
		numInstances = n;
		return true;
	}
	return false;
}

void ObjectId::destroyInstance(u32 instanceInd)
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	const u32 e = RW.objects_id_to_entry[id];
	auto& numInstances = RW.objects_numInstances[0][e];
	assert(instanceInd < numInstances);
	const u32 fmm = RW.objects_firstModelMtx[0][e];
	RW.modelMatrices[fmm + instanceInd] = RW.modelMatrices[fmm + numInstances - 1];
	numInstances--;
}

PointLight* PointLightId::operator->()
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	return &RW.pointLights[id];
}

DirLight* DirLightId::operator->()
{
	auto& RW = RU.renderWorlds[_renderWorld.id];
	return &RW.dirLights[id];
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
		vk::ASSERT_VKRES(vk::createDevice(RU.device, params.instance, bestPhysicalDeviceInfo, queuesInfos, params.deviceFeatures));
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

	auto makeRenderPass = [&](vk::ImageLayout finalLayout) {
		vk::FbAttachmentInfo attachments[] = {
			{.format = RU.swapchain.format.format},
			{.format = RU.depthStencilFormat}
		};
		const vk::AttachmentOps attachmentOps[] = {
			{ // color
				.loadOp = vk::Attachment_LoadOp::clear,
				.expectedLayout = vk::ImageLayout::undefined,
				.finalLayout = finalLayout,
			},
			{ // depth
				.loadOp = vk::Attachment_LoadOp::clear,
				.storeOp = vk::Attachment_StoreOp::dontCare, // we don't care about the contents of the depth buffer after the pass
				.expectedLayout = vk::ImageLayout::undefined,
				.finalLayout = vk::ImageLayout::depthStencilAttachment,
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
	};

	RU.renderPass = makeRenderPass(vk::ImageLayout::presentSrc);
	RU.renderPassOffscreen = makeRenderPass(vk::ImageLayout::shaderReadOnly);

	RU.shaderCompiler.init();
	RU.shaderCompiler.generateDebugInfo = params.generateShadersDebugInfo;

	vk::ASSERT_VKRES(vk::createSwapchainSyncHelper(RU.swapchain, RU.surface, RU.device, RU.swapchainOptions));
	const u32 numScImages = RU.swapchain.numImages;

	RU.cmdPool = RU.device.createCmdPool(RU.queueFamily, { .transientCmdBuffers = true, .reseteableCmdBuffers = true });
	RU.device.allocCmdBuffers(RU.cmdPool, { RU.cmdBuffers_staging, numScImages + 1}, false);
	RU.device.allocCmdBuffers(RU.cmdPool, { RU.cmdBuffers_draw, numScImages }, false);
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

	RU.defaultSamplers.mipmap_anisotropic[0] = RU.device.createSampler(vk::Filter::linear, vk::Filter::linear, vk::SamplerMipmapMode::linear, vk::SamplerAddressMode::clampToEdge);

	RU.imgui.enabled = params.enableImgui;
	if (params.enableImgui) {
		const u32 maxExpectedSamplers = 1024;
		RU.imgui.descPool = makeDescPool({
			.maxSets = maxExpectedSamplers,
			.maxPerType = {.combinedImageSamplers = maxExpectedSamplers},
			.options = {.allowFreeIndividualSets = true}
		});

		ImGui_ImplVulkan_InitInfo info = {
			.Instance = RU.device.instance,
			.PhysicalDevice = RU.device.physicalDevice.handle,
			.Device = RU.device.device,
			.QueueFamily = RU.queueFamily,
			.Queue = RU.device.queues[RU.queueFamily][0],
			.PipelineCache = VK_NULL_HANDLE,
			.DescriptorPool = RU.imgui.descPool.getHandleVk(),
			.Subpass = 0,
			.MinImageCount = RU.swapchainOptions.minImages,
			.ImageCount = numScImages,
			.MSAASamples = VkSampleCountFlagBits(RU.msaa),
			.Allocator = VK_NULL_HANDLE,
			.CheckVkResultFn = nullptr,
		};
		ImGui_ImplVulkan_Init(&info, RU.renderPass);
	}

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
	const u32 e = acquireReusableEntry(RU.renderWorlds_nextFreeEntry, RU.renderWorlds, 0);
	return e;
}

RenderWorldId createRenderWorld()
{
	const u32 entry = acquireRenderWorldEntry();
	auto& RW = RU.renderWorlds[entry];
	RW.id = { entry };

	const u32 numExpectedObjects = 1 << 10;
	std::apply([](auto&... v) { (v.reserve(size_t(numExpectedObjects)), ...); }, RW.objectsVecs(0));
	RW.modelMatrices[0].reserve(numExpectedObjects);
	RW.objects_matricesTmp.reserve(numExpectedObjects);

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
	releaseReusableEntry(RU.renderWorlds_nextFreeEntry, RU.renderWorlds, 0, id.id);
}

// RENDER TARGET
static void createRenderTarget_inPlace(RenderTargetId id, u32 w, u32 h)
{
	auto& rt = RU.renderTargets[id.id];

	for (u32 i = 0; i < RU.swapchain.numImages; i++) {

		rt.colorBuffer[i] = RU.device.createImage(vk::ImageInfo{
			.size = {u16(w), u16(h)},
			.usage = vk::ImageUsage::default_framebuffer(false, true),
		});
		rt.colorBufferView[i] = RU.device.createImageView({ .image = rt.colorBuffer[i] });

		rt.depthBuffer[i] = RU.device.createImage(vk::ImageInfo{
			.size = {u16(w), u16(h)},
			.format = RU.depthStencilFormat,
			.usage = vk::ImageUsage::default_framebuffer(false, true),
		});
		rt.depthBufferView[i] = RU.device.createImageView({ .image = rt.depthBuffer[i] });

		std::array<vk::ImageView, 2> attachments = { rt.colorBufferView[i], rt.depthBufferView[i] };
		rt.framebuffer[i] = RU.device.createFramebuffer(vk::FramebufferInfo{
			.renderPass = RU.renderPassOffscreen,
			.attachments = {attachments.data(), 2},
			.width = w, .height = h,
		});
	}

	rt.w = w;
	rt.h = h;
	rt.needRedraw = u8(-1);
}

static void destroyRenderTarget_inPlace(RenderTargetId id)
{
	assert(id.isValid());
	auto& rt = RU.renderTargets[id.id];
	for (u32 i = 0; i < RU.swapchain.numImages; i++) {
		deferredDestroy_framebuffer(rt.framebuffer[i]);
		deferredDestroy_imageView(rt.colorBufferView[i]);
		deferredDestroy_image(rt.colorBuffer[i]);
		deferredDestroy_imageView(rt.depthBufferView[i]);
		deferredDestroy_image(rt.depthBuffer[i]);
	}

}

RenderTargetId createRenderTarget(const RenderTargetParams& params)
{
	const u32 w = params.w;
	const u32 h = params.h;

	const u32 rtInd = RU.renderTargets.alloc();
	const RenderTargetId id = { rtInd };
	auto& rt = RU.renderTargets[id.id];
	rt.autoRedraw = params.autoRedraw;

	createRenderTarget_inPlace(id, w, h);
	return { rtInd };
}

void destroyRenderTarget(RenderTargetId id)
{
	destroyRenderTarget_inPlace(id);
	RU.renderTargets.free(id.id);
}

vk::Image RenderTargetId::getTextureImage()
{
	return getTextureImage(RU.swapchain.imgInd);
}
vk::Image RenderTargetId::getTextureImage(u32 scImgInd)
{
	return RU.renderTargets[id].colorBuffer[scImgInd];
}
vk::ImageView RenderTargetId::getTextureImageView()
{
	return getTextureImageView(RU.swapchain.imgInd);
}
vk::ImageView RenderTargetId::getTextureImageView(u32 scImgInd)
{
	return RU.renderTargets[id].colorBufferView[scImgInd];
}
VkImageView RenderTargetId::getTextureImageViewVk()
{
	return getTextureImageViewVk(RU.swapchain.imgInd);
}
VkImageView RenderTargetId::getTextureImageViewVk(u32 scImgInd)
{
	return RU.device.getVkHandle(getTextureImageView(scImgInd));
}

void RenderTargetId::requestRedraw()
{
	RU.renderTargets[id].needRedraw = u8(-1);
}

void RenderTargetId::resize(u32 w, u32 h)
{
	destroyRenderTarget_inPlace(*this);
	createRenderTarget_inPlace(*this, w, h);
}

u32 getNumSwapchainImages()
{
	return RU.swapchain.numImages;
}
u32 getCurrentSwapchainImageInd()
{
	return RU.swapchain.imgInd;
}


// ---

static u32 acquireObjectEntry(RenderWorld& RW)
{
	// we don't reuse entries because we will be defracmenting and resorting anyways
	const u32 e = RW.objects_id_to_entry.size();
	std::apply([](auto&... v) { (v.emplace_back(), ...); }, RW.objectsVecs(0));
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

ObjectId RenderWorldId::createObject(MeshRC mesh, const glm::mat4& modelMtx, u32 maxInstances, WorldLayer layer)
{
	return RU.renderWorlds[id].createObject(std::move(mesh), modelMtx, maxInstances);
}
ObjectId RenderWorldId::createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances, WorldLayer layer)
{
	return RU.renderWorlds[id].createObjectWithInstancing(std::move(mesh), instancesMatrices, maxInstances, layer);
}
ObjectId RenderWorldId::createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances, WorldLayer layer)
{
	return RU.renderWorlds[id].createObjectWithInstancing(std::move(mesh), numInstances, maxInstances, layer);
}
void RenderWorldId::destroyObject(ObjectId oid)
{
	RU.renderWorlds[id].destroyObject(oid);
}

PointLightId RenderWorldId::createPointLight(const PointLight& l)
{
	return RU.renderWorlds[id].createPointLight(l);
}
void RenderWorldId::destroyPointLight(PointLightId l)
{
	RU.renderWorlds[id].destroyPointLight(l);
}

DirLightId RenderWorldId::createDirLight(const DirLight& l)
{
	return RU.renderWorlds[id].createDirLight(l);
}
void RenderWorldId::destroyDirLight(DirLightId l)
{
	RU.renderWorlds[id].destroyDirLight(l);
}

void RenderWorldId::debugGui()
{
	if (!RU.imgui.enabled)
		return;

	char windowLabel[16];
	snprintf(windowLabel, std::size(windowLabel), "RenderWorld(%d)", id);
	ImGui::Begin(windowLabel);
	auto& RW = RU.renderWorlds[id];
	for (u32 entry = 0; entry < RW.objects_entry_to_id[0].size(); entry++) {
		const u32 id = RW.objects_entry_to_id[0][entry];
		const auto meshId = RW.objects_mesh[0][entry].id;
		if (!meshId.isValid())
			continue;
		if (ImGui::TreeNode((void*)uintptr_t(id), "%d", id)) {
			ImGui::Text("Mesh: %d", meshId);
			ImGui::Text("Num Alloc Instances: %d", RW.objects_numAllocInstances[entry]);
			ImGui::Text("Num Instances: %d", RW.objects_numInstances[entry]);
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

template<bool PROVIDE_DATA>
static ObjectId _createObjectWithInstancing(RenderWorld& RW, MeshRC mesh, u32 numInstances, const glm::mat4* instancesMatrices, u32 numAllocInstances, WorldLayer sortOrder)
{
	numAllocInstances = glm::max(numAllocInstances, numInstances);
	const u32 e = acquireObjectEntry(RW);
	const u32 oid = acquireObjectId(RW);
	RW.objects_id_to_entry[oid] = e;
	RW.objects_entry_to_id[0][e] = oid;
	RW.objects_mesh[0][e] = mesh;
	RW.objects_numInstances[0][e] = numInstances;
	RW.objects_numAllocInstances[0][e] = numAllocInstances;
	RW.objects_layer[0][e] = sortOrder;
	const u32 firstModelMtx = u32(RW.modelMatrices[0].size());
	RW.objects_firstModelMtx[0][e] = firstModelMtx;

	const u32 newModelMatricesSize = firstModelMtx + numAllocInstances;
	RW.modelMatrices[0].resize(newModelMatricesSize);
	if constexpr (PROVIDE_DATA) {
		for (u32 i = 0; i < numInstances; i++) {
			const auto& m = instancesMatrices[i];
			RW.modelMatrices[0][i] = m;
		}
	}

	return ObjectId(RW.id, { oid });
}

ObjectId RenderWorld::createObject(MeshRC mesh, const glm::mat4& modelMtx, u32 maxInstances, WorldLayer layer)
{
	return _createObjectWithInstancing<true>(*this, std::move(mesh), 1, &modelMtx, maxInstances, layer);
}

ObjectId RenderWorld::createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances, WorldLayer layer)
{
	return _createObjectWithInstancing<true>(*this, std::move(mesh), instancesMatrices.size(), instancesMatrices.data(), maxInstances, layer);
}

ObjectId RenderWorld::createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances, WorldLayer layer)
{
	return _createObjectWithInstancing<false>(*this, std::move(mesh), numInstances, nullptr, maxInstances, layer);
}

PointLightId RenderWorld::createPointLight(const PointLight& l)
{
	const u32 id = pointLights.alloc();
	pointLights[id] = l;
	return PointLightId(this->id, { id });
}
void RenderWorld::destroyPointLight(PointLightId l)
{
	pointLights.free(l.id);
}

DirLightId RenderWorld::createDirLight(const DirLight& l)
{
	const u32 id = dirLights.alloc();
	dirLights[id] = l;
	return DirLightId(this->id, { id });
}

void RenderWorld::destroyDirLight(DirLightId l)
{
	dirLights.free(l.id);
}

void RenderWorld::destroyObject(ObjectId oid)
{
	assert(id.isValid());
	const u32 e = objects_id_to_entry[oid.id];
	objects_mesh[0][e] = MeshRC{};
	//releaseObjectEntry(*this, e);
	releaseObjectId(*this, oid.id);
	needDefragmentObjects = true;
}

void RenderWorld::_sortAndDefragmentObjects()
{
	if (!needResortObjects && !needDefragmentObjects)
		return;

	using Vecs = decltype(objectsVecs(0));
	constexpr u32 numVecs = std::tuple_size_v<Vecs>;
	Vecs vecs[] = { objectsVecs(0), objectsVecs(1) };

	// make space for copyging from vecs[0] to vecs[1] (conservatively high)
	comptime_for<numVecs>([&]<size_t vi>() {
		auto& vec0 = std::get<vi>(vecs[0]);
		auto& vec1 = std::get<vi>(vecs[1]);
		vec1.resize(vec0.size());
	});
	modelMatrices[1].resize(modelMatrices[0].size());

	auto remapObj = [&](u32 objI, u32 objJ, u32& mtxI)
	{
		const u32 firstModelMtx = objects_firstModelMtx[0][objJ];
		const u32 numInstances = objects_numInstances[0][objJ];
		const u32 maxInstances = objects_numAllocInstances[0][objJ];
		const u32 oid = objects_entry_to_id[0][objJ];
		comptime_for<numVecs-1>([&]<size_t vi>() { // -1 because we want to exclude objects_firstModelMtx
			auto& vec0 = std::get<vi>(vecs[0]);
			auto& vec1 = std::get<vi>(vecs[1]);
			vec1[objI] = vec0[objJ];
		});
		objects_firstModelMtx[1][objI] = mtxI;

		assert(objects_id_to_entry[oid] == objJ);
		objects_id_to_entry[oid] = objI;
		const u32 nextObjMtxI = mtxI + maxInstances;
		for (u32 i = 0; i < numInstances; i++) {
			modelMatrices[1][mtxI] = modelMatrices[0][firstModelMtx + i];
			mtxI++;
		}
		mtxI = nextObjMtxI;
	};

	auto swapAndResizeVectors = [&](u32 numObjs, u32 numMatrices)
	{
		comptime_for<numVecs>([&]<size_t vi>() {
			auto& vec0 = std::get<vi>(vecs[0]);
			auto& vec1 = std::get<vi>(vecs[1]);
			vec1.resize(numObjs);
			std::swap(vec0, vec1);
		});
		modelMatrices[1].resize(numMatrices);
		std::swap(modelMatrices[0], modelMatrices[1]);
	};

	const u32 N = objects_mesh[0].size();
	if (!needResortObjects) // only need to defragment
	{
		u32 mtxI = 0;
		u32 objI;
		for (objI = 0; objI < N; objI++) {
			if (objects_mesh[0][objI].id.isValid())
				mtxI += objects_numAllocInstances[0][objI];
			else
				break;
		}

		for (u32 objJ = objI + 1; objJ < N; objJ++) {
			if (objects_mesh[0][objJ].id.isValid()) {
				remapObj(objI, objJ, mtxI);
				objI++;
			}
		}

		swapAndResizeVectors(objI, mtxI);
	}
	else // need toSort, and maybe also defragment
	{
		u32 n = 0;
		bool sorted = true;
		WorldLayer prev, min, max;
		u32 i;
		for (i = 0; i < N; i++) {
			if (objects_mesh[0][i].id.isValid()) {
				prev = min = max = objects_layer[0][i];
				n++;
				break;
			}
		}
		for (; i < N; i++) {
			if (objects_mesh[0][i].id.isValid()) {
				sorted &= prev <= objects_layer[0][i];
				prev = objects_layer[0][i];
				min = std::min(min, prev);
				max = std::max(max, prev);
				n++;
			}
		}

		if (sorted && !needDefragmentObjects)
			return;

		const u32 R = u32(max - min) + 1;
		auto valOffsets_alloc = tk::getStackTmpAllocator().alloc<u32>(R);
		auto valOffsets = std::span(valOffsets_alloc.ptr, R);
		for (size_t i = 0; i < R; i++) {
			if (objects_mesh[0][i].id.isValid()) {
				const u32 ind = u32(objects_layer[0][i] - min);
				valOffsets[ind]++;
			}
		}

		u32 accum = 0;
		for (u32 i = 0; i < N; i++) {
			const u32 a = valOffsets[i];
			valOffsets[i] = accum;
			accum += a;
		}

		auto sortedIndices_alloc = tk::getStackTmpAllocator().alloc<u32>(n);
		auto sortedIndices = std::span(sortedIndices_alloc.ptr, n);
		for (u32 i = 0, j = 0; i < N; i++) {
			if (objects_mesh[0][i].id.isValid()) {
				const u32 ind = u32(objects_layer[0][i] - min);
				sortedIndices[valOffsets[ind]++] = j;
				j++;
			}
		}

		// apply sorting order to all the objects vectors
		u32 mtxI = 0;
		for (u32 i = 0; i < n; i++) {
			remapObj(i, sortedIndices[i], mtxI);
		}

		swapAndResizeVectors(n, mtxI);
	}

	needResortObjects = needDefragmentObjects = false;
}

// *** DRAW ***
static void draw_renderWorld(const RenderWorldViewport& rwViewport, u32 renderTargetInd, u32 viewportInd)
{
	const RenderWorldId& renderWorldId = rwViewport.renderWorld;
	auto& RW = RU.renderWorlds[renderWorldId.id];

	// update global uniform buffer
	const u32 scImgInd = RU.swapchain.imgInd;
	auto& globalUnifBuffer = RW.global_uniformBuffers[scImgInd];
	{
		u8* data = RU.device.getBufferMemPtr(globalUnifBuffer);
		const u32 numPointLights = u32(RW.pointLights.entries.size());
		const u32 numDirLights = u32(RW.dirLights.entries.size());
		const GlobalUniforms_Header header = {
			.ambientLight = glm::vec3(0.1f),
			.numDirLights = numDirLights,
		};
		memcpy(data, &header, sizeof(header));

		struct DirLightV4 {
			glm::vec4 dir;
			glm::vec4 color;
		};
		DirLightV4 dirLightsV4[MAX_DIR_LIGHTS]; // because the GPU has issues with 3-component vectors, we transform to vec4
		for (u32 i = 0; i < numDirLights; i++) {
			const DirLight& dl = RW.dirLights.entries[i];
			dirLightsV4[i].dir = glm::vec4(dl.dir, 0);
			dirLightsV4[i].color = glm::vec4(dl.color, 0);
		}
		memcpy(data + sizeof(header), dirLightsV4, sizeof(dirLightsV4)* numDirLights);

		RU.device.flushBuffer(globalUnifBuffer);
	}

	RW._sortAndDefragmentObjects();

	const size_t numObjects = RW.objects_entry_to_id[0].size();
	u32 totalInstances = 0;
	for (size_t objectI = 0; objectI < numObjects; objectI++) {
		totalInstances += RW.objects_numInstances[0][objectI];
	}
	RW.objects_matricesTmp.resize(totalInstances);

	const glm::mat4 viewProj = rwViewport.projMtx * rwViewport.viewMtx;
	u32 instancesCursor_src = 0;
	u32 instancesCursor_dst = 0;
	auto objects_instancesCursors_alloc = tk::getStackTmpAllocator().alloc<u32>(numObjects);
	std::span<u32> objects_instancesCursors(objects_instancesCursors_alloc.ptr, numObjects);
	for (size_t objectI = 0; objectI < numObjects; objectI++) {
		const auto numInstances = RW.objects_numInstances[0][objectI];
		for (size_t instanceI = 0; instanceI < numInstances; instanceI++) {
			const glm::mat4& modelMtx = RW.modelMatrices[0][instancesCursor_src + instanceI];
			auto& Ms = RW.objects_matricesTmp[instancesCursor_dst + instanceI];
			Ms.modelMtx = modelMtx;
			Ms.modelViewProj = viewProj * modelMtx;
			Ms.invTransModelView = glm::inverse(glm::transpose(modelMtx));
		}
		objects_instancesCursors[objectI] = instancesCursor_dst;
		instancesCursor_src += RW.objects_numAllocInstances[0][objectI];
		instancesCursor_dst += RW.objects_numInstances[0][objectI];
	}

	const size_t instancingBufferRequiredSize = 3 * sizeof(glm::mat4) * size_t(totalInstances);
	const size_t instancingBufferRequiredExtendedSize = 4 * sizeof(glm::mat4) * size_t(totalInstances); // 33% more that the minimum required size
	// NOTE: notice we don't use a staging buffer here
	// From what I've read, since we are going to use it only once, it should be okay to use a host-visible buffer for instancing
	auto& instancingBuffers_scImg = RW.instancingBuffers[RU.swapchain.imgInd];
	if (instancingBuffers_scImg.size() <= renderTargetInd) {
		instancingBuffers_scImg.resize(renderTargetInd + 1);
	}

	auto& instancingBuffers = instancingBuffers_scImg[renderTargetInd];
	if (instancingBuffers.size() <= viewportInd) {
		instancingBuffers.resize(viewportInd + 1);
		vk::Buffer b = RU.device.createBuffer(vk::BufferUsage::vertexBuffer, instancingBufferRequiredExtendedSize, { .sequentialWrite = true });
		instancingBuffers[viewportInd] = b;
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
		auto meshId = RW.objects_mesh[0][objectI].id;
		auto meshInfo = meshId.getInfo();
		auto materialId = meshInfo.material.id;
		
		cmdBuffer_draw.cmd_bindGraphicsPipeline(meshId.getPipeline());
		
		cmdBuffer_draw.cmd_bindDescriptorSet(vk::PipelineBindPoint::graphics, materialId.getPipelineLayout(), DESCSET_GLOBAL, RW.global_descSets[scImgInd]); // TODO: move outside of the loop
		cmdBuffer_draw.cmd_bindDescriptorSet(vk::PipelineBindPoint::graphics, materialId.getPipelineLayout(), DESCSET_MATERIAL, materialId.getDescSet());
		
		const auto geomId = meshInfo.geom.id;
		const auto& geomInfo = geomId.getInfo();
		const auto geomBuffer = RU.device.getVkHandle(geomId.getBuffer());

		// instancing buffer
		cmdBuffer_draw.cmd_bindVertexBuffer(0, RU.device.getVkHandle(instancingBuffer), sizeof(RenderWorld::ObjectMatrices) * objects_instancesCursors[objectI]);
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
		auto numInstances = RW.objects_numInstances[0][objectI];
		if (geomInfo.indsOffset == u32(-1)) {
			cmdBuffer_draw.cmd_draw(geomInfo.numVerts, numInstances, 0, 0);
		}
		else {
			cmdBuffer_draw.cmd_bindIndexBuffer(geomBuffer, VK_INDEX_TYPE_UINT32, geomInfo.indsOffset);
			cmdBuffer_draw.cmd_drawIndexed(geomInfo.numInds, numInstances);
		}
	}
}

void prepareDraw()
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
}

void draw(
	CSpan<RenderWorldViewport> mainViewports,
	CSpan<RenderTargetWorldViewports> renderTargetsViewports
)
{
	ZoneScoped;
	RU.swapchain.waitCanStartFrame(RU.device);

	const auto mainQueue = RU.device.queues[0][0];
	const u32 scImgInd = RU.swapchain.imgInd;

	// destroy resources that had been scheduled
	auto handleDeferredDestroys = [scImgInd](auto& frames, auto& tmp, auto destroyFn) {
		for (auto& x : frames[scImgInd])
			destroyFn(x);
		frames[scImgInd].clear();
		std::swap(frames[scImgInd], tmp);
	};
	handleDeferredDestroys(RU.toDestroy.buffers, RU.toDestroy.buffersTmp, [](auto x) { RU.device.destroyBuffer(x); });
	handleDeferredDestroys(RU.toDestroy.framebuffers, RU.toDestroy.framebuffersTmp, [](auto x) { RU.device.destroyFramebuffer(x); });
	handleDeferredDestroys(RU.toDestroy.imageViews, RU.toDestroy.imageViewsTmp, [](auto x) { RU.device.destroyImageView(x); });
	handleDeferredDestroys(RU.toDestroy.images, RU.toDestroy.imagesTmp, [](auto x) { RU.device.destroyImage(x); });
	for (size_t poolI = 0; poolI < RU.toDestroy.descSets.size(); poolI++) {
		const auto& pool = RU.descPools[poolI];
		auto& descSets = RU.toDestroy.descSets[poolI][scImgInd];
		auto& descSetsTmp = RU.toDestroy.descSetsTmp[poolI];
		if (descSets.size()) {
			RU.device.freeDescriptorSets(pool, descSets);
			descSets.clear();
		}
		std::swap(descSets, descSetsTmp);
	}
	RU.toDestroy.pushToTmp = false;

	auto& framebuffer = RU.framebuffers[scImgInd];
	if (!framebuffer) {
		RU.depthStencilImages[scImgInd] = RU.device.createImage({
			.size = {u16(RU.screenW), u16(RU.screenH)},
			.format = RU.depthStencilFormat,
			.numSamples = RU.msaa,
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
	
	// renderTargets
	for (u32 rtI = 0; rtI < u32(renderTargetsViewports.size()); rtI++) {
		auto& rtv = renderTargetsViewports[rtI];
		auto& rt = RU.renderTargets[rtv.renderTarget.id];

		if (!rt.autoRedraw) {
			if (rt.needRedraw == 0)
				continue;
			else if (rt.needRedraw == u8(-1))
				rt.needRedraw = RU.swapchain.numImages;

			rt.needRedraw--;
		}

		//const vk::Image attachments[] = { rt.colorBuffer[scImgInd], rt.depthBuffer[scImgInd] };
		//cmdBuffer_draw.cmd_pipelineBarrier_imagesToColorAttachment(RU.device, attachments);

		// renderTargets - begin renderPass
		const glm::vec4& c = rtv.clearColor;
		const VkClearValue clearValues[] = {
			{.color = {.float32 = {c.r, c.g, c.b, c.a}}},
			{.depthStencil = {.depth = 1.f, .stencil = 0}},
		};
		cmdBuffer_draw.cmd_beginRenderPass({
			.renderPass = RU.renderPassOffscreen,
			.framebuffer = rt.framebuffer[scImgInd],
			.renderArea = {0, 0, rt.w, rt.h},
			.clearValues = clearValues,
		});

		for (u32 viewportInd = 0; viewportInd < u32(rtv.viewports.size()); viewportInd++) {
			const auto& viewport = rtv.viewports[viewportInd];
			draw_renderWorld(viewport, rtv.renderTarget.id + 1, viewportInd);
		}

		// end render pass
		cmdBuffer_draw.cmd_endRenderPass();

		//cmdBuffer_draw.cmd_pipelineBarrier_images_colorAttachment_to_shaderRead(RU.device, { &rt.colorBuffer[scImgInd], 1 });
	}

	// main - begin renderPass
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
	for (u32 viewportI = 0; viewportI < u32(mainViewports.size()); viewportI++) {
		const auto& viewport = mainViewports[viewportI];
		const RenderWorldId renderWorldId = viewport.renderWorld;
		RenderWorld& RW = RU.renderWorlds[renderWorldId.id];
		draw_renderWorld(viewport, /*main*/ 0, viewportI);
	}

	// imgui
	if(RU.imgui.enabled) {
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer_draw.handle);
	}
	
	// end render pass
	cmdBuffer_draw.cmd_endRenderPass();

	// submit everything
	cmdBuffer_draw.end();
	{
		const std::tuple<VkSemaphore, vk::PipelineStages> waitSemaphores[] = {
			{
				RU.swapchain.semaphore_imageAvailable[RU.swapchain.imageAvailableSemaphoreInd],
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
	RU.toDestroy.pushToTmp = true;

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

// --- imgui ---
VkDescriptorSet createImGuiTextureDescSet(VkSampler sampler, ImageViewId imgView, VkImageLayout layout)
{
	return ImGui_ImplVulkan_AddTexture(sampler, VkImageView(imgView.getInternalHandle()), layout);
}


void imgui_newFrame()
{
	if (!RU.imgui.enabled)
		return;
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

}
}