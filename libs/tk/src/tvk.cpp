#define VMA_IMPLEMENTATION
#include "tvk.hpp"
#include <array>
#include <glm/glm.hpp>
#include <shaderc/shaderc.h>

namespace tk {
namespace vk {
namespace
{
const u8 k_possibleNumSampleValues[] = { 1, 2, 4, 8, 16, 32, 64 };

u32 toVk(Version v) { return VK_MAKE_API_VERSION(0, v.major, v.minor, v.patch); }
u32 versionToU32(Version v) { return (v.major << 24) | (v.minor << 13) | v.patch; }

auto toVk(BufferUsage usage) { return VkBufferUsageFlags(usage); }
auto toVk(Format format) { return VkFormat(format); }
auto fromVk(VkFormat format) { return Format(format); }
auto toVk(Attachment_LoadOp op) {	return VkAttachmentLoadOp(op); }
auto toVk(Attachment_StoreOp op) { return VkAttachmentStoreOp(op); }
auto toVk(ImageLayout layout) { return VkImageLayout(layout); }
auto toVk(PipelineBindPoint point) { return VkPipelineBindPoint(point); }
auto toVk(PipelineStages stages) { return VkPipelineStageFlags(stages); }
auto toVk(ImageViewType type) { return VkImageViewType(type); }
auto toVk(ImageAspects aspects) { return VkImageAspectFlags(aspects); }
auto toVk(Topology t) { return VkPrimitiveTopology(t); }
auto toVk(PolygonMode mode) { return VkPolygonMode(mode); }
auto toVk(CompareOp op) { return VkCompareOp(op); }
auto toVk(StencilOp op) { return VkStencilOp(op); }
auto toVk(BlendFactor f) { return VkBlendFactor(f); }
auto toVk(BlendOp op) { return VkBlendOp(op); }
auto toVk(AccessFlags f) { return VkAccessFlags(f); }
auto toVk(DescriptorType t) { return VkDescriptorType(t); }
auto toVk(DescriptorSetLayoutCreateFlags f) { return VkDescriptorSetLayoutCreateFlags(f); }
auto toVk(Filter f) { return VkFilter(f); }
auto toVk(SamplerMipmapMode m) { return VkSamplerMipmapMode(m); }
auto toVk(SamplerAddressMode m) { return VkSamplerAddressMode(m); }
auto toVk(StencilOpState state) {
	return VkStencilOpState {
		toVk(state.failOp),
		toVk(state.passOp),
		toVk(state.depthFailOp),
		toVk(state.comapreOp),
		state.compareMask,
		state.writeMask,
		state.reference,
	};
}
auto toVk(ColorBlendAttachment a) {
	return VkPipelineColorBlendAttachmentState{
		.blendEnable = a.enableBlending,
		.srcColorBlendFactor = toVk(a.srcColorFactor),
		.dstColorBlendFactor = toVk(a.dstColorFactor),
		.colorBlendOp = toVk(a.colorBlendOp),
		.alphaBlendOp = toVk(a.alphaBlendOp),
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, // TODO
	};
}
auto toVk(ImageSubresourceRange r) {
	return VkImageSubresourceRange {
		.aspectMask = toVk(r.aspects),
		.baseMipLevel = r.baseMip,
		.levelCount = r.numMips,
		.baseArrayLayer = r.baseLayer,
		.layerCount = r.numLayers,
	};
}
auto toVk(MemoryBarrier b) {
	return VkMemoryBarrier {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = toVk(b.srcAccess),
		.dstAccessMask = toVk(b.dstAccess),
	};
}
auto toVk(BufferBarrier b) {
	return VkBufferMemoryBarrier {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = toVk(b.srcAccess),
		.dstAccessMask = toVk(b.dstAccess),
		.srcQueueFamilyIndex = b.srcQueueFamily,
		.dstQueueFamilyIndex = b.dstQueueFamily,
		.buffer = b.buffer,
		.offset = b.offset,
		.size = b.size,
	};
}
auto toVk(ImageBarrier b) {
	return VkImageMemoryBarrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = toVk(b.srcAccess),
		.dstAccessMask = toVk(b.dstAccess),
		.oldLayout = toVk(b.srcLayout),
		.newLayout = toVk(b.dstLayout),
		.srcQueueFamilyIndex = b.srcQueueFamily,
		.dstQueueFamilyIndex = b.dstQueueFamily,
		.image = b.image,
		.subresourceRange = toVk(b.subresourceRange),
	};
}

VkBufferCreateInfo makeBufferCreateInfo(BufferUsage usage, size_t size)
{
	return VkBufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = toVk(usage),
	};
}

std::vector<u8> loadBinaryFile(CStr fileName)
{
	FILE* file = fopen(fileName, "rb");
	if (!file)
		return {};
	defer(fclose(file));

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	std::vector<u8> data(size);
	fread(data.data(), 1, size, file);
	return data;
}

} // namespace (static funcs)

// -----------------------------------------------------------------------------------------

//struct VkResChecker { VkResChecker(VkResult r) { assert(r == VK_SUCCESS); } };
//#define CHECK_VKRES VkResChecker vkResChecker##__COUNTER__ = 

u32 PhysicalDeviceInfo::findGraphicsAndPresentQueueFamily()const {
	const u32 n = queueFamiliesProps.size();
	u32 i = 0;
	for (i = 0; i < n; i++) {
		const bool supportsGraphics = queueFamiliesProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		if (supportsGraphics && queueFamiliesPresentSupported[i])
			break;
	}
	return i;
}

void Device::waitIdle()
{
	ASSERT_VKRES(vkDeviceWaitIdle(device));
}

VkSemaphore Device::createSemaphore()
{
	const VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore semaphore;
	const VkResult vkRes = vkCreateSemaphore(device, &info, nullptr, &semaphore);
	ASSERT_VKRES(vkRes);
	return semaphore;
}

void Device::destroySemaphore(VkSemaphore semaphore)
{
	vkDestroySemaphore(device, VkSemaphore(semaphore), nullptr);
}

VkFence Device::createFence(bool signaled)
{
	const VkFenceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VkFenceCreateFlags(signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0),
	};
	VkFence fence;
	VkResult vkRes = vkCreateFence(device, &info, nullptr, &fence);
	ASSERT_VKRES(vkRes);
	return fence;
}

void Device::destroyFence(VkFence fence)
{
	vkDestroyFence(device, fence, nullptr);
}

static VmaAllocationCreateFlags toVma(BufferHostAccess hostAccess) {
	VmaAllocationCreateFlags flags = 0;
	if (hostAccess.sequentialWrite)
		flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	if (hostAccess.random)
		flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
	if (hostAccess.allowTransferInstead)
		flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
	return flags;
}

u32 Device::getMemTypeInd(BufferUsage usage, BufferHostAccess hostAccess, size_t size)
{
	//if (auto it = bufferMemTypeInds.find({ usage, hostAccess }); it == bufferMemTypeInds.end()) {
		VkBufferCreateInfo bufferInfo = makeBufferCreateInfo(usage, size);
		const VmaAllocationCreateInfo allocCreateInfo = {
			.flags = toVma(hostAccess),
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		u32 memType;
		VkResult vkRes = vmaFindMemoryTypeIndexForBufferInfo(allocator, &bufferInfo, &allocCreateInfo, &memType);
		ASSERT_VKRES(vkRes);
		//bufferMemTypeInds[{usage, hostAccess}] = u8(memType);
		return memType;
	/* }
	else {
		return u32(it->second);
	}*/
}

Buffer Device::createBuffer(BufferUsage usage, size_t size, BufferHostAccess hostAccess)
{
	u32 slot;
	if (buffers_nextFreeSlot != u32(-1)) {
		slot = buffers_nextFreeSlot;
		buffers_nextFreeSlot = u32(u64(buffers[slot].handle) & 0xFFFFFFFF);
	}
	else {
		slot = buffers.size();
		buffers.emplace_back();
	}

	const u32 memType = getMemTypeInd(usage, hostAccess, size);
	VmaAllocationCreateFlags flags = toVma(hostAccess);

	const VmaAllocationCreateInfo allocCreateInfo = {
		.flags = flags,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.memoryTypeBits = u32(1) << memType,
	};
	const auto bufferCreateInfo = makeBufferCreateInfo(usage, size);

	BufferData& buffer = buffers[slot];
	VkResult vkRes = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocCreateInfo, &buffer.handle, &buffer.alloc, &buffer.allocInfo);
	if (vkRes == VK_SUCCESS) {
		VkMemoryPropertyFlags memPropFlags;
		vmaGetAllocationMemoryProperties(allocator, buffer.alloc, &memPropFlags);
		if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			vmaMapMemory(allocator, buffer.alloc, &buffer.allocInfo.pMappedData);
		buffer.allocInfo.pUserData = (void*)(uintptr_t)usage;
		return { slot + 1 };
	}

	return { 0 };
}

void Device::destroyBuffer(Buffer buffer)
{
	if (buffer.id == 0) {
		assert(false);
		return;
	}
	const u32 slot = buffer.id - 1;
	BufferData& bufferData = buffers[slot];
	vmaDestroyBuffer(allocator, bufferData.handle, bufferData.alloc);
	bufferData.handle = VkBuffer(uintptr_t(buffers_nextFreeSlot));
	buffers_nextFreeSlot = slot;
}

