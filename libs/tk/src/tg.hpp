#pragma once

#include "tvk.hpp"
#include "shader_compiler.hpp"
#include "delegate.hpp"

namespace tk {
namespace gfx {

enum class ImageType : u8 {
    _1d, _2d, _3d, cube
};
typedef i8 WorldLayer;

constexpr static u32 DESCSET_GLOBAL = 0;
constexpr static u32 DESCSET_MATERIAL = 1;

struct GlobalUniforms_Header {
    glm::vec3 ambientLight;
    u32 numDirLights;
};
struct GlobalUniforms_DirLight {
    glm::vec4 dir;
    glm::vec4 color;
};

enum class AttribSemantic {
	position,
	normal,
	tangent,
	texcoord_0, //texcoord_1, texcoord_2, texcoord_3,
	color_0, //color_1, color_2, color_3,
	//joints_0,
	//weights_0,
    COUNT,
};
enum class HasVertexNormalsOrTangents { no, normals, normalsAndTangents, };

struct ImageInfo {
    vk::Format format = vk::Format::undefined;
    u16 w = 0, h = 1, depth = 1; // TODO: maybe use u16?
    u8 numMips = 1;
    ImageType type = ImageType::_2d;

    u16 getNumLayers() const { return type == ImageType::_3d ? u16(1) : depth; }
    // TODO: we are assuming that the image is just for sampling in the fragment shader, we should have options for other use cases
};

// id handles
template <typename T>
struct IdT {
    T id = T(-1);
    bool isValid()const { return id != T(-1); }
    auto operator<=>(const IdT<T>&)const = default;
};
typedef IdT<u32> IdU32;

// Reference counted resource handles. Resources will be destroyed automaticaly once all the refereces are destroyed (UniqueResource)
template<typename IdType>
struct RefCounted {
    IdType id;

    explicit RefCounted(IdType id = IdType{}) : id(id) { incRefCount(id); } // dangerous: don't play with this
    RefCounted(const RefCounted& rc) : id(rc.id) { incRefCount(rc.id); }
    RefCounted(RefCounted&& rc) noexcept : id(rc.id) { rc.id = IdType{}; }
    RefCounted<IdType>& operator=(const RefCounted<IdType>& o) {
        decRefCount(id);
        id = o.id;
        incRefCount(id);
        return *this;
    }
    RefCounted<IdType>& operator=(RefCounted<IdType>&& o) {
        decRefCount(id);
        id = o.id;
        o.id = IdType{};
        return *this;
    }
    bool operator==(const RefCounted<IdType>& o)const { return id == o.id; }
    ~RefCounted() { decRefCount(id); }

