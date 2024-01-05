#pragma once

#include "utils.hpp"

#include "tg.hpp"
#include <glm/gtc/quaternion.hpp>

namespace tk {

struct World;
typedef u16 ComponentTypeU16;
typedef u16 EntityTypeU16;

struct WorldId {
    u16 ind = u16(-1);
    auto operator<=>(const WorldId& o)const = default;
    World* operator->();
};

struct World;

struct ComponentId {
    ComponentTypeU16 componentType;
    EntityTypeU16 entityType;
    u32 ind;
};

struct ComponentsDB
{
    static std::vector<std::string> componentTypesNames;
    static ComponentTypeU16 registerComponentType(std::string&& name) {
        componentTypesNames.emplace_back(std::move(name));
        return componentTypesNames.size() - 1;
    }
};

struct EntityId {
    WorldId world;
    EntityTypeU16 type;
    u32 ind;
#ifndef NDEBUG
    u32 counter;
#endif
};

struct EntityFactory
{
    static std::vector<std::string> entityTypesNames;
    static EntityTypeU16 registerEntityType(std::string&& name)
    {
        entityTypesNames.emplace_back(std::move(name));
        return entityTypesNames.size()-1;
    }

    typedef void (*AllocEntitiesFn)(EntityFactory* self, std::span<EntityId> entities, CSpan<const u8*> data);
    typedef void (*ReleaseEntitiesFn)(EntityFactory* self, CSpan<EntityId> entities);
    typedef void* (*AccessComponentFn)(EntityFactory* self, u32 entityIndInFactory, ComponentTypeU16 componentType);

    World& world;
    std::string entityTypeName;
    std::vector<ComponentTypeU16> componentTypes;
    AllocEntitiesFn allocEntitiesFn;
    ReleaseEntitiesFn releaseEntitiesFn;
    AccessComponentFn accessComponentByIndFn;

    EntityFactory(World& world, std::string_view entityTypeName, CSpan<ComponentTypeU16> componentTypes,
        AllocEntitiesFn allocEntitiesFn, ReleaseEntitiesFn releaseEntitiesFn, AccessComponentFn accessComponentFn
    )
        : world(world), entityTypeName(entityTypeName),
        componentTypes(componentTypes.begin(), componentTypes.end()),
        allocEntitiesFn(allocEntitiesFn), releaseEntitiesFn(releaseEntitiesFn), accessComponentByIndFn(accessComponentFn)
    {}

    virtual ~EntityFactory() = default;

    u16 getComponentTypeIndex(ComponentTypeU16 componentType)const {
        for (u16 i = 0; i < u16(componentTypes.size()); i++)
            if (componentTypes[i] == componentType)
                return i;
        return u16(-1);
    }

