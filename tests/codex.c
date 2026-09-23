/* Drive a real headless compositor, foot and tmux from codex.py. */
#define main dwl_main
#include "../dwl.c"
#undef main
#include <wlr/interfaces/wlr_keyboard.h>

static int response_fd;

static int
command(int fd, uint32_t mask, void *data)
{
	Client *c, *normal = NULL;
	char key;
	int count = 0;
	Arg arg = {0};
	struct wlr_pointer_axis_event axis = {
		.orientation = WL_POINTER_AXIS_VERTICAL_SCROLL,
		.source = WL_POINTER_AXIS_SOURCE_WHEEL,
		.relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL,
	};
	const char *termcmd[] = {"footclient", "--app-id=footclient", "sleep", "120", NULL};

	wl_list_for_each(c, &clients, link) {
		if (!c->iscodex)
			normal = c;
	}

	if (read(fd, &key, 1) != 1) {
		quit(NULL);
		return 0;
	}
	switch (key) {
	case 'w':
		keybinding(MODKEY, XKB_KEY_Escape);
		break;
	case 'P':
		if (keybinding(MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Escape) != 1)
			abort();
		break;
	case '.':
		if (keybinding(MODKEY, XKB_KEY_grave) != 1)
			abort();
		break;
	case '+':
	case '-':
		if (!codexclient)
			abort();
		wlr_seat_pointer_notify_enter(seat, client_surface(codexclient), 30, 30);
		axis.delta = key == '+' ? -15 : 15;
		axis.delta_discrete = key == '+' ? -120 : 120;
		axisnotify(NULL, &axis);
		wlr_seat_pointer_notify_frame(seat);
		break;
	case 'e':
	case 'i':
	case 'p':
	case 'r':
		count = key == 'e' ? KEY_ESC : key == 'i' ? KEY_A
				: key == 'r' ? KEY_ENTER : KEY_UP;
		wlr_seat_set_keyboard(seat, data);
		wlr_seat_keyboard_notify_key(seat, 0, count, WL_KEYBOARD_KEY_STATE_PRESSED);
		wlr_seat_keyboard_notify_key(seat, 0, count, WL_KEYBOARD_KEY_STATE_RELEASED);
		break;
	case 't':
		if (keybinding(MODKEY, XKB_KEY_c) != 2)
			abort();
		break;
	case 'm':
	case 'M':
		if (keybinding(MODKEY,
				key == 'm' ? XKB_KEY_period : XKB_KEY_comma) != 1)
			abort();
		break;
	case 'b':
		/* Emulate the usable area left by an exclusive top bar. */
		selmon->w.y += 30;
		selmon->w.height -= 30;
		showcodex();
		break;
	case 'v':
		arg.ui = selmon->tagset[selmon->seltags] == 1 ? 2 : 1;
		view(&arg);
		break;
	case 'k':
		if (codexclient)
			client_send_close(codexclient);
		break;
	case 'n':
		arg.v = termcmd;
		spawn(&arg);
		break;
	case 'f':
		if (normal)
			setfullscreen(normal, 1);
		break;
	case 'a':
		if (normal && codexclient) {
			swallow(normal, codexclient);
			swallow(codexclient, normal);
			if (normal->swallowing || normal->swallowedby
					|| codexclient->swallowing || codexclient->swallowedby)
				abort();
		}
		break;
	case 'u':
		wlr_output_destroy(selmon->wlr_output);
		break;
	case 'q':
		quit(NULL);
		return 0;
	}
	if (key != 's')
		return 0;
	wl_list_for_each(c, &clients, link)
		count += c->iscodex;
	c = codexclient;
	dprintf(response_fd, "TEST {\"server\":%d,\"terminal\":%d,\"count\":%d,"
			"\"shown\":%d,\"visible\":%d,\"enabled\":%d,\"focused\":%d,"
			"\"floating\":%d,\"noswallow\":%d,\"selected\":%d,"
			"\"socket\":\"%s\",\"display\":\"%s\",\"layer\":%d,"
			"\"normal\":%d,\"normalfull\":%d,\"normalfocused\":%d,\"foot\":%d,"
			"\"menufocused\":%d,"
			"\"geometry\":[%d,%d,%d,%d],\"usable\":[%d,%d,%d,%d],"
			"\"size\":[%u,%u],\"border\":%u}\n",
			(int)codexpid[0], (int)codexpid[1], count, codexshown,
			c && VISIBLEON(c, c->mon), c && c->scene->node.enabled,
			c && seat->keyboard_state.focused_surface == client_surface(c),
			c && c->isfloating, c && c->noswallow, c && c->mon == selmon,
			codexsocket, getenv("WAYLAND_DISPLAY"),
			c && c->scene->node.parent == layers[LyrFS], normal != NULL,
			normal && normal->isfullscreen,
			normal && seat->keyboard_state.focused_surface == client_surface(normal),
			(int)child_pid, exclusive_focus != NULL,
			c ? c->geom.x : 0, c ? c->geom.y : 0,
			c ? c->geom.width : 0, c ? c->geom.height : 0,
			selmon->w.x, selmon->w.y, selmon->w.width, selmon->w.height,
			codexwidth, codexheight, c ? c->bw : 0);
	return 0;
}

int
main(int argc, char **argv)
{
	struct wl_event_source *input;
	struct wlr_keyboard keyboard;
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	static const struct wlr_keyboard_impl impl = {.name = "test"};

	if (argc != 2)
		return 2;
	response_fd = dup(STDOUT_FILENO);
	fcntl(response_fd, F_SETFD, FD_CLOEXEC);
	codexcmd = argv[1];
	setup();
	wlr_keyboard_init(&keyboard, &impl, "test");
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names(context, &xkb_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	wlr_keyboard_set_keymap(&keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_seat_set_keyboard(seat, &keyboard);
	wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	input = wl_event_loop_add_fd(event_loop, STDIN_FILENO,
			WL_EVENT_READABLE, command, &keyboard);
	run("exec foot --server --log-level=error");
	wl_event_source_remove(input);
	wlr_keyboard_finish(&keyboard);
	cleanup();
	return 0;
}