u8* Device::getBufferMemPtr(Buffer buffer)
{
	if (buffer.id == 0) {
		assert(false);
		return nullptr;
	}

	const u32 slot = buffer.id - 1;
	return (u8*)buffers[slot].allocInfo.pMappedData;
}

size_t Device::getBufferSize(Buffer buffer)const
{
	if (buffer.id == 0) {
		assert(false);
		return 0;
	}

	const u32 slot = buffer.id - 1;
	return buffers[slot].allocInfo.size;
}

void Device::flushBuffer(Buffer buffer)
{
	if (buffer.id == 0) {
		assert(false);
		return;
	}

	ASSERT_VKRES(vmaFlushAllocation(allocator, buffers[buffer.id - 1].alloc, 0, VK_WHOLE_SIZE));
}

Image Device::registerImage(VkImage img, const ImageInfo& info, VmaAllocation alloc)
{
	u32 slot;
	if (images.nextFreeSlot == u32(-1)) {
		slot = images.handles.size();
		images.handles.emplace_back();
		images.infos.emplace_back();
		images.allocs.emplace_back();
	}
	else {
		slot = images.nextFreeSlot;
		images.nextFreeSlot = u32(uintptr_t(images.handles[slot]) & 0xFFFF'FFFF);
	}
	images.handles[slot] = img;
	images.infos[slot] = info;
	images.allocs[slot] = alloc;

	return { slot + 1 };
}

void Device::deregisterImage(Image img)
{
	if (img.id == 0) {
		assert(false);
		return;
	}

	const u32 slot = img.id - 1;
	images.handles[slot] = VkImage(uintptr_t(images.nextFreeSlot));
	assert(images.infos[slot].dimensions != 0 && "double free?");
	images.infos[slot].dimensions = 0;
	images.nextFreeSlot = slot;
}

Image Device::createImage(const ImageInfo& info)
{
	assert(info.dimensions >= 1 && info.dimensions <= 3);
	assert(info.size.width > 0 && info.size.height > 0 && info.size.depth > 0 && info.size.numMips > 0);
	assert(!(info.dimensions == 1 && info.size.height != 1));
	assert(has(k_possibleNumSampleValues, info.numSamples));
	assert(info.layout == ImageLayout::undefined || info.layout == ImageLayout::preinitialized);

	const VkImageUsageFlags usage = 0
		| (info.usage.transfer_src ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)
		| (info.usage.transfer_dst ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0)
		| (info.usage.sampled ? VK_IMAGE_USAGE_SAMPLED_BIT : 0)
		| (info.usage.storage ? VK_IMAGE_USAGE_STORAGE_BIT : 0)
		| (info.usage.input_attachment ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0)
		| (info.usage.output_attachment ?
			(formatIsDepth(info.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			: 0)
		| (info.usage.transient ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0)
	;

	const VkImageCreateInfo info2 = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0, // VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
		.imageType = VkImageType(info.dimensions - 1),
		.format = toVk(info.format),
		.extent = {
			info.size.width,
			info.size.height,
			info.getNumLayers(),
		},
		.mipLevels = info.size.numMips,
		.arrayLayers = u32(
			(info.dimensions == 1) ? info.size.height :
			(info.dimensions == 2) ? info.size.depth : 1),
		.samples = VkSampleCountFlagBits(info.numSamples),
		.tiling = VK_IMAGE_TILING_OPTIMAL, // TODO: make it configurable
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = toVk(info.layout),
	};
	VkImage image;
	const VmaAllocationCreateInfo allocCreateInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	VmaAllocation alloc;
	VmaAllocationInfo allocInfo;
	VkResult vkRes = vmaCreateImage(allocator, &info2, &allocCreateInfo, &image, &alloc, &allocInfo);
	ASSERT_VKRES(vkRes);
	return registerImage(image, info, alloc);
}

void Device::destroyImage(Image img)
{
	assert(img.id);
	const u32 slot = img.id - 1;
	assert(images.allocs[slot]);
	vmaDestroyImage(allocator, images.handles[slot], images.allocs[slot]);
	deregisterImage(img);
}

ImageView Device::createImageView(ImageViewInfo& info)
{
	assert(info.image.id);
	const u32 imgSlot = info.image.id - 1;
	const auto& imgInfo = images.infos[imgSlot];
	ImageViewType& type = info.type;
	if (type == ImageViewType::count) {
		const u32 numLayers = imgInfo.getNumLayers();
		const u32 numDims = imgInfo.dimensions;
		if (numLayers == 1) {
			assert(numDims >= 1 && numDims <= 3);
			if (numDims == 1) type = ImageViewType::_1d;
			if (numDims == 2) type = ImageViewType::_2d;
			if (numDims == 3) type = ImageViewType::_3d;
		}
		else {
			assert(numDims >= 1 && numDims <= 2);
			if (numDims == 1) type = ImageViewType::_1d_array;
			if (numDims == 2) type = ImageViewType::_2d_array;
		}
	}

	Format& format = info.format;
	if (format == Format::undefined)
		format = images.infos[info.image.id - 1].format;

	auto& aspects = info.aspects;
	if (aspects == ImageAspects::none) {
		if (formatIsColor(format))
			aspects |= ImageAspects::color;
		else {
			if (formatIsDepth(format))
				aspects |= ImageAspects::depth;
			if (formatIsStencil(format))
				aspects |= ImageAspects::stencil;
		}
	}

	u16& numMipLevels = info.numMipLevels;
	if (type != ImageViewType::_3d) { // 3d texture can't have mips
		if (info.numMipLevels == u16(-1))
			numMipLevels = imgInfo.size.numMips;
	}

	u16& numLayers = info.numLayers;
	if (numLayers == u16(-1))
		numLayers = imgInfo.size.numLayers;

	const VkImageViewCreateInfo infoVk = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = getVkHandle(info.image),
		.viewType = toVk(type),
		.format = toVk(format),
		.components = info.components,
		.subresourceRange = {
			.aspectMask = toVk(aspects),
			.baseMipLevel = info.baseMipLevel,
			.levelCount = type == ImageViewType::_3d ? u32(1) : u32(numMipLevels),
			.baseArrayLayer = info.baseLayer,
			.layerCount = numLayers,
		}
	};
	VkImageView view;
	ASSERT_VKRES(vkCreateImageView(device, &infoVk, nullptr, &view));
	
	u32 slot;
	if (imageViews.nextFreeSlot == u32(-1)) {
		slot = imageViews.handles.size();
		imageViews.handles.emplace_back();
		imageViews.infos.emplace_back();
	}
	else {
		slot = imageViews.nextFreeSlot;
		imageViews.nextFreeSlot = u32(uintptr_t(imageViews.handles[slot]) & 0xFFFF'FFFF);
	}
	imageViews.handles[slot] = view;
	imageViews.infos[slot] = info;
	return { slot + 1 };
}

void Device::destroyImageView(ImageView imgView)
{
	if (imgView.id == 0) {
		assert(false);
		return;
	}
	const u32 slot = imgView.id - 1;
	vkDestroyImageView(device, imageViews.handles[slot], nullptr);

	imageViews.handles[slot] = VkImageView(uintptr_t(imageViews.nextFreeSlot));
	assert(imageViews.infos[slot].image.id != 0 && "double free?");
	imageViews.infos[slot].image = { 0 };
	imageViews.nextFreeSlot = slot;
}

VkSampler Device::createSampler(const SamplerInfo& info)
{
	const VkSamplerCreateInfo infoVk = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = toVk(info.magFilter),
		.minFilter = toVk(info.minFilter),
		.mipmapMode = toVk(info.mipmapMode),
		.addressModeU = toVk(info.addressModeX),
		.addressModeV = toVk(info.addressModeY),
		.addressModeW = toVk(info.addressModeZ),
		.mipLodBias = info.mipLodBias,
		.anisotropyEnable = info.anisotropyEnable,
		.maxAnisotropy = info.maxAnisotropy,
		.compareEnable = info.compareEnable,
		.compareOp = toVk(info.compareOp),
		.minLod = info.minLod,
		.maxLod = info.maxLod,
		.borderColor = info.borderColor,
		.unnormalizedCoordinates = info.unnormalizedCoordinates,
	};
	VkSampler sampler;
	ASSERT_VKRES(vkCreateSampler(device, &infoVk, nullptr, &sampler));
	return sampler;
}

VkSampler Device::createSampler(Filter minFilter, Filter magFilter, SamplerMipmapMode mipmapMode, SamplerAddressMode addressMode, float maxAnisotropy)
{
	const SamplerInfo info = {
		.minFilter = minFilter,
		.magFilter = magFilter,
		.mipmapMode = mipmapMode,
		.addressModeX = addressMode,
		.addressModeY = addressMode,
		.addressModeZ = addressMode,
		.anisotropyEnable = maxAnisotropy > 1.f,
		.maxAnisotropy = maxAnisotropy,
	};
	return createSampler(info);
}

void Device::destroySampler(VkSampler sampler)
{
	vkDestroySampler(device, sampler, nullptr);
}

VkCommandPool Device::createCmdPool(u32 queueFamily, CmdPoolOptions options)
{
	VkCommandPoolCreateFlags flags = 0;
	if(options.transientCmdBuffers)
		flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	if (options.reseteableCmdBuffers)
		flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	const VkCommandPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = flags,
	};
	VkCommandPool pool;
	ASSERT_VKRES(vkCreateCommandPool(device, &info, nullptr, &pool));
	return pool;
}