    template <typename Comp>
    Comp& accessComponentByInd(u32 entityIndInFactory, u16 componentTypeInd) {
        assert(componentTypeInd == getComponentTypeIndex(Comp::s_type()));
        return *(Comp*)accessComponentByIndFn(this, entityIndInFactory, componentTypeInd);
    }
    template <typename Comp>
    Comp& accessComponent(u32 entityIndInFactory) {
        const u16 componentTypeInd = getComponentTypeIndex(Comp::s_type);
        assert(componentTypeInd != u16(-1));
        return *(Comp*)accessComponentByIndFn(this, entityIndInFactory, componentTypeInd);
    }
};

// -- COMPONENTS --

// https://stackoverflow.com/questions/185624/static-variables-in-an-inlined-function
#define COMPONENT_TYPE(X) \
    static inline ComponentTypeU16 s_type() { \
        static ComponentTypeU16 id = ComponentsDB::registerComponentType(#X); \
        return id; \
    }

// Position3d
struct Component_Position3d : glm::vec3 {
    COMPONENT_TYPE(Position3d);
};

// Rotation3d
struct Component_Rotation3d : glm::quat {
    COMPONENT_TYPE(Rotation3d);
};

// Scale3d
struct Component_Scale3d : glm::vec3 {
    COMPONENT_TYPE(Scale3d);
};

// RenderableMesh
struct Component_RenderableMesh3d {
    COMPONENT_TYPE(GfxObject);
    u32 gfxObjectInd;
    u32 instanceInd;
};

// -- ENTITIES --

struct System_Render;

struct EntityFactory_Renderable3d : EntityFactory
{
    static EntityTypeU16 s_type() {
        static EntityTypeU16 id = EntityFactory::registerEntityType("Renderable3d");
        return id;
    }

    static inline const ComponentTypeU16 k_componentTypes[] = {
        Component_Position3d::s_type(),
        Component_Rotation3d::s_type(),
        Component_Scale3d::s_type(),
        Component_RenderableMesh3d::s_type(),
    };

    System_Render& system_render;

    std::vector<u32> indsInWorld; // indInFactory -> indInWorld
    std::vector<Component_Position3d> components_position3d;
    std::vector<Component_Rotation3d> components_rotation3d;
    std::vector<Component_Scale3d> components_scale3d;
    std::vector<Component_RenderableMesh3d> components_renderableMesh;
    u32 components_nextFreeEntry = u32(-1);
    
    std::vector<gfx::ObjectId> gfxObjects;
    u32 gfxObjects_nextFreeEntry = u32(-1);

    std::unordered_map<u32, u32> mesh_to_renderableInd;
    std::unordered_map<u64, u32> geomAndMaterial_to_renderableInd;

    EntityFactory_Renderable3d(World& world, System_Render& system_render)
        : EntityFactory(world, "Renderable3d", k_componentTypes, s_allocEntitiesFn, s_releaseEntitiesFn, s_accessComponentByIndFn)
        , system_render(system_render)
    {}

    struct Create {
        glm::vec3 position = glm::vec3(0);
        glm::quat rotation = glm::quat();
        glm::vec3 scale = glm::vec3(0);
        bool separateMaterial = false;
        union {
            struct {
                gfx::GeomRC geom;
                gfx::MaterialRC material;
            };
            gfx::MeshRC mesh;
        };
        ~Create() {
            if (separateMaterial) {
                geom.~RefCounted();
                material.~RefCounted();
            }
            else {
                mesh.~RefCounted();
            }
        }
    };
    EntityId create(const Create& info);

    static void s_allocEntitiesFn(EntityFactory* self, std::span<EntityId> entities, CSpan<const u8*> data);
    static void s_releaseEntitiesFn(EntityFactory* self, CSpan<EntityId> entities);
    static void* s_accessComponentByIndFn(EntityFactory* self, u32 entityIndInFactory, u16 componentTypeInd);
};

// -- SYSTEMS --
struct System_Render
{
    WorldId worldId;
    gfx::RenderWorldId RW;
    EntityFactory_Renderable3d* factory_renderable3d;
    gfx::PbrMaterialManager pbrMaterialManager;

    System_Render(WorldId worldId);
    ~System_Render();

    void update(float dt);
};

struct SystemId_Render {
    WorldId worldId;
};

// -- DefaultBasicWorldSystems --

struct DefaultBasicWorldSystems
{
    System_Render& system_render;

    void update(float dt);
    void destroy();
};

// -- WORLD --
struct World
{
    std::vector<EntityFactory*> entityFactories; // [entityType]
    std::vector<std::string> componentNames; // (I belive the cstr pointer should keep valid if the vector resizes)

    std::vector<EntityTypeU16> entities_type; // [entity.ind]
    std::vector<u32> entities_indInFactory; // [entity.ind]
#ifndef NDEBUG
    std::vector<u32> entities_counter; // [entity.ind] keep track of how many times the entry has been reused. Useful for verifying if a EntityId refers to and old released entity
#endif

    // entity hierarchy
    std::vector<u32> entities_parent; // [entity.ind]
    std::vector<u32> entities_firstChild;
    std::vector<u32> entities_lastChild;
    std::vector<u32> entities_nextSibling; // [entity.ind]
    std::vector<u32> entities_prevSibling; // [entity.ind]

    std::vector<glm::mat4> cachedEntityMatrices;
    std::vector<bool> cachedEntityMatrices_isValid;

    u32 entities_nextFreeEntry = u32(-1);

    // component hierarchy
    std::vector<std::vector<u32>> entitiesComponents; // [entityType][i]

    // systems
    std::vector<void*> systems;

    World();

    WorldId id()const;

    void update(float dt);

    const glm::mat4& getMatrix(u32 entityInd);

    EntityId _createEntity(EntityTypeU16 entityType, u32 indInFactory); // meant to be used by EntityFactories
    void _destroyEntity(EntityId eId); // meant to be used by EntityFactories

    // functions for modifying the scene hierarchy
    void _breakEntityLinks(u32 entityInd);
    void setEntityAsFirstChildOf(EntityId e, EntityId p);
    void setEntityAsLastChildOf(EntityId e, EntityId p);
    void setEntityNextSiblingAfter(EntityId e, EntityId s);

    template <typename EF>
    EntityTypeU16 registerEntityFactory(EF* ef) {
        const auto et = EF::s_type();
        if (et >= entityFactories.size())
            entityFactories.resize(et + 1, nullptr);
        entityFactories[et] = ef;
        return et;
    }

    template <typename EF, typename... Args>
    EntityTypeU16 createAndRegisterEntityFactory(Args&&... args) {
        return registerEntityFactory<EF>( new EF(*this, std::forward<Args>(args)...) );
    }

    template <typename EF>
    EF& getEntityFactory() {
        return *(EF*)entityFactories[EF::s_type()];
    }

    [[nodiscard]]
    DefaultBasicWorldSystems createDefaultBasicSystems();
};

WorldId createWorld();

}