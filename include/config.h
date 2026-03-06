#ifndef CONFIG_H
#define CONFIG_H

/*
 * Color encoding (fits in a plain int):
 *   -1            = no color / terminal default
 *   0..255        = 256-color ANSI index
 *   0x01RRGGBB    = 24-bit true color (bit 24 set)
 */
#define EKO_COLOR_NONE       (-1)
#define EKO_COLOR_IS_NONE(c) ((c) < 0)
#define EKO_COLOR_IS_RGB(c)  ((c) > 255)
#define EKO_COLOR_RGB(r,g,b) (0x01000000 | ((r) << 16) | ((g) << 8) | (b))
#define EKO_COLOR_R(c)       (((c) >> 16) & 0xFF)
#define EKO_COLOR_G(c)       (((c) >> 8) & 0xFF)
#define EKO_COLOR_B(c)       ((c) & 0xFF)

int eko_color_fg(int color, char *buf, int bufsz);
int eko_color_bg(int color, char *buf, int bufsz);

struct eko_config {
    int  tab_size;
    int  expand_tabs;
    char theme[64];
    int  color_comment;
    int  color_keyword1;
    int  color_keyword2;
    int  color_string;
    int  color_number;
    int  color_match;
    int  color_bg;
};

extern struct eko_config config;

void config_load(void);

#endif