    //IdType operator->() { return id; }
};

// we need to all this at the beginning of the application
struct InitRenderUniverseParams {
    VkInstance instance;
    VkSurfaceKHR surface;
    u32 screenW, screenH;
    tk::vk::DeviceFeatures deviceFeatures = { .depthBiasClamp = true, };
    bool enableImgui : 1 = false;
    bool generateShadersDebugInfo : 1 = false;
};
void initRenderUniverse(const InitRenderUniverseParams& params);

// call this when the size of the window changes
void onWindowResized(u32 w, u32 h);

// call only after initRenderUniverse
ShaderCompiler& getShaderCompiler();

// IMAGES
struct ImageId : IdU32 {
    bool operator==(ImageId o)const { return id == o.id; }
    ImageInfo getInfo()const;
    vk::Image getHandle()const;
    void* getInternalHandle()const;
};

void incRefCount(ImageId id);
void decRefCount(ImageId id);
typedef RefCounted<ImageId> ImageRC;

u8 calcNumMipsFromDimensions(u32 w, u32 h);
bool nextMipLevelDown(u32& w, u32& h); // return true when reached the 1x1 level
ImageRC makeImage(const ImageInfo& info, CSpan<u8> data = {}, bool generateRemainingMips = true);
ImageRC getOrLoadImage(Path path, bool srgb, bool generateMipChain = true);

// IMAGE VIEWS
struct ImageViewId : IdU32 {
    bool operator==(ImageViewId o)const { return id == o.id; }
    ImageRC getImage()const;
    vk::ImageView getHandle()const;
    void* getInternalHandle()const;
};
void incRefCount(ImageViewId id);
void decRefCount(ImageViewId id);
typedef RefCounted<ImageViewId> ImageViewRC;

struct MakeImageView {
    ImageRC image;

};
ImageViewRC makeImageView(const MakeImageView& info);

// FRAMEBUFFERS
/*
struct FramebufferId : IdU32 {
    bool operator==(FramebufferId o)const { return id == o.id; }
    //vk::ImageView getHandle()const;
    void* getInternalHandle()const;
};

struct MakeFramebuffer {
    ImageViewRC colorBuffer;
    ImageViewRC depthBuffer;
};
FramebufferId makeFramebuffer(const MakeFramebuffer& info);
void destroyFramebuffer(FramebufferId id);
*/

// DESCRIPTOR SETS
struct DescPoolId : IdU32 {
    auto operator<=>(const DescPoolId& o)const { return id <=> o.id; };
    VkDescriptorPool getHandleVk()const;
};
struct PoolDescSetId : IdU32 {
    auto operator<=>(const PoolDescSetId& o)const { return id <=> o.id; };
};
struct DescSetId {
    DescPoolId descPool;
    PoolDescSetId id;
    auto operator<=>(const DescSetId& o)const { return std::tie(descPool, id) <=> std::tie(o.descPool, o.id); }
    bool isValid()const { return id.isValid(); }
};

struct MakeDescPool {
    u32 maxSets = 0;
    struct MaxPerType {
        u32 uniformBuffers = 0;
        u32 combinedImageSamplers = 0;
    } maxPerType;
    vk::DescPoolOptions options;
};
DescPoolId makeDescPool(const MakeDescPool& info);

void releaseDescSets(DescPoolId poolId, CSpan<VkDescriptorSet> descSets);
inline void releaseDescSet(DescPoolId poolId, VkDescriptorSet descSet) { releaseDescSets(poolId, {&descSet, 1}); }
void releaseImguiDescSet(VkDescriptorSet descSet);

// SAMPLERS
VkSampler getNearestSampler();
VkSampler getAnisotropicFilteringSampler(float maxAniso);

// GEOMETRY
/*struct AttribLocations {
    u32 u_model = -1;
    u32 u_invTransModel = -1;
    u32 u_modelViewProj = -1;
    u32 a_position = -1;
    u32 a_normal = -1;
    u32 a_tangent = -1;
    u32 a_texCoord_0 = -1;
    u32 a_color_0 = -1;
};*/
struct GeomInfo {
    u32 attribOffset_positions = 0;
    u32 attribOffset_normals = u32(-1);
    u32 attribOffset_tangents = u32(-1);
    u32 attribOffset_texCoords = u32(-1);
    u32 attribOffset_colors = u32(-1);
    u32 indsOffset = u32(-1);
    u32 numVerts = 0;
    u32 numInds = 0;
};
struct CreateGeomInfo {
    CSpan<u8> positions = {};
    CSpan<u8> normals = {};
    CSpan<u8> tangents = {};
    CSpan<u8> texCoords = {};
    CSpan<u8> colors = {};
    CSpan<u8> indices = {};
    u32 numVerts = 0;
    u32 numInds = 0;
};
struct GeomId : IdU32
{
    static constexpr GeomId invalid() { return GeomId{ u32(-1) }; }
    const GeomInfo& getInfo()const;
    vk::Buffer getBuffer()const;
    auto operator<=>(const GeomId&)const = default;
};
void incRefCount(GeomId id);
void decRefCount(GeomId id);
typedef RefCounted<GeomId> GeomRC;

GeomRC geom_create();
void geom_resetFromBuffer(const GeomRC& h, const GeomInfo& info, vk::Buffer buffer);
void geom_resetFromInfo(const GeomRC& h, const CreateGeomInfo& info, AABB* aabb = nullptr);
bool geom_resetFromFile(const GeomRC& h, CStr filePath, AABB* aabb = nullptr);
bool geom_resetFromMemFile(const GeomRC& h, CSpan<u8> mem, AABB* aabb = nullptr);

inline GeomRC geom_createFromInfo(const CreateGeomInfo& info) {
    GeomRC h = geom_create();
    geom_resetFromInfo(h, info);
    return h;
};

// keeps track or geoms that have been already loaded, so to avoid loading duplicates
[[nodiscard]]
GeomRC geom_getOrLoadFromFile(CStr path, AABB* aabb = nullptr);

// serialize a geom into a memory buffer. The resulting buffer could be used as input to geom_resetFromMemFile. Or could be stored in a file, and loaded with `geom_resetFromFile` or `geom_getOrLoadFromFile`.
// return the number of written bytes. If data is empty, it returns the needed buffer space
size_t geom_serializeToMem(const CreateGeomInfo& geomInfo, std::span<u8> data);

bool geom_serializeToFile(const CreateGeomInfo& geomInfo, CStr dstPath);

// MATERIAL
enum class MaterialType : u8
{
    PBR,
    WIREFRAME,

