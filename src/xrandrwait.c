/*
 * xrandrwait.c - Wait for a particular XRandR event
 * Copyright (C) 2025 Matthias Kruk
 *
 * Xrandrwait is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 *
 * Xrandrwait is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xrandrwait; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#if DEBUG
#define DBG(hoge) do { hoge; } while (0)
#else
#define DBG(hoge)
#endif /* DEBUG */

#define DEFAULT_MASK (RRCrtcChangeNotifyMask   | \
                      RROutputChangeNotifyMask | \
                      RRScreenChangeNotifyMask)

static const char *rotation_names[] = {
	[0]             = "E",
	[RR_Rotate_0]   = "0",
	[RR_Rotate_90]  = "90",
	[RR_Rotate_180] = "180",
	[RR_Rotate_270] = "270"
};

static const char *reflection_names[] = {
	[0]                           = "0",
	[1]                           = "E",
	[RR_Reflect_X]                = "X",
	[RR_Reflect_Y]                = "Y",
	[RR_Reflect_X | RR_Reflect_Y] = "XY"
};

static const char *connection_names[] = {
	[RR_Connected]         = "Y",
	[RR_Disconnected]      = "N",
	[RR_UnknownConnection] = "?",
	[3]                    = "E"
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

struct context {
	Display *display;
	int screen;
	Window root;
	int event_base;
	int error_base;
};

static int running = 0;
static int monitor = 0;
static int quiet = 0;
static unsigned int timeout = 0;
static int events = 0;

static const char *rotation_name(const unsigned long rot)
{
	unsigned long rotation = rot & 0xf;

	return rotation_names[rotation >= ARRAY_SIZE(rotation_names) ? 0 : rotation];
}

static const char *reflection_name(const unsigned long ref)
{
	unsigned long reflection = ref & 0xf0;

	if (reflection & ~(RR_Reflect_X | RR_Reflect_Y)) {
		reflection = 1;
	}

	return reflection_names[reflection];
}

static const char *connection_name(const unsigned long conn)
{
	return connection_names[conn >= ARRAY_SIZE(connection_names) ? 3 : conn];
}

static void print_usage(const char *cmdname)
{
	printf("Usage: %s [OPTIONS]\n"
	       "Wait for a particular XRandR event\n"
	       "\n"
	       "Options:\n"
	       "  -e  --event    Listen for specific events. If omitted, all events are\n"
	       "                 listened for. This option may be specified more than once.\n"
	       "                 Allowed values: crtc_change, output_change, screen_change\n"
	       "  -h  --help     Print this text\n"
	       "  -m  --monitor  Do not exit after an event occurs\n"
	       "  -q  --quiet    Do not print any output\n"
	       "  -t  --timeout  Exit if no event has occurred within the specified number\n"
	       "                 of seconds\n",
	       cmdname);
}

static int parse_cmdline(int argc, char *argv[])
{
	static const char *shortopts = "e:hmqt:";
	static const struct option cmd_opts[] = {
	        { "event",   required_argument, 0, 'e' },
		{ "help",    no_argument,       0, 'h' },
		{ "monitor", no_argument,       0, 'm' },
		{ "quiet",   no_argument,       0, 'q' },
		{ "timeout", required_argument, 0, 't' },
		{ NULL }
	};

	static struct {
		const char *name;
		int mask;
	} event_map[] = {
		{
			.name = "crtc_change",
			.mask = RRCrtcChangeNotifyMask
		}, {
			.name = "output_change",
			.mask = RROutputChangeNotifyMask
		}, {
			.name = "screen_change",
			.mask = RRScreenChangeNotifyMask
		}
	};

	int opt;
	int i;

	do {
		opt = getopt_long(argc, argv, shortopts, cmd_opts, NULL);

		switch (opt) {
		case 'e':
			for (i = 0; i < ARRAY_SIZE(event_map); i++) {
				if (strcmp(optarg, event_map[i].name) == 0) {
					events |= event_map[i].mask;
					break;
				}
			}
			break;

		case 'm':
			monitor = 1;
			break;

		case 'q':
			quiet = 1;
			break;

		case 't':
			errno = 0;
			timeout = strtol(optarg, NULL, 10);
			if (errno) {
				fprintf(stderr, "Invalid timeout: %s (%s)\n",
					optarg, strerror(errno));
				return 1;
			}

			break;

		case 'h':
		case '?':
			print_usage(argv[0]);
			return 1;

		default:
		        break;
		}
	} while (opt != -1);

	return 0;
}

static void handle_signal(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
	case SIGUSR1:
	case SIGALRM:
	default:
		DBG(fprintf(stderr, "Received signal %d. Stopping.\n", sig));
		running = 0;
		break;
	}
}

