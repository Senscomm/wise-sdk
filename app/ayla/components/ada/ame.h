/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * Ayla Message Encoding support
 */
#ifndef __AYLA_ADA_AME_H__
#define __AYLA_ADA_AME_H__

#include <string.h>
#include <ada/err.h>

/*
 * Encoding flags control how the encoder delimits and nests key-value pairs.
 * The flags are listed in the order they will be applied by the encoder.
 * Prefix flags are applied prior to encoding new data while suffix flags are
 * applied after encoding data. The characters that will be inserted for JSON
 * encoding are shown in the comments.
 */
#define EF_PREFIX_C	0x80	/* continuation of object               , */
#define EF_PREFIX_A	0x40	/* start of array                       [ */
#define EF_PREFIX_O	0x20	/* start of object                      { */

#define EF_PREFIX_MASK	0xe0	/* mask for prefix flags only */

#define EF_PREFIX_CAO	(EF_PREFIX_C | EF_PREFIX_A | EF_PREFIX_O) /*    ,[{ */
#define EF_PREFIX_CA	(EF_PREFIX_C | EF_PREFIX_A)		/*      ,[ */
#define EF_PREFIX_CO	(EF_PREFIX_C | EF_PREFIX_O)		/*      ,{ */
#define EF_PREFIX_AO	(EF_PREFIX_A | EF_PREFIX_O)		/*      [{ */

#define EF_SUFFIX_E	0x04	/* end of object                        } */
#define EF_SUFFIX_Z	0x02	/* end of array                         ] */
#define EF_SUFFIX_M	0x01	/* more elements to come                , */

#define EF_SUFFIX_MASK	0x07	/* mask for suffix flags only */

#define EF_SUFFIX_EZM	(EF_SUFFIX_E | EF_SUFFIX_Z | EF_SUFFIX_M) /*    }], */
#define EF_SUFFIX_EZ	(EF_SUFFIX_E | EF_SUFFIX_Z)		/*      }] */
#define EF_SUFFIX_EM	(EF_SUFFIX_E | EF_SUFFIX_M)		/*      }, */
#define EF_SUFFIX_ZM	(EF_SUFFIX_Z | EF_SUFFIX_M)		/*      ], */

/*
 * Value types
 */
enum ame_type {
	AME_TYPE_UNKNOWN = 0,		/* type not yet known */
	AME_TYPE_OBJECT,		/* value is an object */
	AME_TYPE_ARRAY,			/* value is an array */
	AME_TYPE_NULL,			/* value is null */
	AME_TYPE_BOOLEAN,		/* value 0 or 1 */
	AME_TYPE_UTF8,			/* value UTF8 octet string */
	AME_TYPE_INTEGER,		/* value is an integer number */
	AME_TYPE_DECIMAL,		/* value is a decimal number */
	AME_TYPE_FLOAT,			/* value is floating point */
	AME_TYPE_OPAQUE,		/* opaque data */
	AME_TYPE_COUNT			/* number of valid enum values */
};

#define AME_TYPE_NAMES { \
	[AME_TYPE_UNKNOWN] = "unknown", \
	[AME_TYPE_OBJECT] = "object", \
	[AME_TYPE_ARRAY] = "array", \
	[AME_TYPE_NULL] = "null", \
	[AME_TYPE_BOOLEAN] = "bool", \
	[AME_TYPE_UTF8] = "UTF-8", \
	[AME_TYPE_INTEGER] = "int", \
	[AME_TYPE_DECIMAL] = "dec", \
	[AME_TYPE_FLOAT] = "float", \
	[AME_TYPE_OPAQUE] = "opaque", \
}

/*
 * Keys are currently just field name strings that can be used in JSON and
 * other encoding schemes. The key structure is intended to provide
 * flexibility to move to a TLV or schema-based encoding in the future. All
 * of the APIs use keys rather than names to preserve future flexibility.
 */
struct ame_key {
	const char *tag;
	const char *display_name;
};

/*
 * Macros for defining keys.
 */
#define AME_KEY_INIT(tag, name)	{ (tag), (name) }

#define AME_KEY(var, tag, name) \
	struct ame_key var = AME_KEY_INIT(tag, name)

#define AME_KEY1(var, tag)	AME_KEY(var, tag, tag)

