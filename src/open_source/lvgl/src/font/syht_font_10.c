/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font bb4171.ttf --symbols 报警器状态:开关火焰传感器模式选择用户控制智能触发类型阈值大于小于执行打闭未连接成功名称密码服务器地址端口号设备锁定取消%有无手动一二三档风扇：温度湿强℃灯光照QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm1234567890！~、|\/,.?<>_-+=  --format lvgl -o syht_font_10.c
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "../lvgl.h"
#endif



#ifndef SYHT_FONT_10
#define SYHT_FONT_10 1
#endif

#if SYHT_FONT_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0025 "%" */
    0x61, 0x12, 0x22, 0x48, 0x49, 0x66, 0x52, 0x12,
    0x42, 0x48, 0x89, 0x10, 0xc0,

    /* U+002B "+" */
    0x30, 0xcf, 0xcc, 0x30, 0xc0,

    /* U+002C "," */
    0xf7, 0x0,

    /* U+002D "-" */
    0xe0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x11, 0x12, 0x22, 0x24, 0x44, 0x48,

    /* U+0030 "0" */
    0x79, 0x2c, 0xf3, 0xcf, 0x3c, 0xd2, 0x78,

    /* U+0031 "1" */
    0x77, 0x8c, 0x63, 0x18, 0xc6, 0xf8,

    /* U+0032 "2" */
    0x7a, 0x30, 0xc3, 0x1c, 0x63, 0x18, 0xfc,

    /* U+0033 "3" */
    0x79, 0x30, 0xc3, 0x30, 0x30, 0xe3, 0xf8,

    /* U+0034 "4" */
    0x1c, 0xf2, 0xdb, 0xcf, 0xf0, 0xc3, 0xc,

    /* U+0035 "5" */
    0x7d, 0x4, 0x1e, 0x4c, 0x30, 0xd3, 0x78,

    /* U+0036 "6" */
    0x39, 0x8c, 0x3e, 0xce, 0x3c, 0xd3, 0x38,

    /* U+0037 "7" */
    0xfc, 0x31, 0x86, 0x10, 0xc3, 0xc, 0x30,

    /* U+0038 "8" */
    0x7b, 0x3c, 0xdb, 0x3b, 0x7c, 0xf3, 0x78,

    /* U+0039 "9" */
    0x73, 0x2c, 0x73, 0x7c, 0x30, 0xc6, 0x70,

    /* U+003A ":" */
    0xf0, 0x3c,

    /* U+003C "<" */
    0x0, 0x77, 0x30, 0x78, 0x30,

    /* U+003D "=" */
    0xfc, 0x0, 0x3f,

    /* U+003E ">" */
    0x3, 0x83, 0x83, 0x7b, 0x0,

    /* U+003F "?" */
    0xf4, 0xc6, 0x62, 0x30, 0xc, 0x60,

    /* U+0041 "A" */
    0x18, 0x38, 0x2c, 0x2c, 0x64, 0x7e, 0x46, 0xc2,
    0xc3,

    /* U+0042 "B" */
    0xf9, 0x9b, 0x36, 0x6f, 0x98, 0xf1, 0xe3, 0xfc,

    /* U+0043 "C" */
    0x3e, 0xc3, 0x6, 0xc, 0x18, 0x30, 0x31, 0x3c,

    /* U+0044 "D" */
    0xf9, 0x9b, 0x1e, 0x3c, 0x78, 0xf1, 0xe6, 0xf8,

    /* U+0045 "E" */
    0xff, 0xc, 0x30, 0xfb, 0xc, 0x30, 0xfc,

    /* U+0046 "F" */
    0xff, 0xc, 0x30, 0xfb, 0xc, 0x30, 0xc0,

    /* U+0047 "G" */
    0x3c, 0xcb, 0x6, 0xc, 0xf8, 0xf1, 0xb3, 0x3e,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3f, 0xf8, 0xf1, 0xe3, 0xc6,

    /* U+0049 "I" */
    0xff, 0xff, 0xc0,

    /* U+004A "J" */
    0xc, 0x30, 0xc3, 0xc, 0x30, 0xd3, 0x78,

    /* U+004B "K" */
    0xc7, 0x9b, 0x67, 0x8f, 0x9f, 0x33, 0x63, 0xc6,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xfc,

    /* U+004D "M" */
    0xc3, 0xe7, 0xe7, 0xe7, 0xff, 0xdb, 0xdb, 0xdb,
    0xc3,

    /* U+004E "N" */
    0xc7, 0xcf, 0x9f, 0xbd, 0x7b, 0xf3, 0xe7, 0xc6,

    /* U+004F "O" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x66,
    0x3c,

    /* U+0050 "P" */
    0xfd, 0x8f, 0x1e, 0x3c, 0xff, 0xb0, 0x60, 0xc0,

    /* U+0051 "Q" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x67,
    0x7e, 0x1c, 0xc, 0x7,

    /* U+0052 "R" */
    0xfd, 0x8f, 0x1e, 0x3f, 0xdb, 0x33, 0x67, 0xc6,

    /* U+0053 "S" */
    0x7b, 0x2c, 0x3c, 0x78, 0xf0, 0xe3, 0xf8,

    /* U+0054 "T" */
    0xfe, 0x60, 0xc1, 0x83, 0x6, 0xc, 0x18, 0x30,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xa6, 0x78,

    /* U+0056 "V" */
    0xc3, 0x8d, 0x1b, 0x26, 0xc5, 0x8f, 0x1c, 0x38,

    /* U+0057 "W" */
    0xc4, 0x69, 0xc9, 0x29, 0x35, 0x66, 0xac, 0xd7,
    0x9e, 0xf1, 0x8c, 0x31, 0x80,

    /* U+0058 "X" */
    0x66, 0xc8, 0xb1, 0xc1, 0x87, 0x1b, 0x33, 0xc6,

    /* U+0059 "Y" */
    0x42, 0x66, 0x26, 0x3c, 0x3c, 0x18, 0x18, 0x18,
    0x18,

    /* U+005A "Z" */
    0xfc, 0x31, 0x8c, 0x31, 0x86, 0x30, 0xfc,

    /* U+005C "\\" */
    0xc4, 0x44, 0x62, 0x22, 0x31, 0x11,

    /* U+005F "_" */
    0xfc,

    /* U+0061 "a" */
    0x78, 0x31, 0xdf, 0xcf, 0x37, 0xc0,

    /* U+0062 "b" */
    0xc3, 0xc, 0x3e, 0xcf, 0x3c, 0xf3, 0xcf, 0xe0,

    /* U+0063 "c" */
    0x3d, 0x8c, 0x30, 0xc3, 0x87, 0xc0,

    /* U+0064 "d" */
    0xc, 0x30, 0xdf, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+0065 "e" */
    0x3c, 0xcd, 0x1f, 0xf4, 0xc, 0xf, 0x80,

    /* U+0066 "f" */
    0x3b, 0x19, 0xe6, 0x31, 0x8c, 0x63, 0x0,

    /* U+0067 "g" */
    0x7f, 0x9b, 0x36, 0x67, 0x98, 0x1f, 0x63, 0xc6,
    0xf8,

    /* U+0068 "h" */
    0xc3, 0xc, 0x3e, 0xcf, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+0069 "i" */
    0xf3, 0xff, 0xf0,

    /* U+006A "j" */
    0x33, 0x3, 0x33, 0x33, 0x33, 0x33, 0x60,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0x6d, 0x9e, 0x3c, 0x7c, 0xcd,
    0x98,

    /* U+006C "l" */
    0xdb, 0x6d, 0xb6, 0xdc,

    /* U+006D "m" */
    0xff, 0xb3, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcc,

    /* U+006E "n" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3c, 0xc0,

    /* U+006F "o" */
    0x38, 0xdb, 0x1e, 0x3c, 0x6d, 0x8e, 0x0,

    /* U+0070 "p" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3f, 0xb0, 0xc3, 0x0,

    /* U+0071 "q" */
    0x7f, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0xc, 0x30,

    /* U+0072 "r" */
    0xf7, 0x31, 0x8c, 0x63, 0x0,

    /* U+0073 "s" */
    0x7e, 0x38, 0xf3, 0xcf, 0xc0,

    /* U+0074 "t" */
    0x66, 0xf6, 0x66, 0x66, 0x30,

    /* U+0075 "u" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0xc0,

    /* U+0076 "v" */
    0xc6, 0x89, 0xb3, 0x62, 0x85, 0xe, 0x0,

    /* U+0077 "w" */
    0xcc, 0xd3, 0x36, 0xe9, 0xba, 0x7b, 0x8c, 0xe3,
    0x30,

    /* U+0078 "x" */
    0x4c, 0xd8, 0xe1, 0x83, 0x8d, 0xb3, 0x0,

    /* U+0079 "y" */
    0xc6, 0x89, 0xb3, 0x62, 0x87, 0xe, 0x8, 0x30,
    0xc0,

    /* U+007A "z" */
    0xf9, 0xcc, 0xc6, 0x63, 0xe0,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007E "~" */
    0x70, 0xb8,

    /* U+2103 "℃" */
    0xf1, 0xe9, 0x32, 0x96, 0x6, 0x60, 0x6, 0x0,
    0x60, 0x6, 0x0, 0x32, 0x1, 0xe0,

    /* U+3001 "、" */
    0x46, 0x30,

    /* U+4E00 "一" */
    0xff, 0xe0,

    /* U+4E09 "三" */
    0x7f, 0x80, 0x0, 0x0, 0x0, 0x7f, 0x0, 0x0,
    0x0, 0x0, 0xff, 0xc0,

    /* U+4E8C "二" */
    0x7f, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x7, 0xff,

    /* U+4E8E "于" */
    0x7f, 0xc0, 0x80, 0x10, 0x2, 0xf, 0xfe, 0x8,
    0x1, 0x0, 0x20, 0x4, 0x3, 0x80,

    /* U+4F20 "传" */
    0x33, 0x3, 0xfe, 0x62, 0x7, 0xff, 0x66, 0x6,
    0x60, 0x27, 0xe2, 0xc, 0x27, 0x82, 0x18, 0x20,
    0x80,

    /* U+503C "值" */
    0x22, 0x1f, 0xf4, 0x23, 0x7e, 0xd8, 0xb7, 0xe5,
    0x89, 0x7e, 0x5f, 0x96, 0x27, 0xfc,

    /* U+5149 "光" */
    0x4, 0xc, 0x98, 0xd3, 0x12, 0x4f, 0xfe, 0x36,
    0x4, 0xc0, 0x98, 0x33, 0xc, 0x67, 0x7, 0x80,

    /* U+5173 "关" */
    0x31, 0x82, 0x31, 0xff, 0x2, 0x0, 0x40, 0x8,
    0x3f, 0xf8, 0x70, 0x1f, 0xf, 0x3b, 0x83, 0x80,

    /* U+5236 "制" */
    0x50, 0x2a, 0x17, 0xfa, 0xc8, 0x5f, 0xea, 0x21,
    0x7f, 0xae, 0xb5, 0xd6, 0x3a, 0xc4, 0x43, 0x80,

    /* U+529F "功" */
    0x1, 0x1f, 0x23, 0xe4, 0x17, 0xf2, 0x16, 0x42,
    0xc8, 0x59, 0xdb, 0xf2, 0x60, 0x8c, 0x23, 0x0,

    /* U+52A1 "务" */
    0x18, 0x7, 0xfb, 0xc6, 0xf, 0xf, 0x3c, 0x10,
    0x2, 0x7, 0xfe, 0x18, 0x8e, 0x13, 0x1e, 0x0,

    /* U+52A8 "动" */
    0x1, 0x1f, 0x20, 0x4, 0x3, 0xff, 0x96, 0xc2,
    0xd6, 0x5a, 0x5b, 0xfa, 0x78, 0xc8, 0x13, 0x0,

    /* U+53D1 "发" */
    0x4, 0x3, 0x6c, 0x26, 0x47, 0xfe, 0xc, 0x0,
    0xfe, 0x1c, 0xc3, 0x78, 0x63, 0x4, 0xfe, 0x18,
    0x60,

    /* U+53D6 "取" */
    0xf8, 0xa, 0xf9, 0xc9, 0x39, 0x65, 0x2c, 0xe7,
    0x14, 0x63, 0xcc, 0xf3, 0x82, 0xd8, 0x51, 0x0,

    /* U+53E3 "口" */
    0xff, 0xf0, 0x3c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0xf, 0x3, 0xff, 0xf0, 0x30,

    /* U+53F7 "号" */
    0x7f, 0xcc, 0x19, 0x83, 0x3f, 0xe0, 0x1, 0xff,
    0xcc, 0x1, 0xfe, 0x0, 0x80, 0x30, 0x1e, 0x0,

    /* U+540D "名" */
    0xc, 0x7, 0xf7, 0x9, 0xc6, 0x1b, 0x3, 0x87,
    0xfd, 0x41, 0x10, 0x47, 0xf1, 0x4,

    /* U+5668 "器" */
    0x7b, 0xc9, 0x49, 0x29, 0x3d, 0xe0, 0xd9, 0xff,
    0xdc, 0x77, 0xdf, 0x4a, 0x49, 0x49, 0xef, 0x0,

    /* U+5730 "地" */
    0x61, 0x6, 0x50, 0x65, 0x2f, 0x5e, 0x6f, 0x26,
    0xd2, 0x65, 0x27, 0x56, 0xe4, 0x38, 0x42, 0x7,
    0xe0,

    /* U+5740 "址" */
    0x61, 0xc, 0x21, 0x84, 0x7e, 0x86, 0xde, 0xda,
    0x1f, 0x47, 0xe8, 0x8d, 0x3, 0xfc,

    /* U+578B "型" */
    0x7d, 0x4d, 0x2b, 0xf5, 0x34, 0xa4, 0x85, 0x91,
    0x81, 0x3, 0xfe, 0x4, 0x1f, 0xfc,

    /* U+5907 "备" */
    0x8, 0x1, 0xfc, 0x71, 0x80, 0xe0, 0x3f, 0xe7,
    0xe, 0x3f, 0xc2, 0x4c, 0x3f, 0xc2, 0x4c, 0x3f,
    0xc0,

    /* U+5927 "大" */
    0x4, 0x0, 0x80, 0x10, 0x7f, 0xf0, 0xe0, 0x14,
    0x6, 0xc1, 0x8c, 0x71, 0xdc, 0xc,

    /* U+5B9A "定" */
    0x6, 0xf, 0xfd, 0x0, 0xa0, 0x13, 0xfc, 0x6c,
    0x9, 0xf1, 0xb0, 0x36, 0xf, 0xc3, 0x1f, 0x80,

    /* U+5BC6 "密" */
    0x6, 0xf, 0xfd, 0x10, 0x81, 0xc6, 0xb4, 0x9d,
    0x5f, 0xe3, 0x30, 0x36, 0x46, 0xc8, 0xff, 0x0,

    /* U+5C0F "小" */
    0x6, 0x0, 0x60, 0x6, 0x2, 0x6c, 0x26, 0x66,
    0x66, 0x66, 0x2c, 0x63, 0x46, 0x20, 0x60, 0x1e,
    0x0,

    /* U+5EA6 "度" */
    0x6, 0xf, 0xfd, 0x22, 0x3f, 0xf4, 0x88, 0x9f,
    0x10, 0x3, 0xfe, 0x59, 0x98, 0xe2, 0xf7, 0x80,

    /* U+5F00 "开" */
    0xff, 0xe2, 0x30, 0x46, 0x8, 0xc1, 0x19, 0xff,
    0xcc, 0x61, 0x8c, 0x21, 0x8c, 0x33, 0x6, 0x0,

    /* U+5F0F "式" */
    0x3, 0xc0, 0x6b, 0xff, 0x81, 0x80, 0x31, 0xfe,
    0xc, 0x41, 0x88, 0x31, 0x2f, 0xb7, 0x83, 0x80,

    /* U+5F3A "强" */
    0x7b, 0xe1, 0xa2, 0x1a, 0x27, 0xbe, 0x78, 0x84,
    0x3e, 0xfa, 0xa1, 0xbe, 0x18, 0xa1, 0xff, 0x70,
    0x0,

    /* U+6001 "态" */
    0x4, 0x0, 0x83, 0xff, 0x87, 0x1, 0xb0, 0xfb,
    0xb9, 0x98, 0x62, 0x56, 0x5a, 0x1e, 0x7e, 0x80,

    /* U+611F "感" */
    0x1, 0xcf, 0xfd, 0x4, 0x3f, 0xa7, 0xfc, 0xed,
    0x7f, 0xf8, 0x20, 0x72, 0x5e, 0x5c, 0x7e, 0x0,

    /* U+6210 "成" */
    0x2, 0xc0, 0x24, 0x7f, 0xf6, 0x24, 0x7a, 0x46,
    0xbc, 0x6b, 0x86, 0x9a, 0x7b, 0xbc, 0x7b, 0xc4,
    0xe0,

    /* U+6237 "户" */
    0x6, 0x1, 0x87, 0xfd, 0x81, 0x60, 0x5f, 0xf6,
    0x5, 0x80, 0xc0, 0x30, 0x8, 0x0,

    /* U+6247 "扇" */
    0x4, 0x1f, 0xf4, 0x5, 0xff, 0x40, 0x17, 0xf5,
    0x55, 0x77, 0x5d, 0xfd, 0xd8, 0xcc,

    /* U+624B "手" */
    0x1, 0x8f, 0xe0, 0x10, 0x3f, 0xe0, 0x40, 0x8,
    0x3f, 0xf8, 0x20, 0x4, 0x0, 0x80, 0x70, 0x0,

    /* U+6253 "打" */
    0x60, 0xd, 0xff, 0xc6, 0x30, 0xc6, 0x18, 0xe3,
    0x38, 0x63, 0xc, 0x61, 0x8c, 0x33, 0x1c, 0x0,

    /* U+6267 "执" */
    0x62, 0xc, 0x43, 0xfe, 0x31, 0x46, 0xa8, 0xfd,
    0x39, 0xe3, 0x3c, 0x64, 0x8d, 0x9f, 0xe1, 0x0,

    /* U+62A5 "报" */
    0x6f, 0xcd, 0x8b, 0xf1, 0x7e, 0xe6, 0xc0, 0xff,
    0xff, 0x97, 0x7a, 0x6d, 0xcd, 0xbb, 0xbd, 0x0,

    /* U+62E9 "择" */
    0x6f, 0xcc, 0xdb, 0xce, 0x31, 0xc6, 0xee, 0xf2,
    0x3b, 0xf3, 0x8, 0x6f, 0xec, 0x23, 0x4, 0x0,

    /* U+63A5 "接" */
    0x61, 0xc, 0xff, 0xd9, 0x79, 0x66, 0xfe, 0xe2,
    0x3b, 0xfb, 0x32, 0x67, 0xcc, 0x3b, 0xb9, 0x80,

    /* U+63A7 "控" */
    0x61, 0xd, 0xff, 0xf0, 0xb1, 0x66, 0x46, 0xe8,
    0x3d, 0xf7, 0x8, 0x61, 0xc, 0x23, 0xbf, 0x80,

    /* U+65E0 "无" */
    0x7f, 0xc1, 0x80, 0x30, 0x7f, 0xf0, 0xe0, 0x1c,
    0x7, 0x81, 0xb1, 0x62, 0x38, 0x7c,

    /* U+667A "智" */
    0x40, 0x3f, 0xfa, 0x67, 0xf9, 0x7f, 0xf0, 0x7,
    0xf9, 0x82, 0x7f, 0x98, 0x27, 0xf8,

    /* U+6709 "有" */
    0x8, 0x1f, 0xfc, 0xc0, 0x18, 0x7, 0xfc, 0xe1,
    0xbf, 0xf1, 0x86, 0x3f, 0xc6, 0x18, 0xc6, 0x0,

    /* U+670D "服" */
    0x73, 0xe5, 0x22, 0x52, 0x27, 0x2c, 0x53, 0xf5,
    0x3e, 0x73, 0xa5, 0x2e, 0x52, 0xc5, 0x2e, 0xb3,
    0xa0,

    /* U+672A "未" */
    0x4, 0x0, 0x81, 0xff, 0x2, 0x0, 0x41, 0xff,
    0xc3, 0x81, 0xb8, 0x75, 0xd8, 0x8c, 0x10, 0x0,

    /* U+6863 "档" */
    0x21, 0x2, 0x52, 0xf7, 0x62, 0x50, 0x37, 0xe7,
    0x82, 0x68, 0x2e, 0x7e, 0x20, 0x22, 0x7e, 0x20,
    0x20,

    /* U+6A21 "模" */
    0x26, 0xc5, 0xfd, 0xff, 0x16, 0x23, 0xfc, 0xff,
    0x9c, 0x43, 0x7f, 0x22, 0x85, 0xdc, 0xa1, 0x80,

    /* U+6D88 "消" */
    0x49, 0x26, 0xd2, 0x5, 0x6c, 0xfe, 0x68, 0x20,
    0xfe, 0x28, 0x26, 0x82, 0x6f, 0xec, 0x82, 0x48,
    0xe0,

    /* U+6E29 "温" */
    0x6f, 0xc5, 0x88, 0x3f, 0x66, 0x26, 0xfc, 0x0,
    0xb, 0xf1, 0x5a, 0x6b, 0x49, 0x6b, 0x7f, 0x80,

    /* U+6E7F "湿" */
    0x6f, 0xc5, 0x8, 0x3f, 0x64, 0x26, 0xfc, 0x0,
    0x0, 0xe1, 0x5f, 0x6b, 0xc9, 0x79, 0x7f, 0x80,

    /* U+706B "火" */
    0x6, 0x0, 0x60, 0x26, 0x2, 0x66, 0x66, 0x66,
    0x64, 0xf, 0x0, 0xf0, 0x19, 0x87, 0xe, 0x60,
    0x60,

    /* U+706F "灯" */
    0x60, 0xc, 0xff, 0xa2, 0x7c, 0x4f, 0x9, 0xc1,
    0x18, 0x23, 0x84, 0x58, 0x99, 0x12, 0xe, 0x0,

    /* U+7130 "焰" */
    0x31, 0x6, 0x7d, 0xe9, 0x3e, 0x67, 0x60, 0xcf,
    0xcd, 0x9, 0xbf, 0x6c, 0x28, 0xfd, 0x10, 0x80,

    /* U+7167 "照" */
    0x7f, 0xe4, 0x9a, 0x4b, 0x67, 0xe4, 0x4b, 0xe4,
    0xa2, 0x7a, 0x27, 0xbe, 0x35, 0x66, 0xda, 0xcd,
    0xb0,

    /* U+72B6 "状" */
    0x22, 0x84, 0x5b, 0x88, 0x3f, 0xe2, 0x20, 0xc6,
    0x39, 0xc5, 0x2c, 0x2c, 0x87, 0x18, 0xe1, 0x0,

    /* U+7528 "用" */
    0x7f, 0xd1, 0x14, 0x45, 0xff, 0x44, 0x5f, 0xf4,
    0x45, 0x11, 0x84, 0x61, 0x70,

    /* U+7801 "码" */
    0xff, 0xc8, 0x19, 0x12, 0x22, 0x4f, 0x49, 0xaf,
    0xf4, 0x12, 0xfa, 0x70, 0x48, 0x8, 0x7, 0x0,

    /* U+79F0 "称" */
    0x72, 0xc, 0xfc, 0x95, 0x7e, 0xa2, 0x7c, 0xea,
    0x9d, 0x57, 0x2a, 0xad, 0x64, 0x20, 0x8c, 0x0,

    /* U+7AEF "端" */
    0x6d, 0x4b, 0x5f, 0xfe, 0x40, 0xbf, 0xd8, 0xc6,
    0xfc, 0xbb, 0xfe, 0xf3, 0xb0, 0xec,

    /* U+7C7B "类" */
    0x6d, 0x8b, 0x4f, 0xfc, 0x7c, 0xed, 0xf3, 0x30,
    0xc3, 0xff, 0x1e, 0x3c, 0xfc, 0xc,

    /* U+80FD "能" */
    0x22, 0xd, 0x79, 0xe8, 0x5, 0x37, 0xbc, 0x90,
    0x1e, 0x92, 0x5e, 0x7a, 0x9, 0x4d, 0x6f, 0x0,

    /* U+884C "行" */
    0x20, 0x6, 0xfd, 0x80, 0x28, 0x3, 0x0, 0xdf,
    0xf8, 0x37, 0x6, 0x20, 0xc4, 0x18, 0x83, 0x10,
    0xe0,

    /* U+89E6 "触" */
    0x20, 0x87, 0x88, 0x53, 0xef, 0xaa, 0x6a, 0xa7,
    0xaa, 0x6b, 0xe7, 0x8a, 0x68, 0xa6, 0xfe, 0xfb,
    0x30,

    /* U+8B66 "警" */
    0xfd, 0xc8, 0x7b, 0xfe, 0x7c, 0xc7, 0xa5, 0xff,
    0x8f, 0xe1, 0xfc, 0x7f, 0x8c, 0x11, 0xfe, 0x0,

    /* U+8BBE "设" */
    0xc7, 0x8c, 0x90, 0x12, 0x6c, 0x7c, 0x80, 0x9f,
    0x93, 0x33, 0xbc, 0x63, 0xb, 0xfc, 0x41, 0x0,

    /* U+8FDE "连" */
    0x3, 0x6, 0xfe, 0x2f, 0xe0, 0x68, 0x6f, 0xe2,
    0x8, 0x2f, 0xe2, 0x8, 0x20, 0x8f, 0x2, 0x4f,
    0xe0,

    /* U+9009 "选" */
    0x8a, 0x37, 0xe1, 0x20, 0x48, 0x1f, 0xf1, 0x44,
    0xd1, 0x65, 0x51, 0xf8, 0x9, 0xfc,

    /* U+9501 "锁" */
    0x45, 0x5e, 0xaa, 0x14, 0x3b, 0xe6, 0x54, 0xca,
    0xbd, 0x53, 0x2a, 0x77, 0x4e, 0xd9, 0x31, 0x0,

    /* U+95ED "闭" */
    0x0, 0x17, 0xf4, 0x4, 0x1, 0x82, 0x6f, 0xd8,
    0x66, 0x39, 0xba, 0x68, 0x98, 0xe6, 0x3,

    /* U+9608 "阈" */
    0x0, 0x13, 0xf6, 0x4, 0x15, 0x85, 0x6f, 0xdb,
    0xd6, 0xbd, 0x92, 0xef, 0xd8, 0x86, 0x7,

    /* U+98CE "风" */
    0x7f, 0x88, 0x11, 0x4a, 0x29, 0x45, 0xe8, 0x99,
    0x13, 0x22, 0xf4, 0xf3, 0x94, 0x56, 0x1, 0x0,

    /* U+FF01 "！" */
    0xff, 0xf3, 0xc0,

    /* U+FF1A "：" */
    0xf0, 0xf
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 44, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 185, .box_w = 11, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 19, .adv_w = 62, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 21, .adv_w = 71, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 22, .adv_w = 62, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 74, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 29, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 113, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 91, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 62, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 105, .adv_w = 113, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 108, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 113, .adv_w = 99, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 123, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 131, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 126, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 137, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 118, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 138, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 145, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 63, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 109, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 132, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 111, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 164, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 216, .adv_w = 144, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 148, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 241, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 253, .adv_w = 131, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 120, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 120, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 144, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 119, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 176, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 305, .adv_w = 120, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 112, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 118, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 74, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 335, .adv_w = 109, .box_w = 6, .box_h = 1, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 336, .adv_w = 113, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 342, .adv_w = 124, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 101, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 124, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 112, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 371, .adv_w = 71, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 115, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 387, .adv_w = 123, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 395, .adv_w = 58, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 59, .box_w = 4, .box_h = 13, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 405, .adv_w = 116, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 414, .adv_w = 60, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 418, .adv_w = 185, .box_w = 10, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 123, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 120, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 440, .adv_w = 124, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 448, .adv_w = 124, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 456, .adv_w = 84, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 95, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 466, .adv_w = 81, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 122, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 477, .adv_w = 111, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 166, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 493, .adv_w = 108, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 500, .adv_w = 110, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 509, .adv_w = 98, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 57, .box_w = 1, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 516, .adv_w = 113, .box_w = 7, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 518, .adv_w = 192, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 532, .adv_w = 192, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 534, .adv_w = 192, .box_w = 11, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 536, .adv_w = 192, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 548, .adv_w = 192, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 559, .adv_w = 192, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 573, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 590, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 604, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 620, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 636, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 652, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 668, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 684, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 700, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 717, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 733, .adv_w = 192, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 746, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 762, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 776, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 792, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 809, .adv_w = 192, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 823, .adv_w = 192, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 837, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 854, .adv_w = 192, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 868, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 884, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 900, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 917, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 933, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 949, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 965, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 982, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 998, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1014, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1031, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1045, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1059, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1075, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1091, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1107, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1123, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1139, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1155, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1171, .adv_w = 192, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1185, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1199, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1215, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1232, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1248, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1265, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1281, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1298, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1314, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1330, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1347, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1363, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1379, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1396, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1412, .adv_w = 192, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1425, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1441, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1457, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1471, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1485, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1501, .adv_w = 192, .box_w = 11, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1518, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1535, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1551, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1567, .adv_w = 192, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1584, .adv_w = 192, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1598, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1614, .adv_w = 192, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1629, .adv_w = 192, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1644, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1660, .adv_w = 192, .box_w = 2, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1663, .adv_w = 192, .box_w = 2, .box_h = 8, .ofs_x = 2, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 2, 3, 4, 5, 6,
    7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 0, 18, 19, 20, 21,
    0, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 0, 48, 0, 0, 49
};

