/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_COUNTRY_CODES_H__
#define __AYLA_COUNTRY_CODES_H__

/*
 * Wi-Fi country codes and associated Ayla region codes.
 * See https://en.wikipedia.org/wiki/ISO_3166-1
 * The numerical assignments must remain constant for compatibility.
 *
 * This is used by ADW and possibly by host MCUs.
 */
enum ada_wifi_region {
	ADW_REGION_DEFAULT = 0,
	ADW_REGION_US = 1,	/* United Status */
	ADW_REGION_CN = 2,	/* China */
	ADW_REGION_EU = 3,	/* EU */
	ADW_REGION_JP = 4,	/* Japan */
	ADW_REGION_CA = 5,	/* Canada */
	ADW_REGION_AU = 6,	/* Australia */
	ADW_REGION_AT = 7,	/* Austria */
	ADW_REGION_BE = 8,	/* Belgium */
	ADW_REGION_BG = 9,	/* Bulgaria */
	ADW_REGION_BR = 10,	/* Brazil */
	ADW_REGION_CH = 11,	/* Switzerland */
	ADW_REGION_CY = 12,	/* Cyprus */
	ADW_REGION_CZ = 13,	/* Czechia */
	ADW_REGION_DE = 14,	/* Germany */
	ADW_REGION_DK = 15,	/* Denmark */
	ADW_REGION_EE = 16,	/* Estonia */
	ADW_REGION_ES = 17,	/* Spain */
	ADW_REGION_FI = 18,	/* Finland */
	ADW_REGION_FR = 19,	/* France */
	ADW_REGION_GB = 20,	/* Great Britain */
	ADW_REGION_GR = 21,	/* Greece */
	ADW_REGION_HK = 22,	/* Hong Kong */
	ADW_REGION_HR = 23,	/* Croatia */
	ADW_REGION_HU = 24,	/* Hungary */
	ADW_REGION_IE = 25,	/* Ireland */
	ADW_REGION_IN = 26,	/* India */
	ADW_REGION_IS = 27,	/* Iceland */
	ADW_REGION_IT = 28,	/* Italy */
	ADW_REGION_KR = 29,	/* Korea */
	ADW_REGION_LI = 30,	/* Liechtenstein */
	ADW_REGION_LT = 31,	/* Lithuania */
	ADW_REGION_LU = 32,	/* Luxembourg */
	ADW_REGION_LV = 33,	/* Latvia */
	ADW_REGION_MT = 34,	/* Malta */
	ADW_REGION_MX = 35,	/* Mexico */
	ADW_REGION_NL = 36,	/* Netherlands */
	ADW_REGION_NO = 37,	/* Norway */
	ADW_REGION_NZ = 38,	/* New Zealand */
	ADW_REGION_PL = 39,	/* Poland */
	ADW_REGION_PT = 40,	/* Portugal */
	ADW_REGION_RO = 41,	/* Romania */
	ADW_REGION_SE = 42,	/* Sweden */
	ADW_REGION_SI = 43,	/* Slovenia */
	ADW_REGION_SK = 44,	/* Slovakia */
	ADW_REGION_TW = 45,	/* Taiwan */
};

/*
 * Initializer for region tokens string.
 * This must be in the order of the enum above.
 */
#define ADW_REGION_TOKENS \
	"\0\0"	/* 0 default */ \
	"US"	/* 1 United Status */ \
	"CN"	/* 2 China */ \
	"EU"	/* 3 EU */ \
	"JP"	/* 4 Japan */ \
	"CA"	/* 5 Canada */ \
	"AU"	/* 6 Australia */ \
	"AT"	/* 7 Austria */ \
	"BE"	/* 8 Belgium */ \
	"BG"	/* 9 Bulgaria */ \
	"BR"	/* 10 Brazil */ \
	"CH"	/* 11 Switzerland */ \
	"CY"	/* 12 Cyprus */ \
	"CZ"	/* 13 Czechia */ \
	"DE"	/* 14 Germany */ \
	"DK"	/* 15 Denmark */ \
	"EE"	/* 16 Estonia */ \
	"ES"	/* 17 Spain */ \
	"FI"	/* 18 Finland */ \
	"FR"	/* 19 France */ \
	"GB"	/* 20 Great Britain */ \
	"GR"	/* 21 Greece */ \
	"HK"	/* 22 Hong Kong */ \
	"HR"	/* 23 Croatia */ \
	"HU"	/* 24 Hungary */ \
	"IE"	/* 25 Ireland */ \
	"IN"	/* 26 India */ \
	"IS"	/* 27 Iceland */ \
	"IT"	/* 28 Italy */ \
	"KR"	/* 29 Korea */ \
	"LI"	/* 30 Liechtenstein */ \
	"LT"	/* 31 Lithuania */ \
	"LU"	/* 32 Luxembourg */ \
	"LV"	/* 33 Latvia */ \
	"MT"	/* 34 Malta */ \
	"MX"	/* 35 Mexico */ \
	"NL"	/* 36 Netherlands */ \
	"NO"	/* 37 Norway */ \
	"NZ"	/* 38 New Zealand */ \
	"PL"	/* 39 Poland */ \
	"PT"	/* 40 Portugal */ \
	"RO"	/* 41 Romania */ \
	"SE"	/* 42 Sweden */ \
	"SI"	/* 43 Slovenia */ \
	"SK"	/* 44 Slovakia */ \
	"TW"	/* 45 Taiwan */ \
	""	/* end */

#endif /* __AYLA_COUNTRY_CODES_H__ */
