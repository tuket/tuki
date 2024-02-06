#pragma once

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <assert.h>
#include <vector>
#include <unordered_map>
#include <span>
#include <bitset>
#include <type_traits>
#include <glm/vec4.hpp>
#include "vma.h"
#include "utils.hpp"
#include "delegate.hpp"

namespace tk {
namespace vk {

struct Device;

enum class Format : std::underlying_type_t<VkFormat> {
	undefined = 0,
	R4G4_UNORM = 1,
	R4G4B4A4_UNORM = 2,
	B4G4R4A4_UNORM = 3,
	R5G6B5_UNORM = 4,
	B5G6R5_UNORM = 5,
	R5G5B5A1_UNORM = 6,
	B5G5R5A1_UNORM = 7,
	A1R5G5B5_UNORM = 8,
	R8_UNORM = 9,
	R8_SNORM = 10,
	R8_USCALED = 11,
	R8_SSCALED = 12,
	R8_UINT = 13,
	R8_SINT = 14,
	R8_SRGB = 15,
	RG8_UNORM = 16,
	RG8_SNORM = 17,
	RG8_USCALED = 18,
	RG8_SSCALED = 19,
	RG8_UINT = 20,
	RG8_SINT = 21,
	RG8_SRGB = 22,
	RGB8_UNORM = 23,
	RGB8_SNORM = 24,
	RGB8_USCALED = 25,
	RGB8_SSCALED = 26,
	RGB8_UINT = 27,
	RGB8_SINT = 28,
	RGB8_SRGB = 29,
	BGR8_UNORM = 30,
	BGR8_SNORM = 31,
	BGR8_USCALED = 32,
	BGR8_SSCALED = 33,
	BGR8_UINT = 34,
	BGR8_SINT = 35,
	BGR8_SRGB = 36,
	RGBA8_UNORM = 37,
	RGBA8_SNORM = 38,
	RGBA8_USCALED = 39,
	RGBA8_SSCALED = 40,
	RGBA8_UINT = 41,
	RGBA8_SINT = 42,
	RGBA8_SRGB = 43,
	BGRA8_UNORM = 44,
	BGRA8_SNORM = 45,
	BGRA8_USCALED = 46,
	BGRA8_SSCALED = 47,
	BGRA8_UINT = 48,
	BGRA8_SINT = 49,
	BGRA8_SRGB = 50,
	ABGR8_UNORM = 51,
	ABGR8_SNORM = 52,
	ABGR8_USCALED = 53,
	ABGR8_SSCALED = 54,
	ABGR8_UINT = 55,
	ABGR8_SINT = 56,
	ABGR8_SRGB = 57,
	A2RGB10_UNORM = 58,
	A2RGB10_SNORM = 59,
	A2RGB10_USCALED = 60,
	A2RGB10_SSCALED = 61,
	A2RGB10_UINT = 62,
	A2RGB10_SINT = 63,
	A2BGR10_UNORM = 64,
	A2BGR10_SNORM = 65,
	A2BGR10_USCALED = 66,
	A2BGR10_SSCALED = 67,
	A2BGR10_UINT = 68,
	A2BGR10_SINT = 69,
	R16_UNORM = 70,
	R16_SNORM = 71,
	R16_USCALED = 72,
	R16_SSCALED = 73,
	R16_UINT = 74,
	R16_SINT = 75,
	R16_SFLOAT = 76,
	RG16_UNORM = 77,
	RG16_SNORM = 78,
	RG16_USCALED = 79,
	RG16_SSCALED = 80,
	RG16_UINT = 81,
	RG16_SINT = 82,
	RG16_SFLOAT = 83,
	RGB16_UNORM = 84,
	RGB16_SNORM = 85,
	RGB16_USCALED = 86,
	RGB16_SSCALED = 87,
	RGB16_UINT = 88,
	RGB16_SINT = 89,
	RGB16_SFLOAT = 90,
	RGBA16_UNORM = 91,
	RGBA16_SNORM = 92,
	RGBA16_USCALED = 93,
	RGBA16_SSCALED = 94,
	RGBA16_UINT = 95,
	RGBA16_SINT = 96,
	RGBA16_SFLOAT = 97,
	R32_UINT = 98,
	R32_SINT = 99,
	R32_SFLOAT = 100,
	RG32_UINT = 101,
	RG32_SINT = 102,
	RG32_SFLOAT = 103,
	RGB32_UINT = 104,
	RGB32_SINT = 105,
	RGB32_SFLOAT = 106,
	RGBA32_UINT = 107,
	RGBA32_SINT = 108,
	RGBA32_SFLOAT = 109,
	R64_UINT = 110,
	R64_SINT = 111,
	R64_SFLOAT = 112,
	RG64_UINT = 113,
	RG64_SINT = 114,
	RG64_SFLOAT = 115,
	RGB64_UINT = 116,
	RGB64_SINT = 117,
	RGB64_SFLOAT = 118,
	RGBA64_UINT = 119,
	RGBA64_SINT = 120,
	RGBA64_SFLOAT = 121,
	B10G11R11_UFLOAT = 122,
	E5B9G9R9_UFLOAT = 123,
	D16_UNORM = 124,
	X8_D24_UNORM = 125,
	D32_SFLOAT = 126,
	S8_UINT = 127,
	D16_UNORM_S8_UINT = 128,
	D24_UNORM_S8_UINT = 129,
	D32_SFLOAT_S8_UINT = 130,

