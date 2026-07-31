#pragma once
#include <cstdint>

// CMD Colors
extern const char* CMD_TEXT_WHITE;
extern const char* CMD_TEXT_RED;
extern const char* CMD_TEXT_GREEN;
extern const char* CMD_TEXT_BLUE;
extern const char* CMD_TEXT_YELLOW;
extern const char* CMD_TEXT_GRAY;
extern const char* CMD_TERM_INFO;
extern const char* CMD_TERM_WARN;
extern const char* CMD_TERM_ERROR;

// BASIC
inline constexpr uint32_t COLOR_BLACK               = 0xFF000000;
inline constexpr uint32_t COLOR_WHITE               = 0xFFFFFFFF;
inline constexpr uint32_t COLOR_RED                 = 0xFFFF5555;
inline constexpr uint32_t COLOR_GREEN               = 0xFF59E81B;
inline constexpr uint32_t COLOR_BLUE                = 0xFF59AEED;
inline constexpr uint32_t COLOR_YELLOW              = 0xFFFFD966;
inline constexpr uint32_t COLOR_GRAY                = 0xFFAAAAAA;
inline constexpr uint32_t COLOR_DARK_GRAY           = 0xFF555555;
inline constexpr uint32_t COLOR_LIGHT_GRAY          = 0xFFCCCCCC;

// NasuaOS UI CORE 
inline constexpr uint32_t COLOR_NASUA_BG            = 0xFF121826;
inline constexpr uint32_t COLOR_NASUA_TASKBAR       = 0xFF010102;
inline constexpr uint32_t COLOR_NASUA_START_MENU    = 0xFF1A1A24;
inline constexpr uint32_t COLOR_NASUA_START_MENU_P  = 0xFF101017;
inline constexpr uint32_t COLOR_NASUA_DARK          = 0xFF1C242F;
inline constexpr uint32_t COLOR_NASUA_DARKER        = 0xFF161E29;
inline constexpr uint32_t COLOR_NASUA_LIGHT         = 0xFFE0E0E0;
inline constexpr uint32_t COLOR_NASUA_HIGHLIGHT     = 0xFFFFFFFF;
inline constexpr uint32_t COLOR_MOUSE               = 0xFFFFFFFF;

// WINDOW ELEMENTS 
inline constexpr uint32_t COLOR_TITLEBAR          = 0xFF2B3140;
inline constexpr uint32_t COLOR_TITLEBAR_INACTIVE = 0xFF333A4A;
inline constexpr uint32_t COLOR_WINDOW            = 0xFF1A1F2C;
inline constexpr uint32_t COLOR_BORDER            = 0xFF2D3748;
inline constexpr uint32_t COLOR_BUTTON_FACE       = 0xFF262D3D;
inline constexpr uint32_t COLOR_BUTTON_SHADOW     = 0xFF11141C;
inline constexpr uint32_t COLOR_BUTTON_HL         = 0xFF3B445B;

// TERMINAL COLORS 
inline constexpr uint32_t COLOR_TERM_INFO           = 0xFF7EC2D6;
inline constexpr uint32_t COLOR_TERM_WARN           = 0xFFEBCB8B;
inline constexpr uint32_t COLOR_TERM_ERROR          = 0xFFBF616A;

// SHADOWS / DEPTH 
inline constexpr uint32_t COLOR_SHADOW              = 0xFF1C242F;
inline constexpr uint32_t COLOR_HARD_SHADOW         = 0xFF161E29;