    COUNT,
    INVALID = 255,
};
struct MaterialId : IdU32
{
    MaterialType type; // TODO: we could pack everything in 32 bits: 8 for the managerId, 24 for the materialId

    MaterialId() : IdU32(), type{MaterialType::INVALID} {}
    MaterialId(MaterialType type, u32 id) : type(type), IdU32{ id } {}
    VkPipeline getPipeline(GeomId geomId)const;
    VkPipelineLayout getPipelineLayout()const;
    VkDescriptorSet getDescSet()const;
    //vk::Buffer getBuffer(u32 binding);
    //vk::Image getImage(u32 binding);
};
void incRefCount(MaterialId id);
void decRefCount(MaterialId id);
typedef RefCounted<MaterialId> MaterialRC;

#define DERIVED_MATERIAL_RC(T) \
    struct T##RC : MaterialRC { \
        T##RC() : MaterialRC(MaterialId(MaterialType::INVALID, u32(-1))) {} \
        T##RC(MaterialType type, u32 id = u32(-1)) : MaterialRC(MaterialId(type, id)) {} \
    }

struct MaterialManager {
    void* managerPtr;
    void(*destroyMaterial)(void*, MaterialId);
    VkPipeline(*getPipeline)(void*, MaterialId, GeomId);
    VkPipelineLayout(*getPipelineLayout)(void*, MaterialId);
    VkDescriptorSet(*getDescriptorSet)(void*, MaterialId);
    //AttribLocations(*getAttibLocations)(void*, MaterialId);
    
    MaterialType& type() { return *(MaterialType*)managerPtr; }
};
u32 registerMaterialManager(const MaterialManager& backbacks);

// PBR MATERIAL
struct Texture {
    ImageViewRC imageView;
    vk::SamplerInfo samplerInfo;
};
struct PbrUniforms {
    alignas(16) glm::vec4 albedo = glm::vec4(1.f);
    float metallic = 0.f;
    float roughness = 1.f;
};
struct PbrMaterialInfo {
    glm::vec4 albedo = glm::vec4(1.f);
    float metallic = 0.f;
    float roughness = 1.f;
    ImageViewRC albedoImageView = ImageViewRC(ImageViewId{});
    ImageViewRC normalImageView = ImageViewRC(ImageViewId{});
    ImageViewRC metallicRoughnessImageView = ImageViewRC(ImageViewId{});
    float anisotropicFiltering = 1.f;
    bool doubleSided = false;
};

struct PbrMaterialId : MaterialId {
    glm::vec4 getAlbedo()const;
    float getMetallic()const;
    float getRoughness()const;
};
DERIVED_MATERIAL_RC(PbrMaterial);

struct WireframeMaterialId : MaterialId {
    glm::vec4 getColor()const;
};
DERIVED_MATERIAL_RC(WireframeMaterial);

struct PbrMaterialManager {
    const MaterialType type = MaterialType::PBR;
    u32 maxExpectedMaterials = 0;
    DescPoolId descPool;
    VkDescriptorSetLayout descSetLayouts[/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2] = {};
    VkPipelineLayout pipelineLayouts[/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2] = {};
    VkPipeline pipelines
        [/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2]
        [/*hasVertexNormalsOrTangents*/ 3][/*hasTexCoords*/2][/*hasVertexColors*/ 2][/*doubleSided*/ 2] = {};

    std::vector<PbrMaterialInfo> materials_info;
    std::vector<VkDescriptorSet> materials_descSet;
    //std::vector<u32> materials_customSamplers; // shall be not null when we are not using a sampler from "defaultSamplers". When using custom samplers, we would need to delete the sampler when the material is destroyed
    vk::Buffer uniformBuffer; // a large uniform buffer contaning the data for all materials
    u32 materials_nextFreeEntry = -1;
    
    VkDescriptorSetLayout getCreateDescriptorSetLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture);
    VkPipelineLayout getCreatePipelineLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture);
    VkPipeline getCreatePipeline(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture,
        HasVertexNormalsOrTangents hasVertexNormalsOrTangents, bool hasTexCoords, bool hasVertexColors, bool doubleSided);

    PbrMaterialRC createMaterial(const PbrMaterialInfo& params);
    void destroyMaterial(MaterialId id);

    VkPipeline getPipeline(MaterialId materialId, GeomId geomId);
    VkPipelineLayout getPipelineLayout(MaterialId materialId);
    VkDescriptorSet getDescriptorSet(MaterialId materialId) { return materials_descSet[materialId.id]; }

    static PbrMaterialManager* s_getOrCreate(u32 maxExpectedMaterials = 4 << 10);

    PbrMaterialManager(u32 maxExpectedMaterials);

    ~PbrMaterialManager() {
        printf("~PbrMaterialManager()\n");
    }
};

