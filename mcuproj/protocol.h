
// x: 0~320 ~ 9bit
// y: 0~320
// width: 0~320
// height: 0~320
// color: 16bit
// 6bit / char byte
// => needs at least 9byte to send

// 01------01------01------01------01------01------01------01------01------
//   [    x    ][    y    ]  [    w    ][    h    ]   [ r ]  [  g ]   [ b ]
typedef struct {
	unsigned char c[9];
} DrawRect_t;

#define drawrect_read_x(d) (((unsigned int)((d).c[0] & 0x3F) << 3) | (((d).c[1] >> 3) & 0x07))
#define drawrect_read_y(d) (((unsigned int)((d).c[1] & 0x07) << 6) | (((d).c[2] >> 0) & 0x3F))
#define drawrect_read_w(d) (((unsigned int)((d).c[3] & 0x3F) << 3) | (((d).c[4] >> 3) & 0x07))
#define drawrect_read_h(d) (((unsigned int)((d).c[4] & 0x07) << 6) | (((d).c[5] >> 0) & 0x3F))
#define drawrect_read_r(d) ((d).c[6] & 0x1F)
#define drawrect_read_g(d) ((d).c[7] & 0x3F)
#define drawrect_read_b(d) ((d).c[8] & 0x1F)

#define drawrect_write_init(d) memset(&(d), 0x40, 9);
#define drawrect_write_x(d, x) (d).c[0] |= 0x3F & ((x)>>3); (d).c[1] |= 0x38 & ((x)<<3);
#define drawrect_write_y(d, y) (d).c[1] |= 0x07 & ((y)>>6); (d).c[2] |= 0x3F & ((y)<<0);
#define drawrect_write_w(d, w) (d).c[3] |= 0x3F & ((w)>>3); (d).c[4] |= 0x38 & ((w)<<3);
#define drawrect_write_h(d, h) (d).c[4] |= 0x07 & ((h)>>6); (d).c[5] |= 0x3F & ((h)<<0);
#define drawrect_write_r(d, r) (d).c[6] |= 0x1F & (r);
#define drawrect_write_g(d, g) (d).c[7] |= 0x3F & (g);
#define drawrect_write_b(d, b) (d).c[8] |= 0x1F & (b);
