/*! \file
 * \brief Raw 8-bit data
 *
 * Copyright (C) 2008, Digium, Inc.
 *
 * Distributed under the terms of the GNU General Public License
 *
 */

static uint8_t ex_ilbc[] = {
	0xff, 0xa0, 0xff, 0xfa, 0x0f, 0x60, 0x12, 0x11, 0xa2, 0x47, 
	0x22, 0x8c, 0x00, 0x00, 0x01, 0x02, 0x80, 0x43, 0xa0, 0x40, 
	0x33, 0xff, 0xcf, 0xc0, 0xf3, 0xf3, 0x3f, 0x8f, 0x3f, 0xff, 
	0xff, 0xff, 0xff, 0xfc, 0xf9, 0xe5, 0x55, 0x78, 0x0b, 0xca, 
	0xe1, 0x27, 0x94, 0x7b, 0xa8, 0x91, 0x2c, 0x36, 0x08, 0x56,
};

static struct ast_frame *ilbc_sample(void)
{
	static struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.datalen = sizeof(ex_ilbc),
		/* All frames are 30 ms long */
		.samples = ILBC_SAMPLES,
		.mallocd = 0,
		.offset = 0,
		.src = __PRETTY_FUNCTION__,
		.data.ptr = ex_ilbc,
	};

	ast_format_set(&f.subclass.format, AST_FORMAT_ILBC, 0);
	return &f;
}