	// COMPRESSED FORMATS
	COMPR_BC1_RGB_UNORM = 131,
	COMPR_BC1_RGB_SRGB = 132,
	COMPR_BC1_RGBA_UNORM = 133,
	COMPR_BC1_RGBA_SRGB = 134,
	COMPR_BC2_UNORM = 135,
	COMPR_BC2_SRGB = 136,
	COMPR_BC3_UNORM = 137,
	COMPR_BC3_SRGB = 138,
	COMPR_BC4_UNORM = 139,
	COMPR_BC4_SNORM = 140,
	COMPR_BC5_UNORM = 141,
	COMPR_BC5_SNORM = 142,
	COMPR_BC6H_UFLOAT = 143,
	COMPR_BC6H_SFLOAT = 144,
	COMPR_BC7_UNORM = 145,
	COMPR_BC7_SRGB = 146,
	COMPR_ETC2_R8G8B8_UNORM = 147,
	COMPR_ETC2_R8G8B8_SRGB = 148,
	COMPR_ETC2_R8G8B8A1_UNORM = 149,
	COMPR_ETC2_R8G8B8A1_SRGB = 150,
	COMPR_ETC2_R8G8B8A8_UNORM = 151,
	COMPR_ETC2_R8G8B8A8_SRGB = 152,
	COMPR_EAC_R11_UNORM = 153,
	COMPR_EAC_R11_SNORM = 154,
	COMPR_EAC_R11G11_UNORM = 155,
	COMPR_EAC_R11G11_SNORM = 156,
	COMPR_ASTC_4x4_UNORM = 157,
	COMPR_ASTC_4x4_SRGB = 158,
	COMPR_ASTC_5x4_UNORM = 159,
	COMPR_ASTC_5x4_SRGB = 160,
	COMPR_ASTC_5x5_UNORM = 161,
	COMPR_ASTC_5x5_SRGB = 162,
	COMPR_ASTC_6x5_UNORM = 163,
	COMPR_ASTC_6x5_SRGB = 164,
	COMPR_ASTC_6x6_UNORM = 165,
	COMPR_ASTC_6x6_SRGB = 166,
	COMPR_ASTC_8x5_UNORM = 167,
	COMPR_ASTC_8x5_SRGB = 168,
	COMPR_ASTC_8x6_UNORM = 169,
	COMPR_ASTC_8x6_SRGB = 170,
	COMPR_ASTC_8x8_UNORM = 171,
	COMPR_ASTC_8x8_SRGB = 172,
	COMPR_ASTC_10x5_UNORM = 173,
	COMPR_ASTC_10x5_SRGB = 174,
	COMPR_ASTC_10x6_UNORM = 175,
	COMPR_ASTC_10x6_SRGB = 176,
	COMPR_ASTC_10x8_UNORM = 177,
	COMPR_ASTC_10x8_SRGB = 178,
	COMPR_ASTC_10x10_UNORM = 179,
	COMPR_ASTC_10x10_SRGB = 180,
	COMPR_ASTC_12x10_UNORM = 181,
	COMPR_ASTC_12x10_SRGB = 182,
	COMPR_ASTC_12x12_UNORM = 183,
	COMPR_ASTC_12x12_SRGB = 184,

	// NEED EXTENSIONS
	G8B8G8R8_422_UNORM = 1000156000, // Provided by VK_VERSION_1_1
	B8G8R8G8_422_UNORM = 1000156001, // Provided by VK_VERSION_1_1
	G8_B8_R8_3PLANE_420_UNORM = 1000156002, // Provided by VK_VERSION_1_1
	G8_B8R8_2PLANE_420_UNORM = 1000156003, // Provided by VK_VERSION_1_1
	G8_B8_R8_3PLANE_422_UNORM = 1000156004, // Provided by VK_VERSION_1_1
	G8_B8R8_2PLANE_422_UNORM = 1000156005, // Provided by VK_VERSION_1_1
	G8_B8_R8_3PLANE_444_UNORM = 1000156006, // Provided by VK_VERSION_1_1
	R10X6_UNORM_PACK16 = 1000156007, // Provided by VK_VERSION_1_1
	R10X6G10X6_UNORM_2PACK16 = 1000156008, // Provided by VK_VERSION_1_1
	R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009, // Provided by VK_VERSION_1_1
	G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010, // Provided by VK_VERSION_1_1
	B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011, // Provided by VK_VERSION_1_1
	G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012, // Provided by VK_VERSION_1_1
	G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013, // Provided by VK_VERSION_1_1
	G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014, // Provided by VK_VERSION_1_1
	G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015, // Provided by VK_VERSION_1_1
	G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016, // Provided by VK_VERSION_1_1
	R12X4_UNORM_PACK16 = 1000156017, // Provided by VK_VERSION_1_1
	R12X4G12X4_UNORM_2PACK16 = 1000156018, // Provided by VK_VERSION_1_1
	R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019, // Provided by VK_VERSION_1_1
	G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020, // Provided by VK_VERSION_1_1
	B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021, // Provided by VK_VERSION_1_1
	G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022, // Provided by VK_VERSION_1_1
	G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023, // Provided by VK_VERSION_1_1
	G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024, // Provided by VK_VERSION_1_1
	G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025, // Provided by VK_VERSION_1_1
	G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026, // Provided by VK_VERSION_1_1
	G16B16G16R16_422_UNORM = 1000156027, // Provided by VK_VERSION_1_1
	B16G16R16G16_422_UNORM = 1000156028, // Provided by VK_VERSION_1_1
	G16_B16_R16_3PLANE_420_UNORM = 1000156029, // Provided by VK_VERSION_1_1
	G16_B16R16_2PLANE_420_UNORM = 1000156030, // Provided by VK_VERSION_1_1
	G16_B16_R16_3PLANE_422_UNORM = 1000156031, // Provided by VK_VERSION_1_1
	G16_B16R16_2PLANE_422_UNORM = 1000156032, // Provided by VK_VERSION_1_1
	G16_B16_R16_3PLANE_444_UNORM = 1000156033, // Provided by VK_VERSION_1_1
	G8_B8R8_2PLANE_444_UNORM = 1000330000, // Provided by VK_VERSION_1_3
	G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16 = 1000330001, // Provided by VK_VERSION_1_3
	G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16 = 1000330002, // Provided by VK_VERSION_1_3
	G16_B16R16_2PLANE_444_UNORM = 1000330003, // Provided by VK_VERSION_1_3
	A4R4G4B4_UNORM_PACK16 = 1000340000, // Provided by VK_VERSION_1_3
	A4B4G4R4_UNORM_PACK16 = 1000340001, // Provided by VK_VERSION_1_3
	COMPR_ASTC_4x4_SFLOAT = 1000066000, // Provided by VK_VERSION_1_3
	COMPR_ASTC_5x4_SFLOAT = 1000066001, // Provided by VK_VERSION_1_3
	COMPR_ASTC_5x5_SFLOAT = 1000066002, // Provided by VK_VERSION_1_3
	COMPR_ASTC_6x5_SFLOAT = 1000066003, // Provided by VK_VERSION_1_3
	COMPR_ASTC_6x6_SFLOAT = 1000066004, // Provided by VK_VERSION_1_3
	COMPR_ASTC_8x5_SFLOAT = 1000066005, // Provided by VK_VERSION_1_3
	COMPR_ASTC_8x6_SFLOAT = 1000066006, // Provided by VK_VERSION_1_3
	COMPR_ASTC_8x8_SFLOAT = 1000066007, // Provided by VK_VERSION_1_3
	COMPR_ASTC_10x5_SFLOAT = 1000066008, // Provided by VK_VERSION_1_3
	COMPR_ASTC_10x6_SFLOAT = 1000066009, // Provided by VK_VERSION_1_3
	COMPR_ASTC_10x8_SFLOAT = 1000066010, // Provided by VK_VERSION_1_3
	COMPR_ASTC_10x10_SFLOAT = 1000066011, // Provided by VK_VERSION_1_3
	COMPR_ASTC_12x10_SFLOAT = 1000066012, // Provided by VK_VERSION_1_3
	COMPR_ASTC_12x12_SFLOAT = 1000066013, // Provided by VK_VERSION_1_3
};

struct Buffer { u32 id = 0; };
struct Image { u32 id = 0; };
struct ImageView { u32 id = 0; };

enum class BufferUsage : u16 {
	transferSrc = 1,
	transferDst = 1 << 1,
	uniformTexelBuffer = 1 << 2,
	storageTexelBuffer = 1 << 3,
	uniformBuffer = 1 << 4,
	storageBuffer = 1 << 5,
	indexBuffer = 1 << 6,
	vertexBuffer = 1 << 7,
	indirectBuffer = 1 << 8,

