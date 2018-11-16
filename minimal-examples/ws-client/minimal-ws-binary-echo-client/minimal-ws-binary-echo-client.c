/*
 * minimal-ws-binary-echo-client
 *
 * Copyright (C) 2018 Evgeniy Gorlov <egorlov.n@gmail.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * this client can be built for browser using emscripten. And it able to connect
 * to linux WS server
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#define PORT (7681)

#define LWS_PLUGIN_STATIC
#include "protocol_minimal_echo.c"



static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0 },
	LWS_PLUGIN_PROTOCOL_BINARY_EMSCRIPTEN,
	{ NULL, NULL, 0, 0 } /* terminator */
};

static int interrupted, options;

/* pass pointers to shared vars to the protocol */

static const struct lws_protocol_vhost_options pvo_options = {
	NULL,
	NULL,
	"options",		/* pvo name */
	(void *)&options	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
	&pvo_options,
	NULL,
	"interrupted",		/* pvo name */
	(void *)&interrupted	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,		/* "next" pvo linked-list */
	&pvo_interrupted,	/* "child" pvo linked-list */
	"binary",	/* protocol name we belong to on this vhost */
	""		/* ignored */
};
static const struct lws_extension extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
		 "; client_no_context_takeover"
		 "; client_max_window_bits"
	},
	{ NULL, NULL, NULL /* terminator */ }
};

void sigint_handler(int sig)
{
	printf("main!! interrupted\r\n");
	interrupted = 1;
}

struct lws_context *context;
void main_loop()
{
	if( lws_service(context, 1000) < 0)
		interrupted = 2;
}
int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
//	const char *p;
//	int n = 0;//, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	signal(SIGINT, sigint_handler);

//	if ((p = lws_cmdline_option(argc, argv, "-d")))
//		logs = atoi(p);

	lws_set_log_level(LLL_NOTICE | LLL_USER | LLL_EXT, NULL);
	lwsl_user("LWS minimal ws client + permessage-deflate + multifragment bulk message\n");
	lwsl_user("   needs minimal-ws-server-pmd-bulk running to communicate with\n");
	lwsl_user("   %s [-n (no exts)] [-c (compressible)]\n", argv[0]);

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.pvo = &pvo;
	if (!lws_cmdline_option(argc, argv, "-n"))
		info.extensions = extensions;
	info.pt_serv_buf_size = 32 * 1024;

#if defined(LWS_WITH_TLS)
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

//	if (lws_cmdline_option(argc, argv, "-c"))
		options |= 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}
#if defined(__EMSCRIPTEN__)
	emscripten_set_main_loop(main_loop, 60, 0);
	while(!interrupted)
	{
		emscripten_sleep(1000);
	}
#else
	int n = 0;
	while (n >= 0 && !interrupted)
		n = lws_service(context, 1000);
#endif

	lws_context_destroy(context);

	lwsl_user("Completed %s\n", interrupted == 2 ? "OK" : "failed");

	return interrupted != 2;
}
