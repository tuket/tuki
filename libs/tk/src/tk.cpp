#include "tk.hpp"

#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <physfs.h>
#include <Tracy.hpp>

namespace tk {

bool init(CStr argv0, CStr rootShadersPath)
{
    if (!PHYSFS_init(argv0)) {
        printf("PHYSFS_init() failed!\n  reason: %s.\n", PHYSFS_getLastError());
        return false;
    }

    if (!PHYSFS_mount("data", "/", 0)) {
        printf("could not mount data dir\n");
    }

    if (!PHYSFS_mount(rootShadersPath, "/shaders", 0)) {
        printf("could not mount shaders dir\n");
    }
    tg::getShaderCompiler().rootShadersPath = "shaders";

    if (!PHYSFS_setWriteDir("data")) {
        printf("could not set the write dir\n");
    }
    return true;
}

// -- COMPONENTS --
std::vector<std::string> ComponentsDB::componentTypesNames;

// -- ENTITIES --

std::vector<std::string> EntityFactory::entityTypesNames;

EntityFactory_Renderable3d::~EntityFactory_Renderable3d()
{
    for (gfx::ObjectId o : gfxObjects) {
        system_render.RW.destroyObject(o);
    }
}

EntityId EntityFactory_Renderable3d::create(const Create& info)
{
    const u32 e = u32(components_position3d.size());
    indsInWorld.emplace_back();
    components_position3d.push_back({ info.position });
    components_rotation3d.push_back({ info.rotation });
    components_scale3d.push_back({ info.scale });
    components_renderableMesh.emplace_back();

    u32 gfxObjectInd;
    u32 instanceInd = 0;
    auto addGfxObjectInstance = [this, e, &gfxObjectInd, &instanceInd](u32 _gfxObjectInd) {
        gfxObjectInd = _gfxObjectInd;
        auto& gfxObject = gfxObjects[gfxObjectInd];
        tg::ObjectId oldGfxObject = gfxObject;
        auto gfxObjectInfo = gfxObject.getInfo();
        instanceInd = gfxObjectInfo.numInstances;
        gfxObjectInfo.numInstances++;
        if (gfxObjectInfo.numInstances > gfxObjectInfo.maxInstances) {
            const u32 newMaxInstances =
                gfxObjectInfo.numInstances > 64 ? nextPowerOf2(gfxObjectInfo.numInstances) :
                gfxObjectInfo.numInstances > 16 ? 64 :
                gfxObjectInfo.numInstances > 4 ? 16 : 4;
            auto newGfxObject = system_render.RW.createObjectWithInstancing(gfxObjectInfo.mesh, gfxObjectInfo.numInstances, newMaxInstances);
            gfxObject = newGfxObject;
            system_render.RW.destroyObject(oldGfxObject);
        }
        else {
            gfxObject.addInstances(1);
        }
    };

    if (info.separateMaterial) {
        const u64 geomAndMaterial = (u64(info.geom.id.id) << u64(32)) | u64(info.material.id.id);
        if (auto it = geomAndMaterial_to_gfxObjectInd.find(geomAndMaterial); it == geomAndMaterial_to_gfxObjectInd.end()) {
            auto mesh = gfx::makeMesh({ .geom = info.geom, .material = info.material });
            gfxObjects.emplace_back(system_render.RW.createObject(mesh));
            gfxObjectInd = u32(gfxObjects.size() - 1);
            geomAndMaterial_to_gfxObjectInd[geomAndMaterial] = gfxObjectInd;
        }
        else {
            addGfxObjectInstance(it->second);
        }
    }
    else {
        const u32 meshInd = info.mesh.id.id;
        if (auto it = mesh_to_gfxObjectInd.find(meshInd); it == mesh_to_gfxObjectInd.end()) {
            gfxObjectInd = u32(gfxObjects.size());
            gfxObjects.emplace_back(system_render.RW.createObject(info.mesh));
            mesh_to_gfxObjectInd[meshInd] = gfxObjectInd;
        }
        else {
            addGfxObjectInstance(it->second);
        }
    }
    components_renderableMesh[e] = { gfxObjectInd, instanceInd };

    auto id = world->_createEntity(s_type(), e);
    indsInWorld[e] = id.ind;
    return id;
}

void EntityFactory_Renderable3d::s_releaseEntitiesFn(EntityFactory* self, CSpan<u32> entitiesIndInWorld)
{
    auto& W = self->world;
    auto& factory = *(EntityFactory_Renderable3d*)self;
    auto RW = factory.system_render.RW;
    auto& indsInFactory = W->entities_indInFactory;
    for (u32 indInWorld : entitiesIndInWorld) {
        const u32 indInFactory = indsInFactory[indInWorld];
        factory.indsInWorld[indInFactory] = u32(-1); // mark the entries to delete
    }

    { // iterate though the entities and delete the ones that were marked with u32(-1)
        u32 i; // set i to the first entry marked with u32(-1)
        for (i = 0; i < factory.indsInWorld.size(); i++) {
            if (factory.indsInWorld[i] == u32(-1))
                break;
        }
        u32 j; // we use the index j to try to find non-marked entries, then we copy from [j] to [i]
        for (j = i + 1; j < factory.indsInWorld.size(); j++) {
            const u32 indInWorldJ = factory.indsInWorld[j];
            if (indInWorldJ == u32(-1))
                continue;

            W->entities_indInFactory[indInWorldJ] = i;
            factory.indsInWorld[i] = factory.indsInWorld[j];
            factory.components_position3d[i] = factory.components_position3d[j];
            factory.components_rotation3d[i] = factory.components_rotation3d[j];
            factory.components_scale3d[i] = factory.components_scale3d[j];
            factory.components_renderableMesh[i] = factory.components_renderableMesh[j];
            i++;
        }
        factory.indsInWorld.resize(i); // this sucks: I wish there was a SOA vector in C++
        factory.components_position3d.resize(i);
        factory.components_rotation3d.resize(i);
        factory.components_scale3d.resize(i);
        factory.components_renderableMesh.resize(i);
    }

    { // now let's find unused gfxObjects and delete them
        std::vector<u32> useCount(factory.gfxObjects.size(), 0);
        for (auto& rm : factory.components_renderableMesh) {
            rm.instanceInd = useCount[rm.gfxObjectInd]++;
        }

        u32 i; // set i to the first gfxObject with useCount == 0
        for (i = 0; i < useCount.size(); i++) {
            if (useCount[i] == 0) {
                RW.destroyObject(factory.gfxObjects[i]);
                break;
            }
            const bool ok = factory.gfxObjects[i].changeNumInstances(useCount[i]);
            assert(ok);
        }
        std::vector<u32> gfxObjectIndRemapping(factory.gfxObjects.size(), u32(-1));
        u32 j; // we use index j to find gfxObjects with useCount != 0, then we copy from [j] to [i]
        for (j = i + 1; j < useCount.size(); j++) {
            if (useCount[j] == 0) {
                RW.destroyObject(factory.gfxObjects[j]);
                continue;
            }

            gfxObjectIndRemapping[j] = i;
            factory.gfxObjects[i] = factory.gfxObjects[j];
            factory.gfxObjects[i].changeNumInstances(useCount[j]);
            i++;
        }

        // update indices to gfxObjects using the table(gfxObjectIndRemapping)
        for (auto& rm : factory.components_renderableMesh) {
            const u32 newInd = gfxObjectIndRemapping[rm.gfxObjectInd];
            if(newInd != u32(-1))
                rm.gfxObjectInd = newInd;
        }
    }
}

void* EntityFactory_Renderable3d::s_accessComponentByIndFn(EntityFactory* self, u32 entityIndInFactory, u16 componentTypeInd)
{
    auto* factory = (EntityFactory_Renderable3d*)self;
    switch (componentTypeInd) {
        case 0: return &factory->components_position3d[entityIndInFactory];
        case 1: return &factory->components_rotation3d[entityIndInFactory];
        case 2: return &factory->components_scale3d[entityIndInFactory];
        case 3: return &factory->components_renderableMesh[entityIndInFactory];
    }
    assert(false);
    return nullptr;
}

// -- SYSTEMS --
System_Render::System_Render(WorldId worldId)
    : worldId(worldId)
{
    pbrMaterialManager = gfx::PbrMaterialManager::s_getOrCreate();
    RW = gfx::createRenderWorld();
    auto et = worldId->createAndRegisterEntityFactory<EntityFactory_Renderable3d>(*this);
    factory_renderable3d = (EntityFactory_Renderable3d*)worldId->entityFactories[et].get();
}

System_Render::~System_Render()
{
    gfx::destroyRenderWorld(RW);
}

void System_Render::update(float dt)
{
    ZoneScoped;
    for (size_t i = 0; i < factory_renderable3d->components_position3d.size(); i++) {
        const glm::vec3& position = factory_renderable3d->components_position3d[i];
        const glm::quat& rotation = factory_renderable3d->components_rotation3d[i];
        const glm::vec3& scale = factory_renderable3d->components_scale3d[i];

        const auto& rendMeshComp = factory_renderable3d->components_renderableMesh[i];
        const u32 gfxObjectInd = rendMeshComp.gfxObjectInd;
        auto& gfxObject = factory_renderable3d->gfxObjects[gfxObjectInd];
        gfxObject.setModelMatrix(buildMtx(position, rotation, scale), rendMeshComp.instanceInd);
    }

#if 1
    RW.debugGui();
#endif
}

// -- DefaultBasicWorldSystems --

void DefaultBasicWorldSystems::update(float dt)
{
    system_render->update(dt);
}

void DefaultBasicWorldSystems::destroy()
{
    delete system_render;
}

// -- WORLD --
static std::vector<World> g_worlds;
static u16 g_worlds_nextFreeEntry = u16(-1);

World::World()
{
    // entity 0 (invalid)
    _createEntity(0, 0);
    cachedEntityMatrices.push_back(glm::mat4(1.f));
    cachedEntityMatrices_isValid.push_back(true);
    // entity type 0 has no components
    entitiesComponents.push_back({});
}

World::~World()
{
    printf("~World\n");
}

WorldId World::id()const
{
    return { u16(this - g_worlds.data()) };
}

void World::update(float dt)
{
    ZoneScoped;
    if (entitiesToDelete.size()) {
        // 1) delete the entities in the factories
        auto sortedByType = entitiesToDelete;
        std::sort(sortedByType.begin(), sortedByType.end(), [this](u32 a, u32 b) {
            return entities_type[a] < entities_type[b];
        });

        for (u32 i = 0; i < sortedByType.size(); ) {
            const u16 type = entities_type[sortedByType[i]];
            u32 count;
            for (count = 1;
                i + count < sortedByType.size() && type == entities_type[sortedByType[i + count]];
                count++);
            entityFactories[type]->releaseEntities(CSpan<u32>(sortedByType).subspan(i, count));
            i += count;
        }

        // 2) release the entities in the world
        for (u32 ind : entitiesToDelete)
            _breakEntityLinks(ind);
        for (u32 ind : entitiesToDelete)
            _destroyIsolatedEntity(ind);

        entitiesToDelete.clear();
    }

    std::fill(cachedEntityMatrices_isValid.begin(), cachedEntityMatrices_isValid.end(), false);
}

const glm::mat4& World::getMatrix(u32 entityInd)
{
    if (cachedEntityMatrices_isValid[entityInd])
        return cachedEntityMatrices[entityInd];
    
    const u16 entityType = entities_type[entityInd];
    auto* EF = entityFactories[entityType].get();
    glm::vec3 pos = { 0, 0, 0 };
    glm::quat rot = {};
    glm::vec3 scale = {};
    for (u32 i = 0; i < EF->componentTypes.size(); i++) {
        auto componentType = EF->componentTypes[i];
        if (componentType == Component_Position3d::s_type())
            pos = EF->accessComponentByInd<Component_Position3d>(entityInd, i);
        if (componentType == Component_Rotation3d::s_type())
            rot = EF->accessComponentByInd<Component_Rotation3d>(entityInd, i);
        if (componentType == Component_Scale3d::s_type())
            scale = EF->accessComponentByInd<Component_Scale3d>(entityInd, i);
    }

    const u32 parentEntityInd = entities_parent[entityInd];
    glm::mat4 m = glm::toMat4(rot) * glm::scale(m, scale);
    m[3] = glm::vec4(pos, 1);
    m = glm::scale(m, scale);
    if (parentEntityInd)
        m = getMatrix(parentEntityInd) * m;

    cachedEntityMatrices_isValid[entityInd] = true;
    cachedEntityMatrices[entityInd] = m;
    
    return cachedEntityMatrices[entityInd];
}

static void assertEntityIsValidInWorld(EntityId e, const World& W)
{
    assert(e.world == W.id());
    assert(e.counter == W.entities_counter[e.ind]);
    assert(e.type == W.entities_type[e.ind]);
}

EntityId World::_createEntity(EntityTypeU16 entityType, u32 indInFactory, u32 parent)
{
    const u32 e = acquireReusableEntry(entities_nextFreeEntry,
        entities_indInFactory, 0,
        entities_type,
        entities_parent, entities_firstChild, entities_lastChild, entities_nextSibling, entities_prevSibling
#ifndef NDEBUG
        , entities_counter
#endif
    );
    entities_type[e] = entityType;
    entities_indInFactory[e] = indInFactory;

    const EntityId eid = { id(), entityType, e,
#ifndef NDEBUG
        entities_counter[e]
#endif
    };
    setEntityAsLastChildOf(eid, getRootEntity());

    return eid;
}

EntityId World::getRootEntity()const
{
    return EntityId{
        .world = id().ind,
        .type = 0,
        .ind = 0,
    };
}

void World::addEntitiesToDelete(CSpan<u32> entities)
{
    entitiesToDelete.insert(entitiesToDelete.end(), entities.begin(), entities.end());
}

EntityId World::getEntityByInd(u32 ind)const
{
    if (ind == u32(-1))
        return EntityId{ .world = id(), .ind = u32(-1) };

    return EntityId{
        .world = id(),
        .type = entities_type[ind],
        .ind = ind,
#ifndef NDEBUG
        .counter = entities_counter[ind],
#endif
    };
}

EntityId World::getParent(EntityId e)const
{
    assertEntityIsValidInWorld(e, *this);
    return getEntityByInd(entities_parent[e.ind]);
}

EntityId World::firstChild(EntityId e)const
{
    assertEntityIsValidInWorld(e, *this);
    return getEntityByInd(entities_firstChild[e.ind]);
}

EntityId World::nextSibling(EntityId e)const
{
    assertEntityIsValidInWorld(e, *this);
    return getEntityByInd(entities_nextSibling[e.ind]);
}

void World::_destroyIsolatedEntity(u32 indInWorld)
{
#ifndef NDEBUG
    entities_counter[indInWorld]++;
#endif
    releaseReusableEntry(entities_nextFreeEntry, entities_indInFactory, 0, indInWorld);
}

void World::_breakEntityLinks(u32 ei)
{
    const u32 parent = entities_parent[ei];
    const u32 prev = entities_prevSibling[ei];
    const u32 next = entities_nextSibling[ei];
    if (prev != u32(-1)) {
        entities_nextSibling[prev] = next;
        entities_prevSibling[ei] = u32(-1);
    }
    if (next != u32(-1)) {
        entities_prevSibling[next] = prev;
        entities_nextSibling[ei] = u32(-1);
    }
    if (parent != u32(-1)) {
        entities_parent[ei] = u32(-1);
        if (entities_firstChild[parent] == ei)
            entities_firstChild[parent] = next;
    }
}
void World::setEntityAsFirstChildOf(EntityId e, EntityId p)
{
    assertEntityIsValidInWorld(e, *this);
    assertEntityIsValidInWorld(p, *this);
    _breakEntityLinks(e.ind);
    const u32 oldFirst = entities_firstChild[p.ind];
    entities_firstChild[p.ind] = e.ind;
    entities_parent[e.ind] = p.ind;
    if (oldFirst == u32(0)) {
        // if p didn't have children, e will be both the first and the last child
        entities_lastChild[p.ind] = e.ind;
    }
    else {
        entities_nextSibling[e.ind] = oldFirst;
        entities_prevSibling[oldFirst] = e.ind;
    }
}
void World::setEntityAsLastChildOf(EntityId e, EntityId p)
{
    assertEntityIsValidInWorld(e, *this);
    assertEntityIsValidInWorld(p, *this);
    _breakEntityLinks(e.ind);
    const u32 oldLast = entities_lastChild[p.ind];
    entities_parent[e.ind] = p.ind;
    entities_lastChild[p.ind] = e.ind;
    if (oldLast == u32(0)) {
        // if p didn't have children, e will be both the first and the last child
        entities_firstChild[p.ind] = e.ind;
    }
    else {
        entities_nextSibling[oldLast] = e.ind;
        entities_prevSibling[e.ind] = oldLast;
    }
}
void World::setEntityNextSiblingAfter(EntityId e, EntityId s)
{
    assertEntityIsValidInWorld(e, *this);
    assertEntityIsValidInWorld(s, *this);
    _breakEntityLinks(e.ind);
    const u32 next = entities_nextSibling[s.ind];
    const u32 parent = entities_parent[s.ind];
    entities_nextSibling[s.ind] = e.ind;
    entities_prevSibling[e.ind] = s.ind;
    entities_nextSibling[e.ind] = next;
    if (next)
        entities_prevSibling[next] = e.ind;
    else
        entities_lastChild[parent] = e.ind;
    entities_parent[e.ind] = parent;
}

DefaultBasicWorldSystems World::createDefaultBasicSystems()
{
    auto system_render = new System_Render(id());
    EntityTypeU16 entityType_renderable3d = createAndRegisterEntityFactory<EntityFactory_Renderable3d>(*system_render);

    return DefaultBasicWorldSystems {
        system_render
    };
}

WorldId createWorld()
{
    const u16 slot = tk::acquireReusableEntry(g_worlds_nextFreeEntry, g_worlds, offsetof(World, entities_nextFreeEntry));
    return WorldId{ slot };
}

void destroyWorld(WorldId worldId)
{
    auto& W = g_worlds[worldId.ind];
    W = {};
    tk::releaseReusableEntry(g_worlds_nextFreeEntry, g_worlds, offsetof(World, entities_nextFreeEntry), worldId.ind);
}

World* WorldId::operator->()
{
    return &g_worlds[ind];
}
const World* WorldId::operator->()const
{
    return &g_worlds[ind];
}

// EntityId
bool EntityId::valid()const
{
    return ind != u32(-1);
}
EntityId EntityId::parent()const
{
    return world->getParent(*this);
}
EntityId EntityId::firstChild()const
{
    return world->firstChild(*this);
}
EntityId EntityId::nextSibling()const
{
    return world->nextSibling(*this);
}

void EntityId::destroy()
{
    world->addEntitiesToDelete({ &ind, 1 });
}

// --- PROJECT ---
#if 0
Project::Project()
{
    // root folder
    files_name.push_back("");
    files_type.push_back(FileType::folder);
    files_parent.push_back(u32(-1));
    files_firstChild.push_back(u32(-1));
    files_nextSibling.push_back(u32(-1));
}

Project::File Project::firstChild(File folder)
{
    assert(isFolder(folder));
    return File{files_firstChild[folder.id]};
}

Project::FileType Project::getType(File file)
{
    return files_type[file.id];
}

bool Project::isFolder(File file)
{
    return files_type[file.id] == FileType::folder;
}

Project::File Project::nextSibling(File fileOrFolder)
{
    return File{ files_nextSibling[fileOrFolder.id] };
}

Project::File Project::findChildWithName(File folder, std::string_view name)
{
    for (auto child = firstChild(folder); child.isValid(); child = nextSibling(child)) {
        if (files_name[child.id] == name)
            return child;
    }
    return File{};
}

Project::File Project::addChildFile(File folder, std::string name, FileType type)
{
    assert(isFolder(folder));
    assert(!findChildWithName(folder, name).isValid());
    const File file = _allocFileH();
    files_name[file.id] = std::move(name);
    files_type[file.id] = type;
    files_parent[file.id] = folder.id;
    files_nextSibling[file.id] = files_firstChild[folder.id];
    files_firstChild[folder.id] = file.id;
    if (type == FileType::geom) {

    }
    return file;
}

void Project::setFileName(File fileOrFolder, std::string name)
{
    files_name[fileOrFolder.id] = std::move(name);
}

void Project::deleteFile(File file)
{
    const FileType type = files_type[file.id];
    assert(!isFolder(file));
    if (type == FileType::geom) {

    }
    //else if (type == FileType::) {}
    _releaseFileH(file);
}

void Project::deleteFolder(File folder, bool recursive)
{
    assert(isFolder(folder));
    if (recursive) {
        for (auto child = firstChild(folder); child.isValid(); child = nextSibling(child)) {
            if (isFolder(child))
                deleteFolder(child, true);
            else
                deleteFile(child);
        }
    }

    _releaseFileH(folder);
}

void Project::deleteFileOrFolder(File fileOrFolder, bool recursive)
{
    if (isFolder(fileOrFolder))
        deleteFolder(fileOrFolder);
    else
        deleteFile(fileOrFolder);
}

void Project::setGeomFile(File geomFile, CSpan<u8> data, const tg::GeomInfo& info)
{
    assert(getType(geomFile) == FileType::geom);
    const u32 ind = toSpecificInd[geomFile.id];
    geoms_data[ind].assign(data.begin(), data.end());
    geoms_info[ind] = info;
}

void Project::setGeomFile(File geomFile, tg::CreateGeomInfo createInfo)
{
#if 0
    assert(getType(geomFile) == FileType::geom);
    const u32 ind = toSpecificInd[geomFile.id];
    u32 offset = 0;
    geoms_data[ind].assign(createInfo.positions.begin(), createInfo.positions.end());
    if(createInfo.normals.size()) {
        geoms_info[ind].attribOffset_normals = offset;
        geoms_data[ind].insert(geoms_data[ind].begin(), createInfo.normals.begin(), createInfo.normals.end());
        offset += createInfo.normals.size();
    }
    if (createInfo.tangents.size()) {
        geoms_info[ind].attribOffset_tangents = offset;
        geoms_data[ind].insert(geoms_data[ind].begin(), createInfo.tangents.begin(), createInfo.tangents.end());
        offset += createInfo.tangents.size();
    }
    if (createInfo.texCoords.size()) {
        geoms_info[ind].attribOffset_texCoords = offset;
        geoms_data[ind].insert(geoms_data[ind].begin(), createInfo.texCoords.begin(), createInfo.texCoords.end());
        offset += createInfo.texCoords.size();
    }
    if (createInfo.colors.size()) {
        geoms_info[ind].attribOffset_colors = offset;
        geoms_data[ind].insert(geoms_data[ind].begin(), createInfo.colors.begin(), createInfo.colors.end());
        offset += createInfo.colors.size();
    }
    if (createInfo.indices.size()) {
        geoms_info[ind].attribOffset_indices = offset;
        geoms_data[ind].insert(geoms_data[ind].begin(), createInfo.indices.begin(), createInfo.indices.end());
        offset += createInfo.indices.size();
    }
    geoms_data[ind].assign(createInfo.positions.begin(), data.end());
    geoms_info[ind] = info;
#endif
}

Project::File Project::_allocFileH()
{
    if (files_nextFreeEntry == u32(-1)) {
        const File file{files_name.size()};
        files_name.emplace_back();
        files_type.emplace_back();
        files_parent.emplace_back();
        files_firstChild.emplace_back();
        files_nextSibling.emplace_back();
        return file;
    }
    else {
        const File file{ files_nextFreeEntry };
        files_nextFreeEntry = files_nextSibling[files_nextFreeEntry];
        return file;
    }
}

void Project::_releaseFileH(File file)
{
    assert(file.isValid());
    files_name[file.id] = {}; // makes debugging easier and might help release string memory
    files_nextSibling[file.id] = files_nextFreeEntry;
    files_nextFreeEntry = file.id;
}
#endif

}