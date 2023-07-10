/* FilmMeasure_client.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>				  /* for defining several useful types and macros */
#include <stdio.h>				  /* for performing input and output */
#include <stdlib.h>				  /* for performing a variety of operations */
#include <string.h>
#include <math.h>               /* basic math functions */
#include <assert.h>
#include <stdint.h>             /* C99 extension to get known width integers */

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"		/* Server support */
// #include "FilmMeasure.h"			/* Access to the spectrometer information */
#include "FilmMeasure_client.h"	/* For prototypes				*/

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
static void cleanup(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

#ifdef LOCAL_CLIENT_TEST
	
int main(int argc, char *argv[]) {

	int rc, client_version, server_version;
	char *server_IP;

	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* Machine in laser room */
//	server_IP = "128.253.129.71";					/* Machine in open lab room */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */

	if ( (rc = Init_FilmMeasure_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = FilmMeasure_Remote_Query_Client_Version();
	server_version = FilmMeasure_Remote_Query_Server_Version();
	printf("Client version: %4.4d\n", client_version); fflush(stdout);
	printf("Server version: %4.4d\n", server_version); fflush(stdout);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	rc = FilmMeasure_Remote_Measure();
	printf("Measure returned %d\n", rc);
	fflush(stdout);

	rc = FilmMeasure_Remote_SaveData("test_data.csv");
	printf("SaveData returned %d\n", rc);
	fflush(stdout);

	double vars[20];
	rc = FilmMeasure_Remote_QueryFit(vars, 10);
	printf("QueryParms returned %d:", rc);
	for (int i=0; i<min(rc,20); i++) printf(" %.2f", vars[i]);
	printf("\n"); fflush(stdout);

	/* Shut down cleanly before exiting */
	Shutdown_FilmMeasure_Client();

	return 0;
}

#endif		/* LOCAL_CLIENT_TEST */


/* ===========================================================================
-- Routine to open and initialize the socket to the Spec server
--
-- Usage: int Init_FilmMeasure_Client(char *IP_address);
--
-- Inputs: IP_address - IP address in normal form.  Use "127.0.0.1" for loopback test
--                      if NULL, uses DFLT_SERVER_IP_ADDRESS
--
-- Output: Creates MUTEX semaphores, opens socket, sets atexit() to ensure closure
--
-- Return:  0 - successful
--          1 - unable to open the sockets (one or the other)
--          2 - unable to query the server version
--          3 - server / client version mismatch
--
-- Notes: Must be called before any attempt to communicate across the socket
=========================================================================== */
static CLIENT_DATA_BLOCK *Film_Remote = NULL;		/* Connection to the server */

int Init_FilmMeasure_Client(char *IP_address) {
	static char *rname = "Init_FilmMeasure_Client";
	int rc;
	int server_version;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

	/* Shutdown sockets if already open (reinitialization allowed) */
	if (Film_Remote != NULL) { CloseServerConnection(Film_Remote); Film_Remote = NULL; }

	if ( (Film_Remote = ConnectToServer("FilmMeasure", IP_address, FILM_MSG_LISTEN_PORT, &rc)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to the server\n", rname); fflush(stderr);
		return 1;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	server_version = FilmMeasure_Remote_Query_Server_Version();
	if (server_version <= 0) {
		fprintf(stderr, "ERROR[%s]: Unable to query server version\n", rname); fflush(stderr);
		CloseServerConnection(Film_Remote); Film_Remote = NULL;
		return 2;
	} else if (server_version != FILM_CLIENT_SERVER_VERSION) {
		fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, FILM_CLIENT_SERVER_VERSION); fflush(stderr);
		CloseServerConnection(Film_Remote); Film_Remote = NULL;
		return 3;
	}

	/* Report success, and if not close everything that has been started */
	fprintf(stderr, "INFO: Connected to FilmMeasure server on %s\n", IP_address); fflush(stderr);
	return 0;
}

/* ===========================================================================
-- Routine to shutdown high level Spec remote socket server
--
-- Usage: void Shutdown_FilmMeasure_Client(void)
--
-- Inputs: none
--
-- Output: Closes the server connection and marks it as unused.
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_FilmMeasure_Client(void) {

	if (Film_Remote != NULL) { CloseServerConnection(Film_Remote); Film_Remote = NULL; }
	return 0;
}

/* ===========================================================================
-- Quick routine to check for errors and print messages for TCP transaction
--
-- Usage: int Error_Check(int rc, CS_MSG *reply, int expect_msg);
--
-- Inputs: rc - return code from StandardServerExchange()
--         reply - message returned from the exchange
--         expect_msg - the message sent, which should be returned
--
-- Output: prints error message if error or mismatch
--
-- Return: 0 if no errors, otherwise -1
=========================================================================== */
static int Error_Check(int rc, CS_MSG *reply, int expect_msg) {

	if (rc != 0) {
		fprintf(stderr, "ERROR: Unexpected error from StandardServerExchange (rc=%d)\n", rc); fflush(stderr);
		return -1;
	} else if (reply->msg != expect_msg) {
		fprintf(stderr, "ERROR: Expected %d in reply message but got %d back\n", expect_msg, reply->msg); fflush(stderr);
		return -1;
	}
	return 0;
}

/* ===========================================================================
--	Routine to return current version of this code
--
--	Usage:  int FilmMeasure_Remote_Query_Server_Version(void);
--         int FilmMeasure_Remote_Query_Client_Version(void);
--
--	Inputs: none
--		
--	Output: none
--
-- Return: Integer version number.  
--
--	Notes: The verison number returned is that given in this code when
--        compiled. The routine simply returns this version value and allows
--        a program that may be running in the client/server model to verify
--        that the server is actually running the expected version.  Programs
--        should always call and verify the expected returns.
=========================================================================== */
int FilmMeasure_Remote_Query_Client_Version(void) {
	return FILM_CLIENT_SERVER_VERSION;
}

int FilmMeasure_Remote_Query_Server_Version(void) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = FILM_QUERY_VERSION;
	
	/* Get the response */
	rc = StandardServerExchange(Film_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, FILM_QUERY_VERSION) != 0) return -1;

	return reply.rc;					/* Will be the version number */
}

/* ===========================================================================
--	Routine to execute a measurement
--
--	Usage:  int FilmMeasure_Remote_Measure(void);
--
--	Inputs: none
--		
--	Output: none
--
-- Return: 0 if successful
=========================================================================== */
int FilmMeasure_Remote_Measure(void) {

	CS_MSG request, reply;
	int rc;
	
	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = FILM_DO_MEASURE;

	/* Get the response */
	rc = StandardServerExchange(Film_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, FILM_DO_MEASURE) != 0) return rc;

	return reply.option;					/* Will be error return if any */
}


/* ===========================================================================
--	Routine to save the current data from FilmMeasure to a file
--
--	Usage:  int FilmMeasure_Remote_SaveData(char *path);
--
--	Inputs: path - full path where data is to be stored
--		
--	Output: saves the data as if user selected on dialog box
--
-- Return: 0 if successful, error value from call otherwise
=========================================================================== */
int FilmMeasure_Remote_SaveData(char *path) {
	
	CS_MSG request, reply;
	int rc;

	/* Default pathname if invalid given */
	if (path == NULL || *path == '\0') path = "filmmeasure.dat";

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = FILM_SAVE_DATA;
	request.data_len = strlen(path);

	/* Get the response */
	rc = StandardServerExchange(Film_Remote, request, (void *) path, &reply, NULL);
	if (Error_Check(rc, &reply, FILM_SAVE_DATA) != 0) return rc;

	return reply.rc;					/* Will be error return if any */
}

/* ===========================================================================
--	Routine to return values that were autocalculated with FilmMeasure
--
--	Usage:  int FilmMeasure_Remote_QueryVaryParms(double *vals, int maxvals);
--
--	Inputs: vals    - array to receive values
--         maxvals - maximum number of values to transfer
--		
--	Output: *vals[] - varied parameters
--
-- Return: # of values stored, 0 if none exist, or <0 on error
=========================================================================== */
int FilmMeasure_Remote_QueryFit(double *vals, int maxvals) {

	CS_MSG request, reply;
	int rc;

	int i;
	double *rvals = NULL;

	/* Fill in default response (no data) */
	for (i=0; i<maxvals; i++) vals[i] = 0.0;
	
	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = FILM_QUERY_FIT;

	/* Get the response (rc is number of bytes returned) */
	rc = StandardServerExchange(Film_Remote, request, NULL, &reply, (void **) &rvals);
	if (Error_Check(rc, &reply, FILM_QUERY_FIT) != 0) return rc;

	if (rvals != NULL) {
		for (i=0; i<min(maxvals, reply.option); i++) vals[i] = rvals[i];
		free(rvals);
	}

	return reply.option;
}


/* ===========================================================================
-- Routine to connect a server on a specific machine and a specific port
--
-- Usage: SOCKET OpenServer(ULONG IP_address, USHORT port, char *server_name);
--
-- Inputs: IP_address  - address of the server.  This is encoded and
--                       typically sent as inet_addr("128.84.249.249")
--         port        - port to be connected
--         server_name - name (informational only - included on error messages)
--
-- Output: opens a socket to the specified server port
--
-- Return: Socket for communication with the server if successful
--         SOCKET_INVALID if unsuccessful for any reason.  Reason printed to stderr
=========================================================================== */
static SOCKET OpenServer(unsigned long IP_address, unsigned short port, char *server_name) {
	static char *rname = "OpenServer";

	SOCKADDR_IN clientService;
	SOCKET m_socket;

	if ( InitSockets() != 0) return INVALID_SOCKET;		/* Make sure socket support has been initialized */

/* Create a socket for my use */
	m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( m_socket == INVALID_SOCKET ) {
#ifdef _WIN32
		fprintf(stderr, "ERROR[%s]: Failed to create socket for \"%s\": %ld\n", rname, server_name, WSAGetLastError() ); fflush(stderr);
#elif __linux__
		fprintf(stderr, "ERROR[%s]: Failed to create socket for \"%s\": %m\n", rname, server_name); fflush(stderr);
#endif
		return INVALID_SOCKET;
	}

/* Connect to the service */
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = IP_address;
	clientService.sin_port = htons(port);
	if ( connect( m_socket, (SOCKADDR*) &clientService, sizeof(clientService) ) == SOCKET_ERROR) {
		fprintf(stderr, "ERROR[%s]: OpenServer failed to connect to service \"%s\"\n", rname, server_name); fflush(stderr);
		closesocket(m_socket);
		return INVALID_SOCKET;
	}

	return m_socket;
}


/* ===========================================================================
-- Routine to close a server previously opened by OpenServer
--
-- Usage: int CloseServer(SOCKET socket);
--
-- Inputs: socket - a socket previously returned by OpenServer
--
-- Output: closes the socket
--
-- Return: 0 if successful.  !0 on error
=========================================================================== */
static int CloseServer(SOCKET m_socket) {
	shutdown(m_socket, SD_BOTH);
	closesocket(m_socket);
	return 0;
}	



/* ===========================================================================
-- atexit routinen to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (Film_Remote != NULL) { CloseServerConnection(Film_Remote); Film_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