void Device::allocCmdBuffers(VkCommandPool pool, std::span<CmdBuffer> cmdBuffers, bool secondary)
{
	const VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = u32(cmdBuffers.size()),
	};

	VkCommandBuffer* cmdBuffersVk = (VkCommandBuffer*)cmdBuffers.data();
#ifndef NDEBUG
	// in debug  CmdBuffer had some debug member variables so the raw cast is not valid
	std::vector<VkCommandBuffer> cmdBuffersVec(cmdBuffers.size());
	cmdBuffersVk = cmdBuffersVec.data();
#endif

	ASSERT_VKRES(vkAllocateCommandBuffers(device, &info, cmdBuffersVk));

#ifndef NDEBUG
	for (size_t i = 0; i < cmdBuffers.size(); i++)
		cmdBuffers[i].handle = cmdBuffersVec[i];
#endif
}

VkDescriptorSetLayout Device::createDescriptorSetLayout(CSpan<DescriptorSetLayoutBindingInfo> bindings, DescriptorSetLayoutCreateFlags flags)
{
	const VkDescriptorSetLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = toVk(flags),
		.bindingCount = u32(bindings.size()),
		.pBindings = (const VkDescriptorSetLayoutBinding*)bindings.data(),
	};
	VkDescriptorSetLayout layout;
	ASSERT_VKRES(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));
	return layout;
}

void Device::destroyDescriptorSetLayout(VkDescriptorSetLayout descSetLayout)
{
	vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
}

VkDescriptorPool Device::createDescriptorPool(u32 maxSets, CSpan<VkDescriptorPoolSize> maxPerType, DescPoolOptions options)
{
	VkDescriptorPoolCreateFlags flags = 0;
	if (options.allowFreeIndividualSets)
		flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	if (options.allowUpdateAfterBind)
		flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	const VkDescriptorPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = flags,
		.maxSets = maxSets,
		.poolSizeCount = u32(maxPerType.size()),
		.pPoolSizes = maxPerType.data(),
	};
	VkDescriptorPool pool;
	ASSERT_VKRES(vkCreateDescriptorPool(device, &info, nullptr, &pool));
	return pool;
}

void Device::destroyDescriptorPool(VkDescriptorPool pool)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkResult Device::allocDescriptorSets(VkDescriptorPool pool, CSpan<VkDescriptorSetLayout> layouts, std::span<VkDescriptorSet> descSets)
{
	assert(layouts.size() == descSets.size());
	const VkDescriptorSetAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool,
		.descriptorSetCount = u32(descSets.size()),
		.pSetLayouts = layouts.data(),
	};
	return vkAllocateDescriptorSets(device, &info, descSets.data());
}

VkResult Device::allocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout layout, std::span<VkDescriptorSet> descSets)
{
	std::vector<VkDescriptorSetLayout> layouts(descSets.size(), layout);
	return allocDescriptorSets(pool, layouts, descSets);
}

void Device::freeDescriptorSets(VkDescriptorPool pool, CSpan<VkDescriptorSet> descSets)
{
	ASSERT_VKRES(vkFreeDescriptorSets(device, pool, u32(descSets.size()), descSets.data()));
}

void Device::updateDescriptorSets(CSpan<DescriptorSetArrayWrite> writes, CSpan<DescriptorSetArrayCopy> copies)
{
	std::vector<VkWriteDescriptorSet> writesVk(writes.size());
	for (size_t i = 0; i < writes.size(); i++) {
		const auto& w = writes[i];
		writesVk[i] = VkWriteDescriptorSet {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = w.descSet,
			.dstBinding = w.binding,
			.dstArrayElement = w.startElem,
			.descriptorType = toVk(w.type),
		};
		static const DescriptorType k_imagesDescTypes[] = {
			DescriptorType::sampler,
			DescriptorType::combinedImageSampler,
			DescriptorType::sampledImage,
			DescriptorType::storageImage,
			DescriptorType::inputAttachment
		};
		static const DescriptorType k_bufferDescTypes[] = {
			DescriptorType::uniformBuffer,
			DescriptorType::storageBuffer,
			DescriptorType::uniformBufferDynamic,
			DescriptorType::storageBufferDynamic,
		};
		if (has(k_imagesDescTypes, w.type)) {
			writesVk[i].descriptorCount = w.imageInfos.size();
			writesVk[i].pImageInfo = (const VkDescriptorImageInfo*)w.imageInfos.data();
		}
		else if (has(k_bufferDescTypes, w.type)) {
			writesVk[i].descriptorCount = w.bufferInfos.size();
			writesVk[i].pBufferInfo = (const VkDescriptorBufferInfo*)w.bufferInfos.data();
		}
		else {
			writesVk[i].descriptorCount = w.bufferInfos.size();
			writesVk[i].pTexelBufferView = w.texelBufferViews.data();
		}
	}

	std::vector<VkCopyDescriptorSet> copiesVk(copies.size());
	for (size_t i = 0; i < copies.size(); i++) {
		const auto& c = copies[i];
		copiesVk[i] = {
			.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
			.srcSet = c.srcSet,
			.srcBinding = c.srcBinding,
			.srcArrayElement = c.srcStartElem,
			.dstSet = c.dstSet,
			.dstBinding = c.dstBinding,
			.dstArrayElement = c.dstStartElem,
			.descriptorCount = c.numArrayElems,
		};
	}

	vkUpdateDescriptorSets(device, u32(writesVk.size()), writesVk.data(), u32(copiesVk.size()), copiesVk.data());
}

void Device::writeDescriptorSet(const DescriptorSetWrite& write)
{
	const DescriptorSetArrayWrite write2 = {
		.descSet = write.descSet,
		.binding = write.binding,
		.startElem = 0,
		.type = write.type,
		.imageInfos = {&write.imageInfo, 1}, // assuming this works for the union type
	};
	writeDescriptorSetArrays({ &write2, 1 });
}

void Device::writeDescriptorSets(CSpan<DescriptorSetWrite> writes)
{
	std::vector<DescriptorSetArrayWrite> writes2(writes.size());
	for (size_t i = 0; i < writes.size(); i++) {
		auto& write = writes[i];
		writes2[i] = {
			.descSet = write.descSet,
			.binding = write.binding,
			.startElem = 0,
			.type = write.type,
			.imageInfos = { &write.imageInfo, 1 }, // assuming this works for the union type
		};
	}
	writeDescriptorSetArrays(writes2);
}

VkRenderPass Device::createRenderPass(const RenderPassInfo& info)
{
	const u32 numAttachments = u32(info.attachments.size());
	VkAttachmentDescription attachments[16];
	assert(numAttachments <= std::size(attachments));
	for (u32 i = 0; i < numAttachments; i++) {
		const auto& attachment = info.attachments[i];
		const auto& ops = info.attachmentOps[i];
		assert(has(k_possibleNumSampleValues, u8(attachment.numSamples)));
		attachments[i] = {
			.flags = VkAttachmentDescriptionFlags(attachment.mayAlias ? VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT : 0),
			.format = toVk(attachment.format),
			.samples = VkSampleCountFlagBits(attachment.numSamples),
			.loadOp = toVk(ops.loadOp),
			.storeOp = toVk(ops.storeOp),
			.stencilLoadOp = toVk(ops.stencilLoadOp),
			.stencilStoreOp = toVk(ops.stencilStoreOp),
			.initialLayout = toVk(ops.expectedLayout),
			.finalLayout = toVk(ops.finalLayout),
		};
	}

	const u32 numSubpasses = u32(info.subpasses.size());
	VkSubpassDescription subpasses[16];
	assert(numSubpasses <= std::size(subpasses));
	std::array<VkAttachmentReference, 128> attachmentRefs;
	u32 attachmentRefs_start = 0;
	std::array<u32, 128> preserveAttachmentsArray;
	u32 preserveAttachmentsArray_start = 0;
	for (u32 i = 0; i < numSubpasses; i++) {
		const auto& subpass = info.subpasses[i];
		std::bitset<64> usedAttachments(0);
		const u32 numInputAttachments = u32(subpass.inputAttachments.size());
		const u32 numColorAttachments = u32(subpass.colorAttachments.size());

		auto inputAttachments = std::span(attachmentRefs).subspan(attachmentRefs_start);
		for (u32 i = 0; i < numInputAttachments; i++) {
			const u32 ind = subpass.inputAttachments[i];
			inputAttachments[i] = {
				.attachment = ind,
				.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			usedAttachments.set(ind);
		}
		attachmentRefs_start += numInputAttachments;

		auto colorAttachments = std::span(attachmentRefs).subspan(attachmentRefs_start);
		for (u32 i = 0; i < numColorAttachments; i++) {
			const u32 ind = subpass.colorAttachments[i];
			colorAttachments[i] = {
				.attachment = ind,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};
			usedAttachments.set(ind);
		}
		attachmentRefs_start += numColorAttachments;

		const VkAttachmentReference depthStencilAttachment = {
			.attachment = subpass.depthStencilAttachment,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};
		if(subpass.depthStencilAttachment != u32(-1))
			usedAttachments.set(subpass.depthStencilAttachment);

		auto preserveAttachments = std::span(preserveAttachmentsArray).subspan(preserveAttachmentsArray_start);
		u32 numPreserveAttachments = 0;
		for (u32 i = 0; i < numAttachments; i++) {
			if (!usedAttachments[i] && subpass.preserveAttachments[i]) {
				preserveAttachments[numPreserveAttachments] = i;
				numPreserveAttachments++;
			}
		}

		subpasses[i] = {
			.pipelineBindPoint = toVk(subpass.pipelineBindPoint),
			.inputAttachmentCount = numInputAttachments,
			.pInputAttachments = &inputAttachments[0],
			.colorAttachmentCount = numColorAttachments,
			.pColorAttachments = &colorAttachments[0],
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = subpass.depthStencilAttachment == u32(-1) ? nullptr : &depthStencilAttachment,
			.preserveAttachmentCount = numPreserveAttachments,
			.pPreserveAttachments = &preserveAttachments[0],
		};
	}

	const u32 numDeps = u32(info.dependencies.size());
	std::array<VkSubpassDependency, 64> deps;
	for (u32 i = 0; i < numDeps; i++) {
		const auto& dep = info.dependencies[i];
		deps[i] = {
			.srcSubpass = dep.srcSubpass,
			.dstSubpass = dep.dstSubpass,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // TODO: optimize
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // usume that images are read in the fragment shader?
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // TODO: maybe we can optimize this by checking the actual attachments of the src subpass
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, // ... and for the dst subpass too
			.dependencyFlags = VkDependencyFlags(dep.supportRegionTiling ? VK_DEPENDENCY_BY_REGION_BIT : 0),
		};
	}

	const VkRenderPassCreateInfo info2 = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = u32(info.attachments.size()),
		.pAttachments = attachments,
		.subpassCount = numSubpasses,
		.pSubpasses = subpasses,
		.dependencyCount = numDeps,
		.pDependencies = &deps[0],
	};
	VkRenderPass handle;
	VkResult vkRes = vkCreateRenderPass(device, &info2, nullptr, &handle);
	ASSERT_VKRES(vkRes);
	return handle;
}

