//
// This file is inteneded to hold settings for different model structure options,
// implemented as compile-time flags. The options are here (with nothing else) to
// improve testability.
//

#ifndef MODEL_STRUCTURES_H
#define MODEL_STRUCTURES_H

// Begin definitions for choosing different model structures
// See also sipnet.c for other options (that should be moved here if/when testing is added)

#define EVENT_HANDLER 0
// Read in and process agronomic events. Expects a file named <FILENAME>.event to exist.


#endif //MODEL_STRUCTURES_H
