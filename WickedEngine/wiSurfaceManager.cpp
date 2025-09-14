#include "wiSurfaceManager.h"
#include "wiHelper.h"
#include "wiScene.h"

extern "C" {
#include "LUA/lua.h"
#include "LUA/lualib.h"
#include "LUA/lauxlib.h"
}

// This is an assumption, if it fails, I will need to find where these functions are defined.
#include "wiLua.h" 
#include "wiBacklog.h"

namespace wi
{

    SurfaceManager* SurfaceManager::s_instance = nullptr; // Initialize static pointer

    void SurfaceManager::Create()
    {
        if (s_instance == nullptr)
        {
            s_instance = new SurfaceManager();
        }
    }

    void SurfaceManager::Destroy()
    {
        if (s_instance != nullptr)
        {
            delete s_instance;
            s_instance = nullptr;
        }
    }

    SurfaceManager& SurfaceManager::Get()
    {
        assert(s_instance != nullptr && "SurfaceManager not created! Call SurfaceManager::Create() first.");
        return *s_instance;
    }

    SurfaceManager::~SurfaceManager()
    {
        // Clear resources here
        surfaces.clear();
        surface_map.clear();
    }

    void SurfaceManager::LoadSurfaces(const std::string& filename)
    {
        // Clear old data
        surfaces.clear();
        surface_map.clear();

        // Create a default surface
        Surface default_surface;
        default_surface.name = "default";
        surfaces.push_back(default_surface);
        surface_map[default_surface.name] = 0;

        // Load the Lua script
        if (!wi::lua::RunFile(filename))
        {
            // Don't log error if file simply doesn't exist
            if (wi::helper::FileExists(filename))
            {
                wi::backlog::post("Failed to load surfaces script: " + filename, wi::backlog::LogLevel::Error);
            }
            return;
        }
        lua_State* L = wi::lua::GetLuaState();

        // Get the global 'surfaces' table
        lua_getglobal(L, "surfaces");
        if (lua_istable(L, -1))
        {
            // Iterate through the table
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_istable(L, -1))
                {
                    Surface surface;

                    // Get name
                    lua_pushstring(L, "name");
                    lua_gettable(L, -2);
                    if (lua_isstring(L, -1))
                    {
                        surface.name = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);

                    // Get friction
                    lua_pushstring(L, "friction");
                    lua_gettable(L, -2);
                    if (lua_isnumber(L, -1))
                    {
                        surface.friction = (float)lua_tonumber(L, -1);
                    }
                    lua_pop(L, 1);

                    // Get density
                    lua_pushstring(L, "density");
                    lua_gettable(L, -2);
                    if (lua_isnumber(L, -1))
                    {
                        surface.density = (float)lua_tonumber(L, -1);
                    }
                    lua_pop(L, 1);

                    // Get restitution
                    lua_pushstring(L, "restitution");
                    lua_gettable(L, -2);
                    if (lua_isnumber(L, -1))
                    {
                        surface.restitution = (float)lua_tonumber(L, -1);
                    }
                    lua_pop(L, 1);

                    // Get impact_effect_path
                    lua_pushstring(L, "impact_effect_path");
                    lua_gettable(L, -2);
                    if (lua_isstring(L, -1))
                    {
                        const char* impact_effect_path = lua_tostring(L, -1);
                        surface.impact_effect_prefab = std::make_shared<wi::scene::Scene>();
                        wi::scene::LoadModel(*surface.impact_effect_prefab, impact_effect_path);
                    }
                    lua_pop(L, 1);

                    // Get impact_sound
                    lua_pushstring(L, "impact_sound");
                    lua_gettable(L, -2);
                    if (lua_isstring(L, -1))
                    {
                        const char* impact_sound_path = lua_tostring(L, -1);
                        wi::audio::CreateSound(impact_sound_path, &surface.impact_sound);
                    }
                    lua_pop(L, 1);

                    // Add the surface
                    if (!surface.name.empty())
                    {
                        // If a surface with this name already exists, overwrite it
                        auto it = surface_map.find(surface.name);
                        if (it != surface_map.end())
                        {
                            surfaces[it->second] = surface;
                        }
                        else
                        {
                            int index = (int)surfaces.size();
                            surfaces.push_back(surface);
                            surface_map[surface.name] = index;
                        }
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1); // pop the 'surfaces' table
    }

    const Surface* SurfaceManager::GetSurface(int index) const
    {
        if (index >= 0 && index < (int)surfaces.size())
        {
            return &surfaces[index];
        }
        // Always return at least the default surface
        return &surfaces[0];
    }

    int SurfaceManager::GetSurfaceIndex(const std::string& name) const
    {
        auto it = surface_map.find(name);
        if (it != surface_map.end())
        {
            return it->second;
        }
        return -1;
    }

    size_t SurfaceManager::GetSurfaceCount() const
    {
        return surfaces.size();
    }

} // namespace wi