struct WireframeMaterialInfo {
    glm::vec4 color;
    //float thickness;
};
typedef WireframeMaterialInfo WireframeUniforms;

struct WireframeMaterialManager {
    const MaterialType type = MaterialType::WIREFRAME;
    u32 maxExpectedMaterials = 0;
    DescPoolId descPool;
    VkDescriptorSetLayout descSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    std::vector<WireframeMaterialInfo> materials_info;
    std::vector<VkDescriptorSet> materials_descSet;
    vk::Buffer uniformBuffer; // a large uniform buffer contaning the data for all materials
    u32 materials_nextFreeEntry = -1;

    WireframeMaterialManager(u32 maxExpectedMaterials);

    WireframeMaterialRC createMaterial(const WireframeMaterialInfo& params);
    void destroyMaterial(MaterialId id);

    VkPipeline getPipeline(MaterialId materialId, GeomId geomId) { return pipeline; }
    VkPipelineLayout getPipelineLayout(MaterialId materialId) { return pipelineLayout; }
    VkDescriptorSet getDescriptorSet(MaterialId materialId) { return materials_descSet[materialId.id]; }

    static WireframeMaterialManager* s_getOrCreate(u32 maxExpectedMaterials = 4 << 10);
};

// MESH
struct MeshInfo {
    GeomRC geom = GeomRC{};
    MaterialRC material = MaterialRC{};
};
struct MeshId : IdU32
{
    const MeshInfo getInfo()const;
    VkPipeline getPipeline()const;
};
void incRefCount(MeshId id);
void decRefCount(MeshId id);
typedef RefCounted<MeshId> MeshRC;

MeshRC makeMesh(MeshInfo&& info);

// OBJECT
struct RenderWorldId;
struct ObjectInfo {
    MeshRC mesh;
    u32 numInstances = 1;
    u32 maxInstances = 1;
};
struct ObjectId : IdU32 {
    IdU32 _renderWorld = {};

    ObjectId() : ObjectId(IdU32(-1), IdU32(-1)) {};
    ObjectId(IdU32 worldId, IdU32 id) : IdU32(id), _renderWorld(worldId) {}
    const RenderWorldId& renderWorld()const { return *(const RenderWorldId*)&_renderWorld; }
    const MeshRC& getMesh()const;
    u32 getNumInstances()const;
    u32 getNumAllocInstances()const;
    WorldLayer& layer();
    void setModelMatrix(const glm::mat4& m, u32 instanceInd = 0);
    void setModelMatrices(CSpan<glm::mat4> matrices, u32 firstInstanceInd = 0);
    bool addInstances(u32 n);
    bool changeNumInstances(u32 n);
    void destroyInstance(u32 instanceInd);
};

struct PointLight {
    glm::vec3 position = {0,0,0};
    float maxDistance = 1;
    glm::vec3 color = {1,1,1};
    float halfDistIntensity = 0.5f; // [0, 1] how much intensity is left at maxDistance/2
};

struct DirLight {
    glm::vec3 dir;
    glm::vec3 color;
};

struct PointLightId : IdU32
{
    IdU32 _renderWorld = {};

    PointLightId() : PointLightId(IdU32(-1), IdU32(-1)) {};
    PointLightId(IdU32 worldId, IdU32 id) : IdU32(id), _renderWorld(worldId) {}
    const RenderWorldId& renderWorld()const { return *(const RenderWorldId*)&_renderWorld; }

    PointLight* operator->();
};

struct DirLightId : IdU32
{
    IdU32 _renderWorld = {};
    
    DirLightId() : DirLightId(IdU32(-1), IdU32(-1)) {};
    DirLightId(IdU32 worldId, IdU32 id) : IdU32(id), _renderWorld(worldId) {}
    const RenderWorldId& renderWorld()const { return *(const RenderWorldId*)&_renderWorld; }

    DirLight* operator->();
};

// RENDER WORLDS
struct RenderWorld;

struct RenderWorldId : IdU32 {
    ObjectId createObject(MeshRC mesh, const glm::mat4& modelMtx = glm::mat4(1), u32 maxInstances = 0, WorldLayer layer = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances = 0, WorldLayer layer = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances = 0, WorldLayer layer = 0);
    void destroyObject(ObjectId oid);