/*
 * Key-value pair structure is used for decoding messages
 */
struct ame_kvp {
	enum ame_type type;
	struct ame_kvp *parent;	/* parent of this pair, NULL if root */
	struct ame_kvp *child;	/* child object or array */
	struct ame_kvp *next;	/* next kv_pair for parent */
	const void *key;	/* start of key data  */
	size_t key_length;	/* length of encoded key */
	const void *value;	/* start of encoded value */
	size_t value_length;	/* length of encoded value */
	const struct ame_tag *template_node; /* NULL if to be skipped */
};

/*
 * Parser state.
 */
enum ame_parser_state {
	AME_STATE_VALUE,	/* expect a value */
	AME_STATE_NAME,		/* expect name of a name-value pair */
	AME_STATE_SEPARATOR,	/* expect name value separator */
	AME_STATE_CONTINUE,	/* expect value-separator or end of object */
	AME_STATE_VALUE_STR,
	AME_STATE_NAME_STR,
	AME_STATE_VALUE_LITERAL,
	AME_STATE_VALUE_NUM_0,
	AME_STATE_VALUE_NUM_1,
	AME_STATE_VALUE_NUM_2,
	AME_STATE_VALUE_NUM_3,
	AME_STATE_VALUE_NUM_3A,
	AME_STATE_VALUE_NUM_4,
	AME_STATE_VALUE_NUM_5,
	AME_STATE_VALUE_NUM_6,
};

#define AME_PARSER_STATE_NAMES { \
	[AME_STATE_VALUE] = "VALUE",	\
	[AME_STATE_NAME] = "NAME",	\
	[AME_STATE_SEPARATOR] = "SEP",	\
	[AME_STATE_CONTINUE] = "CONT",	\
	[AME_STATE_VALUE_STR] = "VALUE_STR",	\
	[AME_STATE_NAME_STR] = "NAME_STR",	\
	[AME_STATE_VALUE_LITERAL] = "VALUE_LIT",	\
	[AME_STATE_VALUE_NUM_0] = "VALUE_NUM_0",	\
	[AME_STATE_VALUE_NUM_1] = "VALUE_NUM_1",	\
	[AME_STATE_VALUE_NUM_2] = "VALUE_NUM_2",	\
	[AME_STATE_VALUE_NUM_3] = "VALUE_NUM_3",	\
	[AME_STATE_VALUE_NUM_3A] = "VALUE_NUM_3A",	\
	[AME_STATE_VALUE_NUM_4] = "VALUE_NUM_4",	\
	[AME_STATE_VALUE_NUM_5] = "VALUE_NUM_5",	\
	[AME_STATE_VALUE_NUM_6] = "VALUE_NUM_6",	\
}

/*
 * Structure to hold the state of encoding for a particular buffer
 */
struct ame_encoder_state {
	enum ada_err err;
	u8 *buffer;
	size_t size;
	size_t offset;
};

/*
 * Structure for an encoder instance.
 */
struct ame_encoder {
	/* Encode prefix only */
	void (*enc_prefix)(struct ame_encoder_state *es, u32 flags);
	/* Encode suffix only */
	void (*enc_suffix)(struct ame_encoder_state *es, u32 flags);
	/* Encode prefix and key portion of key-value pair */
	void (*enc_key)(struct ame_encoder_state *es, u32 flags,
	    const struct ame_key *key);
	/* Encode a UTF8 string */
	void (*enc_utf8)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, const char *utf8);
	/* Encode an opaque binary buffer. With JSON, the buffer will
	 * first be base64 encoded */
	void (*enc_opaque)(struct ame_encoder_state *es, u32 flags,
	    const struct ame_key *key, const void *data, size_t length);
	/* Encode a signed 64 bit number */
	void (*enc_s64)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s64 value);
	/* Encode a signed 32 bit number */
	void (*enc_s32)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s32 value);
	/* Encode a signed 16 bit number */
	void (*enc_s16)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s16 value);
	/* Encode a signed 8 bit number */
	void (*enc_s8)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s8 value);
	/* Encode a unsigned 64 bit number */
	void (*enc_u64)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, u64 value);
	/* Encode a unsigned 32 bit number */
	void (*enc_u32)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, u32 value);
	/* Encode a unsigned 16 bit number */
	void (*enc_u16)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, u16 value);
	/* Encode a unsigned 8 bit number */
	void (*enc_u8)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, u8 value);
	/* Encode a decimal (signed, base10-scaled) 32 bit number
	 * real_value = value / (10 ^ scale) */
	void (*enc_d32)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s32 value, u8 scale);
	/* Encode a boolean value */
	void (*enc_boolean)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key, s32 value);
	/* Encode a null value */
	void (*enc_null)(struct ame_encoder_state *es,
	    u32 flags, const struct ame_key *key);
};

