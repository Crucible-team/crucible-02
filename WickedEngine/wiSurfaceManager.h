#pragma once
#include "wiSurface.h"
#include "wiVector.h"
#include "wiUnorderedMap.h"
#include <string>
#include <cassert> // For assert in Get()

namespace wi
{
    class SurfaceManager
    {
    private:
        static SurfaceManager* s_instance; // Pointer to the single instance
        SurfaceManager() = default; // Private constructor
        ~SurfaceManager(); // Private destructor

        wi::vector<Surface> surfaces;
        wi::unordered_map<std::string, int> surface_map;

    public:
        // No public constructor or copy constructor/assignment operator
        SurfaceManager(const SurfaceManager&) = delete;
        SurfaceManager& operator=(const SurfaceManager&) = delete;

        static SurfaceManager& Get(); // Accessor for the instance
        static void Create(); // Explicit creation
        static void Destroy(); // Explicit destruction

        // Load surface properties from a Lua script
        void LoadSurfaces(const std::string& filename);

        // Clears all loaded surfaces and their associated resources
        void Clear();

        // Get a surface by its index
        const Surface* GetSurface(int index) const;

        // Get a surface index by its name
        int GetSurfaceIndex(const std::string& name) const;

        // Get the total number of surfaces
        size_t GetSurfaceCount() const;
    };
}
