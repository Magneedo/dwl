/* Exercise compositor state without starting a backend or opening a display. */
#include <assert.h>
#include <sys/prctl.h>

#define main dwl_main
#include "../dwl.c"
#undef main

typedef struct {
	Client client;
	struct wlr_surface surface;
	struct wlr_xdg_surface xdg;
	struct wlr_xdg_toplevel toplevel;
} TestClient;

static void
initclient(TestClient *test, Monitor *mon)
{
	memset(test, 0, sizeof(*test));
	test->xdg.surface = &test->surface;
	test->xdg.toplevel = &test->toplevel;
	test->client.surface.xdg = &test->xdg;
	test->client.mon = mon;
	test->client.tags = 1;
	test->client.bw = borderpx;
	test->client.geom = (struct wlr_box){10, 20, 640, 480};
	test->client.scene = wlr_scene_tree_create(&scene->tree);
	wl_list_insert(&clients, &test->client.link);
	wl_list_insert(&fstack, &test->client.flink);
}

static void
resetclients(void)
{
	wl_list_init(&clients);
	wl_list_init(&fstack);
}

static void
testparentpid(void)
{
	static const char *names[] = {"ordinary", "with spaces", "x)y (z)", "line\nbreak)"};
	char original[16];
	size_t i;

	assert(prctl(PR_GET_NAME, original) == 0);
	for (i = 0; i < LENGTH(names); i++) {
		assert(prctl(PR_SET_NAME, names[i]) == 0);
		assert(parentpid(getpid()) == getppid());
	}
	assert(prctl(PR_SET_NAME, original) == 0);
	assert(parentpid(0) == 0);
	assert(parentpid(-1) == 0);
}

static void
testswallow(Monitor *mon, Monitor *other)
{
	TestClient application, terminal, third;
	Client *app = &application.client, *terminal_client = &terminal.client;

	resetclients();
	initclient(&terminal, mon);
	initclient(&application, NULL);
	terminal_client->tags = 4;
	swallow(app, terminal_client);
	assert(app->mon == mon && terminal_client->mon == mon);
	assert(app->tags == 4 && terminal_client->tags == 4);
	assert(app->swallowing == terminal_client && terminal_client->swallowedby == app);
	assert(app->bw == 2 * borderpx);
	assert(wl_list_length(&clients) == 2 && wl_list_length(&fstack) == 2);

	app->tags = 2;
	swallow(app, NULL);
	assert(!app->swallowing && !terminal_client->swallowedby);
	assert(terminal_client->tags == 2 && app->bw == borderpx);

	/* Moving the replacement also moves its hidden terminal. */
	swallow(app, terminal_client);
	setmon(app, other, 8);
	assert(app->mon == other && terminal_client->mon == other);
	assert(app->tags == 8 && terminal_client->tags == 8);

	/* A second application must not steal an already hidden terminal. */
	initclient(&third, mon);
	swallow(&third.client, terminal_client);
	assert(!third.client.swallowing && terminal_client->swallowedby == app);
	swallow(terminal_client, app);
	assert(!terminal_client->swallowing && app->swallowing == terminal_client);
	swallow(app, NULL);
	swallow(app, app);
	assert(!app->swallowing && !app->swallowedby);

	/* A manual swallow across monitors adopts the target's monitor. */
	setmon(app, mon, 1);
	swallow(app, terminal_client);
	assert(app->mon == other && terminal_client->mon == other);
	assert(app->tags == terminal_client->tags);
}

static void
testnestedunmap(Monitor *mon)
{
	TestClient outer, middle, inner;

	resetclients();
	initclient(&inner, mon);
	initclient(&middle, mon);
	initclient(&outer, mon);
	swallow(&middle.client, &inner.client);
	swallow(&outer.client, &middle.client);
	assert(outer.client.swallowing == &middle.client);
	assert(middle.client.swallowing == &inner.client);
	unmapnotify(&middle.client.unmap, NULL);
	assert(!outer.client.swallowing && !inner.client.swallowedby);
	assert(!middle.client.swallowing && !middle.client.swallowedby);
	assert(!middle.client.mon);
	assert(wl_list_length(&clients) == 2 && wl_list_length(&fstack) == 2);

	resetclients();
	initclient(&inner, mon);
	initclient(&middle, mon);
	initclient(&outer, mon);
	swallow(&middle.client, &inner.client);
	swallow(&outer.client, &middle.client);
	assert(outer.client.bw == 3 * borderpx);
	unmapnotify(&inner.client.unmap, NULL);
	assert(outer.client.swallowing == &middle.client);
	assert(!middle.client.swallowing);
	assert(outer.client.bw == 2 * borderpx);
	assert(outer.client.bw == BORDERPX(&outer.client));
}

static void
testtoggle(Monitor *mon)
{
	TestClient first, second;
	Arg arg = {0};

	resetclients();
	selmon = mon;
	initclient(&first, mon);
	toggleswallow(&arg);
	assert(!first.client.swallowing && !first.client.swallowedby);
	initclient(&second, mon);
	toggleswallow(&arg);
	assert(second.client.swallowing == &first.client);
	toggleswallow(&arg);
	assert(!second.client.swallowing && !first.client.swallowedby);
}

#ifdef XWAYLAND
static void
testunmappedx11(Monitor *mon)
{
	TestClient application, terminal;
	struct wlr_xwayland_surface xsurface = {0};

	resetclients();
	selmon = mon;
	initclient(&terminal, mon);
	initclient(&application, mon);
	terminal.client.pid = getppid();
	terminal.client.isterm = 1;
	xsurface.surface = &application.surface;
	xsurface.pid = getpid();
	xsurface.class = "dwl-regression-client";
	application.client.type = X11;
	application.client.surface.xwayland = &xsurface;
	applyrules(&application.client);
	assert(!application.client.swallowing && !terminal.client.swallowedby);
}
#endif

int
main(void)
{
	struct wlr_output output = {0};
	Monitor mon = {.wlr_output = &output, .tagset = {1, 1}};
	Monitor other = {.wlr_output = &output, .tagset = {1, 1}};
	int i;

	wl_list_init(&mons);
	dpy = wl_display_create();
	assert(dpy);
	seat = wlr_seat_create(dpy, "test-seat");
	scene = wlr_scene_create();
	assert(seat && scene);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	cursor = wlr_cursor_create();
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	assert(cursor && cursor_mgr);
	/* Avoid keyboard focus changes for these unmapped synthetic surfaces. */
	locked = 1;

	testparentpid();
	testswallow(&mon, &other);
	testnestedunmap(&mon);
	testtoggle(&mon);
#ifdef XWAYLAND
	testunmappedx11(&mon);
#endif
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);
	wlr_scene_node_destroy(&scene->tree.node);
	wl_display_destroy(dpy);
	puts("Swallowing regression tests passed");
	return 0;
}
