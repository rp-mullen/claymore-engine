#pragma once

// GLFW-compatible key and mouse/button/action constants to preserve public API
// Only a commonly used subset is defined; extend as needed.

// Actions
#ifndef GLFW_PRESS
#define GLFW_PRESS   1
#endif
#ifndef GLFW_RELEASE
#define GLFW_RELEASE 0
#endif
#ifndef GLFW_REPEAT
#define GLFW_REPEAT  2
#endif

// Mouse buttons
#ifndef GLFW_MOUSE_BUTTON_LEFT
#define GLFW_MOUSE_BUTTON_LEFT   0
#endif
#ifndef GLFW_MOUSE_BUTTON_RIGHT
#define GLFW_MOUSE_BUTTON_RIGHT  1
#endif
#ifndef GLFW_MOUSE_BUTTON_MIDDLE
#define GLFW_MOUSE_BUTTON_MIDDLE 2
#endif

// Printable keys (match GLFW values)
#ifndef GLFW_KEY_SPACE
#define GLFW_KEY_SPACE              32
#endif
#ifndef GLFW_KEY_APOSTROPHE
#define GLFW_KEY_APOSTROPHE         39
#endif
#ifndef GLFW_KEY_COMMA
#define GLFW_KEY_COMMA              44
#endif
#ifndef GLFW_KEY_MINUS
#define GLFW_KEY_MINUS              45
#endif
#ifndef GLFW_KEY_PERIOD
#define GLFW_KEY_PERIOD             46
#endif
#ifndef GLFW_KEY_SLASH
#define GLFW_KEY_SLASH              47
#endif
#ifndef GLFW_KEY_0
#define GLFW_KEY_0                  48
#endif
#ifndef GLFW_KEY_1
#define GLFW_KEY_1                  49
#endif
#ifndef GLFW_KEY_2
#define GLFW_KEY_2                  50
#endif
#ifndef GLFW_KEY_3
#define GLFW_KEY_3                  51
#endif
#ifndef GLFW_KEY_4
#define GLFW_KEY_4                  52
#endif
#ifndef GLFW_KEY_5
#define GLFW_KEY_5                  53
#endif
#ifndef GLFW_KEY_6
#define GLFW_KEY_6                  54
#endif
#ifndef GLFW_KEY_7
#define GLFW_KEY_7                  55
#endif
#ifndef GLFW_KEY_8
#define GLFW_KEY_8                  56
#endif
#ifndef GLFW_KEY_9
#define GLFW_KEY_9                  57
#endif
#ifndef GLFW_KEY_SEMICOLON
#define GLFW_KEY_SEMICOLON          59
#endif
#ifndef GLFW_KEY_EQUAL
#define GLFW_KEY_EQUAL              61
#endif
#ifndef GLFW_KEY_A
#define GLFW_KEY_A                  65
#endif
#ifndef GLFW_KEY_B
#define GLFW_KEY_B                  66
#endif
#ifndef GLFW_KEY_C
#define GLFW_KEY_C                  67
#endif
#ifndef GLFW_KEY_D
#define GLFW_KEY_D                  68
#endif
#ifndef GLFW_KEY_E
#define GLFW_KEY_E                  69
#endif
#ifndef GLFW_KEY_F
#define GLFW_KEY_F                  70
#endif
#ifndef GLFW_KEY_G
#define GLFW_KEY_G                  71
#endif
#ifndef GLFW_KEY_H
#define GLFW_KEY_H                  72
#endif
#ifndef GLFW_KEY_I
#define GLFW_KEY_I                  73
#endif
#ifndef GLFW_KEY_J
#define GLFW_KEY_J                  74
#endif
#ifndef GLFW_KEY_K
#define GLFW_KEY_K                  75
#endif
#ifndef GLFW_KEY_L
#define GLFW_KEY_L                  76
#endif
#ifndef GLFW_KEY_M
#define GLFW_KEY_M                  77
#endif
#ifndef GLFW_KEY_N
#define GLFW_KEY_N                  78
#endif
#ifndef GLFW_KEY_O
#define GLFW_KEY_O                  79
#endif
#ifndef GLFW_KEY_P
#define GLFW_KEY_P                  80
#endif
#ifndef GLFW_KEY_Q
#define GLFW_KEY_Q                  81
#endif
#ifndef GLFW_KEY_R
#define GLFW_KEY_R                  82
#endif
#ifndef GLFW_KEY_S
#define GLFW_KEY_S                  83
#endif
#ifndef GLFW_KEY_T
#define GLFW_KEY_T                  84
#endif
#ifndef GLFW_KEY_U
#define GLFW_KEY_U                  85
#endif
#ifndef GLFW_KEY_V
#define GLFW_KEY_V                  86
#endif
#ifndef GLFW_KEY_W
#define GLFW_KEY_W                  87
#endif
#ifndef GLFW_KEY_X
#define GLFW_KEY_X                  88
#endif
#ifndef GLFW_KEY_Y
#define GLFW_KEY_Y                  89
#endif
#ifndef GLFW_KEY_Z
#define GLFW_KEY_Z                  90
#endif

// Function keys (subset)
#ifndef GLFW_KEY_ESCAPE
#define GLFW_KEY_ESCAPE             256
#endif
#ifndef GLFW_KEY_ENTER
#define GLFW_KEY_ENTER              257
#endif
#ifndef GLFW_KEY_TAB
#define GLFW_KEY_TAB                258
#endif
#ifndef GLFW_KEY_BACKSPACE
#define GLFW_KEY_BACKSPACE          259
#endif
#ifndef GLFW_KEY_INSERT
#define GLFW_KEY_INSERT             260
#endif
#ifndef GLFW_KEY_DELETE
#define GLFW_KEY_DELETE             261
#endif
#ifndef GLFW_KEY_RIGHT
#define GLFW_KEY_RIGHT              262
#endif
#ifndef GLFW_KEY_LEFT
#define GLFW_KEY_LEFT               263
#endif
#ifndef GLFW_KEY_DOWN
#define GLFW_KEY_DOWN               264
#endif
#ifndef GLFW_KEY_UP
#define GLFW_KEY_UP                 265
#endif

// Modifiers (no direct values needed here; ImGui handles mods)


