/* FilmMeasure_server.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

#define NEED_WINDOWS_LIBRARY			/* Define to include windows.h call functions */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>				  /* for defining several useful types and macros */
#include <stdio.h>				  /* for performing input and output */
#include <stdlib.h>				  /* for performing a variety of operations */
#include <string.h>
#include <math.h>               /* basic math functions */
#include <time.h>
#include <assert.h>
#include <stdint.h>             /* C99 extension to get known width integers */

/* Standard Windows libraries */
#ifdef NEED_WINDOWS_LIBRARY
	#define STRICT							/* define before including windows.h for stricter type checking */
	#include <windows.h>					/* master include file for Windows applications */
	#include <windowsx.h>				/* Extensions for GET_X_LPARAM */
	#include <commctrl.h>
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"		/* Server support routine */
#include "FilmMeasure_server.h"	/* Prototypes for main	  */
#include "FilmMeasure_client.h"	/* Version info and port  */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int server_msg_handler(SERVER_DATA_BLOCK *block);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Routine to initialize high level Spec remote socket server
--
-- Usage: void Init_FilmMeasure_Server(void)
--
-- Inputs: none
--
-- Output: Spawns thread running the Spec high function server(s)
--
-- Return:  0 if all was successful
--            1 ==> Unable to create mutex
--            2 ==> Unable to spawn the server thread
=========================================================================== */
static BOOL film_msg_server_up = FALSE;				/* Server has been started */
static HANDLE film_server_mutex = NULL;				/* access to the client/server communication */

int Init_FilmMeasure_Server(void) {
	static char *rname = "Init_FilmMeasure_Server";

	/* Don't start multiple times :-) */
	if (film_msg_server_up) return 0;

/* Create mutex for work */
	if (film_server_mutex == NULL && (film_server_mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to create the server access semaphores\n", rname); fflush(stderr);
		return 1;
	}

/* Bring up the message based server */
	if ( ! (film_msg_server_up = (RunServerThread("FilmMeasure", FILM_MSG_LISTEN_PORT, server_msg_handler, NULL) == 0)) ) {
		fprintf(stderr, "ERROR[%s]: Unable to start the FILMMEASURE message based remote server\n", rname); fflush(stderr);
		return 2;
	}

	return 0;
}

/* ===========================================================================
-- Routine to shutdown high level Spec remote socket server
--
-- Usage: void Shutdown_FilmMeasure_Server(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_FilmMeasure_Server(void) {

	return 0;
}

/* ===========================================================================
-- Actual server routine to process messages received on the open socket.
--
-- Usage: int server_msg_handler(SERVER_DATA_BLOCK *block)
--
-- Inputs: socket - an open socket for exchanging messages with the client
--
-- Output: whatever needs to be done
--
-- Return: 0 ==> close this socket but continue listening for new clients
--         1 ==> Timeout waiting for semaphore
--         2 ==> socket appears to have been abandoned
=========================================================================== */
static int server_msg_handler(SERVER_DATA_BLOCK *block) {
	static char *rname = "server_msg_handler";

	CS_MSG request, reply;
	void *request_data, *reply_data;
	BOOL ServerActive;

#define	MAX_VARS	(20)
	double vars[MAX_VARS];
	int nvars;

/* Get standard request from client and process */
	ServerActive = TRUE;
	while (ServerActive && GetStandardServerRequest(block, &request, &request_data) == 0) {	/* Exit if client disappears */

		/* Create a default reply message */
		memcpy(&reply, &request, sizeof(reply));
		reply.rc = reply.data_len = 0;			/* All okay and no extra data */
		reply_data = NULL;							/* No extra data on return */

		/* Be very careful ... only allow one socket message to be in process at any time */
		/* The code should already protect, but not sure how interleaved messages may impact operations */
		if (WaitForSingleObject(film_server_mutex, FILM_SERVER_WAIT) != WAIT_OBJECT_0) {
			fprintf(stderr, "ERROR[%s]: Timeout waiting for the FilmMeasure semaphore\n", rname); fflush(stderr);
			reply.msg = -1; reply.rc = -1;

		} else switch (request.msg) {
			case SERVER_END:
				fprintf(stderr, "  Film msg server: SERVER_END\n"); fflush(stderr);
				ServerActive = FALSE;
				break;

			case FILM_QUERY_VERSION:
				fprintf(stderr, "  Film msg server: FILM_QUERY_VERSION()\n");	fflush(stderr);
				reply.rc = FILM_CLIENT_SERVER_VERSION;
				break;

			case FILM_DO_MEASURE:
				fprintf(stderr, "  Film msg server: FILM_DO_MEASURE()\n");	fflush(stderr);
				reply.rc = FilmMeasure_Do_Measure();
				break;
				
			case FILM_SAVE_DATA:
				fprintf(stderr, "  Film msg server: FILM_SAVE_DATA(%s)\n", (char *) request_data); fflush(stderr);
				reply.rc = FilmMeasure_Save_Data((char *) request_data);
				break;

			case FILM_QUERY_FIT:
				fprintf(stderr, "  Film msg server: FILM_QUERY_FIT()\n"); fflush(stderr);
				reply.rc = FilmMeasure_Query_Fit_Parms(&nvars, vars, MAX_VARS);
				reply.option = nvars;
				reply.data_len = nvars*sizeof(*vars);
				reply_data = (void *) vars;
				break;

			default:
				fprintf(stderr, "ERROR: FilmMeasure server message received (%d) that was not recognized.\n"
						  "       Will be ignored with rc=-1 return code.\n", request.msg);
				fflush(stderr);
				reply.rc = -1;
				break;
		}
		ReleaseMutex(film_server_mutex);
		if (request_data != NULL) { free(request_data); request_data = NULL; }

		/* Send the standard response and any associated data */
		if (SendStandardServerResponse(block, reply, reply_data) != 0) {
			fprintf(stderr, "ERROR: DCx server failed to send response we requested.\n");
			fflush(stderr);
		}
	}

	EndServerHandler(block);								/* Cleanly exit the server structure always */
	return 0;
}
