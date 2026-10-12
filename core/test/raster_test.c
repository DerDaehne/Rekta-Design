/* A first look at the renderer: shapes, icons and text at 2x, written as a PNG. */
#include "../src/rk_internal.h"
#include "../src/rk_icons.h"

int main(int argc, char **argv) {
  float k = 2;
  rk_bitmap *b = rk__bitmap_create((int)(720 * k), (int)(420 * k));
  rk__bitmap_clear(b, 0xffffffffu);
  rk_canvas c; memset(&c, 0, sizeof c);
  rk__canvas_init(&c, b, k);
  rk_canvas_fill_rect(&c, (rk_rect){ 16, 16, 150, 150 }, rk_color_of(RK_ROLE_INFO_TILE));
  rk_canvas_fill_round_rect(&c, (rk_rect){ 174, 16, 150, 150 }, 12, rk_color_of(RK_ROLE_SUCCESS_TILE));
  rk_canvas_fill_round_rect(&c, (rk_rect){ 332.3f, 16.4f, 150, 72 }, 2, rk_color_of(RK_ROLE_WARNING_TILE));
  rk_canvas_stroke_rect(&c, (rk_rect){ 332, 96, 150, 44 }, 2, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "deploy", (rk_point){ 348, 104 }, RK_TEXT_STRONG, rk_color_of(RK_ROLE_TEXT));
  rk_point spark[8] = { {490,60},{520,50},{550,55},{580,35},{610,40},{640,22},{670,27},{700,12} };
  rk_canvas_polyline(&c, spark, 8, 2, rk_color_of(RK_ROLE_INFO_FULL));
  rk_canvas_text(&c, "12", (rk_point){ 28, 100 }, RK_TEXT_STAT, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "builds today", (rk_point){ 28, 140 }, RK_TEXT_SMALL, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "active", (rk_point){ 126, 24 }, RK_TEXT_LABEL, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "Tiles, not cards. AV To Wa 0123456789", (rk_point){ 16, 176 }, RK_TEXT_GROUP, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "summary", (rk_point){ 16, 210 }, RK_TEXT_PIVOT, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_text(&c, "activity  files", (rk_point){ 160, 222 }, RK_TEXT_BODY, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  rk_canvas_text(&c, "A kicker, tracked", (rk_point){ 16, 262 }, RK_TEXT_KICKER, rk_color_of(RK_ROLE_TEXT_HIGH));
  for (int i = 0; i < RK_ICON_COUNT; i++) {
    float x = 16 + (i % 17) * 32, y = 290 + (i / 17) * 32;
    rk_canvas_icon(&c, rk_icon_defs[i].name, (rk_rect){ x, y, 20, 20 }, rk_color_of(RK_ROLE_TEXT));
  }
  bool ok = rk__png_write(b, argc > 1 ? argv[1] : "raster.png");
  rk__bitmap_free(b);
  return ok ? 0 : 1;
}