VkRenderPass Device::createRenderPass_simple(const RenderPassInfo_simple& info)
{
	const SubPassInfo subpass = {
		.pipelineBindPoint = PipelineBindPoint::graphics,
		.inputAttachments = {},
		.colorAttachments = info.colorAttachments,
		.depthStencilAttachment = info.depthStencilAttachment,
	};

	const RenderPassInfo info2 = {
		.attachments = info.attachments,
		.attachmentOps = info.attachmentOps,
		.subpasses = {&subpass, 1},
		.dependencies = {},
	};
	return createRenderPass(info2);
}

void Device::destroyRenderPass(VkRenderPass rp)
{
	vkDestroyRenderPass(device, rp, nullptr);
}

VkPipelineLayout Device::createPipelineLayout(CSpan<VkDescriptorSetLayout> descSetLayouts, CSpan<VkPushConstantRange> pushConstantRanges)
{
	const VkPipelineLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.flags = 0, // TODO?
		.setLayoutCount = u32(descSetLayouts.size()),
		.pSetLayouts = descSetLayouts.data(),
		.pushConstantRangeCount = u32(pushConstantRanges.size()), // TODO?
		.pPushConstantRanges = pushConstantRanges.data(),
	};
	VkPipelineLayout layout;
	ASSERT_VKRES(vkCreatePipelineLayout(device, &info, nullptr, &layout));
	return layout;
}

VkFramebuffer Device::createFramebuffer(const FramebufferInfo& info)
{
	u32 wh[] = { info.width, info.height };
	u32 h = info.height;
	for (u32 i = 0; i < 2; i++) {
		u32& d = wh[i];
		if (d != 0)
			continue;
		for (auto& a : info.attachments) {
			auto& imgViewInfo = imageViews.infos[a.id - 1];
			assert(imgViewInfo.numMipLevels == 1);
			assert(imgViewInfo.type == ImageViewType::_2d); // TODO: maybe accept ImageViewType::_2d_array as well
			const u32 imgInd = imgViewInfo.image.id - 1;
			const u32 d2 = images.infos[imgInd].size.vec()[i];
			if (d == 0)
				d = d2;
			else
				d = glm::min(d, d2);
		}
	}
	assert(wh[0] > 0 && wh[1] > 0);

	std::array<VkImageView, 16> attachments;
	for (size_t i = 0; i < info.attachments.size(); i++) {
		const ImageView imgView = info.attachments[i];
		attachments[i] = imageViews.handles[imgView.id - 1];
	}

	const VkFramebufferCreateInfo info2 = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = info.renderPass,
		.attachmentCount = u32(info.attachments.size()),
		.pAttachments = &attachments[0],
		.width = wh[0],
		.height = wh[1],
		.layers = 1,
	};
	VkFramebuffer handle;
	ASSERT_VKRES(vkCreateFramebuffer(device, &info2, nullptr, &handle));
	return handle;
}

void Device::destroyFramebuffer(VkFramebuffer fb)
{
	vkDestroyFramebuffer(device, fb, nullptr);
}

Shader Device::createShader(CSpan<u32> spirvCode)
{
	const VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = 4 * spirvCode.size(),
		.pCode = spirvCode.data(),
	};
	VkShaderModule module;
	ASSERT_VKRES(vkCreateShaderModule(device, &info, nullptr, &module));
	return { module };
}

Shader Device::loadShader(CStr spirvPath)
{
	auto data = loadBinaryFile(spirvPath);
	if (data.empty())
		return { 0 };
	
	return createShader({(u32*)data.data(), data.size() / 4 });
}

void Device::destroyPipelineLayout(VkPipelineLayout layout)
{
	vkDestroyPipelineLayout(device, layout, nullptr);
}

