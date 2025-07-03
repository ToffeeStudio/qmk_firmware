#include "anim_solid.h"

// No state needed
typedef struct {} solid_state_t;
static solid_state_t state_instance;

static HSV solid_task(animation_params_t* params) {
    // This animation is very simple. It just returns the
    // currently configured color without any changes.
    return params->config->color;
}

animation_t anim_solid = {
    .name       = "Solid Color",
    .init       = NULL,
    .task       = solid_task,
    .state      = &state_instance,
    .state_size = sizeof(solid_state_t),
};
