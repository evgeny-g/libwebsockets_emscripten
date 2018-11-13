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

#define LWS_PLUGIN_STATIC
#include "protocol_minimal_echo.c"

//#define USE_SSL


#if defined(USE_SSL)
static struct lws *client_wsi;
#endif

#if defined(USE_SSL)
static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason,
              void *user, void *in, size_t len)
{
        switch (reason) {

        /* because we are protocols[0] ... */
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
                         in ? (char *)in : "(null)");
                client_wsi = NULL;
                break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
                lwsl_user("%s: established\n", __func__);
                break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
                lwsl_user("RX: %s\n", (const char *)in);
                break;

        case LWS_CALLBACK_CLIENT_CLOSED:
                client_wsi = NULL;
                break;

        default:
                break;
        }

        return lws_callback_http_dummy(wsi, reason, user, in, len);
}
#endif

static struct lws_protocols protocols[] = {
#if defined(USE_SSL)
	{ "http", callback_http, 0, 0 },
#else
	{ "http", lws_callback_http_dummy, 0, 0 },
#endif
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
#if defined(USE_SSL)
	struct lws_client_connect_info i;
#endif
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

	//lws_set_log_level(LLL_NOTICE | LLL_USER | LLL_EXT, NULL);
	lws_set_log_level(4095, NULL);
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
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

//	if (lws_cmdline_option(argc, argv, "-c"))
		options |= 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}
#if defined (USE_SSL)
	memset(&i, 0, sizeof i); /* otherwise uninitialized garbage */
	i.context = context;
	i.port = 7681;
	i.address = "127.0.0.1";
	i.path = "/";
	i.host = "localhost";
	i.origin = i.address;
	i.ssl_connection = LCCSCF_USE_SSL;
	i.protocol = "binary";//protocols[0].name;
	i.pwsi = &client_wsi;

	i.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;

	lws_client_connect_via_info(&i);
#endif
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