VkResult Device::createGraphicsPipelines(std::span<VkPipeline> pipelines, CSpan<GraphicsPipelineInfo> infos, VkPipelineCache cache)
{
	const size_t N = infos.size();
	assert(pipelines.size() == N);
	std::vector<VkGraphicsPipelineCreateInfo> infos2(N);

	std::vector<u32> numStages(N);
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.reserve(N * 4);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		u32 n = 0;
		auto addStage = [&](VkShaderStageFlagBits stage, auto stageInfo) {
			if (stageInfo.shader.handle) {
				stages.push_back(VkPipelineShaderStageCreateInfo{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.flags = 0, // TODO
					.stage = stage,
					.module = VkShaderModule(stageInfo.shader.handle),
					.pName = stageInfo.entryFnName,
					.pSpecializationInfo = &stageInfo.specialization,
				});
				n++;
			}
		};

		addStage(VK_SHADER_STAGE_VERTEX_BIT, info.shaderStages.vertex);
		addStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, info.shaderStages.tessControl);
		addStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, info.shaderStages.tessEval);
		addStage(VK_SHADER_STAGE_GEOMETRY_BIT, info.shaderStages.geom);
		addStage(VK_SHADER_STAGE_FRAGMENT_BIT, info.shaderStages.fragment);
		numStages[i] = n;
	}

	size_t totalVertexInputBindings = 0, totalVertexInputAttribs = 0;
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		totalVertexInputBindings += info.vertexInputBindings.size();
		totalVertexInputAttribs += info.vertexInputAttribs.size();
	}

	std::vector<VkPipelineVertexInputStateCreateInfo> vertexInputInfos(N);
	std::vector<VkVertexInputBindingDescription> vertexInputBindings(totalVertexInputBindings);
	std::vector<VkVertexInputAttributeDescription> vertexInputAttribs(totalVertexInputAttribs);
	u32 indVertexInputBinding = 0, indVertexInputAttrib = 0;
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		const u32 numVertexInputBindings = info.vertexInputBindings.size();
		for (u32 j = 0; j < numVertexInputBindings; j++) {
			vertexInputBindings[indVertexInputBinding + j] = {
				.binding = info.vertexInputBindings[j].binding,
				.stride = info.vertexInputBindings[j].stride,
				.inputRate = info.vertexInputBindings[j].perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
			};
		}
		const u32 numVertexInputAttribs = info.vertexInputAttribs.size();
		for (u32 j = 0; j < numVertexInputAttribs; j++) {
			vertexInputAttribs[indVertexInputAttrib + j] = {
				.location = info.vertexInputAttribs[j].location,
				.binding = info.vertexInputAttribs[j].binding,
				.format = toVk(info.vertexInputAttribs[j].format),
				.offset = info.vertexInputAttribs[j].offset,
			};
		}
		vertexInputInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = numVertexInputBindings,
			.pVertexBindingDescriptions = &vertexInputBindings[indVertexInputBinding],
			.vertexAttributeDescriptionCount = numVertexInputAttribs,
			.pVertexAttributeDescriptions = &vertexInputAttribs[indVertexInputAttrib],
		};

		indVertexInputBinding += info.vertexInputBindings.size();
		indVertexInputAttrib += info.vertexInputAttribs.size();
	}

	std::vector<VkPipelineInputAssemblyStateCreateInfo> inputAssemblyInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		inputAssemblyInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = toVk(info.topology),
			.primitiveRestartEnable = VK_FALSE, // TODO
		};
	}

	std::vector<VkPipelineViewportStateCreateInfo> viewportInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		viewportInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = (const VkViewport*)&info.viewport,
			.scissorCount = 1,
			.pScissors = (const VkRect2D*)&info.scissor,
		};
	}

	std::vector<VkPipelineRasterizationStateCreateInfo> rasterizationInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		rasterizationInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE, // TODO
			.rasterizerDiscardEnable = VK_FALSE, // TODO
			.polygonMode = toVk(info.polygonMode),
			.cullMode = VkCullModeFlags((info.cull_back ? VK_CULL_MODE_BACK_BIT : 0) | (info.cull_front ? VK_CULL_MODE_FRONT_BIT : 0)),
			.frontFace = info.clockwiseFaces ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE, // TODO
			.depthBiasConstantFactor = 0, // TODO
			.depthBiasClamp = 0, // TODO
			.depthBiasSlopeFactor = 0, // TODO
			.lineWidth = 1, // TODO
		};
	}

	std::vector<VkPipelineMultisampleStateCreateInfo> multisampleInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		multisampleInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VkSampleCountFlagBits(info.numSamples),
			.sampleShadingEnable = VK_FALSE, // TODO
		};
	}

	std::vector<VkPipelineDepthStencilStateCreateInfo> depthStencilInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		depthStencilInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = info.depthTestEnable,
			.depthWriteEnable = info.depthWriteEnable,
			.depthCompareOp = toVk(info.depthCompareOp),
			.depthBoundsTestEnable = VK_FALSE, // TODO
			.stencilTestEnable = info.stencilTestEnable,
			.front = toVk(info.frontStencilOpState),
			.back = toVk(info.backStencilOpState),
			.minDepthBounds = 0, // TODO
			.maxDepthBounds = 1, // TODO
		};
	}

	u32 totalColorBlendAttachments = 0;
	for (size_t i = 0; i < N; i++)
		totalColorBlendAttachments += u32(infos[i].colorBlendAttachments.size());
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	colorBlendAttachments.reserve(totalColorBlendAttachments);
	for (size_t i = 0; i < N; i++) {
		for (const auto& a : infos[i].colorBlendAttachments)
			colorBlendAttachments.push_back(toVk(a));
	}

	u32 indColorBlendAttachments = 0;
	std::vector<VkPipelineColorBlendStateCreateInfo> colorBlendInfos(N);
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		colorBlendInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE, // TODO,
			.logicOp = VK_LOGIC_OP_CLEAR, // TODO
			.attachmentCount = u32(info.colorBlendAttachments.size()),
			.pAttachments = &colorBlendAttachments[indColorBlendAttachments],
			.blendConstants = {info.blendConstant.r, info.blendConstant.g, info.blendConstant.b, info.blendConstant.a},
		};
		indColorBlendAttachments += u32(info.colorBlendAttachments.size());
	}

	auto iterateDynamicStates = [](PipelineDynamicStates s, auto fn) {
		if(s.viewport)
			fn(VK_DYNAMIC_STATE_VIEWPORT);
		if(s.scissor)
			fn(VK_DYNAMIC_STATE_SCISSOR);
		if(s.lineWidth)
			fn(VK_DYNAMIC_STATE_LINE_WIDTH);
		if(s.depthBias)
			fn(VK_DYNAMIC_STATE_DEPTH_BIAS);
		if(s.blendConstant)
			fn(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		if(s.stencilComapreMask)
			fn(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
		if(s.stencilWriteMask)
			fn(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
		if (s.stencilReference)
			fn(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
	};
	u32 totalDynamicStates = 0;
	for (size_t i = 0; i < N; i++)
		iterateDynamicStates(infos[i].dynamicStates, [&totalDynamicStates](VkDynamicState s) { totalDynamicStates++; });
	std::vector<VkDynamicState> dynamicStates;
	dynamicStates.reserve(totalDynamicStates);
	for (size_t i = 0; i < N; i++)
		iterateDynamicStates(infos[i].dynamicStates, [&dynamicStates](VkDynamicState s) { dynamicStates.push_back(s); });
	std::vector<VkPipelineDynamicStateCreateInfo> dynamicStatesInfos(N);
	u32 indDynamicState = 0;
	for (size_t i = 0; i < N; i++) {
		u32 n = 0;
		iterateDynamicStates(infos[i].dynamicStates, [&n](VkDynamicState s) { n++; });
		dynamicStatesInfos[i] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = n,
			.pDynamicStates = &dynamicStates[indDynamicState],
		};
		indDynamicState += n;
	}

	u32 indStages = 0;
	for (size_t i = 0; i < N; i++) {
		const auto& info = infos[i];
		infos2[i] = VkGraphicsPipelineCreateInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = numStages[i],
			.pStages = &stages[indStages],
			.pVertexInputState = &vertexInputInfos[i],
			.pInputAssemblyState = &inputAssemblyInfos[i],
			.pTessellationState = nullptr, // TODO
			.pViewportState = &viewportInfos[i],
			.pRasterizationState = &rasterizationInfos[i],
			.pMultisampleState = multisampleInfos.data() + i, // TODO
			.pDepthStencilState = &depthStencilInfos[i],
			.pColorBlendState = &colorBlendInfos[i],
			.pDynamicState = &dynamicStatesInfos[i],
			.layout = info.layout,
			.renderPass = info.renderPass,
			.subpass = info.subpassInd,
			.basePipelineHandle = VK_NULL_HANDLE, // TODO
			.basePipelineIndex = 0, // TODO
		};
		indStages += numStages[i];
	}
	return vkCreateGraphicsPipelines(device, cache, infos2.size(), infos2.data(), nullptr, pipelines.data());
}

void Device::destroyPipeline(VkPipeline pipeline)
{
	vkDestroyPipeline(device, pipeline, nullptr);
}

void Device::submit(VkQueue queue, CSpan<SubmitInfo> submits, VkFence signalFence)
{
#ifndef NDEBUG
	for (auto& submit : submits) {
		for (auto& cmdBuffer : submit.cmdBuffers) {
			auto& state = *const_cast<CmdBuffer::State*>(&cmdBuffer.state);
			assert(state == CmdBuffer::State::executable || state == CmdBuffer::State::executable_oneTimeSubmit || state == CmdBuffer::State::executableOrPending);
			state = state == CmdBuffer::State::executable_oneTimeSubmit ? CmdBuffer::State::pendingOrInvalid : CmdBuffer::State::executableOrPending;
		}
	}
#endif
	const u32 N = submits.size();
	u32 indWaitSemaphores = 0;
	u32 indCmdBuffers = 0;
	u32 indSignalSemaphores = 0;
	for (u32 i = 0; i < N; i++) {
		const auto& submit = submits[i];
		indWaitSemaphores += submit.waitSemaphores.size();
		indCmdBuffers += submit.cmdBuffers.size();
		indSignalSemaphores += submit.signalSemaphores.size();
	}
	std::vector<VkSemaphore> waitSemaphores(indWaitSemaphores);
	std::vector<VkPipelineStageFlags> waitStagesMasks(indWaitSemaphores);
	std::vector<VkCommandBuffer> cmdBuffers(indCmdBuffers); // TODO: can be optimized in release mode: in debug mode there is the state variable in CmdBuffer, but in release we can directly cast pointers to avoid copying
	std::vector<VkSemaphore> signalSemaphores(indSignalSemaphores);
	indWaitSemaphores = indCmdBuffers = indSignalSemaphores = 0;
	for (u32 i = 0; i < N; i++) {
		const auto& submit = submits[i];
		for (u32 j = 0; j < u32(submit.signalSemaphores.size()); j++) {
			waitSemaphores[indWaitSemaphores + j] = std::get<0>(submit.waitSemaphores[j]);
			waitStagesMasks[indWaitSemaphores + j] = toVk(std::get<1>(submit.waitSemaphores[j]));
		}

		for (u32 j = 0; j < u32(submit.cmdBuffers.size()); j++)
			cmdBuffers[indCmdBuffers + j] = submit.cmdBuffers[j].handle;

		for (u32 j = 0; j < u32(submit.signalSemaphores.size()); j++)
			signalSemaphores[indSignalSemaphores + j] = submit.signalSemaphores[j];

		indWaitSemaphores += submit.waitSemaphores.size();
		indCmdBuffers += submit.cmdBuffers.size();
		indSignalSemaphores += submit.signalSemaphores.size();
	}

	indWaitSemaphores = indCmdBuffers = indSignalSemaphores = 0;
	std::vector<VkSubmitInfo> infos(N);
	for (u32 i = 0; i < N; i++) {
		const auto& submit = submits[i];
		infos[i] = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = u32(submit.waitSemaphores.size()),
			.pWaitSemaphores = waitSemaphores.data() + indWaitSemaphores,
			.pWaitDstStageMask = waitStagesMasks.data() + indWaitSemaphores,
			.commandBufferCount = u32(submit.cmdBuffers.size()),
			.pCommandBuffers = cmdBuffers.data() + indCmdBuffers,
			.signalSemaphoreCount = u32(submit.signalSemaphores.size()),
			.pSignalSemaphores = signalSemaphores.data() + indSignalSemaphores,
		};

		indWaitSemaphores += submit.waitSemaphores.size();
		indCmdBuffers += submit.cmdBuffers.size();
		indSignalSemaphores += submit.signalSemaphores.size();
	}
	vkQueueSubmit(queue, N, infos.data(), signalFence);
}

void Device::getSupportedSurfaceFormats(VkSurfaceKHR surface, u32& count, SurfaceFormat* formats)
{
	ASSERT_VKRES(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.handle, surface, &count, (VkSurfaceFormatKHR*)formats));
}

SurfaceFormat Device::getBasicSupportedSurfaceFormat(VkSurfaceKHR surface)
{
	u32 n;
	getSupportedSurfaceFormats(surface, n, nullptr);
	std::vector<SurfaceFormat> formats(n);
	getSupportedSurfaceFormats(surface, n, formats.data());
	assert(n > 0);
	for (auto f : formats) {
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == Format::RGBA8_SRGB)
			return f;
	}
	for (auto f : formats) {
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == Format::ABGR8_SRGB)
			return f;
	}
	for (auto f : formats) {
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == Format::BGRA8_SRGB)
			return f;
	}
	for (auto f : formats) {
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == Format::RGB8_SRGB)
			return f;
	}
	for (auto f : formats) {
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && f.format == Format::BGR8_SRGB)
			return f;
	}
	return formats[0];
}

