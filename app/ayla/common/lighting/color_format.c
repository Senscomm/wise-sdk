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

#include "color_format.h"

#include <math.h>

#define clamp(a, min, max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))

RgbColor_t HsvToRgb(HsvColor_t hsv)
{
    RgbColor_t rgb;

    uint8_t region, p, q, t;
    uint32_t h, s, v, remainder;

    if (hsv.s == 0)
    {
        rgb.r = rgb.g = rgb.b = hsv.v;
    }
    else
    {
        h = hsv.h;
        s = hsv.s;
        v = hsv.v;

        region    = h / 43;
        remainder = (h - (region * 43)) * 6;
        p         = (v * (255 - s)) >> 8;
        q         = (v * (255 - ((s * remainder) >> 8))) >> 8;
        t         = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
        switch (region)
        {
        case 0:
            rgb.r = v, rgb.g = t, rgb.b = p;
            break;
        case 1:
            rgb.r = q, rgb.g = v, rgb.b = p;
            break;
        case 2:
            rgb.r = p, rgb.g = v, rgb.b = t;
            break;
        case 3:
            rgb.r = p, rgb.g = q, rgb.b = v;
            break;
        case 4:
            rgb.r = t, rgb.g = p, rgb.b = v;
            break;
        case 5:
        default:
            rgb.r = v, rgb.g = p, rgb.b = q;
            break;
        }
    }

    return rgb;
}

RgbColor_t XYToRgb(uint8_t Level, uint16_t currentX, uint16_t currentY)
{
    // convert xyY color space to RGB

    // https://www.easyrgb.com/en/math.php
    // https://en.wikipedia.org/wiki/SRGB
    // refer https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xy_chromaticity_diagram_and_the_CIE_xyY_color_space

    // The currentX/currentY attribute contains the current value of the normalized chromaticity value of x/y.
    // The value of x/y shall be related to the currentX/currentY attribute by the relationship
    // x = currentX/65536
    // y = currentY/65536
    // z = 1-x-y

    RgbColor_t rgb;

    float x, y, z;
    float X, Y, Z;
    float r, g, b;

    x = ((float) currentX) / 65535.0f;
    y = ((float) currentY) / 65535.0f;

    z = 1.0f - x - y;

    // Calculate XYZ values

    // Y - given brightness in 0 - 1 range
    Y = ((float) Level) / 254.0f;
    X = (Y / y) * x;
    Z = (Y / y) * z;

    // X, Y and Z input refer to a D65/2° standard illuminant.
    // sR, sG and sB (standard RGB) output range = 0 ÷ 255
    // convert XYZ to RGB - CIE XYZ to sRGB
    X = X / 100.0f;
    Y = Y / 100.0f;
    Z = Z / 100.0f;

    r = (X * 3.2406f) - (Y * 1.5372f) - (Z * 0.4986f);
    g = -(X * 0.9689f) + (Y * 1.8758f) + (Z * 0.0415f);
    b = (X * 0.0557f) - (Y * 0.2040f) + (Z * 1.0570f);

    // apply gamma 2.2 correction
    r = (r <= 0.0031308f ? 12.92f * r : (1.055f) * pow(r, (1.0f / 2.4f)) - 0.055f);
    g = (g <= 0.0031308f ? 12.92f * g : (1.055f) * pow(g, (1.0f / 2.4f)) - 0.055f);
    b = (b <= 0.0031308f ? 12.92f * b : (1.055f) * pow(b, (1.0f / 2.4f)) - 0.055f);

    // Round off
    r = clamp(r, 0, 1);
    g = clamp(g, 0, 1);
    b = clamp(b, 0, 1);

    // these rgb values are in  the range of 0 to 1, convert to limit of HW specific LED
    rgb.r = (uint8_t) (r * 255);
    rgb.g = (uint8_t) (g * 255);
    rgb.b = (uint8_t) (b * 255);

    return rgb;
}

RgbColor_t CTToRgb(CtColor_t ct)
{
    RgbColor_t rgb;
    float r, g, b;

    // Algorithm credits to Tanner Helland: https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html

    // Convert Mireds to centiKelvins. k = 1,000,000/mired
    float ctCentiKelvin = 10000 / ct.ctMireds;

    // Red
    if (ctCentiKelvin <= 66)
    {
        r = 255;
    }
    else
    {
        r = 329.698727446f * pow(ctCentiKelvin - 60, -0.1332047592f);
    }

    // Green
    if (ctCentiKelvin <= 66)
    {
        g = 99.4708025861f * log(ctCentiKelvin) - 161.1195681661f;
    }
    else
    {
        g = 288.1221695283f * pow(ctCentiKelvin - 60, -0.0755148492f);
    }

    // Blue
    if (ctCentiKelvin >= 66)
    {
        b = 255;
    }
    else
    {
        if (ctCentiKelvin <= 19)
        {
            b = 0;
        }
        else
        {
            b = 138.5177312231 * log(ctCentiKelvin - 10) - 305.0447927307;
        }
    }
    rgb.r = (uint8_t) clamp(r, 0, 255);
    rgb.g = (uint8_t) clamp(g, 0, 255);
    rgb.b = (uint8_t) clamp(b, 0, 255);

    return rgb;
}