/*
 * Decoder parsing options
 */
enum ame_parse_op {
	AME_PARSE_FULL,		/* fully parse the buffer */
	AME_PARSE_FIRST,	/* parse first value object */
	AME_PARSE_NEXT		/* parse next value object */
};

/*
 * Structure to hold the state of decoder for a particular buffer
 */
struct ame_decoder_state {
	enum ame_parser_state state;

	/*
	 * KVP allocation state.
	 */
	struct ame_kvp *kvp_stack; /* stack for allocations of kvps */
	size_t kvp_count;	/* number of kvps allowed to be allocated */
	size_t kvp_used;	/* number of kvps allocated */

	/*
	 * Input buffer state.
	 */
	const u8 *buffer;	/* input buffer */
	size_t length;		/* length of input buffer */
	size_t offset;		/* next input character offset in input */
	size_t error_offset;	/* buffer offset where error occurred */

	/*
	 * Nodes relating to current parse, tree, and current node.
	 */
	struct ame_kvp *parent;	/* parent of current node on stack */
	struct ame_kvp *kvp;	/* node being parsed */
	struct ame_kvp *kvp1;	/* parent of node at AME_STATE_CONTINUE */
	struct ame_kvp *incr_parent; /* root of incremental object in parse */
	struct ame_kvp *pkvp;	/* node parsed at previous incremental return */

	/*
	 * Value substates and flags.
	 * These flags are used in parser-dependent ways.
	 * Keep flags together for smaller structure size.
	 */
	const u8 *literal;	/* pointer to rest of expected literal */
	u8 hexdigs;		/* number of hex digits expected */
	u8 incremental:1;	/* return at end of sub-objects */
	u8 escape:1;		/* previous byte was an escape */
	u8 is_first:1;		/* expecting first item in object or array */
	u8 first_called:1;	/* AME_PARSE_FIRST performed */
	u8 obj_parsed:1;	/* pkvp has been parsed */
	u8 ext_mode:1;		/* extended mode */
	enum ada_err err;	/* error from previous incremental parse */

	/*
	 * Extended / External mode parsed-data buffer.
	 */
	u8 *data_buf;		/* parsed data */
	size_t data_buf_size;
	size_t data_len;	/* length of parsed data */

	/*
	 * Template tree of desired keys.  Null if all keys desired.
	 */
	const struct ame_tag *template_tree; /* (optional) desired keys */
	void *template_arg;	/* argument for handler callback */
	const struct ame_decoder *dec; /* used only if template_tree assigned */
};

/*
 * Decoder parses a received message hierarchy into a stack of ame_kv_pairs.
 *
 * Once a message has been parsed, access functions provide
 * for retrieving data from the object.
 */
