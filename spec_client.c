/* spec_client.c */

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
#include "spec.h"						/* Access to the spectrometer information */
#include "spec_client.h"			/* For prototypes				*/

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

	SPEC_SPECTRUM_INFO info;
	SPEC_SPECTROMETER_INFO status;
	int i,j, npt;
	int count, iavg;
	double *wavelengths, *spectrum, ms;
	int rc, client_version, server_version;
	BOOL bDark, bNL;
	char *server_IP;

	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* Machine in laser room */
//	server_IP = "128.253.129.71";					/* Machine in open lab room */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */

	if ( (rc = Init_Spec_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = Spec_Remote_Query_Client_Version();
	server_version = Spec_Remote_Query_Server_Version();
	printf("Client version: %4.4d\n", client_version); fflush(stdout);
	printf("Server version: %4.4d\n", server_version); fflush(stdout);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	if ( (rc = Spec_Remote_Get_Spectrometer_Info(&status)) == 0) {
		printf("Spectrometer information\n");
		printf("  Status: %s\n", status.spec_ok ? "Ok" : "Not OK");
		if (status.spec_ok) {
			printf("  Model:  %s\n", status.model);
			printf("  Serial: %s\n", status.serial);
			printf("  # of points: %d\n", status.npoints);
			printf("  wavelength range: [%.2f , %.2f ] nm\n", status.lambda_min, status.lambda_max);
			printf("  integration time: %.2f ms\n", status.ms_integrate);
			printf("  averaging number: %d\n", status.num_average);
			printf("  Electric dark correct: %s\n", status.use_dark_pixel ? "Yes" : "No");
			printf("  Non-linear correct:    %s\n", status.use_nl_correct ? "Yes" : "No");
		} else {
			fprintf(stderr, "ERROR: Not reasonable to proceed further\n"); fflush(stderr);
			return 3;
		}
		fflush(stdout);
	} else {
		fprintf(stderr, "ERROR: Unable to get spectrometer information\n"); fflush(stderr);
	}

	if ( (rc = Spec_Remote_Get_Wavelengths(&count, &wavelengths)) == 0) {
		int i,j;
		printf("Number of wavelengths: %d\n", count);
		for (i=0; i<50; i+=10) {
			for (j=0; j<10; j++) printf(" %.2f", wavelengths[i+j]);
			printf("\n"); fflush(stdout);
		}
	} else {
		fprintf(stderr, "ERROR: Unable to get spectrometer wavelength info\n"); fflush(stderr);
	}

	if ( (rc = Spec_Remote_GetIntegrationParms(&ms, &iavg, &bDark, &bNL)) == 0) {
		printf("Integration parms: ms=%f  iavg=%d  bDark=%d  bNL=%d\n", ms, iavg, bDark, bNL);
		fflush(stdout);
	} else {
		fprintf(stderr, "ERROR: Unable to get spectrometer wavelength info\n"); fflush(stderr);
	}

	/* Test acquiring a spectrum with information */
	if ( (rc = Spec_Remote_Acquire_Spectrum(&info, &spectrum)) == 0) {
		fprintf(stderr, "Raw spectrum_Info:\n");
		fprintf(stderr, "  npoints: %d\n", info.npoints);
		fprintf(stderr, "  lambda: %.3f %.3f\n", info.lambda_min, info.lambda_max);
		fprintf(stderr, "  ms_integrate: %.2f   average: %d\n", info.ms_integrate, info.num_average);
		fprintf(stderr, "  flags: %d %d\n", info.use_dark_pixel, info.use_nl_correct);
		fflush(stderr);
		for (i=0; i<50; i+=10) {
			for (j=0; j<10; j++) fprintf(stderr, " %.2f", spectrum[i+j]);
			fprintf(stderr, "\n");
		}
		free(spectrum); spectrum = NULL;
	} else {
		fprintf(stderr, "ERROR: Unable to get spectrum\n"); fflush(stderr);
	}
	fflush(stderr);

	/* Test getting one of the saved spectra */
	if ( (rc = Spec_Remote_Grab_Saved(SPEC_SPECTRUM_REFERENCE, &spectrum, &npt)) == 0) {
		fprintf(stderr, "Reference spectrum [npt=%d  spectrum=%p]\n", npt, spectrum);
		for (j=0; j<5; j++) {
			fprintf(stderr, "[%4.4d]:", j*200);
			for (i=0; i<10; i++) fprintf(stderr, " %6.1f", spectrum[j*200+i]);
			fprintf(stderr, "\n"); fflush(stderr);
		}
		free(spectrum); spectrum = NULL;
	} else {
		fprintf(stderr, "ERROR: Unable to get reference spectrum\n"); fflush(stderr);
	}


	/* Shut down cleanly before exiting */
	Shutdown_Spec_Client();

	return 0;
}

#endif		/* LOCAL_CLIENT_TEST */


/* ===========================================================================
-- Routine to open and initialize the socket to the Spec server
--
-- Usage: int Init_Spec_Client(char *IP_address);
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
static CLIENT_DATA_BLOCK *Spec_Remote = NULL;		/* Connection to the server */

int Init_Spec_Client(char *IP_address) {
	static char *rname = "Init_Spec_Client";
	int rc;
	int server_version;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

	/* Shutdown sockets if already open (reinitialization allowed) */
	if (Spec_Remote != NULL) { CloseServerConnection(Spec_Remote); Spec_Remote = NULL; }

	if ( (Spec_Remote = ConnectToServer("Spec", IP_address, SPEC_MSG_LISTEN_PORT, &rc)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to the server\n", rname); fflush(stderr);
		return 1;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	server_version = Spec_Remote_Query_Server_Version();
	if (server_version <= 0) {
		fprintf(stderr, "ERROR[%s]: Unable to query server version\n", rname); fflush(stderr);
		CloseServerConnection(Spec_Remote); Spec_Remote = NULL;
		return 2;
	} else if (server_version != SPEC_CLIENT_SERVER_VERSION) {
		fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, SPEC_CLIENT_SERVER_VERSION); fflush(stderr);
		CloseServerConnection(Spec_Remote); Spec_Remote = NULL;
		return 3;
	}

	/* Report success, and if not close everything that has been started */
	fprintf(stderr, "INFO: Connected to Spec server on %s\n", IP_address); fflush(stderr);
	return 0;
}

/* ===========================================================================
-- Routine to shutdown high level Spec remote socket server
--
-- Usage: void Shutdown_Spec_Client(void)
--
-- Inputs: none
--
-- Output: Closes the server connection and marks it as unused.
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_Spec_Client(void) {

	if (Spec_Remote != NULL) { CloseServerConnection(Spec_Remote); Spec_Remote = NULL; }
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
--	Usage:  int Spec_Remote_Query_Server_Version(void);
--         int Spec_Remote_Query_Client_Version(void);
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
int Spec_Remote_Query_Client_Version(void) {
	return SPEC_CLIENT_SERVER_VERSION;
}

int Spec_Remote_Query_Server_Version(void) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_QUERY_VERSION;
	
	/* Get the response */
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, SPEC_QUERY_VERSION) != 0) return -1;

	return reply.rc;					/* Will be the version number */
}