CSpan<Format> Device::getSupportedDepthStencilFormats(std::span<Format> formats)
{
	VkImageFormatProperties props;

	const Format allFormats[] = {
		Format::D16_UNORM,
		Format::X8_D24_UNORM,
		Format::D32_SFLOAT,
		Format::S8_UINT,
		Format::D16_UNORM_S8_UINT,
		Format::D24_UNORM_S8_UINT,
		Format::D32_SFLOAT_S8_UINT,
	};
	assert(formats.size() >= std::size(allFormats));
	size_t count = 0;
	for (u32 i = 0; i < std::size(allFormats); i++) {
		VkResult res = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.handle, toVk(allFormats[i]),
			VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VkImageCreateFlags(0), &props);
		if (VK_ERROR_FORMAT_NOT_SUPPORTED != res) {
			formats[count] = allFormats[i];
			count++;
		}
	}
	return formats.subspan(0, count);
}

Format Device::getSupportedDepthStencilFormat_firstAmong(CSpan<Format> formats)
{
	VkImageFormatProperties props;
	for (auto f : formats) {
		assert(formatIsDepth(f) || formatIsStencil(f));
		VkResult res = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.handle, toVk(f),
			VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VkImageCreateFlags(0), &props);
		if (VK_ERROR_FORMAT_NOT_SUPPORTED != res) {
			return f;
		}
	}
	return Format::undefined;
}

void CmdBuffer::begin(CmdBufferUsageFlags usageFlags, const VkCommandBufferInheritanceInfo* inheritanceInfo)
{
	VkCommandBufferUsageFlags flags = 0;
	if (usageFlags.oneTimeSubmit)
		flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (usageFlags.renderPassContinue)
		flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	if (usageFlags.simultaneousUse)
		flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	const VkCommandBufferBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = flags,
		.pInheritanceInfo = inheritanceInfo,
	};
	ASSERT_VKRES(vkBeginCommandBuffer(handle, &info));
#ifndef NDEBUG
	assert(state != State::recording && state != State::recording_oneTimeSubmit);
	state = usageFlags.oneTimeSubmit ? State::recording_oneTimeSubmit : State::recording;
#endif
}

void CmdBuffer::reset(bool releaseResources)
{
	const VkCommandBufferResetFlags flags = releaseResources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;
	vkResetCommandBuffer(handle, flags);
#ifndef NDEBUG
	//assert(state != State::recording);
	state = State::initial;
#endif
}

void CmdBuffer::end()
{
	ASSERT_VKRES(vkEndCommandBuffer(handle));
#ifndef NDEBUG
	assert(state == State::recording || state == State::recording_oneTimeSubmit);
	state = state == State::recording_oneTimeSubmit ? State::executable_oneTimeSubmit : State::executable;
#endif
}

#define CMD_BUFFER_ASSERT_RECORDING assert(state == State::recording || state == State::recording_oneTimeSubmit);

void CmdBuffer::cmd_copy(VkBuffer src, VkBuffer dst, CSpan<VkBufferCopy> regions)
{
	CMD_BUFFER_ASSERT_RECORDING;
	vkCmdCopyBuffer(handle, src, dst, u32(regions.size()), regions.data());
}

void CmdBuffer::cmd_copy(VkBuffer src, VkBuffer dst, size_t srcOffset, size_t dstOffset, size_t size)
{
	const VkBufferCopy regions[] = { {.srcOffset = srcOffset, .dstOffset = dstOffset, .size = size } };
	cmd_copy(src, dst, regions);
}

void CmdBuffer::cmd_beginRenderPass(const RenderPassBegin& beg)
{
	const VkRenderPassBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = beg.renderPass,
		.framebuffer = beg.framebuffer,
		.renderArea = beg.renderArea.vkRect2d,
		.clearValueCount = u32(beg.clearValues.size()),
		.pClearValues = beg.clearValues.data(),
	};
	const auto subpassContents = beg.subPassContentsInSecondaryCmdBuffers ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
	vkCmdBeginRenderPass(handle, &info, subpassContents);
}

void CmdBuffer::cmd_copy(VkBuffer src, VkImage dst, CSpan<VkBufferImageCopy> regions)
{
	vkCmdCopyBufferToImage(handle, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, u32(regions.size()), regions.data());
}

void CmdBuffer::cmd_copy(Buffer src, Image dst, const Device& device, size_t bufferOffset, u32 mipLevel)
{
	const VkImage dstVk = device.getVkHandle(dst);
	const ImageInfo imgInfo = device.getInfo(dst);
	const VkBufferImageCopy regions[] = { {
		.bufferOffset = bufferOffset,
		.bufferRowLength = imgInfo.size.width,
		.bufferImageHeight = imgInfo.size.height,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = mipLevel,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0,0,0},
		.imageExtent = {imgInfo.size.width, imgInfo.size.height, imgInfo.size.depth},
	} };
	cmd_copy(device.getVkHandle(src), dstVk, regions);
}

void CmdBuffer::cmd_blitToNextMip(const Device& device, Image img, u32 srcMip)
{
	const auto& imgInfo = device.getInfo(img);
	const auto& imgSize = imgInfo.size;
	const u32 numLayers = imgInfo.getNumLayers();
	const u32 dstMip = srcMip + 1;
	auto calcMipSize = [](u32 size, u32 downLevels)->u16 { return glm::max(u32(1), size >> downLevels); };
	const VkImageBlit blit = {
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = srcMip,
			.baseArrayLayer = 0,
			.layerCount = numLayers,
		},
		.srcOffsets = {
			{0,0,0},
			{calcMipSize(imgSize.width, srcMip), calcMipSize(imgSize.height, srcMip), calcMipSize(imgSize.depth, srcMip)},
		},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = dstMip,
			.baseArrayLayer = 0,
			.layerCount = numLayers,
		},
		.dstOffsets = {
			{0,0,0},
			{calcMipSize(imgSize.width, dstMip), calcMipSize(imgSize.height, dstMip), calcMipSize(imgSize.depth, dstMip)},
		},
	};
	const VkImage imgVk = device.getVkHandle(img);
	vkCmdBlitImage(handle, imgVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imgVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
}

void CmdBuffer::cmd_endRenderPass()
{
	vkCmdEndRenderPass(handle);
}

void CmdBuffer::cmd_pipelineBarrier(const PipelineBarrier& barrier)
{
	CMD_BUFFER_ASSERT_RECORDING;
	std::vector<VkMemoryBarrier> memoryBarriers(barrier.memoryBarriers.size());
	for (size_t i = 0; i < memoryBarriers.size(); i++)
		memoryBarriers[i] = toVk(barrier.memoryBarriers[i]);

	std::vector<VkBufferMemoryBarrier> bufferBarriers(barrier.bufferBarriers.size());
	for (size_t i = 0; i < bufferBarriers.size(); i++)
		bufferBarriers[i] = toVk(barrier.bufferBarriers[i]);

	std::vector<VkImageMemoryBarrier> imageBarriers(barrier.imageBarriers.size());
	for (size_t i = 0; i < imageBarriers.size(); i++)
		imageBarriers[i] = toVk(barrier.imageBarriers[i]);

	vkCmdPipelineBarrier(handle,
		toVk(barrier.srcStages), toVk(barrier.dstStages),
		VkDependencyFlags(barrier.dependencyFlags),
		memoryBarriers.size(), memoryBarriers.data(),
		bufferBarriers.size(), bufferBarriers.data(),
		imageBarriers.size(), imageBarriers.data()
	);
}

void CmdBuffer::cmd_pipelineBarrier_imagesToTransferDstLayout(Device& device, CSpan<Image> images)
{
	std::vector<ImageBarrier> barriers(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		const auto& imgInfo = device.getInfo(images[i]);
		barriers[i] = {
			.srcAccess = AccessFlags::none,
			.dstAccess = AccessFlags::transferWrite,
			.srcLayout = ImageLayout::undefined,
			.dstLayout = ImageLayout::transferDst,
			.image = device.getVkHandle(images[i]),
			.subresourceRange = {
				.numMips = imgInfo.size.numMips,
				.numLayers = imgInfo.size.numLayers,
			},
		};
	}
	const PipelineBarrier pb = {
		.srcStages = PipelineStages::host,
		.dstStages = PipelineStages::transfer,
		.imageBarriers = barriers,
	};
	cmd_pipelineBarrier(pb);
}

void CmdBuffer::cmd_pipelineBarrier_imagesToShaderReadLayout(Device& device, CSpan<Image> images)
{
	std::vector<ImageBarrier> barriers(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		const auto& imgInfo = device.getInfo(images[i]);
		barriers[i] = {
			.srcAccess = AccessFlags::transferWrite,
			.dstAccess = AccessFlags::shaderRead,
			.srcLayout = ImageLayout::transferDst,
			.dstLayout = ImageLayout::shaderReadOnly,
			.image = device.getVkHandle(images[i]),
			.subresourceRange = {
				.numMips = imgInfo.size.numMips,
				.numLayers = imgInfo.size.numLayers,
			},
		};
	}
	const PipelineBarrier pb = {
		.srcStages = PipelineStages::transfer,
		.dstStages = PipelineStages::fragmentShader,
		.imageBarriers = barriers,
	};
	cmd_pipelineBarrier(pb);
}

