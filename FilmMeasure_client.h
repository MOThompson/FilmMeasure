#ifndef _FILM_CLIENT_INCLUDED

#define	_FILM_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	FILM_CLIENT_SERVER_VERSION	(1001)			/* Version of this code */

/* =============================
-- Port that the server runs
============================= */
#define	FILM_MSG_LISTEN_PORT		(1921)			/* Port for high level FilmMeasure function interface */

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */

/* List of the allowed requests */
#define SERVER_END						(0)			/* Shut down server (please don't use) */
#define FILM_QUERY_VERSION				(1)			/* Return version of the server code */
#define FILM_DO_MEASURE					(2)			/* Do a measurement (press MEASURE button) */
#define FILM_SAVE_DATA					(3)			/* Save the data to a file */
#define FILM_QUERY_FIT					(4)			/* Query film thickness fitting parameters */

/* Structures associated with the client/server interactions (packing important) */
typedef int32_t	BOOL32;					/* Local 32 bit boolean */

/* ===========================================================================
-- Routine to open and initialize the socket to the DCx server
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
int Init_FilmMeasure_Client(char *IP_address);

/* ===========================================================================
-- Routine to shutdown high level Spec remote socket server
--
-- Usage: void Shutdown_Spec_Client(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_FilmMeasure_Client(void);

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
int FilmMeasure_Remote_Query_Client_Version(void);
int FilmMeasure_Remote_Query_Server_Version(void);


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
int FilmMeasure_Remote_Measure(void);


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
int FilmMeasure_Remote_SaveData(char *path);

/* ===========================================================================
--	Routine to return values from ThinFilm fit (thickness + scaling)
--
--	Usage:  int FilmMeasure_Remote_QueryFit(double *vals, int maxvals);
--
--	Inputs: vals    - array to receive values
--         maxvals - maximum number of values to transfer
--		
--	Output: *vals[] - varied parameters
--
-- Return: # of values stored, 0 if none exist, or <0 on error
--
-- Note: All thickness parameters in the structure are returned, followed
--       by the scaling constant
=========================================================================== */
int FilmMeasure_Remote_QueryFit(double *vals, int maxvals);

#endif		/* _FILM_CLIENT_INCLUDED */
