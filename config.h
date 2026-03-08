#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }
/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 2;  /* border pixel of windows */
static const float rootcolor[]             = COLOR(0x222222ff);
static const float bordercolor[]           = COLOR(0x444444ff);
static const float focuscolor[]            = COLOR(0x005577ff);
static const float urgentcolor[]           = COLOR(0xff0000ff);
static const float fullscreen_bg[]         = {0.1f, 0.1f, 0.1f, 1.0f}; /* You can also use glsl colors */

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (5)

/* logging */
static int log_level = WLR_ERROR;

/* swallow */
static int enableautoswallow = 1;
static const Rule rules[] = {
    /* app_id  title  tags  isfloat  isterm  noswallow  monitor */
    { "footclient", NULL, 0, 0, 1, 0, -1 },
};

/* layout(s) */
static const Layout layouts[] = {
	{ "[]=", tile },
};

/* monitors */
static const MonitorRule monrules[] = {
	{ "eDP-1", 0.5f, 1, 1.5f, &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL, -1, -1 },
	{ NULL   , 0.5f, 1, 1.0f, &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL, -1, -1 },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
};

static const int repeat_rate = 75;
static const int repeat_delay = 300;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 1;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_LOGO

#define TAGKEYS(KEY,SKEY,TAG) \
	{ MODKEY,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,            toggleview,      {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SKEY,           tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT,SKEY,toggletag, {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *term[]            = { "footclient", NULL };
static const char *tofi[]            = { "sh", "-c", "tofi-run | xargs -r sh -c", NULL };
static const char *brave[]           = { "brave", NULL };
static const char *wlogoutcmd[]      = { "wlogout", "-b", "2", NULL };
static const char *swaylockcmd[]     = { "swaylock", NULL };
static const char *br_down[]         = { "brightnessctl", "-q", "set",     "1%-",        NULL };
static const char *br_up[]           = { "brightnessctl", "-q", "set",     "1%+",        NULL };
static const char *vol_down[]        = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "1%-", NULL };
static const char *vol_up[]          = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "1%+", NULL };
static const char *vol_toggle[]      = { "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle", NULL };
static const char *bluefilter_up[]   = { "/home/bren/.local/bin/bluefilter", "up",   NULL };
static const char *bluefilter_down[] = { "/home/bren/.local/bin/bluefilter", "down", NULL };
static const char *screenshots[]     = { "/home/bren/.local/bin/screenshots", NULL };

static const Key keys[] = {
	/* modifier                  key                 function          argument */
	{ MODKEY,                    XKB_KEY_Return,     spawn,            {.v = term} },
	{ MODKEY,                    XKB_KEY_Tab,        spawn,            {.v = tofi} },
	{ MODKEY,                    XKB_KEY_b,          spawn,            {.v = brave} },
	{ MODKEY,                    XKB_KEY_Escape,     spawn,            {.v = wlogoutcmd } },
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_L,          spawn,            {.v = swaylockcmd } },
    { MODKEY,                    XKB_KEY_j,          focusstack,       {.i = +1} },
	{ MODKEY,                    XKB_KEY_k,          focusstack,       {.i = -1} },
	{ MODKEY,                    XKB_KEY_i,          incnmaster,       {.i = +1} },
	{ MODKEY,                    XKB_KEY_d,          incnmaster,       {.i = -1} },
	{ MODKEY,                    XKB_KEY_h,          setmfact,         {.f = -0.05f} },
	{ MODKEY,                    XKB_KEY_l,          setmfact,         {.f = +0.05f} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Return,     zoom,             {0} },
	{ MODKEY,                    XKB_KEY_q,          killclient,       {0} },
	{ MODKEY,                    XKB_KEY_f,          togglefullscreen, {0} },
	{ MODKEY,                    XKB_KEY_0,          view,             {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenright, tag,              {.ui = ~0} },
	{ MODKEY,                    XKB_KEY_comma,      focusmon,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_period,     focusmon,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_less,       tagmon,           {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_greater,    tagmon,           {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q,          quit,             {0} },
    
    TAGKEYS(          XKB_KEY_1, XKB_KEY_exclam,                     0),
	TAGKEYS(          XKB_KEY_2, XKB_KEY_at,                         1),
	TAGKEYS(          XKB_KEY_3, XKB_KEY_numbersign,                 2),
	TAGKEYS(          XKB_KEY_4, XKB_KEY_dollar,                     3),
	TAGKEYS(          XKB_KEY_5, XKB_KEY_percent,                    4),
    
    { 0, XKB_KEY_XF86MonBrightnessDown, spawn, {.v = br_down} },
	{ 0, XKB_KEY_XF86MonBrightnessUp,   spawn, {.v = br_up} },
	{ 0, XKB_KEY_XF86AudioLowerVolume,  spawn, {.v = vol_down} },
	{ 0, XKB_KEY_XF86AudioRaiseVolume,  spawn, {.v = vol_up} },
	{ 0, XKB_KEY_XF86AudioMute,         spawn, {.v = vol_toggle} },
    { 0, XKB_KEY_Print,                 spawn, {.v = screenshots} },
    
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_XF86MonBrightnessUp,   spawn, {.v = bluefilter_up} },
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_XF86MonBrightnessDown, spawn, {.v = bluefilter_down} },

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_Terminate_Server, quit, {0} },
	/* Ctrl-Alt-Fx is used to switch to another VT, if you don't know what a VT is do not remove them. */
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
    { MODKEY, XKB_KEY_a, toggleswallow, {0} },
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_A, toggleautoswallow, {0} },
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
	{ MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
};