/* Function to perform gamma correction */
static double gamma_correct(double c)
{
    return (c > 0.04045) ? pow((c + 0.055) / (1.0 + 0.055), 2.4) : (c / 12.92);
}

XyColor_t RgbToXy(uint8_t r, uint8_t g, uint8_t b)
{
    XyColor_t xy;
    double x, y;

    /* Normalize RGB */
    double rf = (double)r / 255.0;
    double gf = (double)g / 255.0;
    double bf = (double)b / 255.0;

    /* Apply gamma correction */
    rf = gamma_correct(rf);
    gf = gamma_correct(gf);
    bf = gamma_correct(bf);

    /* Convert to XYZ */
    double X = rf * 0.4124564 + gf * 0.3575761 + bf * 0.1804375;
    double Y = rf * 0.2126729 + gf * 0.7151522 + bf * 0.0721750;
    double Z = rf * 0.0193339 + gf * 0.1191920 + bf * 0.9503041;

    /* Convert to xy */
    if ((X + Y + Z) == 0) { /* Handle black or near-black colors */
        x = 0.3127; /* D65 white point x */
        y = 0.3290; /* D65 white point y */
    } else {
        x = X / (X + Y + Z);
        y = Y / (X + Y + Z);
    }

    xy.x = (uint16_t)(x * 65536);
    xy.y = (uint16_t)(y * 65536);

    return xy;
}


#include <stdio.h>
#include <math.h>

HsvColor_t RgbToHsv(uint8_t r, uint8_t g, uint8_t b)
{
    HsvColor_t hsv;
    double h, s, v;
    double rf = r / 255.0f;
    double gf = g / 255.0f;
    double bf = b / 255.0f;

    double max = fmaxf(rf, fmaxf(gf, bf));
    double min = fminf(rf, fminf(gf, bf));
    double delta = max - min;

    /* Value */
    v = max;

    /* Saturation */
    if (max == 0) {
        s = 0;
    } else {
        s = delta / max;
    }

    /* Hue */
    if (delta == 0) {
        h = 0; /* Undefined hue → 0 */
    } else if (max == rf) {
        h = 60.0f * fmodf(((gf - bf) / delta), 6.0f);
    } else if (max == gf) {
        h = 60.0f * (((bf - rf) / delta) + 2.0f);
    } else {/* max == bf */
        h = 60.0f * (((rf - gf) / delta) + 4.0f);
    }

    if (h < 0)
        h += 360.0f;

    hsv.h = (uint8_t)((h * 254) / 360);
    hsv.s = (uint8_t)(s * 254);
    hsv.v = (uint8_t)(v * 254);

    return hsv;
}

CtColor_t RgbToCtt(uint8_t r, uint8_t g, uint8_t b)
{
    CtColor_t cct;

    /* Step 1: Normalize and linearize RGB */
    double R = r / 255.0;
    double G = g / 255.0;
    double B = b / 255.0;

    double X = (-0.14282)*(R) + (1.54924)*(G) + (-0.95641)*(B);
    double Y = (-0.32466)*(R) + (1.57837)*(G) + (-0.73191)*(B);
    double Z = (-0.68202)*(R) + (0.77073)*(G) + (0.56332)*(B);

    double sum = X + Y + Z;
    if (sum == 0.0) {
        cct.ctMireds = 0; /* black → undefined CCT */
    } else {
        /* Step 3: Chromaticity coordinates */
        double x = X / sum;
        double y = Y / sum;

        /* Step 4: McCamy's approximation */
        double n = (x - 0.3320) / (0.1858 - y);
        double CCT = 437.0 * pow(n, 3) + 3601.0 * pow(n, 2) + 6861.0 * n + 5517.0;
        cct.ctMireds = (uint16_t)(1000000 / CCT);
    }

    return cct;
}

#if 1
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <cli.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[ 0 ] ) )
#endif

static int do_cf(int argc, char *argv[])
{
    const char *format;
	int value_r, value_g, value_b;
    XyColor_t xy;
    HsvColor_t hsv;
    CtColor_t cct;

	if (argc != 5) {
		return CMD_RET_USAGE;
	}

    format = argv[1];
	value_r = atoi(argv[2]);
	value_g = atoi(argv[3]);
	value_b = atoi(argv[4]);

	if (value_r > 255 || value_g > 255 || value_b > 255) {
		return CMD_RET_USAGE;
	}

    if (!strcmp(format, "xy")) {
        xy = RgbToXy(value_r, value_g, value_b);
        printf("x: %d, y: %d\n", xy.x, xy.y);
    } else if (!strcmp(format, "hsv")) {
        hsv = RgbToHsv(value_r, value_g, value_b);
        printf("h: %d, s: %d, v: %d\n", hsv.h, hsv.s, hsv.v);
    } else if (!strcmp(format, "cct")) {
        cct = RgbToCtt(value_r, value_g, value_r);
        printf("cct: %d\n", cct.ctMireds);
    } else {
        return CMD_RET_USAGE;
    }

	return CMD_RET_SUCCESS;
}

CMD(cf, do_cf,
		"Test Color format conversion",
		"cf <xy|hsv|cct> <r> <g> <b>"
   );

#endif