static const uint16_t unicode_list_2[] = {
    0x0, 0x2, 0x2087, 0x2f85, 0x4d84, 0x4d8d, 0x4e10, 0x4e12,
    0x4ea4, 0x4fc0, 0x50cd, 0x50f7, 0x51ba, 0x5223, 0x5225, 0x522c,
    0x5355, 0x535a, 0x5367, 0x537b, 0x5391, 0x55ec, 0x56b4, 0x56c4,
    0x570f, 0x588b, 0x58ab, 0x5b1e, 0x5b4a, 0x5b93, 0x5e2a, 0x5e84,
    0x5e93, 0x5ebe, 0x5f85, 0x60a3, 0x6194, 0x61bb, 0x61cb, 0x61cf,
    0x61d7, 0x61eb, 0x6229, 0x626d, 0x6329, 0x632b, 0x6564, 0x65fe,
    0x668d, 0x6691, 0x66ae, 0x67e7, 0x69a5, 0x6d0c, 0x6dad, 0x6e03,
    0x6fef, 0x6ff3, 0x70b4, 0x70eb, 0x723a, 0x74ac, 0x7785, 0x7974,
    0x7a73, 0x7bff, 0x8081, 0x87d0, 0x896a, 0x8aea, 0x8b42, 0x8f62,
    0x8f8d, 0x9485, 0x9571, 0x958c, 0x9852, 0xfe85, 0xfe9e
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 64, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 64, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    },
    {
        .range_start = 97, .range_length = 26, .glyph_id_start = 51,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 124, .range_length = 65183, .glyph_id_start = 77,
        .unicode_list = unicode_list_2, .glyph_id_ofs_list = NULL, .list_length = 79, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 0, 1, 2, 1, 3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 4, 0, 0, 0, 0, 5,
    6, 7, 8, 9, 10, 11, 0, 0,
    12, 13, 14, 0, 0, 8, 15, 8,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 0, 26, 27, 28, 0, 29,
    30, 31, 32, 0, 0, 33, 34, 32,
    32, 27, 27, 35, 36, 37, 38, 35,
    39, 40, 41, 42, 43, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 0, 1, 2, 3, 4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 5, 0, 0, 0, 6, 7,
    0, 8, 0, 0, 0, 8, 0, 0,
    9, 0, 0, 0, 0, 8, 0, 8,
    0, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 0, 19, 0, 20, 20, 20,
    21, 22, 0, 23, 24, 0, 0, 25,
    25, 20, 25, 20, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -22, -6, -16, -8, 0, -23,
    0, 0, 0, -2, 0, 0, 0, 7,
    0, 0, -13, 0, -12, -7, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, -5, -12, 0, -4, -2, -7,
    -16, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -5, 0, -3, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    -9, -2, -17, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -6, -4, 0, -2,
    3, 3, 0, 0, 0, -4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -7, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -7, -1, -2, 0, 0, -15, -3,
    -4, 0, -1, -4, -1, -9, 4, 0,
    -2, 0, 0, 0, 0, 4, -4, -1,
    -3, -2, -2, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, -4, -6,
    0, -2, -2, -2, -4, -2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -2,
    0, -4, -2, -2, -4, 0, 0, -7,
    0, 0, 0, 0, 0, -7, -2, -6,
    -3, -4, -2, -2, -2, -3, -2, 0,
    0, 0, 0, -4, 0, 0, 0, 0,
    -7, -2, -4, -2, 0, -4, 0, 0,
    0, 0, -3, 0, 0, -2, 0, -11,
    0, -6, 0, -2, -1, -5, -4, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
    0, 0, -6, 0, -2, 0, -9, -2,
    0, -19, 0, -19, -12, 0, 0, -8,
    -2, -29, -6, 0, 0, -1, -1, -6,
    -2, -7, 0, -8, -4, 0, -6, 0,
    0, -4, -6, -2, -4, -7, -5, -7,
    -5, -10, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, 0, -4, 0, -4, -2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -6, 0, -6, 0, 0,
    0, 0, 0, -9, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -10, 0, 0,
    0, -3, -2, -6, 0, -4, -5, -4,
    -4, -2, 0, -5, 0, 0, 0, -3,
    0, 0, 0, -2, 0, 0, -10, -3,
    -7, -5, -5, -7, -4, 0, -10, 0,
    0, 0, -10, -1, -7, 0, -6, -26,
    -7, -17, -13, 0, -18, 0, -17, 0,
    -3, -4, -2, 0, 0, 0, 0, -6,
    -2, -11, -8, 0, -11, 0, -28, -6,
    -28, -12, 0, 0, -11, 0, -31, -2,
    -4, 0, 0, 0, -6, -2, -13, 0,
    -8, -5, 0, -4, 0, 0, 0, -2,
    0, 0, 0, 0, -4, 0, -4, 0,
    -7, 0, 0, 0, 0, -2, 0, -3,
    -3, -4, 0, -2, 1, -2, -2, -2,
    0, -2, -2, 0, -2, 0, 0, 0,
    0, 0, 0, 0, 0, -3, 0, -3,
    0, 4, 0, 0, 0, 0, 0, 0,
    -4, -4, -4, 0, 0, 0, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -6, 0, 0, 0, 0, 0,
    -2, -25, -14, -25, -16, -4, 0, -9,
    -6, -27, -6, 0, 0, 0, 0, -4,
    -4, -10, 0, -14, -16, -3, -14, 0,
    0, -9, -13, -3, -9, -6, -6, -6,
    -6, -15, -5, 0, -5, -4, 0, 0,
    -3, 0, -12, -2, 0, 0, -2, 0,
    -2, -4, 0, 0, -2, 0, 0, -2,
    0, 0, 0, -2, 0, 0, 0, 0,
    -3, 0, 0, -16, -4, -16, -7, 0,
    0, -4, -2, -14, -2, 0, -2, 2,
    0, 0, 0, -4, 0, -7, -4, 0,
    -4, 0, 0, -4, -4, 0, -7, -2,
    -2, -4, -2, -5, -8, -2, -8, -3,
    0, 0, 0, -1, -11, -1, 0, 0,
    0, 0, 0, 0, -1, 0, -3, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, -7, 0,
    0, 0, 0, -1, -5, -2, -4, -4,
    -2, 0, 0, 0, 0, 0, 0, -2,
    -3, -5, 0, 0, 0, 0, 0, -5,
    -2, -5, -4, -2, -5, -4, -23, -16,
    -23, -12, -7, -3, -4, -4, -22, -4,
    -4, -2, 0, 0, 0, 0, -5, 0,
    -16, -11, 0, -13, 0, 0, -9, -11,
    -10, -8, -4, -6, -8, -4, -12, 0,
    -6, 0, 0, 0, 0, -1, -4, -6,
    -7, 0, -2, -1, -1, 0, -4, -3,
    0, -3, -3, -4, -3, 0, 0, 0,
    0, -4, -4, -3, -3, -6, -3, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -16, -4, -10, -4, 0, -14,
    0, 0, 0, 0, 0, 6, 0, 14,
    0, 0, 0, 0, -4, -2, 0, 1,
    0, 0, 0, 0, 0, 0, -6, 0,
    0, 0, 0, -6, 0, -5, -2, 0,
    -6, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -4, 0, 0, 0, 0,
    0, 0, -2, 1, -2, 0, 0, -6,
    0, 0, 0, 0, -13, 0, -4, 0,
    -2, -12, 0, -7, -3, 0, -1, 0,
    0, 0, 0, -1, -5, 0, -2, -2,
    -5, -2, -2, 0, -7, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, -4,
    0, 0, -6, 0, 0, -2, -6, 0,
    -2, 0, 0, 0, 0, -2, 0, 1,
    1, 0, 1, 0, 0, 2, 0, 0,
    0, -3, 0, 0, -4, -4, -6, 0,
    -5, -2, 0, -7, 0, -7, -3, 0,
    -1, -2, 0, 0, 0, 0, -2, 0,
    -1, -1, -3, -1, -1, -13, -4, -13,
    -1, 0, 5, 0, 0, 0, 0, 9,
    0, 14, 9, 6, 11, 0, 9, -4,
    -2, 0, -3, 0, -2, 0, -2, 0,
    0, 1, 0, -2, 0, -4, 0, 0,
    0, 7, 0, -9, 0, 0, 0, 0,
    -7, 0, 0, 0, 0, -4, 0, 0,
    -4, -4, 0, 0, 0, 10, 0, 0,
    0, 0, -2, -2, 0, 2, -4, 0,
    0, 0, 0, 0, -3, 0, 0, 0,
    0, -6, 0, -2, 0, 0, -5, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, -16, 1, 0, 1, -6, 0, 0,
    0, 0, -10, 0, 0, 0, 0, -3,
    0, 0, -2, -6, 0, -2, 0, -2,
    0, 0, -7, -4, 0, 0, -3, 0,
    -3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, -4, 0,
    0, -7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -17, -6, -17, -6, 3,
    0, -4, 0, -15, 0, 0, 0, 0,
    0, 0, 0, -2, 1, -6, -2, 0,
    -2, 0, 0, 0, -2, 0, 0, 3,
    2, 0, 3, -2, 0, 2, 0, 0,
    0, -5, 0, 0, 0, 0, -6, 0,
    -2, 0, 0, -4, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -2, -7, -2,
    0, 1, -7, 0, 0, 0, 0, -4,
    0, 0, 0, 0, -2, 0, 0, -4,
    -4, 0, -2, 0, 0, 0, -2, -4,
    0, 0, 0, -3, 0, 0, -12, -3,
    -12, -4, 0, 0, -3, 0, -9, 0,
    -4, 0, -2, 0, 0, -4, -2, 0,
    -4, -2, 0, 0, 0, -2, 0, 0,
    0, 0, 0, 0, 0, 0, -6, -16,
    0, -16, -1, 0, 0, -2, 0, -7,
    0, -6, 0, -2, 0, -4, -6, 0,
    0, -2, -2, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, -5,
    3, -4, -3, 0, 0, 0, 0, -2,
    0, -2, -6, 0, -4, 0, -2, -8,
    0, 0, -2, -5, 0, 0, 0, 0,
    0, 0, -6, 0, 0, 0, 0, -2,
    0, -12, 0, -12, -1, 0, 0, 0,
    0, -9, 0, -4, 0, -2, 0, -2,
    -3, 0, 0, -4, -2, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, -4,
    0, -6, 0, -5, 0, 0, 0, 0,
    0, 0, 0, 0, -5, 0, 0, 0,
    0, -4, 0, 0, -4, -2, 0, -1,
    0, 0, 0, 0, 0, -2, -2, 0,
    0, -2, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 43,
    .right_class_cnt     = 33,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t syht_font_10 = {
#else
lv_font_t syht_font_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    // .static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SYHT_FONT_10*/