	all = (1 << 9) - 1,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(BufferUsage)

enum class PipelineStages : VkPipelineStageFlags {
	none = 0,
	topOfPipe = 1,
	drawIndirect = 1 << 1,
	vertexInput = 1 << 2,
	vertexShader = 1 << 3,
	tessellationControlShader = 1 << 4,
	tessellationEvaluationShader = 1 << 5,
	geometryShader = 1 << 6,
	fragmentShader = 1 << 7,
	earlyFragmentTests = 1 << 8,
	lateFragmentTests = 1 << 9,
	colorAttachmentOutput = 1 << 10,
	computeShader = 1 << 11,
	transfer = 1 << 12,
	bottomOfPipe = 1 << 13,
	host = 1 << 14,
	allGraphics = 1 << 15,
	allCmds = 1 << 16,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(PipelineStages)

enum class ShaderStages : VkShaderStageFlags {
	vertex = 1,
	tessellationControl = 0x00000002,
	tessellationEvaluation = 0x00000004,
	geometry = 0x00000008,
	fragment = 0x00000010,
	compute = 0x00000020,
	allGraphics = 0x0000001F,
	all = 0x7FFFFFFF,
	raygen = 0x00000100,
	anyHit = 0x00000200,
	closestHit = 0x00000400,
	miss = 0x00000800,
	intersection = 0x00001000,
	callable = 0x00002000,
	task = 0x00000040,
	mesh = 0x00000080,
	subpassShading_huawei = 0x00004000,
	clusterCulling_huawei = 0x00080000,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(ShaderStages)

enum class DependencyFlags { // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDependencyFlagBits.html
	none = 0,
	byRegion = 1,
	deviceGroup = 1 << 1,
	viewLocal = 1 << 2,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(DependencyFlags)

enum class AccessFlags : VkAccessFlags {
	none = 0,
	indirectCommandRead = 1,
	indexRead = 1 << 1,
	vertexAttributeRead = 1 << 2,
	uniformRead = 1 << 3,
	inputAttachmentRead = 1 << 4,
	shaderRead = 1 << 5,
	shaderWrite = 1 << 6,
	colorAtachmentRead = 1 << 7,
	colorAtachmentWrite = 1 << 8,
	depthStencilAtachmentRead = 1 << 9,
	depthStencilAtachmentWrite = 1 << 10,
	transferRead = 1 << 11,
	transferWrite = 1 << 12,
	hostRead = 1 << 13,
	hostWrite = 1 << 14,
	memoryRead = 1 << 15,
	memoryWrite = 1 << 16,
	// extensions
	accelerationStructRead = 1 << 17,
	accelerationStructWrite = 1 << 18,
	fragmentShadingRateAttachmentRead = 1 << 19,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(AccessFlags)

union Rect2d { struct { i32 x, y; u32 w, h; }; VkRect2D vkRect2d = { 0,0,0,0 }; };
struct Viewport { float x = 0, y = 0, w = 1, h = 1, minDepth = 0, maxDepth = 1; };

struct ImageSize {
	u16 width = 1, height = 1;
	union {
		u16 depth = 1;
		u16 numLayers;
	};
	u16 numMips = 1;

	auto vec() { return std::span<u16, 4>{&width, 4}; }
	auto vec()const { return std::span<const u16, 4>{&width, 4}; }
};
enum class ImageLayout : std::underlying_type_t<VkImageLayout> {
	undefined,
	general,
	colorAttachment,
	depthStencilAttachment,
	depthStencilReadOnly,
	shaderReadOnly,
	transferSrc,
	transferDst,
	preinitialized,
	// extensions
	depthReadOnly_stencilAttachment = 1000117000,
	depthAttachment_stencilReadOnly = 1000117001,
	depthAttachment = 1000241000,
	depthReadOnly = 1000241001,
	stencilAttachment = 1000241002,
	stencilReadOnly = 1000241003,
	readOnly = 1000314000,
	attachment = 1000314001,
	presentSrc = 1000001002,
	videoDecodeDst = 1000024000,
	videoDecodeSrc = 1000024001,
	videoDecodeDpb = 1000024002,
	sharedPresent = 1000111000,
	fragmentDensityMap = 1000218000,
	fragmentShadingRateAttachment = 1000164003,
};
struct ImageUsage { // (VkImageUsageFlagBits)
	bool transfer_src : 1 = false;
	bool transfer_dst : 1 = false;
	bool sampled : 1 = false;
	bool storage : 1 = false;
	bool input_attachment : 1 = false; // unlike in Vulkan, we don't have a separate flag for color and depth-stencil, we will distinguish the actual vulkan flag from the format
	bool output_attachment : 1 = false;
	bool transient : 1 = false;