/* ===========================================================================
--	Routine to return information on the spectrometer
--
--	Usage:  int Spec_Remote_Get_Spectrometer_Info(SPEC_STECTROMETER_INFO *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful
=========================================================================== */
int Spec_Remote_Get_Spectrometer_Info(SPEC_SPECTROMETER_INFO *info) {

	CS_MSG request, reply;
	SPEC_SPECTROMETER_INFO *my_info = NULL;

	int rc;

	/* Fill in default response (no data) */
	if (info != NULL) memset(info, 0, sizeof(*info));

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_GET_SPECTROMETER_INFO;

	/* Get the response */
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, (void **) &my_info);
	if (Error_Check(rc, &reply, SPEC_GET_SPECTROMETER_INFO) != 0) return rc;

	if (info != NULL) memcpy(info, my_info, sizeof(*info));
	free(my_info);

	return 0;
}

/* ===========================================================================
--	Routine to return the wavelengths of each pixel
--
--	Usage:  int Spec_Remote_Get_Wavelengths(int *count, double **wavelengths);
--
--	Inputs: count - pointer to receive number of values returned
--         wavelength - pointer to variable to receive wavelength array
--		
--	Output: *status filled with information
--
-- Return: 0 if successful
--
-- Note: wavelength array is allocated.  Caller is responsible for
-- releasing the memory
=========================================================================== */
int Spec_Remote_Get_Wavelengths(int *count, double **wavelengths) {

	CS_MSG request, reply;
	int rc;

	/* Fill in default response (no data) */
	if (count != NULL) *count = 0;
	if (wavelengths != NULL) *wavelengths = NULL;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_GET_WAVELENGTHS;

	/* Get the response (rc is number of bytes returned) */
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, (void **) wavelengths);
	if (Error_Check(rc, &reply, SPEC_GET_WAVELENGTHS) != 0) return rc;

	if (count != NULL) *count = reply.option;
	return 0;
}

