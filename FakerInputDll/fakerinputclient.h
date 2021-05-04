#ifndef FAKERINPUTCLIENT_H
#define FAKERINPUTCLIENT_H

#include "fakerinputcommon.h"

typedef struct _fakerinput_client_t* pfakerinput_client;

#ifdef FAKERINPUTLIB_EXPORTS
#define FAKERINPUTLIB_API __declspec(dllexport)
#else
#define FAKERINPUTLIB_API __declspec(dllimport)
#endif

EXTERN_C_START

    FAKERINPUTLIB_API pfakerinput_client fakerinput_alloc();

    FAKERINPUTLIB_API void fakerinput_free(pfakerinput_client clientHandle);

    FAKERINPUTLIB_API bool fakerinput_connect(pfakerinput_client clientHandle);

    FAKERINPUTLIB_API void fakerinput_disconnect(pfakerinput_client clientHandle);

    FAKERINPUTLIB_API bool fakerinput_update_keyboard(pfakerinput_client clientHandle, BYTE shiftKeyFlags, BYTE keyCodes[KBD_KEY_CODES]);

    FAKERINPUTLIB_API bool fakerinput_update_keyboard_enhanced(pfakerinput_client clientHandle, BYTE multiKeys, BYTE extraKeys);

    FAKERINPUTLIB_API bool fakerinput_update_relative_mouse(pfakerinput_client clientHandle, BYTE button,
        SHORT x, SHORT y, BYTE wheelPosition, BYTE hWheelPosition);

EXTERN_C_END

#endif // FAKERINPUTCLIENT_H