/*
void CmdBuffer::cmd_pipelineBarrier_images_colorAttachment_to_shaderRead(Device& device, CSpan<Image> images)
{
	std::vector<ImageBarrier> barriers(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		const auto& imgInfo = device.getInfo(images[i]);
		barriers[i] = {
			.srcAccess = AccessFlags::colorAtachmentWrite,
			.dstAccess = AccessFlags::shaderRead,
			.srcLayout = ImageLayout::colorAttachment,
			.dstLayout = ImageLayout::shaderReadOnly,
			.image = device.getVkHandle(images[i]),
			.subresourceRange = {
				.numMips = imgInfo.size.numMips,
				.numLayers = imgInfo.size.numLayers,
			},
		};
	}
	const PipelineBarrier pb = {
		.srcStages = PipelineStages::colorAttachmentOutput,
		.dstStages = PipelineStages::fragmentShader,
		.imageBarriers = barriers,
	};
	cmd_pipelineBarrier(pb);
}
*/

/*
void CmdBuffer::cmd_pipelineBarrier_imagesToColorAttachment(Device& device, CSpan<Image> images)
{
	std::vector<ImageBarrier> barriers(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		const auto& imgInfo = device.getInfo(images[i]);
		barriers[i] = {
			.srcAccess = AccessFlags::none,
			.dstAccess = AccessFlags::colorAtachmentWrite,
			.srcLayout = ImageLayout::undefined,
			.dstLayout = ImageLayout::colorAttachment,
			.image = device.getVkHandle(images[i]),
			.subresourceRange = {
				.numMips = imgInfo.size.numMips,
				.numLayers = imgInfo.size.numLayers,
			},
		};
	}
	const PipelineBarrier pb = {
		.srcStages = PipelineStages::none,
		.dstStages = PipelineStages::colorAttachmentOutput,
		.imageBarriers = barriers,
	};
	cmd_pipelineBarrier(pb);
}*/

/*void CmdBuffer::cmd_pipelineBarrier_imagesToDepthAttachmentLayout(Device& device, CSpan<Image> images)
{
	std::vector<ImageBarrier> barriers(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		const auto& imgInfo = device.getInfo(images[i]);
		barriers[i] = {
			.srcAccess = AccessFlags::none,
			.dstAccess = AccessFlags::depthStencilAtachmentRead | AccessFlags::depthStencilAtachmentWrite,
			.srcLayout = ImageLayout::undefined,
			.dstLayout = formatIsStencil(imgInfo.format) ? ImageLayout::depthStencilAttachment : ImageLayout::depthAttachment,
			.image = device.getVkHandle(images[i]),
			.subresourceRange = {
				.numMips = imgInfo.size.numMips,
				.numLayers = imgInfo.size.numLayers,
			},
		};
	}
	const PipelineBarrier pb = {
		.srcStages = PipelineStages::host,
		.dstStages = PipelineStages::earlyFragmentTests | PipelineStages::earlyFragmentTests,
		.imageBarriers = barriers,
	};
	cmd_pipelineBarrier(pb);
}*/

void CmdBuffer::cmd_bindPipeline(VkPipeline pipeline, PipelineBindPoint point)
{
	vkCmdBindPipeline(handle, toVk(point), pipeline);
}

void CmdBuffer::cmd_scissor(Rect2d r)
{
	vkCmdSetScissor(handle, 0, 1, (const VkRect2D*) &r);
}

void CmdBuffer::cmd_viewport(Viewport vp)
{
	vkCmdSetViewport(handle, 0, 1, (const VkViewport*)&vp);
}

void CmdBuffer::cmd_bindDescriptorSets(PipelineBindPoint bindPoint, VkPipelineLayout layout, u32 firstBinding, CSpan<VkDescriptorSet> descSets, CSpan<u32> dynamicOffsets)
{
	vkCmdBindDescriptorSets(handle, toVk(bindPoint), layout, firstBinding, u32(descSets.size()), descSets.data(), u32(dynamicOffsets.size()), dynamicOffsets.data());
}
void CmdBuffer::cmd_bindDescriptorSet(PipelineBindPoint bindPoint, VkPipelineLayout layout, u32 binding, VkDescriptorSet descSet)
{
	vkCmdBindDescriptorSets(handle, toVk(bindPoint), layout, binding, 1, &descSet, 0, nullptr);
}

void CmdBuffer::cmd_bindVertexBuffers(u32 firstBinding, CSpan<VkBuffer> vbs, CSpan<size_t> offsets)
{
	vkCmdBindVertexBuffers(handle, firstBinding, u32(vbs.size()), vbs.data(), offsets.data());
}

void CmdBuffer::cmd_bindVertexBuffers(u32 firstBinding, CSpan<VkBuffer> vbs)
{
	std::vector<size_t> offsets;
	offsets = std::vector<size_t>(vbs.size(), 0);
	cmd_bindVertexBuffers(firstBinding, vbs, offsets);
}

void CmdBuffer::cmd_bindVertexBuffer(u32 bindPoint, VkBuffer vb, size_t offset)
{
	vkCmdBindVertexBuffers(handle, bindPoint, 1, &vb, &offset);
}

void CmdBuffer::cmd_bindIndexBuffer(VkBuffer ib, VkIndexType indType, size_t offset)
{
	vkCmdBindIndexBuffer(handle, ib, offset, indType);
}

void CmdBuffer::cmd_draw(u32 numVerts, u32 numInstances, u32 firstVertex, u32 firstInstance)
{
	vkCmdDraw(handle, numVerts, numInstances, firstVertex, firstInstance);
}

void CmdBuffer::cmd_drawIndexed(u32 numInds, u32 numInstances, u32 firstInd, i32 vertOffset, u32 firstInstance)
{
	vkCmdDrawIndexed(handle, numInds, numInstances, firstInd, vertOffset, firstInstance);
}

VkInstance createInstance(const AppInfo& appInfo, CSpan<CStr> layers, CSpan<CStr> extensions)
{
	const VkApplicationInfo appInfoVk = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = appInfo.appName,
		.applicationVersion = versionToU32(appInfo.appVersion),
		.pEngineName = appInfo.engineName,
		.engineVersion = versionToU32(appInfo.engineVersion),
		.apiVersion = toVk(appInfo.apiVersion),
	};
	const VkInstanceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfoVk,
		.enabledLayerCount = u32(layers.size()),
		.ppEnabledLayerNames = layers.data(),
		.enabledExtensionCount = u32(extensions.size()),
		.ppEnabledExtensionNames = extensions.data(),
	};
	VkInstance instance;
	ASSERT_VKRES(vkCreateInstance(&info, nullptr, &instance));
	return instance;
}

void destroyInstance(VkInstance instance)
{
	vkDestroyInstance(instance, nullptr);
}

u32 getNumPhysicalDevices(VkInstance instance)
{
	u32 num;
	ASSERT_VKRES(vkEnumeratePhysicalDevices(instance, &num, nullptr));
	return num;
}

void getPhysicalDeviceInfos(VkInstance instance, std::vector<PhysicalDeviceInfo>& infos, VkSurfaceKHR surface)
{
	const u32 MAX_DEVICES = 16;
	u32 numDevices = getNumPhysicalDevices(instance);
	assert(numDevices <= MAX_DEVICES);
	VkPhysicalDevice physicalDevices[MAX_DEVICES];
	ASSERT_VKRES(vkEnumeratePhysicalDevices(instance, &numDevices, physicalDevices));

	infos.resize(numDevices);
	for (u32 i = 0; i < numDevices; i++) {
		infos[i].handle = physicalDevices[i];
		vkGetPhysicalDeviceFeatures(physicalDevices[i], &infos[i].features);
		vkGetPhysicalDeviceProperties(physicalDevices[i], &infos[i].props);
		vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &infos[i].memProps);

		u32 numQueueFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &numQueueFamilies, nullptr);
		infos[i].queueFamiliesProps.resize(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &numQueueFamilies, infos[i].queueFamiliesProps.data());

		infos[i].queueFamiliesPresentSupported.resize(numQueueFamilies);
		if (surface) {
			for (u32 j = 0; j < numQueueFamilies; j++) {
				VkBool32 supported = false;
				ASSERT_VKRES(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, surface, &supported));
				infos[i].queueFamiliesPresentSupported[j] = supported;
			}
		}
	}
}

bool PhysicalDeviceFilterAndCompare::defaultFilter_graphicsAndPresent(const PhysicalDeviceInfo& info)
{
	const size_t numQueueFamilies = info.queueFamiliesProps.size();
	assert(numQueueFamilies == info.queueFamiliesPresentSupported.size());
	for (size_t i = 0; i < numQueueFamilies; i++) {
		const bool supportsGraphics = info.queueFamiliesProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		if (supportsGraphics && info.queueFamiliesPresentSupported[i])
			return true;
	}
	return false;
}
int PhysicalDeviceFilterAndCompare::defaultCompare_discreteThenMemory(const PhysicalDeviceInfo& a, const PhysicalDeviceInfo& b)
{
	auto rateDeviceType = [](VkPhysicalDeviceType t) -> int {
		switch (t) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 10;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 9;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 8;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return 7;
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: return 6;
		default: assert(false); return 0;
		}
	};
	const int typeCmp = rateDeviceType(a.props.deviceType) - rateDeviceType(b.props.deviceType);
	if (typeCmp)
		return typeCmp;

	auto calcMem = [](const VkPhysicalDeviceMemoryProperties& memProps) -> int {
		int MB = 0;
		for (u32 i = 0; i < memProps.memoryHeapCount; i++) {
			if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				const int mb = memProps.memoryHeaps[i].size >> 20;
				MB += mb;
			}
		}
		return MB;
	};
	const int memA = calcMem(b.memProps);
	const int memB = calcMem(b.memProps);
	return memA - memB;
}

