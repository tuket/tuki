#pragma once

namespace tk {

struct Transform3d {
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat quat;
};

struct PipelineLayoutKey {
    bool hasAlbedoTexture;
    bool hasNormalTexture;
    bool hasMetallicRoughnessTexture;

};

struct RenderWorld
{
    struct Primitive {
        tvk::PrimitiveType primitiveType;
        tvk::Buffer vertexBuffer;
        tvk::Buffer indexBuffer;
        u32 vertexCount;
        u32 indexCount;

        u32 positionsOffset;
        u32 normalsOffset;
        u32 texCoordsOffset;
        u32 vertColorsOffset;

        u32 materialInd;
    };

    struct MaterialPbr {
        vec4 albedo;
        float metallicFactor;
        float roughnessFactor;
        tvk::Image albedoImage;
        tvk::Sampler albedoSampler;

    };

    struct Object {
        std::vector<u32> primitiveInd;
        Transform3d transform;
        u32 materialInd;
    };

    struct MakeMeshInfo {
        CSpan<u8> vertsData = {}, indsData = {};
        tvk::PrimitiveType primitiveType = tvk::PrimitiveType::triangles;
        tvk::IndexType indexType = tvk::IndexType::u32; // u8, u16, u32
        u32 vertexCount = 0, indexCount = 0;
        u32 positionsOffset = 0, normalsOffset = -1, tangentsOffset = -1, texCoordsOffset = -1, vertColorsOffset = -1;
    };

    struct MakeImageInfo {
        CSpan<u8> data;
        tvk::Format format = tvk::Format::undefined;
        u32 w = 0, h = 0;
        u32 providedMips = 1;
        bool genrateRemainingMips = true;
        // for now we assume that the image will be loaded for just sampling it in the fragment shader, in the future we should give options to handle other use cases
        //VkFence fence_imageLoaded;
    };

    struct MakePbrMaterialInfo {
        vec4 albedo;
        float metallic;
        float roughness;
        ImageRC albedoImage;
        ImageRC normalImage;
        ImageRC metallicRoughnessImage;
    };

    std::vector<> pipelineLayouts;
    std::vector<> pipelines;
    std::vector<> images;
    std::vector<> meshes;
    std::vector<> materials;
    std::vector<> materials_descSets;
    std::vector<> effects;
    std::vector<> pointLights;
    std::vector<> dirLights;
    CubeMap environmentMap;
    std::vector<> objects;

    MeshId makeMesh(const MakeMeshInfo& info);
    MeshId_RC refCounted(MeshId id);
    MeshId_RC makeMesh_RC(const MakeMeshInfo& info);

    ImageId makeImage(const MakeImageInfo& info);
    ImageId_RC refCounted(ImageId id);
    ImageId_RC makeImage_RC(const MakeImageInfo& info) { return refCounted(makeImage(info)); }

    MaterialRC makePbrMaterial(const MakePbrMaterialInfo& info);
    MaterialId registerCustomMaterial(VkDecriptorSet descSet);
};

extern RenderWorld* g_renderWorld;
RenderWorld* initRenderWorld();

}