struct ame_decoder {
	/* Parse the decoder buffer as indicated by the op arg */
	enum ada_err (*parse)(struct ame_decoder_state *ds,
	    enum ame_parse_op op, struct ame_kvp **pkvp);
	/* Decode opaque binary data from buffer. With JSON, the
	 * data must be a base64 encoded JSON string */
	enum ada_err (*get_opaque)(struct ame_kvp *parent,
	    const struct ame_key *key, void *value, size_t *length);
	/* Decode a UTF8 string */
	enum ada_err (*get_utf8)(struct ame_kvp *parent,
	    const struct ame_key *key, char *value, size_t *length);
	/* Decode a signed 64 bit number */
	enum ada_err (*get_s64)(struct ame_kvp *parent,
	    const struct ame_key *key, s64 *value);
	/* Decode a signed 32 bit number */
	enum ada_err (*get_s32)(struct ame_kvp *parent,
	    const struct ame_key *key, s32 *value);
	/* Decode a signed 16 bit number */
	enum ada_err (*get_s16)(struct ame_kvp *parent,
	    const struct ame_key *key, s16 *value);
	/* Decode a signed 8 bit number */
	enum ada_err (*get_s8)(struct ame_kvp *parent,
	    const struct ame_key *key, s8 *value);
	/* Decode an unsigned 64 bit number */
	enum ada_err (*get_u64)(struct ame_kvp *parent,
	    const struct ame_key *key, u64 *value);
	/* Decode an unsigned 32 bit number */
	enum ada_err (*get_u32)(struct ame_kvp *parent,
	    const struct ame_key *key, u32 *value);
	/* Decode an unsigned 16 bit number */
	enum ada_err (*get_u16)(struct ame_kvp *parent,
	    const struct ame_key *key, u16 *value);
	/* Decode an unsigned 8 bit number */
	enum ada_err (*get_u8)(struct ame_kvp *parent,
	    const struct ame_key *key, u8 *value);
	/* Decode a signed base10-scaled 32 bit number
	 * value = (s32)(number * (10 ^ scale)) */
	enum ada_err (*get_d32)(struct ame_kvp *parent,
	    const struct ame_key *key, s32 *value, u8 scale);
	/* Decode boolean value */
	enum ada_err (*get_boolean)(struct ame_kvp *parent,
	    const struct ame_key *key, s32 *value);
};

/*
 * Structure of ame_tag, which can optionally limit parse.
 */
struct ame_tag {
	const char *name;
	const struct ame_tag *subtags;
	int (*cb)(const struct ame_decoder *dec, void *arg,
	    struct ame_kvp *kvp);
};

/*
 * Macro AME_TAG is used to init tag / template tree for object to be parsed.
 */
#define AME_TAG(_name, _subtags, _cb) \
	{				\
		.name = _name,		\
		.subtags = _subtags,	\
		.cb = _cb,		\
	}

#define AME_TAG_END	AME_TAG(NULL, NULL, NULL)

/*
 * Singleton value for ame_tag.subtags which indicates that the contents of the
 * object should be passed in pieces to the callback as received.
 *
 * This is useful when parsing a large object.
 *
 * After all parts are presented to the callback, it is called with a NULL kvp.
 */
extern const struct ame_tag ame_tag_partial;
#define AME_TAG_PARTIAL(_name, _cb) AME_TAG(_name, &ame_tag_partial, _cb)

/*
 * Singleton value for ame_tag.subtags which indicates that the object should
 * be parsed and the callback called once all of the sub-objects are fully
 * parsed.  No sub objects will have callbacks.
 *
 * This is useful when parsing objects containing unknown fields.
 */
extern const struct ame_tag ame_tag_full;
#define AME_TAG_FULL(_name, _cb) AME_TAG(_name, &ame_tag_full, _cb)

/* Get the string name for the KVP type */
const char *ame_kvp_type_name(enum ame_type type);
/* Allocate the next key-value pair from the kvp stack */
enum ada_err ame_kvp_alloc(struct ame_decoder_state *ds,
    struct ame_kvp **new_kvp);
/* Free the leaf key-value pair from the stack */
enum ada_err ame_kvp_pop(struct ame_decoder_state *ds);
/*
 * Get a direct child key-value pair of the specified parent
 *	parent - the parent object to get from
 *	key - the key that identifies the child
 *	child - return value, NULL if not found
 */
enum ada_err ame_get_child(struct ame_kvp *parent, const struct ame_key *key,
    struct ame_kvp **child);

/* Set the output buffer for the encoder */
static inline void ame_encoder_buffer_set(struct ame_encoder_state *es,
    u8 *buffer, size_t size)
{
	es->buffer = buffer;
	es->size = size;
	es->offset = 0;
	es->err = AE_OK;
}

/* Set the key-value pair stack for the decoder */
static inline void ame_decoder_stack_set(struct ame_decoder_state *ds,
    struct ame_kvp *stack, size_t count)
{
	memset(ds, 0, sizeof(*ds));
	memset(stack, 0, count * sizeof(*stack));
	ds->state = AME_STATE_VALUE;
	ds->kvp_stack = stack;
	ds->kvp_count = count;
}

