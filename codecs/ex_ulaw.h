/*! \file
 * \brief 8-bit data
 *
 * Copyright (C) 2008, Digium, Inc.
 *
 * Distributed under the terms of the GNU General Public License
 *
 */

static uint8_t ex_ulaw[] = {
	0x00, 0x03, 0x06, 0x09, 0x0c, 0x0f, 0x12, 0x15,
	0x10, 0x18, 0x1b, 0x1e, 0x21, 0x24, 0x27, 0x2a,
	0x20, 0x2d, 0x30, 0x33, 0x36, 0x39, 0x3c, 0x3f,
	0x30, 0x42, 0x45, 0x48, 0x4b, 0x4e, 0x51, 0x54,
	0x40, 0x57, 0x5a, 0x5d, 0x60, 0x63, 0x66, 0x69,
	0x50, 0x6c, 0x6f, 0x72, 0x75, 0x78, 0x7b, 0x7e,
	0x60, 0x81, 0x84, 0x87, 0x8a, 0x8d, 0x90, 0x93,
	0x70, 0x96, 0x99, 0x9c, 0x9f, 0xa2, 0xa5, 0xa8,
	0x80, 0xab, 0xae, 0xb1, 0xb4, 0xb7, 0xba, 0xbd,
	0x90, 0xc0, 0xc3, 0xc6, 0xc9, 0xcc, 0xcf, 0xd2,
};

static struct ast_frame *ulaw_sample(void)
{
	static struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.datalen = sizeof(ex_ulaw),
		.samples = ARRAY_LEN(ex_ulaw),
		.mallocd = 0,
		.offset = 0,
		.src = __PRETTY_FUNCTION__,
		.data.ptr = ex_ulaw,
	};

	ast_format_set(&f.subclass.format, AST_FORMAT_ULAW, 0);
	return &f;
}