static void setup_signals(void)
{
	static const int signals[] = {
		SIGINT,
		SIGHUP,
		SIGTERM,
		SIGUSR1,
		SIGALRM
	};
	struct sigaction action;
	unsigned int i;

	memset(&action, 0, sizeof(action));
	action.sa_handler = handle_signal;

	for (i = 0; i < (sizeof(signals) / sizeof(signals[0])); i++) {
		sigaction(signals[i], &action, NULL);
	}
}

static int context_init_xrr(struct context *ctx)
{
	int event_mask;
	
	if (!XRRQueryExtension(ctx->display,
			       &ctx->event_base,
			       &ctx->error_base)) {
		return -ENOTSUP;
	}

	if (events) {
		event_mask = events;
	} else {
		event_mask = DEFAULT_MASK;
	}

	XRRSelectInput(ctx->display, ctx->root, event_mask);
	
	return 0;
}

static int context_close(struct context *ctx)
{
	if (!ctx) {
		return -EINVAL;
	}
	
	if (!ctx->display) {
		return -EALREADY;
	}
	
	XCloseDisplay(ctx->display);
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

static int context_open(struct context *ctx)
{
	int err;

	ctx->display = XOpenDisplay(NULL);
	err = 0;

	if (!ctx->display) {
		DBG(fprintf(stderr, "Could not open display\n"));
		return -EIO;
	}

	ctx->screen = DefaultScreen(ctx->display);
	ctx->root = RootWindow(ctx->display, ctx->screen);

	if ((err = context_init_xrr(ctx)) < 0) {
		DBG(fprintf(stderr, "Could not initialize XRandR extension\n"));
		context_close(ctx);
	}
	
	return err;
}

static int handle_output_change_event(struct context *ctx, XRROutputChangeNotifyEvent *event)
{
	if (!quiet) {
		printf("XRROutputChangeNotifyEvent output=0x%lx crtc=0x%lx mode=0x%lx connection=%s\n",
		       event->output, event->crtc, event->mode, connection_name(event->connection));
	}

	return 0;
}

static int handle_crtc_change_event(struct context *ctx, XRRCrtcChangeNotifyEvent *event)
{
	if (!quiet) {
		printf("XRRCrtcChangeNotifyEvent crtc=0x%lx res=%dx%d pos=%dx%d mode=0x%lx rotation=%s reflection=%s\n",
		       event->crtc, event->width, event->height, event->x, event->y, event->mode,
		       rotation_name(event->rotation), reflection_name(event->rotation));
	}

	return 0;
}

static int handle_events(struct context *ctx)
{
	XEvent event;
	int handled;

	handled = 0;

	while (XEventsQueued(ctx->display, QueuedAfterFlush) > 0) {
#if DEBUG
		const char *event_type;
#endif /* DEBUG */

		if (XNextEvent(ctx->display, &event) != 0) {
			continue;
		}

		switch(event.type - ctx->event_base) {
		case RRScreenChangeNotify:
			DBG(event_type = "RRScreenChangeNotify");
			handled = 1;
			break;

		case RRNotify:
			switch (((XRRNotifyEvent*)&event)->subtype) {
			case RRNotify_OutputChange:
				DBG(event_type = "RRNotify_OutputChange");
				handle_output_change_event(ctx, (XRROutputChangeNotifyEvent*)&event);
				handled = 1;
				break;
				
			case RRNotify_CrtcChange:
				DBG(event_type = "RRNotify_CrtcChange");
				handle_crtc_change_event(ctx, (XRRCrtcChangeNotifyEvent*)&event);
				handled = 1;
				break;

			default:
				DBG(event_type = "(other XRandR event)");
				break;
			}
			break;

		default:
			DBG(event_type = "(other event)");
			break;
		}

		DBG(printf("%s\n", event_type));
	}

	/* Terminate if we're not in monitor mode */
	if (handled && !monitor) {
		running = 0;
	}

	return handled ? 0 : 1;
}

int main(int argc, char *argv[])
{
	struct context ctx;
	int err;
	
	if (parse_cmdline(argc, argv) != 0) {
		DBG(fprintf(stderr, "Could not parse commandline\n"));
		return 2;
	}

	setup_signals();

	if (timeout) {
		alarm(timeout);
	}

	if (!(err = context_open(&ctx))) {
		struct timespec ts = {
			.tv_sec = 0,
			.tv_nsec = 100000000
		};
		running = 1;

		DBG(fprintf(stderr, "Running\n"));
		
		while (running) {
			err = handle_events(&ctx);
			nanosleep(&ts, NULL);
		}
		
		context_close(&ctx);
	}
	
	return err;
}