    PointLightId createPointLight(const PointLight& l = {});
    void destroyPointLight(PointLightId l);

    DirLightId createDirLight(const DirLight& l = {});
    void destroyDirLight(DirLightId l);

    void debugGui();
};
struct RenderWorld {
    struct ObjectMatrices {
        glm::mat4 modelMtx;
        glm::mat4 modelViewProj;
        glm::mat3 invTransModelView;
    };

    RenderWorldId id = {};
    std::vector<u32> objects_id_to_entry;
    std::vector<u32> objects_entry_to_id[2]; // for all the following vectors that have two entries... this is because we use the second one as an intermediate buffer for sorting and deframentation operations. the [0] item is the current one
    std::vector<MeshRC> objects_mesh[2];
    std::vector<u32> objects_numInstances[2];
    std::vector<u32> objects_numAllocInstances[2];
    std::vector<WorldLayer> objects_layer[2];
    std::vector<u32> objects_firstModelMtx[2];
    std::vector<glm::mat4> modelMatrices[2];
    std::vector<ObjectMatrices> objects_matricesTmp;
    u32 numObjects = 0;
    u32 objects_nextFreeId = u32(-1);
    bool needDefragmentObjects : 1 = false;
    bool needResortObjects : 1 = false;
    std::vector<std::vector<std::vector<vk::Buffer>>> instancingBuffers; // [swapchainImgInd][renderTargetInd][viewportInd]
    std::vector<vk::Buffer> global_uniformBuffers;
    VkDescriptorPool global_descPool;
    VkDescriptorSetLayout global_descSetLayout;
    std::vector<VkDescriptorSet> global_descSets;
    tk::EntriesArray<PointLight> pointLights;
    tk::EntriesArray<DirLight> dirLights;

    // ** you can access the following members directly **
    glm::vec3 ambientLight = glm::vec3(0.1f);

    auto objectsVecs(u32 i) { return make_refs_tuple(objects_entry_to_id[i], objects_mesh[i], objects_numInstances[i], objects_numAllocInstances[i], objects_layer[i], objects_firstModelMtx[i]); };

    ObjectId createObject(MeshRC mesh, const glm::mat4& modelMtx = glm::mat4(1), u32 maxInstances = 0, WorldLayer layer = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances = 0, WorldLayer layer = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances = 0, WorldLayer layer = 0);
    void destroyObject(ObjectId oid);
    void _sortAndDefragmentObjects();

    PointLightId createPointLight(const PointLight& l = {});
    void destroyPointLight(PointLightId l);

    DirLightId createDirLight(const DirLight& l = {});
    void destroyDirLight(DirLightId l);

};
RenderWorldId createRenderWorld();
void destroyRenderWorld(RenderWorldId id);


// RENDER TARGET
struct RenderTargetParams {
    u32 w = 128, h = 128;
    bool resizeUpOnly = true;
    bool autoRedraw = true;
};

struct RenderTargetId : IdU32
{
    vk::Image getTextureImage();
    vk::Image getTextureImage(u32 scImgInd);
    vk::ImageView getTextureImageView();
    vk::ImageView getTextureImageView(u32 scImgInd);
    VkImageView getTextureImageViewVk();
    VkImageView getTextureImageViewVk(u32 scImgInd);

    void requestRedraw();
    void resize(u32 w, u32 h);
};

RenderTargetId createRenderTarget(const RenderTargetParams& params);
void destroyRenderTarget(RenderTargetId id);

u32 getNumSwapchainImages();
u32 getCurrentSwapchainImageInd();

// draw
struct RenderWorldViewport {
    RenderWorldId renderWorld;
    glm::mat4 viewMtx;
    glm::mat4 projMtx;
    vk::Viewport viewport;
    vk::Rect2d scissor;
    RenderTargetId renderTarget = {};
};
struct RenderTargetWorldViewports {
    RenderTargetId renderTarget;
    glm::vec4 clearColor = { 0.1f, 0.1f, 0.1f, 0.f };
    std::vector<RenderWorldViewport> viewports;
};

void prepareDraw();
void draw(
    CSpan<RenderWorldViewport> mainViewports,
    CSpan<RenderTargetWorldViewports> renderTargetsViewports
);

// imgui
VkDescriptorSet createImGuiTextureDescSet(VkSampler sampler, ImageViewId imgView, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
void imgui_newFrame();

}
}

namespace tg = tk::gfx;