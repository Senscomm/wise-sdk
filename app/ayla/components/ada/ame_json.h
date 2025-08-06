/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_AME_JSON_H__
#define __AYLA_ADA_AME_JSON_H__

/* A JSON encoder */
extern const struct ame_encoder ame_json_enc;
/* A JSON decoder */
extern const struct ame_decoder ame_json_dec;
#ifdef AME_ENCODER_TEST
void ame_json_test(void);
#endif
#endif
