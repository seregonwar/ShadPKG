#pragma once

// FontAwesome 6 Free Solid Codepoints
#define ICON_FA_FOLDER "\xef\x81\xbb"               // U+F07B
#define ICON_FA_FOLDER_OPEN "\xef\x81\xbc"          // U+F07C
#define ICON_FA_FILE "\xef\x85\x9b"                 // U+F15B
#define ICON_FA_FILE_ZIPPER "\xef\x87\x86"          // U+F1C6
#define ICON_FA_SEARCH "\xef\x80\x82"               // U+F002
#define ICON_FA_GEAR "\xef\x80\x93"                 // U+F013
#define ICON_FA_DOWNLOAD "\xef\x80\x99"             // U+F019
#define ICON_FA_CHECK "\xef\x80\x8c"                // U+F00C
#define ICON_FA_XMARK "\xef\x80\x8d"                // U+F00D
#define ICON_FA_INFO_CIRCLE "\xef\x81\x9a"          // U+F05A
#define ICON_FA_TRIANGLE_EXCLAMATION "\xef\x81\xb1" // U+F071
#define ICON_FA_CIRCLE_XMARK "\xef\x81\x97"         // U+F057
#define ICON_FA_KEY "\xef\x82\x84"                  // U+F084
#define ICON_FA_IMAGE "\xef\x80\xbe"                // U+F03E
#define ICON_FA_BOX_ARCHIVE "\xef\x86\x87"          // U+F187
#define ICON_FA_BOX_ARCHIVE "\xef\x86\x87"          // U+F187
#define ICON_FA_TERMINAL "\xef\x84\xa0"             // U+F120
#define ICON_FA_USER "\xef\x80\x87"                 // U+F007
#define ICON_FA_GITHUB "\xef\x82\x9b"               // U+F09B
#define ICON_FA_HEART "\xef\x80\x84"                // U+F004
#define ICON_FA_MUG_HOT "\xef\x9e\xb6"              // U+F7B6

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  FontAwesome 6 uses TWO separate Unicode ranges:                         │
// │  - Legacy range: 0xF000 - 0xF8FF (traditional FA icons)                  │
// │  - New FA6 range: 0xE000 - 0xE6FF (new/moved icons)                      │
// └─────────────────────────────────────────────────────────────────────────┘
#define ICON_MIN_FA 0xe000 // Start of FA6 new icons range
#define ICON_MAX_FA 0xf8ff // End of legacy range (covers both)