	static constexpr ImageUsage default_texture() { return { .transfer_src = true, .transfer_dst = true, .sampled = true, }; }
	static constexpr ImageUsage default_framebuffer(bool input_attachment, bool sampled) { return { .sampled = sampled, .input_attachment = input_attachment, .output_attachment = true }; }
};
struct ImageInfo {
	ImageSize size;
	Format format = Format::RGBA8_SRGB;
	u8 dimensions = 2; // 1d, 2d, or 3d
	u8 numSamples = 1;
	ImageUsage usage = ImageUsage::default_texture();
	ImageLayout layout = ImageLayout::undefined;

	u16 getNumLayers()const { return dimensions == 3 ? 1 : size.numLayers; }
};

enum class ImageAspects {
	none = 0,
	color = 1,
	depth = 1 << 1,
	stencil = 1 << 2,
	metadata = 1 << 3,
	plane_0 = 1 << 4,
	plane_1 = 1 << 5,
	plane_2 = 1 << 6,
	// extensions
	memory_plane_0 = 1 << 7,
	memory_plane_1 = 1 << 8,
	memory_plane_2 = 1 << 9,
	memory_plane_3 = 1 << 10,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(ImageAspects);

enum class Filter { nearest, linear };
enum class SamplerMipmapMode { nearest, linear };
enum class SamplerAddressMode { repeat, mirroredRepeat, clampToEdge, clampToBorder, mirrorClampToEdge };

struct ImageSubresourceRange {
	ImageAspects aspects = ImageAspects::color;
	u32 baseMip = 0;
	u32 numMips = 1;
	u32 baseLayer = 0;
	u32 numLayers = 1;
};

struct BufferData {
	VkBuffer handle;
	VmaAllocation alloc;
	VmaAllocationInfo allocInfo;
};

struct PhysicalDeviceInfo {
	VkPhysicalDevice handle;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceProperties props;
	VkPhysicalDeviceMemoryProperties memProps;
	std::vector<VkQueueFamilyProperties> queueFamiliesProps;
	std::vector<bool> queueFamiliesPresentSupported;

	u32 findGraphicsAndPresentQueueFamily()const;
};

enum class PipelineBindPoint {
	graphics,
	compute,
};

struct BufferHostAccess {  // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html#ggad9889c10c798b040d59c92f257cae597a9be224df3bfc1cfa06203aed689a30c5
	bool sequentialWrite : 1 = false;
	bool random : 1 = false;
	bool allowTransferInstead : 1 = false; // if there is a non-host-visible memory type, and it may improve performance, use that memory instead and enable transfer
};

struct CmdPoolOptions {
	bool transientCmdBuffers : 1 = false;
	bool reseteableCmdBuffers : 1 = false;
};

struct CmdBufferUsageFlags {
	bool oneTimeSubmit : 1 = true; // the cmd buffer will be reset and rerecorded each time before submission
	bool renderPassContinue : 1 = false; // (ignored if primary cmd buffer)
	bool simultaneousUse : 1 = false; // can be submitted to multiple queues of a queue family simultaneously
};

struct MemoryBarrier {
	AccessFlags srcAccess = AccessFlags::none;
	AccessFlags dstAccess = AccessFlags::none;
};
struct BufferBarrier { 
	AccessFlags srcAccess = AccessFlags::none;
	AccessFlags dstAccess = AccessFlags::none;
	u32 srcQueueFamily = 0;
	u32 dstQueueFamily = 0;
	VkBuffer buffer;
	size_t offset = 0;
	size_t size = VK_WHOLE_SIZE;
};
struct ImageBarrier {
	AccessFlags srcAccess = AccessFlags::none;
	AccessFlags dstAccess = AccessFlags::none;
	ImageLayout srcLayout;
	ImageLayout dstLayout;
	u32 srcQueueFamily = 0;
	u32 dstQueueFamily = 0;
	VkImage image = VK_NULL_HANDLE;
	ImageSubresourceRange subresourceRange = {};
};
struct PipelineBarrier {
	PipelineStages srcStages = PipelineStages::none;
	PipelineStages dstStages = PipelineStages::none;
	DependencyFlags dependencyFlags;
	CSpan<MemoryBarrier> memoryBarriers;
	CSpan<BufferBarrier> bufferBarriers;
	CSpan<ImageBarrier> imageBarriers;
};
struct RenderPassBegin {
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	Rect2d renderArea;
	CSpan<VkClearValue> clearValues;
	bool subPassContentsInSecondaryCmdBuffers = false;
};

struct CmdBuffer {
	VkCommandBuffer handle;
#ifndef NDEBUG
	enum class State { initial, recording, recording_oneTimeSubmit, executable, executable_oneTimeSubmit, executableOrPending, pendingOrInvalid };
	State state = State::initial;
#endif

	void begin(CmdBufferUsageFlags usageFlags, const VkCommandBufferInheritanceInfo* inheritanceInfo = nullptr);
	void end();
	void reset(bool releaseResources = false);

	void cmd_copy(VkBuffer src, VkBuffer dst, CSpan<VkBufferCopy> regions);
	void cmd_copy(VkBuffer src, VkBuffer dst, size_t srcOffset, size_t dstOffset, size_t size);

	void cmd_copy(VkBuffer src, VkImage dst, CSpan<VkBufferImageCopy> regions);
	void cmd_copy(Buffer src, Image dst, const Device& device, size_t bufferOffset = 0, u32 mipLevel = 0);

	void cmd_blitToNextMip(const Device& device, Image img, u32 srcMip);

	void cmd_beginRenderPass(const RenderPassBegin& beg);
	void cmd_endRenderPass();

	void cmd_pipelineBarrier(const PipelineBarrier& barrier);

	void cmd_pipelineBarrier_imagesToTransferDstLayout(Device& device, CSpan<Image> images);
	void cmd_pipelineBarrier_imagesToShaderReadLayout(Device& device, CSpan<Image> images);
	//void cmd_pipelineBarrier_images_colorAttachment_to_shaderRead(Device& device, CSpan<Image> images);
	//void cmd_pipelineBarrier_imagesToColorAttachment(Device& device, CSpan<Image> images);
	//void cmd_pipelineBarrier_imagesToDepthAttachmentLayout(Device& device, CSpan<Image> images);


	void cmd_bindPipeline(VkPipeline pipeline, PipelineBindPoint point);
	void cmd_bindGraphicsPipeline(VkPipeline pipeline) { cmd_bindPipeline(pipeline, PipelineBindPoint::graphics); }

	void cmd_scissor(Rect2d r);
	void cmd_viewport(Viewport vp);