/* ===========================================================================
-- Routine to query current integration parameters
--
-- Usage: int int Spec_Remote_GetIntegrationParms(double *ms, int *iavg, BOOL *bDark, BOOL *bNL);
--
-- Inputs: ms    - integration time in ms
--         iavg  - number of averages for each collection
--         bDark - enable use of dark pixels for dark current correction
--         bNL   - enable non-linearity corrections (requires bDark)
--
-- Output: if !NULL, each parameter set to current values
--
-- Return: 0 on success
=========================================================================== */
int Spec_Remote_GetIntegrationParms(double *ms, int *iavg, BOOL *bDark, BOOL *bNL) {

	CS_MSG request, reply;
	SPEC_INTEGRATION_PARMS *parms;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_GET_INTEGRATION_PARMS;
	request.data_len = 0;

	/* Get the response */
	rc = StandardServerExchange(Spec_Remote, request, (void *) &parms, &reply, (void **) &parms);

	if (ms    != NULL) *ms    = parms->ms_integrate;
	if (iavg  != NULL) *iavg  = parms->num_average;
	if (bDark != NULL) *bDark = parms->use_dark_pixel;
	if (bNL   != NULL) *bNL   = parms->use_nl_correct;

	return Error_Check(rc, &reply, SPEC_GET_INTEGRATION_PARMS);
}

/* ===========================================================================
-- Routine (used by spec_client) to set new integration parameters
--
-- Usage: int int Spec_Remote_SetIntegrationParms(double ms, int iavg, BOOL bDark, BOOL bNL);
--
-- Inputs: ms    - desired integration time in ms.  0.001 <= ms <= 65000
--         iavg  - number of averages for each collection.  1 <= iavg <= 999
--         bDark - enable use of dark pixels for dark current correction
--         bNL   - enable non-linearity corrections (requires bDark)
--
-- Output: Sets parameters into internal structures and modifies dialog box
--
-- Return: 0 if successful; otherwise bit-flag of parameters issues
--           0x0001 - ms below limit
--           0x0002 - ms above limit
--           0x0010 - iavg below limit
--           0x0020 - iavg above limit
--           0x0100 - bNL TRUE but bDark FALSE (no correction will be performed)
=========================================================================== */
int Spec_Remote_SetIntegrationParms(double ms, int iavg, BOOL bDark, BOOL bNL) {

	CS_MSG request, reply;
	SPEC_INTEGRATION_PARMS parms;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_SET_INTEGRATION_PARMS;
	request.data_len = sizeof(parms);
	parms.ms_integrate = ms;
	parms.num_average = iavg;
	parms.use_dark_pixel = bDark;
	parms.use_nl_correct = bNL;

	/* Get the response */
	rc = StandardServerExchange(Spec_Remote, request, (void *) &parms, &reply, NULL);

	return Error_Check(rc, &reply, SPEC_SET_INTEGRATION_PARMS);
}

