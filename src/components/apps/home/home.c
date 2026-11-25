/*
 * Paper & Earth OS — watchOS-direction home screen
 *
 * 480x480 LVGL v8 port of html-mosaico-ui/index-watchos.html.
 *
 * The static parts of the design are pre-rendered as EZIP images (status
 * bar, quick folder, token card, VIBE button, action buttons and app drawer).
 * This keeps the EPIC render list and the LVGL heap small: the home screen
 * only needs a handful of image draws plus the live clock/date labels.
 */

#include <rtthread.h>
#include <stdio.h>
#include <time.h>

#include "gui_app_fwk.h"
#include "lvgl.h"
#include "lvsf.h"

#define APP_ID "home"

#define C_FACE 0x000000
#define C_SURFACE1 0x2C2C2E
#define C_SURFACE2 0x3A3A3C
#define C_WHITE 0xFFFFFF
#define C_GRAY 0x8E8E93
#define C_ORANGE 0xFF9F0A
#define C_GREEN 0x30D158
#define C_TRACK 0x3A3A3C
#define C_BLACK 0x000000

#define SCREEN_PAD 20
#define CONTENT_W (LV_HOR_RES_MAX - 2 * SCREEN_PAD)
#define CONTENT_H (LV_VER_RES_MAX - 2 * SCREEN_PAD)

LV_IMG_DECLARE(wos_status);
LV_IMG_DECLARE(wos_folder);
LV_IMG_DECLARE(wos_chart);
LV_IMG_DECLARE(wos_openai);
LV_IMG_DECLARE(wos_code_black);
LV_IMG_DECLARE(wos_arrows_black);
LV_IMG_DECLARE(wos_camera_btn);
LV_IMG_DECLARE(wos_apps_btn);
LV_IMG_DECLARE(wos_apps_view);

static lv_obj_t *s_root;
static lv_obj_t *s_home;
static lv_obj_t *s_apps;
static lv_obj_t *s_clock;
static lv_obj_t *s_date;
static lv_obj_t *s_toast;
static lv_obj_t *s_toast_label;
static lv_timer_t *s_clock_timer;
static lv_timer_t *s_toast_timer;

static const char *const s_app_names[4] = {"设置", "天气", "音乐", "游戏"};

static void app_click_cb(lv_event_t *event);
static void open_apps_cb(lv_event_t *event);
static void close_apps_cb(lv_event_t *event);
static void vibe_click_cb(lv_event_t *event);
static void camera_click_cb(lv_event_t *event);

static lv_obj_t *new_obj(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t w, lv_coord_t h, uint32_t bg,
                         lv_coord_t radius) {
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

  return obj;
}

static lv_obj_t *new_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h) {
  lv_obj_t *obj = lv_obj_create(parent);

  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

  return obj;
}

static lv_obj_t *new_label(lv_obj_t *parent, const lv_font_t *font,
                           uint32_t color, const char *text) {
  lv_obj_t *label = lv_label_create(parent);

  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

  return label;
}

static lv_obj_t *new_img(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         const void *src) {
  lv_obj_t *img = lv_img_create(parent);

  lv_img_set_src(img, src);
  lv_obj_set_pos(img, x, y);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);

  return img;
}

static lv_obj_t *new_click(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, lv_event_cb_t cb,
                           void *user_data) {
  lv_obj_t *obj = new_container(parent, x, y, w, h);

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, user_data);

  return obj;
}

static void set_flex(lv_obj_t *obj, lv_flex_flow_t flow, lv_flex_align_t main,
                     lv_flex_align_t cross) {
  lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(obj, flow);
  lv_obj_set_flex_align(obj, main, cross, cross);
}