	void cmd_bindDescriptorSets(PipelineBindPoint bindPoint, VkPipelineLayout layout, u32 firstBinding, CSpan<VkDescriptorSet> descSets, CSpan<u32> dynamicOffsets);
	void cmd_bindDescriptorSet(PipelineBindPoint bindPoint, VkPipelineLayout layout, u32 binding, VkDescriptorSet descSet);

	void cmd_bindVertexBuffers(u32 firstBinding, CSpan<VkBuffer> vbs, CSpan<size_t> offsets);
	void cmd_bindVertexBuffers(u32 firstBinding, CSpan<VkBuffer> vbs);
	void cmd_bindVertexBuffer(u32 bindPoint, VkBuffer vb, size_t offset = 0);
	void cmd_bindIndexBuffer(VkBuffer ib, VkIndexType indType = VK_INDEX_TYPE_UINT32, size_t offset = 0);

	void cmd_draw(u32 numVerts, u32 numInstances = 1, u32 firstVertex = 0, u32 firstInstance = 0);
	void cmd_drawIndexed(u32 numInds, u32 numInstances = 1, u32 firstInd = 0, i32 vertOffset = 0, u32 firstInstance = 0);
};

struct DescPoolOptions {
	bool allowFreeIndividualSets : 1 = false;
	bool allowUpdateAfterBind : 1 = false;
};

struct Shader { VkShaderModule handle; };
struct VertShader : Shader {};
struct TessControlShader : Shader {};
struct TessEvalShader : Shader {};
struct GeomShader : Shader {};
struct FragShader : Shader {};

struct SpecializationInfoMaker {
	std::vector<u8> data;
	std::vector<VkSpecializationMapEntry> entries;

	SpecializationInfoMaker() {
		data.reserve(4 << 10);
		entries.reserve(32);
	}

	template<typename T>
	void add(u32 id, const T& datum) {
#ifndef NDEBUG
		for (const auto& e : entries)
			assert(e.constantID != id);
#endif
		const u32 prevSize = data.size();
		data.resize(prevSize + sizeof(datum));
		memcpy(&data[prevSize], &datum, sizeof(datum));

		entries.push_back(VkSpecializationMapEntry{
			.constantID = id,
			.offset = prevSize,
			.size = sizeof(datum),
		});
	}

	auto getInfo() const {
		return VkSpecializationInfo{
			.mapEntryCount = u32(entries.size()),
			.pMapEntries = entries.data(),
			.dataSize = u32(data.size()),
			.pData = data.data(),
		};
	}

