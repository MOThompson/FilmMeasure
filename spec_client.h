#ifndef _SPEC_CLIENT_INCLUDED

#define	_SPEC_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	SPEC_CLIENT_SERVER_VERSION	(1002)			/* Version of this code */

/* =============================
-- Port that the server runs
============================= */
#define	SPEC_MSG_LISTEN_PORT		(1920)			/* Port for high level Spec function interface */

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */

/* List of the allowed requests */
#define SERVER_END						(0)			/* Shut down server (please don't use) */
#define SPEC_QUERY_VERSION				(1)			/* Return version of the server code */
#define SPEC_GET_SPECTROMETER_INFO	(2)			/* Return structure with camera data */
#define SPEC_GET_WAVELENGTHS			(3)			/* Get the list of wavelengths (reply.option = npt) */
#define SPEC_GET_INTEGRATION_PARMS	(4)			/* Return integration parameters */
#define SPEC_SET_INTEGRATION_PARMS	(5)			/* Set integration parameters */
#define SPEC_ACQUIRE_SPECTRUM			(6)			/* Acquire a spectrum */
#define SPEC_GET_SPECTRUM_INFO		(7)			/* Get information on the spectrum */
#define SPEC_GET_SPECTRUM_DATA		(8)			/* Get the spectrum data */
#define SPEC_GET_LIVE_SPECTRUM		(9)			/* Return current saved live spectrum (reply.option = npt) */
#define SPEC_GET_DARK_SPECTRUM		(10)			/* Return saved dark spectrum (reply.option = npt) */
#define SPEC_GET_REFERENCE_SPECTRUM	(11)			/* Return saved reference spectrum (reply.option = npt) */
#define SPEC_GET_TEST_SPECTRUM		(12)			/* Return saved extra (test) spectrum (reply.option = npt) */

/* Structures associated with the client/server interactions (packing important) */
typedef int32_t	BOOL32;					/* Local 32 bit boolean */

#pragma pack(4)
typedef struct _SPEC_SPECTROMETER_INFO {	/* Information about instrument */
	BOOL32 spec_ok;							/* Boolean Spectrometer exists and ok */
	char model[32];							/* Character string with model */
	char serial[32];							/* Serial number of the mounted spectrometer */
	int32_t npoints;							/* Number of points in a spectrum */
	double lambda_min, lambda_max;		/* Min and max wavelength of spectrometer */
	double ms_integrate;						/* Integration target */
	int32_t num_average;						/* Averaging specified */
	BOOL32 use_dark_pixel;					/* Enable use of dark pixels for dark-current correction */
	BOOL32 use_nl_correct;					/* Enable non-linear correction to counts */
} SPEC_SPECTROMETER_INFO;
#pragma pack()

#pragma pack(4)
/* Need to add the time of the acquisition to this structure */
typedef struct _SPEC_SPECTRUM_INFO {	/* Details about current spectrum */
	int32_t npoints;							/* Number of points in a spectrum */
	double lambda_min, lambda_max;		/* Min and max wavelength of spectrometer */
	double ms_integrate;						/* Integration target */
	int32_t num_average;						/* Averaging specified */
	BOOL32 use_dark_pixel;					/* Boolean flag for dark pixel correction */
	BOOL32 use_nl_correct;					/* Boolean flag for non-linear correction */
	time_t timestamp;							/* Time at acquisition completion (from time()) */
} SPEC_SPECTRUM_INFO;
#pragma pack()

#pragma pack(4)
typedef struct _SPEC_INTEGRATION_PARMS {
	double ms_integrate;						/* Integration target */
	int32_t num_average;						/* Averaging specified */
	BOOL32 use_dark_pixel;					/* Enable use of dark pixels for dark-current correction */
	BOOL32 use_nl_correct;					/* Enable non-linear correction to counts */
} SPEC_INTEGRATION_PARMS;	
#pragma pack()

/* ===========================================================================
-- Routine to open and initialize the socket to the DCx server
--
-- Usage: int Init_Spec_Client(char *IP_address);
--
-- Inputs: IP_address - IP address in normal form.  Use "127.0.0.1" for loopback test
--                      if NULL, uses DFLT_SERVER_IP_ADDRESS
--
-- Output: Creates MUTEX semaphores, opens socket, sets atexit() to ensure closure
--
-- Return:  0 - successful
--          1 - unable to create semaphores for controlling access to hardware
--          2 - unable to open the sockets (one or the other)
--
-- Notes: Must be called before any attempt to communicate across the socket
=========================================================================== */
int Init_Spec_Client(char *IP_address);

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
int Shutdown_Spec_Client(void);

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
int Spec_Remote_Query_Client_Version(void);
int Spec_Remote_Query_Server_Version(void);


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
int Spec_Remote_Get_Spectrometer_Info(SPEC_SPECTROMETER_INFO *info);

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
int Spec_Remote_Get_Wavelengths(int *count, double **wavelengths);

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
int Spec_Remote_GetIntegrationParms(double *ms, int *iavg, BOOL *bDark, BOOL *bNL);

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
int Spec_Remote_SetIntegrationParms(double ms, int iavg, BOOL bDark, BOOL bNL);

/* ===========================================================================
--	Routine to acquire an image (local save)
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
int Spec_Remote_Acquire_Spectrum(SPEC_SPECTRUM_INFO *info, double **spectrum);

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
typedef enum _SPEC_GRAB_TYPE { SPEC_SPECTRUM_RAW=0, SPEC_SPECTRUM_DARK=1, SPEC_SPECTRUM_REFERENCE=2, SPEC_SPECTRUM_TEST=4 } SPEC_GRAB_TYPE;

int Spec_Remote_Grab_Saved(SPEC_GRAB_TYPE target, double **data, int *npt);

#endif		/* _SPEC_CLIENT_INCLUDED */
