#include "tk.hpp"

namespace tk {

RenderWorld* g_renderWorld = nullptr;
RenderWorld* initRenderWorld()
{
    g_renderWorld = new RenderWorld();
    return g_renderWorld;
}

}