	void clear() {
		data.clear();
		entries.clear();
	}
};

template <typename ShaderT>
struct ShaderStageInfo {
	ShaderT shader = { 0 };
	const char* entryFnName = "main";
	VkSpecializationInfo specialization = {};
};
struct ShaderStagesInfo {
	ShaderStageInfo<VertShader> vertex;
	ShaderStageInfo<TessControlShader> tessControl;
	ShaderStageInfo<TessEvalShader> tessEval;
	ShaderStageInfo<GeomShader> geom;
	ShaderStageInfo<FragShader> fragment;
};
struct FbAttachmentInfo {
	Format format;
	u32 numSamples = 1;
	bool mayAlias = false;
};
enum class Attachment_LoadOp {
	load,
	clear,
	dontCare,
};
enum class Attachment_StoreOp {
	store,
	dontCare,
};
enum class ImageViewType {
	_1d,
	_2d,
	_3d,
	cube,
	_1d_array,
	_2d_array,
	cube_array,
	count,
};
enum class FbSpaceStage {
	// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#synchronization-framebuffer-regions
	fragmentShader,
	earlyFramentTests,
	lateFragmentTests,
	colorOutput,
};
enum class BlendFactor : u8 {
	zero,
	one,
	src_color,
	one_minus_src_color,
	dst_color,
	one_minus_dst_color,
	src_alpha,
	one_minus_src_alpha,
	dst_alpha,
	one_minus_dst_alpha,
	constant_color,
	one_minus_constant_color,
	constant_alpha,
	one_minus_constant_alpha,
	src_alpha_saturate,
	src1_color,
	one_minus_src1_color,
	src1_alpha,
	one_minus_src1_alpha,
};
enum class BlendOp :u8 {
	add,
	substract,
	reverse_substract,
	min,
	max
};
struct AttachmentOps {
	Attachment_LoadOp loadOp = Attachment_LoadOp::dontCare; // applies to color and depth attachment components
	Attachment_StoreOp storeOp = Attachment_StoreOp::store; // applies to color and depth attachment components
	Attachment_LoadOp stencilLoadOp = Attachment_LoadOp::dontCare;
	Attachment_StoreOp stencilStoreOp = Attachment_StoreOp::store;
	ImageLayout expectedLayout = ImageLayout::undefined; //(INITIAL_LAYOUT in vulkan, but I think expected layout is a better name)
	ImageLayout finalLayout = ImageLayout::undefined; // layout to transition to when the renderPass instance ends
};
struct SubPassDependency {
	u32 srcSubpass;
	u32 dstSubpass;
	bool supportRegionTiling = true;
};
struct SubPassInfo {
	PipelineBindPoint pipelineBindPoint;
	CSpan<u32> inputAttachments;
	CSpan<u32> colorAttachments;
	u32 depthStencilAttachment = u32(-1);
	std::bitset<64> preserveAttachments = { u64(-1) }; // by default preserve all attachments
};
struct RenderPassInfo {
	CSpan<FbAttachmentInfo> attachments;
	CSpan<AttachmentOps> attachmentOps;
	CSpan<SubPassInfo> subpasses;
	CSpan<SubPassDependency> dependencies;
};
struct RenderPassInfo_simple {
	CSpan<FbAttachmentInfo> attachments;
	CSpan<AttachmentOps> attachmentOps;
	CSpan<u32> colorAttachments;
	u32 depthStencilAttachment = u32(-1);
};
struct FramebufferInfo {
	VkRenderPass renderPass;
	CSpan<ImageView> attachments;
	u32 width = 0, height = 0; // if with or height is 0, we will try to infer it from the given attachments
};
struct ImageViewInfo {
	Image image;
	ImageViewType type = ImageViewType::count; // count means that we will automatically assign the type depending of the image
	Format format = Format::undefined; // undefined means use the same format as the image
	u16 baseMipLevel = 0;
	u16 numMipLevels = u16(-1);
	u16 baseLayer = 0;
	u16 numLayers = u16(-1);
	VkComponentMapping components = {};
	ImageAspects aspects = ImageAspects::none; // none means automatically determine the aspect from the image
};
struct VertexInputBindingInfo {
	u32 binding;
	u32 stride;
	bool perInstance = false;
};
struct VertexInputAttribInfo {
	u32 location;
	u32 binding;
	Format format;
	u32 offset = 0;
};
struct PipelineDynamicStates {
	bool viewport : 1;
	bool scissor : 1;
	bool lineWidth : 1;
	bool depthBias : 1;
	bool blendConstant : 1;
	//bool depthBounds : 1;
	bool stencilComapreMask : 1;
	bool stencilWriteMask : 1;
	bool stencilReference : 1;
};
enum class Topology : u8 { points, lines, line_strip, triangles, triangle_strip, triangle_fan, /* lines_withadj ... */ };
enum class PolygonMode : u8 { fill, line, point, };
enum class CompareOp : u8 { never, less, equal, less_or_equal, greater, not_equal, greater_or_equal, always };
enum class StencilOp : u8 { keep, zero, replace, inc_and_clamp, dec_and_clamp, invert, inc_and_wrap, dec_and_wrap };

struct StencilOpState {
	StencilOp failOp;
	StencilOp passOp;
	StencilOp depthFailOp;
	CompareOp comapreOp;
	u32 compareMask;
	u32 writeMask;
	u32 reference;
};
struct ColorBlendAttachment {
	bool enableBlending : 1 = false;
	BlendFactor srcColorFactor : 5 = BlendFactor::src_alpha;
	BlendFactor dstColorFactor : 5 = BlendFactor::one_minus_src_alpha;
	BlendOp colorBlendOp : 3 = BlendOp::add;
	BlendFactor srcAlphaFactor : 5 = BlendFactor::one_minus_dst_alpha;
	BlendFactor dstAlphaFactor : 5 = BlendFactor::one;
	BlendOp alphaBlendOp : 3 = BlendOp::add;
};

struct GraphicsPipelineInfo {
	ShaderStagesInfo shaderStages;
	CSpan<VertexInputBindingInfo> vertexInputBindings;
	CSpan<VertexInputAttribInfo> vertexInputAttribs;
	Viewport viewport = {};
	Rect2d scissor = {};
	u8 numSamples = 1;
	Topology topology : 3 = Topology::triangles;
	PolygonMode polygonMode : 2 = PolygonMode::fill;
	bool cull_front : 1 = false;
	bool cull_back : 1 = true;
	bool clockwiseFaces : 1 = false;
	bool depthTestEnable : 1 = false;
	bool depthWriteEnable : 1 = false;
	CompareOp depthCompareOp : 3 = CompareOp::less;
	bool stencilTestEnable : 1 = false;
	StencilOpState frontStencilOpState = {};
	StencilOpState backStencilOpState = {};
	CSpan<ColorBlendAttachment> colorBlendAttachments;
	glm::vec4 blendConstant;
	PipelineDynamicStates dynamicStates = { .viewport = true, .scissor = true, };
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	u32 subpassInd = 0;
};

struct SubmitInfo {
	CSpan<std::tuple<VkSemaphore, PipelineStages>> waitSemaphores;
	CSpan<CmdBuffer> cmdBuffers;
	CSpan<VkSemaphore> signalSemaphores;
};

struct Version {
	unsigned major : 8 = 0;
	unsigned minor : 11 = 0;
	unsigned patch : 13 = 0;
};

struct AppInfo {
	Version apiVersion = { 1, 0, 0 };
	CStr appName = nullptr;
	Version appVersion = {};
	CStr engineName = nullptr;
	Version engineVersion = {};
};

struct QueuesCreateInfo {
	u32 queueFamily;
	CSpan<float> queuePriorities;
	bool protectedCapable = false;
};

struct SurfaceFormat {
	Format format;
	VkColorSpaceKHR colorSpace;
};

struct DescriptorImageInfo {
	VkSampler sampler = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	ImageLayout imageLayout = ImageLayout::shaderReadOnly;
};

struct DescriptorBufferInfo {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	VkDeviceSize range = VK_WHOLE_SIZE;
};

enum class DescriptorType : std::underlying_type_t<VkDescriptorType> {
	sampler = 0,
	combinedImageSampler = 1,
	sampledImage = 2,
	storageImage = 3,
	uniformTextelBuffer = 4,
	storageTexelBuffer = 5,
	uniformBuffer = 6,
	storageBuffer = 7,
	uniformBufferDynamic = 8,
	storageBufferDynamic = 9,
	inputAttachment = 10,
	inlineUniformBlock = 1000138000,
	accelerationStructure = 1000150000,
	accelerationStructure_NV = 1000165000,
	sampleWeightImage_qcom = 1000440000,
	blockMatchImage_qcom = 1000440001,
	mutable_valve = 1000351000,
	maxEnum = VK_DESCRIPTOR_TYPE_MAX_ENUM,
};

struct DescriptorSetArrayWrite {
	VkDescriptorSet descSet = VK_NULL_HANDLE;
	u32 binding = -1;
	u32 startElem = 0;
	DescriptorType type = DescriptorType::maxEnum;
	union {
		CSpan<DescriptorImageInfo> imageInfos = {};
		CSpan<DescriptorBufferInfo> bufferInfos;
		CSpan<VkBufferView> texelBufferViews;
	};
};

struct DescriptorSetWrite {
	VkDescriptorSet descSet = VK_NULL_HANDLE;
	u32 binding = u32(-1);
	DescriptorType type = DescriptorType::maxEnum;
	union {
		DescriptorImageInfo imageInfo = {};
		DescriptorBufferInfo bufferInfo;
		VkBufferView texelBufferView;
	};
};

struct DescriptorSetArrayCopy {
	VkDescriptorSet srcSet = VK_NULL_HANDLE;
	u32 srcBinding = -1;
	u32 srcStartElem = -1;
	VkDescriptorSet dstSet  = VK_NULL_HANDLE;
	u32 dstBinding = -1;
	u32 dstStartElem = -1;
	u32 numArrayElems = 1;
};

struct SamplerInfo {
	Filter minFilter = Filter::linear;
	Filter magFilter = Filter::linear;
	SamplerMipmapMode mipmapMode = SamplerMipmapMode::linear;
	SamplerAddressMode addressModeX = SamplerAddressMode::repeat;
	SamplerAddressMode addressModeY = SamplerAddressMode::repeat;
	SamplerAddressMode addressModeZ = SamplerAddressMode::repeat;
	float mipLodBias = 0.f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 1.f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::less;
	float minLod = 0;
	float maxLod = VK_LOD_CLAMP_NONE;
	VkBorderColor borderColor;
	bool unnormalizedCoordinates = false;
};

struct DescriptorSetLayoutBindingInfo {
	u32 binding;
	DescriptorType descriptorType;
	u32 descriptorCount = 1;
	ShaderStages accessStages;
	const VkSampler* pImmutableSamplers = nullptr;
};

enum class DescriptorSetLayoutCreateFlags : std::underlying_type_t<VkDescriptorSetLayoutCreateFlagBits> {
	none = 0,
	updateAfterBindPool = 0x00000002,
	pushDescriptor = 0x00000001,
	descriptorBuffer = 0x00000010,
	embeddedImmutableSamplers = 0x00000020,
	indirectBindable_NV = 0x00000080,
	hostOnlyPool = 0x00000004,
};
DEFINE_ENUM_CLASS_LOGIC_OPS(DescriptorSetLayoutCreateFlags)

struct Device
{
	VkInstance instance;
	VkDevice device = VK_NULL_HANDLE;
	PhysicalDeviceInfo physicalDevice;
	VmaAllocator allocator;
	std::vector<std::vector<VkQueue>> queues; // [queueFamily][queue]
	std::vector<BufferData> buffers;
	u32 buffers_nextFreeSlot = u32(-1);
	//std::unordered_map<std::tuple<BufferUsage, BufferHostAccess>, u8> bufferMemTypeInds;
	struct Images {
		std::vector<VkImage> handles;
		std::vector<ImageInfo> infos;
		std::vector<VmaAllocation> allocs;
		u32 nextFreeSlot = u32(-1);
	} images;
	struct ImageViews {
		std::vector<VkImageView> handles;
		std::vector<ImageViewInfo> infos;
		u32 nextFreeSlot = u32(-1);
	} imageViews;