u32 chooseBestPhysicalDevice(CSpan<PhysicalDeviceInfo> infos, PhysicalDeviceFilterAndCompare fns)
{
	auto filter = fns.filter;
	auto compare = fns.compare;
	if (compare.isNull())
		compare = make_plain_delegate(&PhysicalDeviceFilterAndCompare::defaultCompare_discreteThenMemory);

	const u32 numInfos = infos.size();
	u32 bestInd = numInfos;
	for (u32 i = 0; i < numInfos; i++) {
		if (filter && !filter(infos[i]))
			continue;

		if (bestInd == numInfos)
			bestInd = i;
		else if (compare(infos[i], infos[bestInd]) > 0)
			bestInd = i;
	}

	return bestInd;
}

VkResult createDevice(Device& device, VkInstance instance, const PhysicalDeviceInfo& physicalDeviceInfo,
	CSpan<QueuesCreateInfo> queuesInfos, CSpan<CStr> extensions)
{
	assert(device.device == VK_NULL_HANDLE && "device created twice?");
	device.instance = instance;
	device.physicalDevice = physicalDeviceInfo;

	VkDeviceQueueCreateInfo queuesInfosVk[64];
	const u32 numFamilies = queuesInfos.size();
	assert(numFamilies <= u32(std::size(queuesInfosVk)));
	for (u32 i = 0; i < numFamilies; i++) {
		queuesInfosVk[i] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.flags = VkDeviceQueueCreateFlags(queuesInfos[i].protectedCapable ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0),
			.queueFamilyIndex = queuesInfos[i].queueFamily,
			.queueCount = u32(queuesInfos[i].queuePriorities.size()),
			.pQueuePriorities = queuesInfos[i].queuePriorities.data(),
		};
	}

	const VkPhysicalDeviceFeatures features = {
		.samplerAnisotropy = VK_TRUE,
	};

	const VkDeviceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = numFamilies,
		.pQueueCreateInfos = queuesInfosVk,
		.enabledExtensionCount = u32(extensions.size()),
		.ppEnabledExtensionNames = extensions.data(),
		.pEnabledFeatures = &features,
	};
	VkResult vkRes = vkCreateDevice(device.physicalDevice.handle, &info, nullptr, &device.device);
	if (vkRes != VK_SUCCESS)
		return vkRes;

	device.queues.resize(numFamilies);
	for (u32 i = 0; i < numFamilies; i++) {
		const u32 numQueues = queuesInfos[i].queuePriorities.size();
		device.queues[i].resize(numQueues);
		for(u32 j = 0; j < numQueues; j++)
			vkGetDeviceQueue(device.device, i, j, &device.queues[i][j]);
	}

	const VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = physicalDeviceInfo.handle,
		.device = device.device,
		.instance = instance,
	};
	vkRes = vmaCreateAllocator(&allocatorInfo, &device.allocator);
	if (vkRes != VK_SUCCESS)
		return vkRes;

	return VK_SUCCESS;
}

VkResult createSwapchain(Swapchain& o, VkSurfaceKHR surface, Device& device, const SwapchainOptions& options)
{
	if (o.swapchain) {
		// destroy old image views
		for (u32 i = 0; i < o.numImages; i++)
			device.destroyImageView(o.imageViews[i]);
		// deregister old images
		for (u32 i = 0; i < o.numImages; i++)
			device.deregisterImage(o.images[i]);
	}

	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice.handle, surface, &surfaceCaps);
	o.surface = surface;
	o.w = surfaceCaps.currentExtent.width;
	o.h = surfaceCaps.currentExtent.height;

	o.format = options.format;
	if (o.format.format == Format::undefined)
		o.format = device.getBasicSupportedSurfaceFormat(surface);

	const VkSwapchainCreateInfoKHR swapchainInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = options.minImages,
		.imageFormat = toVk(o.format.format),
		.imageColorSpace = o.format.colorSpace, // the way the swapchain interprets image data
		.imageExtent = {o.w, o.h},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // only one queue will access an image at the same time. We can still reference the image from different queues, but not at the same time. We must use a memory barrier for this!
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // this can be used to have transparent windows in some window systems
		.presentMode = options.presentMode, // with 2 images, MAILBOX and FIFO are equivalent
		.clipped = VK_TRUE, // this allows to discard the rendering of hidden pixel regions. E.g a window is partially covered by another
		.oldSwapchain = o.swapchain,
	};
	VkResult vkRes = vkCreateSwapchainKHR(device.device, &swapchainInfo, nullptr, &o.swapchain);
	if (vkRes != VK_SUCCESS)
		return vkRes;

	// get images
	VkImage imagesVk[Swapchain::MAX_IMAGES];
	ASSERT_VKRES(vkGetSwapchainImagesKHR(device.device, o.swapchain, &o.numImages, nullptr));
	assert(o.numImages <= Swapchain::MAX_IMAGES);
	ASSERT_VKRES(vkGetSwapchainImagesKHR(device.device, o.swapchain, &o.numImages, imagesVk));

	// register images
	const ImageInfo imageInfo = {
		.size = {u16(o.w), u16(o.h)},
		.format = o.format.format,
		.usage = {.output_attachment = true},
		.layout = ImageLayout::undefined, // SPEC: All presentable images are initially in the VK_IMAGE_LAYOUT_UNDEFINED layout, thus before using presentable images, the application must transition them to a valid layout for the intended use.
	};
	for (u32 i = 0; i < o.numImages; i++)
		o.images[i] = device.registerImage(imagesVk[i], imageInfo, VK_NULL_HANDLE);

	// create image views
	assert(vkRes == VK_SUCCESS);
	ImageViewInfo imageViewInfo = {
		//.image = o.images[i],
		.type = ImageViewType::_2d,
	};
	for (u32 i = 0; i < o.numImages; i++) {
		imageViewInfo.image = o.images[i];
		o.imageViews[i] = device.createImageView(imageViewInfo);
	}

	return VK_SUCCESS;
}

VkResult createSwapchainSyncHelper(SwapchainSyncHelper& o, VkSurfaceKHR surface, Device& device, const SwapchainOptions& options)
{
	if (o.swapchain) {
		// destroy old stuff
		for (int i = 0; i < o.numImages; i++) {
			device.destroySemaphore(o.semaphore_imageAvailable[i]);
			device.destroySemaphore(o.semaphore_drawFinished[i]);
			device.destroyFence(o.fence_drawFinished[i]);
		}
		device.destroySemaphore(o.semaphore_imageAvailable[o.numImages]);
	}
	
	VkResult vkRes = createSwapchain(o, surface, device, options);

	// create new stuff
	for (int i = 0; i < o.numImages; i++) {
		o.semaphore_imageAvailable[i] = device.createSemaphore();
		o.semaphore_drawFinished[i] = device.createSemaphore();
		o.fence_drawFinished[i] = device.createFence(true);
	}
	o.semaphore_imageAvailable[o.numImages] = device.createSemaphore();

	return vkRes;
}

void SwapchainSyncHelper::acquireNextImage(Device& device)
{
	ASSERT_VKRES(vkAcquireNextImageKHR(device.device, swapchain, u64(-1), semaphore_imageAvailable[imageAvailableSemaphoreInd], VK_NULL_HANDLE, &imgInd));
}

void SwapchainSyncHelper::waitCanStartFrame(Device& device)
{
	VkFence& fence = fence_drawFinished[imgInd];
	ASSERT_VKRES(vkWaitForFences(device.device, 1, &fence, VK_FALSE, u64(-1)));
	ASSERT_VKRES(vkResetFences(device.device, 1, &fence));
}

void SwapchainSyncHelper::present(VkQueue queue)
{
	VkResult vkRes;
	const VkPresentInfoKHR info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &semaphore_drawFinished[imgInd],
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &imgInd,
		.pResults = &vkRes,
	};
	vkQueuePresentKHR(queue, &info);
	ASSERT_VKRES(vkRes);
	imageAvailableSemaphoreInd = (imageAvailableSemaphoreInd + 1) % (numImages + 1);
}

bool formatIsColor(Format format)
{
	return format != Format::undefined && !formatIsDepth(format) && !formatIsStencil(format);
}

bool formatIsDepth(Format format)
{
	static const Format k_depthFormats[] = {
		Format::D16_UNORM,
		Format::X8_D24_UNORM,
		Format::D32_SFLOAT,
		Format::D16_UNORM_S8_UINT,
		Format::D24_UNORM_S8_UINT,
		Format::D32_SFLOAT_S8_UINT,
	};
	return has(k_depthFormats, format);
}

bool formatIsStencil(Format format)
{
	static const Format k_depthFormats[] = {
		Format::S8_UINT,
		Format::D16_UNORM_S8_UINT,
		Format::D24_UNORM_S8_UINT,
		Format::D32_SFLOAT_S8_UINT,
	};
	return has(k_depthFormats, format);
}

}
}