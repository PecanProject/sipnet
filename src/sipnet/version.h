#ifndef SIPNET_VERSION_H
#define SIPNET_VERSION_H

#define NUMERIC_VERSION "2.0.0"

// To enable, compile with `-DGIT_HASH="<your revision here>"`
#ifdef GIT_HASH
#define str(x) #x
#define xstr(x) str(x)  // bah, macro string expansion rules
#define VERSION_STRING NUMERIC_VERSION " (" xstr(GIT_HASH) ")"
#else
#define VERSION_STRING NUMERIC_VERSION
#endif  // GIT_HASH

#endif  // SIPNET_VERSION_H