	void waitIdle();

	VkSemaphore createSemaphore();
	void destroySemaphore(VkSemaphore semaphore);

	VkFence createFence(bool signaled);
	void destroyFence(VkFence fence);

	u32 getMemTypeInd(BufferUsage usage, BufferHostAccess hostAccess, size_t size = 1);

	Buffer createBuffer(BufferUsage usage, size_t size, BufferHostAccess hostAccess);
	void destroyBuffer(Buffer buffer);
	VkBuffer getVkHandle(Buffer buffer)const { assert(buffer.id); return buffers[buffer.id - 1].handle; }

	u8* getBufferMemPtr(Buffer buffer);
	size_t getBufferSize(Buffer buffer)const;
	void flushBuffer(Buffer buffer);

	Image registerImage(VkImage imgVk, const ImageInfo& info, VmaAllocation alloc);
	void deregisterImage(Image img);
	Image createImage(const ImageInfo& info);
	void destroyImage(Image img);
	VkImage getVkHandle(Image image)const { assert(image.id); return images.handles[image.id - 1]; }
	const ImageInfo& getInfo(Image image)const { assert(image.id); return images.infos[image.id - 1]; }

	ImageView createImageView(const ImageViewInfo& info) { ImageViewInfo info2 = info; return createImageView(info2); }
	ImageView createImageView(ImageViewInfo& info);
	void destroyImageView(ImageView imgView);
	VkImageView getVkHandle(ImageView view)const { assert(view.id); return imageViews.handles[view.id - 1]; }
	const ImageViewInfo& getInfo(ImageView view)const { assert(view.id); return imageViews.infos[view.id - 1]; }

	VkSampler createSampler(const SamplerInfo& info);
	VkSampler createSampler(Filter minFilter, Filter magFilter, SamplerMipmapMode mipmapMode, SamplerAddressMode addressMode, float maxAnisotropy = 1.f);
	void destroySampler(VkSampler sampler);

	VkCommandPool createCmdPool(u32 queueFamily, CmdPoolOptions options);
	void allocCmdBuffers(VkCommandPool pool, std::span<CmdBuffer> cmdBuffers, bool secondary = false);

	VkDescriptorSetLayout createDescriptorSetLayout(CSpan<DescriptorSetLayoutBindingInfo> bindings, DescriptorSetLayoutCreateFlags flags = {});
	void destroyDescriptorSetLayout(VkDescriptorSetLayout descSetLayout);

	VkDescriptorPool createDescriptorPool(u32 maxSets, CSpan<VkDescriptorPoolSize> maxPerType, DescPoolOptions options);
	void destroyDescriptorPool(VkDescriptorPool pool);

	VkResult allocDescriptorSets(VkDescriptorPool pool, CSpan<VkDescriptorSetLayout> layouts, std::span<VkDescriptorSet> descSets);
	VkResult allocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout layout, std::span<VkDescriptorSet> descSets);
	void freeDescriptorSets(VkDescriptorPool pool, CSpan<VkDescriptorSet> descSets);
	void freeDescriptorSet(VkDescriptorPool pool, VkDescriptorSet descSet) { freeDescriptorSets(pool, { &descSet, 1 }); }

	void updateDescriptorSets(CSpan<DescriptorSetArrayWrite> writes, CSpan<DescriptorSetArrayCopy> copies);
	void writeDescriptorSetArrays(CSpan<DescriptorSetArrayWrite> writes) { updateDescriptorSets(writes, {}); }
	void copyDescriptorSetArrays(CSpan<DescriptorSetArrayCopy> copies) { updateDescriptorSets({}, copies); }
	void writeDescriptorSet(const DescriptorSetWrite& write);
	void writeDescriptorSets(CSpan<DescriptorSetWrite> writes);

	VkRenderPass createRenderPass(const RenderPassInfo& info);
	VkRenderPass createRenderPass_simple(const RenderPassInfo_simple& info);
	void destroyRenderPass(VkRenderPass rp);

	VkFramebuffer createFramebuffer(const FramebufferInfo& info);
	void destroyFramebuffer(VkFramebuffer fb);

