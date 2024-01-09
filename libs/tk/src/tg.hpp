#pragma once

#include "tvk.hpp"
#include "shader_compiler.hpp"
#include "delegate.hpp"

namespace tk {
namespace gfx {

enum class ImageType : u8 {
    _1d, _2d, _3d, cube
};

constexpr static u32 DESCSET_GLOBAL = 0;
constexpr static u32 DESCSET_MATERIAL = 1;
//constexpr static u32 DESCSET_OBJECT = 2;

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
	joints_0,
	weights_0,
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
};
typedef IdT<u32> IdU32;

// Reference counted resource handles. Resources will be destroyed automaticaly once all the refereces are destroyed (RAII)
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
    ~RefCounted() { decRefCount(id); }
    //operator->()
};

// we need to all this at the beginning of the application
struct InitRenderUniverseParams {
    VkInstance instance;
    VkSurfaceKHR surface;
    u32 screenW, screenH;
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
};
void incRefCount(ImageViewId id);
void decRefCount(ImageViewId id);
typedef RefCounted<ImageViewId> ImageViewRC;

struct MakeImageView {
    ImageRC image;

};
ImageViewRC makeImageView(const MakeImageView& info);

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
};
void incRefCount(GeomId id);
void decRefCount(GeomId id);
typedef RefCounted<GeomId> GeomRC;

GeomRC registerGeom(const GeomInfo& info, vk::Buffer buffer);
GeomRC createGeom(const CreateGeomInfo& info);

// MATERIAL
struct MaterialManagerId : IdU32 {};
struct MaterialId : IdU32
{
    MaterialManagerId manager; // TODO: we could pack everything in 32 bits: 8 for the managerId, 24 for the materialId

    MaterialId() : IdU32(), manager{} {}
    MaterialId(MaterialManagerId manager, u32 id) : manager(manager), IdU32{ id } {}
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
        T##RC() : MaterialRC(MaterialId(MaterialManagerId{}, u32(-1))) {} \
        T##RC(MaterialManagerId manager, u32 id = u32(-1)) : MaterialRC(MaterialId(manager, id)) {} \
    }

struct MaterialManager {
    void* managerPtr;
    void(*setManagerId)(void*, u32);
    void(*destroyMaterial)(void*, MaterialId);
    VkPipeline(*getPipeline)(void*, MaterialId, GeomId);
    VkPipelineLayout(*getPipelineLayout)(void*, MaterialId);
    VkDescriptorSet(*getDescriptorSet)(void*, MaterialId);
    //AttribLocations(*getAttibLocations)(void*, MaterialId);
};
void registerMaterialManager(const MaterialManager& backbacks);

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
    bool doubleSided = false;
};

struct PbrMaterialId : MaterialId {
    glm::vec4 getAlbedo()const;
    float getMetallic()const;
    float getRoughness()const;
};
DERIVED_MATERIAL_RC(PbrMaterial);

struct PbrMaterialManager {
    MaterialManagerId managerId;
    u32 maxExpectedMaterials = 0;
    VkDescriptorPool descPool;
    VkDescriptorSetLayout descSetLayouts[/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2] = {};
    VkPipelineLayout pipelineLayouts[/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2] = {};
    VkPipeline pipelines
        [/*hasAlbedoTexture*/ 2][/*hasNormalTexture*/ 2][/*hasMetallicRoughnessTexture*/ 2]
        [/*hasVertexNormalsOrTangents*/ 3][/*hasTexCoords*/2][/*hasVertexColors*/ 2][/*doubleSided*/ 2] = {};

    std::vector<PbrMaterialInfo> materials_info;
    std::vector<VkDescriptorSet> materials_descSet;
    vk::Buffer uniformBuffer; // a large uniform buffer contaning the data for all materials
    u32 materials_nextFreeEntry = -1;

    void init(u32 maxExpectedMaterials = 4 << 10);

    MaterialManager getRegisterCallbacks()const {
        return { (void*)this, s_setManagerId, s_destroyMaterial, s_getPipeline, s_getPipelineLayout, s_getDescriptorSet };
    }
    
