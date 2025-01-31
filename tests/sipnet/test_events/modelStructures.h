//
// DO NOT ADD CUSTOM CODE TO THIS FILE (other than #define overrides for unit tests)
// IT MAY BE OVERWRITTEN BY update_model_structures.sh
//
// This file is intended to hold settings for different model structure options,
// implemented as compile-time flags. The options are here (with nothing else) to
// improve testability.
//
// If this file is changed, consider running tests/update_model_structures.sh to update
// the corresponding unit test versions of this file.

#ifndef MODEL_STRUCTURES_H
#define MODEL_STRUCTURES_H

// Begin definitions for choosing different model structures
// See also sipnet.c for other options (that should be moved here if/when testing is added)

#define EVENT_HANDLER 1
// Read in and process agronomic events. SIPNET expects a file named events.in to exist, though
// unit tests may use other names.

// have extra litter pool, in addition to soil c pool
#define LITTER_POOL 0

// Whether we model root dynamics
#define ROOTS 1

#endif //MODEL_STRUCTURES_H
