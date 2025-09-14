#pragma once
#include "CommonInclude.h"
#include "wiScene_decl.h"
#include "wiAudio.h"
#include <memory>

namespace wi
{
    struct Surface
    {
        std::string name;

        // Physics properties
        float friction = 0.5f;
        float density = 1.0f;
        float restitution = 0.1f; // bounciness

        // Sound properties
        wi::audio::Sound impact_sound;

        // Effect properties (prefab scene)
        std::shared_ptr<wi::scene::Scene> impact_effect_prefab;

        // Destructibility
        float health = 0; // 0 for indestructible
    };
}