/* ===========================================================================
--	Routine to acquire a spectrum (local save)
--
--	Usage:  int Spec_Remote_Acquire_Spectrum(SPEC_SPECTRUM_INFO *info, double **spectrum);
--
--	Inputs: info - pointer to buffer to receive information about image
--         spectrum - pointer to get malloc'd memory with the spectrum itself
--                    caller responsible for releasing this memory
-- 
--	Output: info and spectrum defined if new spectrum obtained
--
-- Return: 0 if successful, other error indication
--         On error *spectrum will be NULL and *info will be zero
--
-- Note: This is really 3 transactions with the server
--         (1) SPEC_ACQUIRE_SPECTRUM   [captures the spectrum]
--         (2) SPEC_GET_SPECTRUM_INFO  [transmits information about spectrum]
--         (3) SPEC_GET_SPECTRUM_DATA  [transmits actual spectrum doubles]
=========================================================================== */
int Spec_Remote_Acquire_Spectrum(SPEC_SPECTRUM_INFO *info, double **spectrum) {

	CS_MSG request, reply;
	SPEC_SPECTROMETER_INFO *my_info = NULL;

	int rc;

	/* Fill in default response (no data) */
	if (info  != NULL) memset(info, 0, sizeof(*info));
	if (spectrum != NULL) *spectrum = NULL;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_ACQUIRE_SPECTRUM;
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, SPEC_ACQUIRE_SPECTRUM) != 0) return -1;

	/* Acquire information about the image */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_GET_SPECTRUM_INFO;
	my_info = NULL;
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, SPEC_GET_SPECTRUM_INFO) != 0 || reply.rc != 0) return -1;

	/* Copy the info over to user space */
	if (info != NULL) memcpy(info, my_info, sizeof(*info));
	free(my_info);

	/* Get the actual image data (will be big) */
	memset(&request, 0, sizeof(request));
	request.msg   = SPEC_GET_SPECTRUM_DATA;
	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, (void **) spectrum);
	if (Error_Check(rc, &reply, SPEC_GET_SPECTRUM_DATA) != 0) return -1;
	
	return 0;
}


/* ===========================================================================
--	Routine to grab one of the save spectra in the SPEC program
--
--	Usage:  int Spec_Remote_Grab_Saved(SPEC_GRAB_TYPE target, double **data, int *npt);
--
--	Inputs: target - which to acquire
--                    0 = SPEC_SPECTRUM_RAW
--							 1 = SPEC_SPECTRUM_DARK
--							 2 = SPEC_SPECTRUM_REFERENCE
--							 3 = SPEC_SPECTRUM_TEST
--         data   - pointer to location to save the returned data buffer
--                  caller responsible for releasing this memory
--         npt    - pointer with number of points
-- 
--	Output: *data - pointer to a malloc'd space with the data (or NULL on error)
--         *npt  - number of points transferred
--
-- Return: 0 if successful, other error indication
--         On error *data will be NULL and *npt will be zero
--
-- Note: Returns the data that part of the CURVE buffers from SPEC
=========================================================================== */
int Spec_Remote_Grab_Saved(SPEC_GRAB_TYPE target, double **data, int *npt) {

	CS_MSG request, reply;
	int rc;

	/* Verify parameters and set default values */
	if (data != NULL) *data = NULL;
	if (npt  != NULL) *npt  = 0;
	if (data == NULL || npt == NULL) return -1;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg = (target == SPEC_SPECTRUM_RAW )		  ? SPEC_GET_LIVE_SPECTRUM :
					  (target == SPEC_SPECTRUM_DARK)		  ? SPEC_GET_DARK_SPECTRUM :
					  (target == SPEC_SPECTRUM_TEST)		  ? SPEC_GET_TEST_SPECTRUM :
					  (target == SPEC_SPECTRUM_REFERENCE) ? SPEC_GET_REFERENCE_SPECTRUM :
					  -1;
	if (request.msg < 0) return -1;								/* Not a valid entry */

	rc = StandardServerExchange(Spec_Remote, request, NULL, &reply, (void **) data);
	if (Error_Check(rc, &reply, request.msg) != 0) return -1;

	/* Also save the number of points which better match the length information */
	*npt  = reply.option;
	return 0;
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

	if (Spec_Remote != NULL) { CloseServerConnection(Spec_Remote); Spec_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