/* Reset the decoder */
static inline void ame_decoder_reset(struct ame_decoder_state *ds)
{
	ds->state = AME_STATE_VALUE;
	memset(ds->kvp_stack, 0, ds->kvp_count * sizeof(*ds->kvp_stack));
	/* ds->kvp_count; keep unchanged */
	ds->kvp_used = 0;
	ds->parent = NULL;
	/* ds->buffer; keep unchanged */
	/* ds->length; keep unchanged */
	/* ds->offset; keep unchanged */
	/* ds->error_offset; ignore */
	ds->kvp = NULL;
	ds->kvp1 = NULL;
	ds->incr_parent = NULL;
	/* ds->incremental; keep unchanged */
	ds->pkvp = NULL;
	ds->is_first = 0;
	ds->first_called = 0;
	ds->obj_parsed = 0;
	ds->escape = 0;
	ds->hexdigs = 0;
	ds->literal = NULL;
	/* ds->ext_mode; keep unchanged */
	if (ds->data_buf) {
		memset(ds->data_buf, 0, ds->data_buf_size);
	}
	/* ds->data_buf_size; keep unchanged */
	ds->data_len = 0;
}

/*
 * Set the decode buffer. When decoding incrementally, this function
 * may be called multiple times to feed new data to the decoder. The
 * caller should take care to ensure any unconsumed data from previous
 * parsing calls is at the start of the buffer that is being set.
 */
static inline void ame_decoder_buffer_set(struct ame_decoder_state *ds,
    const u8 *buffer, size_t length)
{
	ds->buffer = buffer;
	ds->length = length;
	ds->offset = 0;
}

/*
 * Enable or disable extended mode for ame-json.
 *
 * Feature of extension mode:
 *   (1). Parser returns on each array-element is parsed.
 *   (2). Support data input like a stream which is composed data blocks
 *	  set by ame_decoder_buffer_set(). This is important for ame-json
 *	  AME_PARSE_FIRST/AME_PARSE_NEXT operations.
 *   (3). Extended mode does not support AME_PARSE_FULL operation.
 */
void ame_decoder_set_ext_mode(struct ame_decoder_state *ds, u8 on,
	u8 *parsed_data_buf, size_t buf_size);

/*
 * Get the first child of the specified parent. If the parent is an
 * array, this will return the first array element. If the parent
 * is an object, the children are in not in any particular order.
 */
static inline struct ame_kvp *ame_get_first(struct ame_kvp *parent)
{
	return parent->child;
}

/*
 * Get the next child with the same parent. If the parent is an
 * array, this will return the next array element.
 */
static inline struct ame_kvp *ame_get_next(struct ame_kvp *current)
{
	return current->next;
}

/*
 * Allocate a decoder state with a kvp stack from the heap.
 */
struct ame_decoder_state *ame_decoder_state_alloc(int stack_size);

/*
 * Free a previously allocated decoder state.
 */
void ame_decoder_state_free(struct ame_decoder_state *ds);

/*
 * Find kvp matching key as child of the parent.
 */
struct ame_kvp *ame_find_child(const struct ame_kvp *parent,
				const struct ame_key *);

/*
 * Set the template tree of tags for the decoder.
 * Causes other tags to be skipped during parse.
 * Used in ext mode only, to save buffer space.
 * The parsed tree nodes point at the template tree, so it must be
 * persistent during the parse.
 */
static inline void ame_decoder_tree_set(struct ame_decoder_state *ds,
				const struct ame_decoder *dec,
				const struct ame_tag *tree, void *arg)
{
	ds->dec = dec;
	ds->template_tree = tree;
	ds->template_arg = arg;
}

/*
 * Find the associated template node, if the given kvp is in the desired tree.
 */
void ame_decoder_template_find(struct ame_decoder_state *, struct ame_kvp *);

#ifdef AME_ENCODER_TEST
extern u8 ame_test_output[256];
extern struct ame_kvp ame_test_stack[50];
void ame_test(const struct ame_encoder *enc, struct ame_encoder_state *es,
    const struct ame_decoder *dec, struct ame_decoder_state *ds);
void ame_test_dump(const u8 *buf, size_t size);

void ame_test_kvp_print(struct ame_kvp *kvp, unsigned int depth);
void ame_test_kvp_tree_print(struct ame_decoder_state *ds,
	struct ame_kvp *parent, unsigned int depth);
#endif

#endif