    VkDescriptorSetLayout getCreateDescriptorSetLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture);
    VkPipelineLayout getCreatePipelineLayout(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture);
    VkPipeline getCreatePipeline(bool hasAlbedoTexture, bool hasNormalTexture, bool hasMetallicRoughnessTexture,
        HasVertexNormalsOrTangents hasVertexNormalsOrTangents, bool hasTexCoords, bool hasVertexColors, bool doubleSided);

    PbrMaterialRC createMaterial(const PbrMaterialInfo& params);
    void destroyMaterial(MaterialId id);
    VkPipeline getPipeline(MaterialId materialId, GeomId geomId);
    VkPipelineLayout getPipelineLayout(MaterialId materialId);
    VkDescriptorSet getDescriptorSet(MaterialId materialId) { return materials_descSet[materialId.id]; }
    //AttribLocations getAttribLocations(MaterialId materialId)const { return { 0, 4, 8, 12, 13, 14, 15, 16, }; }
    static void s_setManagerId(void* self, u32 id) { ((PbrMaterialManager*)self)->managerId = { id }; }
    static void s_destroyMaterial(void* self, MaterialId id) { ((PbrMaterialManager*)self)->destroyMaterial(id); }
    static VkPipeline s_getPipeline(void* self, MaterialId materialId, GeomId geomId) { return ((PbrMaterialManager*)self)->getPipeline(materialId, geomId); }
    static VkPipelineLayout s_getPipelineLayout(void* self, MaterialId materialId) { return ((PbrMaterialManager*)self)->getPipelineLayout(materialId); }
    static VkDescriptorSet s_getDescriptorSet(void* self, MaterialId materialId) { return ((PbrMaterialManager*)self)->getDescriptorSet(materialId); }
    //static AttribLocations s_getAttribLocations(void* self, MaterialId materialId) { return ((PbrMaterialManager*)self)->getAttribLocations(materialId); }

    ~PbrMaterialManager() {
        printf("~PbrMaterialManager()\n");
    }
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

    ObjectId(IdU32 worldId, IdU32 id) : IdU32(id), _renderWorld(worldId) {}
    const RenderWorldId& renderWorld()const { return *(const RenderWorldId*)&_renderWorld; }
    ObjectInfo getInfo()const;
    void setModelMatrix(const glm::mat4& m, u32 instanceInd = 0);
    void setModelMatrices(CSpan<glm::mat4> matrices, u32 firstInstanceInd = 0);
    bool addInstances(u32 n);
    void destroyInstance(u32 instanceInd);
};

// RENDER WORLDS
struct RenderWorldId : IdU32 {
    ObjectId createObject(MeshRC mesh, const glm::mat4& modelMtx = glm::mat4(1), u32 maxInstances = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances = 0);
    void destroyObject(ObjectId oid);
};
struct RenderWorld {
    struct ObjectMatrices {
        glm::mat4 modelMtx;
        glm::mat4 modelViewProj;
        glm::mat3 invTransModelView;
    };

    RenderWorldId id = {};
    std::vector<u32> objects_id_to_entry;
    std::vector<u32> objects_entry_to_id;
    std::vector<ObjectInfo> objects_info;
    std::vector<u32> objects_firstModelMtx;
    std::vector<glm::mat4> modelMatrices;
    std::vector<ObjectMatrices> objects_matricesTmp;
    std::vector<u32> objects_instancesCursorsTmp;
    u32 numObjects = 0;
    u32 objects_nextFreeId = u32(-1);
    bool needDefragmentObjects = false;
    std::vector<std::vector<vk::Buffer>> instancingBuffers; // [swapchainImgInd][viewportInd]
    std::vector<vk::Buffer> global_uniformBuffers;
    VkDescriptorPool global_descPool;
    VkDescriptorSetLayout global_descSetLayout;
    std::vector<VkDescriptorSet> global_descSets;

    // ** you can access the following members directly **
    glm::vec3 ambientLight = glm::vec3(0.1f);

    ObjectId createObject(MeshRC mesh, const glm::mat4& modelMtx = glm::mat4(1), u32 maxInstances = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, CSpan<glm::mat4> instancesMatrices, u32 maxInstances = 0);
    ObjectId createObjectWithInstancing(MeshRC mesh, u32 numInstances, u32 maxInstances = 0);
    void destroyObject(ObjectId oid);
    void _defragmentObjects();
};
RenderWorldId createRenderWorld();
void destroyRenderWorld(RenderWorldId id);

// draw
struct RenderWorldViewport { RenderWorldId renderWorld; glm::mat4 viewMtx; glm::mat4 projMtx; vk::Viewport viewport; vk::Rect2d scissor; };
void draw(CSpan<RenderWorldViewport> viewports);

}
}

namespace tg = tk::gfx;