static void add_border(lv_obj_t *obj, uint32_t color, lv_coord_t width) {
  lv_obj_set_style_border_width(obj, width, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

static void build_status(lv_obj_t *parent) {
  new_img(parent, 0, 0, &wos_status);
}

static void build_top(lv_obj_t *parent) {
  lv_obj_t *left = new_container(parent, 0, 30, 239, 144);
  lv_obj_t *clock_box = new_container(left, 0, 20, 239, 72);
  lv_obj_t *folder = new_obj(parent, 256, 30, 184, 144, C_SURFACE1, 28);
  static const lv_coord_t app_pos[4][2] = {
      {267, 41}, {383, 41}, {267, 117}, {383, 117}};
  static const lv_coord_t dot_pos[2] = {333, 87};

  new_img(folder, 0, 0, &wos_folder);

  s_clock = new_label(clock_box, LV_EXT_FONT_GET(FONT_SUPER), C_WHITE, "00:00");
  lv_obj_set_pos(s_clock, 0, -16);
  lv_obj_set_width(s_clock, 239);
  lv_obj_set_style_text_letter_space(s_clock, -2, LV_PART_MAIN);

  s_date = new_label(left, LV_EXT_FONT_GET(FONT_SUBTITLE), C_GRAY, "");
  lv_obj_set_pos(s_date, 0, 98);
  lv_obj_set_width(s_date, 239);

  for (uint8_t i = 0; i < 4; i++) {
    new_click(parent, app_pos[i][0], app_pos[i][1], 48, 48, app_click_cb,
              (void *)s_app_names[i]);
  }

  new_click(parent, dot_pos[0], dot_pos[1], 32, 32, open_apps_cb, NULL);
}

static void build_token_card(lv_obj_t *parent) {
  lv_obj_t *card = new_obj(parent, 0, 186, CONTENT_W, 184, C_SURFACE1, 24);
  lv_obj_t *name_box;
  lv_obj_t *stat;
  lv_obj_t *track;
  lv_obj_t *reset;
  lv_obj_t *label;

  add_border(card, C_SURFACE2, 1);

  new_img(card, 18, 6, &wos_openai);

  name_box = new_container(card, 72, 10, 140, 36);
  set_flex(name_box, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START,
           LV_FLEX_ALIGN_CENTER);
  new_label(name_box, LV_EXT_FONT_GET(FONT_SUBTITLE), C_WHITE, "Codex");

  /* 5h quota */
  stat = new_container(card, 16, 54, 196, 29);
  set_flex(stat, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_SPACE_BETWEEN,
           LV_FLEX_ALIGN_END);
  label = new_label(stat, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "5h");
  lv_obj_set_style_translate_y(label, -1, LV_PART_MAIN);
  new_label(stat, LV_EXT_FONT_GET(FONT_SUBTITLE), C_WHITE, "99%");

  track = new_obj(card, 16, 87, 196, 6, C_TRACK, 3);
  new_obj(track, 0, 0, 196 * 99 / 100, 6, C_GREEN, 3);

  reset = new_container(card, 16, 100, 196, 19);
  set_flex(reset, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  label = new_label(reset, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "11/23 17:14");
  lv_obj_set_style_translate_y(label, 3, LV_PART_MAIN);

  /* Week quota */
  stat = new_container(card, 16, 117, 196, 24);
  set_flex(stat, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_SPACE_BETWEEN,
           LV_FLEX_ALIGN_END);
  new_label(stat, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "Week");
  new_label(stat, LV_EXT_FONT_GET(FONT_NORMAL), C_WHITE, "19%");

  track = new_obj(card, 16, 145, 196, 6, C_TRACK, 3);
  new_obj(track, 0, 0, 196 * 19 / 100, 6, C_GREEN, 3);

  reset = new_container(card, 16, 160, 196, 19);
  set_flex(reset, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  label = new_label(reset, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "11/29 09:45");
  lv_obj_set_style_translate_y(label, 3, LV_PART_MAIN);
  label = new_label(reset, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "×2");
  lv_obj_set_style_translate_y(label, 3, LV_PART_MAIN);
  lv_obj_add_flag(label, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);

  new_obj(card, 219, 14, 1, 159, C_SURFACE2, 0);
  new_img(card, 227, 14, &wos_chart);
}

static void build_actions(lv_obj_t *parent) {
  lv_obj_t *vibe = new_obj(parent, 0, 384, 296, 56, C_ORANGE, 28);
  lv_obj_t *title;
  lv_obj_t *arrows;

  set_flex(vibe, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(vibe, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_right(vibe, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_column(vibe, 10, LV_PART_MAIN);
  lv_obj_add_flag(vibe, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vibe, vibe_click_cb, LV_EVENT_CLICKED, NULL);

  new_img(vibe, 0, 0, &wos_code_black);
  title =
      new_label(vibe, LV_EXT_FONT_GET(FONT_SUBTITLE), C_BLACK, "VIBE CODING");
  lv_obj_set_style_translate_y(title, 4, LV_PART_MAIN);
  arrows = new_img(vibe, 0, 0, &wos_arrows_black);
  (void)arrows;

  new_img(parent, 312, 384, &wos_camera_btn);
  new_click(parent, 312, 384, 56, 56, camera_click_cb, NULL);

  new_img(parent, 384, 384, &wos_apps_btn);
  new_click(parent, 384, 384, 56, 56, open_apps_cb, NULL);
}

static void build_toast(lv_obj_t *parent) {
  lv_obj_t *pill;

  s_toast = new_container(parent, 0, 334, CONTENT_W, 30);
  lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

  pill = new_obj(s_toast, 0, 0, LV_SIZE_CONTENT, 30, C_SURFACE1, 15);
  lv_obj_set_style_border_width(pill, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(pill, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
  lv_obj_set_style_border_opa(pill, LV_OPA_COVER, LV_PART_MAIN);
  set_flex(pill, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(pill, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_right(pill, 12, LV_PART_MAIN);
  s_toast_label = new_label(pill, LV_EXT_FONT_GET(FONT_SMALL), C_WHITE, "");
  lv_obj_align(pill, LV_ALIGN_CENTER, 0, 0);
}

static void build_apps_view(lv_obj_t *parent) {
  static const lv_coord_t cell_pos[4][2] = {
      {0, 48}, {228, 48}, {0, 252}, {228, 252}};

  s_apps = new_container(parent, 0, 0, CONTENT_W, CONTENT_H);
  lv_obj_add_flag(s_apps, LV_OBJ_FLAG_HIDDEN);

  new_obj(s_apps, 0, 0, CONTENT_W, CONTENT_H, C_FACE, 0);
  new_img(s_apps, 0, 0, &wos_apps_view);

  new_click(s_apps, 408, 4, 32, 32, close_apps_cb, NULL);
  for (uint8_t i = 0; i < 4; i++) {
    new_click(s_apps, cell_pos[i][0], cell_pos[i][1], 212, 188, app_click_cb,
              (void *)s_app_names[i]);
  }
}

static void update_clock(void) {
  static const char *const week[] = {"日", "一", "二", "三", "四", "五", "六"};
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char buf[32];

  if (!tm) {
    return;
  }

  snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
  lv_label_set_text(s_clock, buf);

  snprintf(buf, sizeof(buf), "%d月%d日 周%s", tm->tm_mon + 1, tm->tm_mday,
           week[tm->tm_wday]);
  lv_label_set_text(s_date, buf);
}

static void clock_timer_cb(lv_timer_t *timer) {
  (void)timer;
  update_clock();
}

static void hide_toast(void) {
  if (s_toast) {
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
  }
}

static void toast_timer_cb(lv_timer_t *timer) {
  (void)timer;
  s_toast_timer = NULL;
  hide_toast();
}

static void show_toast(const char *text) {
  if (!s_toast || !s_toast_label) {
    return;
  }

  lv_label_set_text(s_toast_label, text);
  lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_toast);

  if (s_toast_timer) {
    lv_timer_del(s_toast_timer);
    s_toast_timer = NULL;
  }
  s_toast_timer = lv_timer_create(toast_timer_cb, 1500, NULL);
  lv_timer_set_repeat_count(s_toast_timer, 1);
}

static void app_click_cb(lv_event_t *event) {
  const char *name = (const char *)lv_event_get_user_data(event);
  char buf[32];

  if (s_apps) {
    lv_obj_add_flag(s_apps, LV_OBJ_FLAG_HIDDEN);
  }

  snprintf(buf, sizeof(buf), "打开 %s", name ? name : "");
  show_toast(buf);
}

static void open_apps_cb(lv_event_t *event) {
  (void)event;
  if (s_apps) {
    lv_obj_clear_flag(s_apps, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_apps);
  }
}

static void close_apps_cb(lv_event_t *event) {
  (void)event;
  if (s_apps) {
    lv_obj_add_flag(s_apps, LV_OBJ_FLAG_HIDDEN);
  }
}

static void vibe_click_cb(lv_event_t *event) {
  (void)event;
  show_toast("VIBE CODING");
}

static void camera_click_cb(lv_event_t *event) {
  (void)event;
  show_toast("摄像头");
}

static void build_home(lv_obj_t *parent) {
  build_status(parent);
  build_top(parent);
  build_token_card(parent);
  build_actions(parent);
  build_toast(parent);
  build_apps_view(parent);
}

static void on_start(void) {
  lv_obj_t *root;

  if (s_clock_timer) {
    lv_timer_del(s_clock_timer);
    s_clock_timer = NULL;
  }
  if (s_toast_timer) {
    lv_timer_del(s_toast_timer);
    s_toast_timer = NULL;
  }
  if (s_root && lv_obj_is_valid(s_root)) {
    lv_obj_del(s_root);
  }
  s_root = NULL;
  s_home = NULL;
  s_apps = NULL;
  s_toast = NULL;
  s_toast_label = NULL;

  root = new_obj(lv_scr_act(), 0, 0, LV_HOR_RES_MAX, LV_VER_RES_MAX, C_FACE, 0);
  lv_obj_set_style_pad_all(root, SCREEN_PAD, LV_PART_MAIN);
  s_root = root;

  s_home = new_container(root, 0, 0, CONTENT_W, CONTENT_H);
  build_home(s_home);

  update_clock();
  s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
}

static void on_pause(void) {
  if (s_clock_timer) {
    lv_timer_del(s_clock_timer);
    s_clock_timer = NULL;
  }
  if (s_toast_timer) {
    lv_timer_del(s_toast_timer);
    s_toast_timer = NULL;
  }
}

static void on_resume(void) {
  update_clock();

  if (!s_clock_timer) {
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
  }
}

static void on_stop(void) {
  on_pause();
  s_root = NULL;
}

static void msg_handler(gui_app_msg_type_t msg, void *param) {
  (void)param;

  switch (msg) {
  case GUI_APP_MSG_ONSTART:
    on_start();
    break;

  case GUI_APP_MSG_ONRESUME:
    on_resume();
    break;

  case GUI_APP_MSG_ONPAUSE:
    on_pause();
    break;

  case GUI_APP_MSG_ONSTOP:
    on_stop();
    break;

  default:
    break;
  }
}

static int app_main(intent_t i) {
  (void)i;
  gui_app_regist_msg_handler(APP_ID, msg_handler);

  return 0;
}

BUILTIN_APP_EXPORT(0, NULL, APP_ID, app_main, 1);
