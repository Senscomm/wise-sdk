/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#ifndef __COLOR_FORMAT_H__
#define __COLOR_FORMAT_H__

#include <stdint.h>

typedef struct RgbColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RgbColor_t;

typedef struct HsvColor
{
    uint8_t h;
    uint8_t s;
    uint8_t v;
} HsvColor_t;

typedef struct XyColor
{
    uint16_t x;
    uint16_t y;
} XyColor_t;

typedef struct CtColor
{
    uint16_t ctMireds;
} CtColor_t;

RgbColor_t XYToRgb(uint8_t Level, uint16_t currentX, uint16_t currentY);
RgbColor_t HsvToRgb(HsvColor_t hsv);
RgbColor_t CTToRgb(CtColor_t ct);

XyColor_t RgbToXy(uint8_t r, uint8_t g, uint8_t b);
HsvColor_t RgbToHsv(uint8_t r, uint8_t g, uint8_t b);
CtColor_t RgbToCtt(uint8_t r, uint8_t g, uint8_t b);
#endif