	Shader createShader(CSpan<u32> spirvCode);
	VertShader createVertShader(CSpan<u32> spirvCode) { return VertShader(createShader(spirvCode)); }
	FragShader createFragShader(CSpan<u32> spirvCode) { return FragShader(createShader(spirvCode)); }
	GeomShader createGeomShader(CSpan<u32> spirvCode) { return GeomShader(createShader(spirvCode)); }
	TessControlShader createTessControlShader(CSpan<u32> spirvCode) { return TessControlShader(createShader(spirvCode)); }
	TessEvalShader createTessEvalShader(CSpan<u32> spirvCode) { return TessEvalShader(createShader(spirvCode)); }

	Shader loadShader(CStr spirvPath);
	VertShader loadVertShader(CStr spirvPath) { return VertShader(loadShader(spirvPath)); }
	FragShader loadFragShader(CStr spirvPath) { return FragShader(loadShader(spirvPath)); }
	GeomShader loadGeomShader(CStr spirvPath) { return GeomShader(loadShader(spirvPath)); }
	TessControlShader loadTessControlShader(CStr spirvPath) { return TessControlShader(loadShader(spirvPath)); }
	TessEvalShader loadTessEvalShader(CStr spirvPath) { return TessEvalShader(loadShader(spirvPath)); }

	VkPipelineLayout createPipelineLayout(CSpan<VkDescriptorSetLayout> descSetLayouts, CSpan<VkPushConstantRange> pushConstantRanges = {});
	void destroyPipelineLayout(VkPipelineLayout layout);

	VkResult createGraphicsPipelines(std::span<VkPipeline> pipelines, CSpan<GraphicsPipelineInfo> infos, VkPipelineCache cache = VK_NULL_HANDLE);
	void destroyPipeline(VkPipeline pipeline);

	void submit(VkQueue queue, CSpan<SubmitInfo> submits, VkFence signalFence = VK_NULL_HANDLE);
	void submit(VkQueue queue, std::initializer_list<SubmitInfo> submits, VkFence signalFence = VK_NULL_HANDLE) { submit(queue, CSpan(submits.begin(), submits.end()), signalFence); }

	void getSupportedSurfaceFormats(VkSurfaceKHR surface, u32& count, SurfaceFormat* formats);
	SurfaceFormat getBasicSupportedSurfaceFormat(VkSurfaceKHR surface);

	// possible depth-stencil formats: D16_UNORM, X8_D24_UNORM, D32_SFLOAT, S8_UINT, D16_UNORM_S8_UINT, D24_UNORM_S8_UINT, D32_SFLOAT_S8_UINT
	CSpan<Format> getSupportedDepthStencilFormats(std::span<Format> formats);
	Format getSupportedDepthStencilFormat_firstAmong(CSpan<Format> formats);
};

struct SwapchainOptions {
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	u32 minImages = 2;
	SurfaceFormat format = { .format = Format::undefined, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR };
};

struct Swapchain {
	static constexpr u32 MAX_IMAGES = 4;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	u32 numImages = 0;
	u32 w, h;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	SurfaceFormat format;
	Image images[MAX_IMAGES];
	ImageView imageViews[MAX_IMAGES];
	VkPresentModeKHR presentMode;
};

struct SwapchainSyncHelper : Swapchain {
	VkSemaphore semaphore_imageAvailable[MAX_IMAGES+1];
	VkSemaphore semaphore_drawFinished[MAX_IMAGES];
	VkFence fence_drawFinished[MAX_IMAGES];
	u32 imgInd = 0; // the image index to draw to and later present
	u32 imageAvailableSemaphoreInd = 0;
	//VkCommandPool cmdPools[MAX_IMAGES];
	//VkCommandBuffer cmdBuffers_draw[MAX_IMAGES];
	//VkCommandBuffer cmdBuffers_transfer[MAX_IMAGES];

	void acquireNextImage(Device& device);
	void waitCanStartFrame(Device& device);
	void present(VkQueue queue);
};

static constexpr CStr default_deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static inline void ASSERT_VKRES(VkResult vkRes) { assert(vkRes == VK_SUCCESS); }

VkInstance createInstance(const AppInfo& appInfo, CSpan<CStr> layers, CSpan<CStr> extensions);
void destroyInstance(VkInstance instance);

u32 getNumPhysicalDevices(VkInstance instance);
void getPhysicalDeviceInfos(VkInstance instance, std::vector<PhysicalDeviceInfo>& infos, VkSurfaceKHR surface = VK_NULL_HANDLE);

typedef tk::delegate<bool(const PhysicalDeviceInfo& info)> PhysicalDeviceFilterDelegate;
typedef tk::delegate<int(const PhysicalDeviceInfo& a, const PhysicalDeviceInfo& b)> PhysicalDeviceCompareDelegate;
struct PhysicalDeviceFilterAndCompare {
	// this function should return true when the device can be considered
	PhysicalDeviceFilterDelegate filter = {}; // if no filter function is specified, we don't filter anything
	// this function should return true when the device can be considered
	PhysicalDeviceCompareDelegate compare = {}; // if no compare function is specified, we will use "defaultCompare_discreteThenMemory"

	static bool defaultFilter_graphicsAndPresent(const PhysicalDeviceInfo& info);
	static int defaultCompare_discreteThenMemory(const PhysicalDeviceInfo& a, const PhysicalDeviceInfo& b);
};
// returns an index to the best physical device, according to the given functions
// if there is no suitable device, returns infos.size()
u32 chooseBestPhysicalDevice(CSpan<PhysicalDeviceInfo> infos, PhysicalDeviceFilterAndCompare fns);

VkResult createDevice(Device& device, VkInstance instance, const PhysicalDeviceInfo& physicalDeviceInfo,
	CSpan<QueuesCreateInfo> queuesInfos, CSpan<CStr> extensions = default_deviceExtensions);

// create a simple swapchain. If you need also help with synchronization use createSwapchainSynHelper instead
VkResult createSwapchain(Swapchain& o, VkSurfaceKHR surface, Device& device, const SwapchainOptions& options);
// create a swapchain with synchronization helpers. Yuo can use createSwapchain instead if you don't want synchronization help done for you
VkResult createSwapchainSyncHelper(SwapchainSyncHelper& o, VkSurfaceKHR surface, Device& device, const SwapchainOptions& options);

bool formatIsColor(Format format);
bool formatIsDepth(Format format);
bool formatIsStencil(Format format);

}
}