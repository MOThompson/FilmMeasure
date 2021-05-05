/* FilmMeasure.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

#define NEED_WINDOWS_LIBRARY			/* Define to include windows.h call functions */

#define	SPEC_CLIENT_IP			LOOPBACK_SERVER_IP_ADDRESS

#define	ERROR_BEEP_FREQ	(880)
#define	ERROR_BEEP_MS		(300)


/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>					  /* for defining several useful types and macros */
#include <stdio.h>					  /* for performing input and output */
#include <stdlib.h>					  /* for performing a variety of operations */
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <math.h>						  /* basic math functions */
#include <float.h>
#include <limits.h>
#include <assert.h>
#include <stdint.h>				     /* C99 extension to get known width integers */
#include <time.h>
#include <io.h>							/* Contains _findfirst */
#include <sys/types.h>
#include <sys/stat.h>

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
#include "server_support.h"		/* Server support */
#include "spec.h"						/* Access to the spectrometer information */
#include "spec_client.h"			/* For prototypes				*/
#include "win32ex.h"
#include "graph.h"
#include "resource.h"
#include "tfoc.h"
#include "curfit.h"
#include "filmmeasure.h"			/* Depends on structures in previous .h */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

#ifndef PATH_MAX
	#define	PATH_MAX	260
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
INT_PTR CALLBACK MainDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void strip_white_space(char *buffer);
static int validate_IP_address(char *ip);
static GRAPH_CURVE *ReallocRawCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color);
static GRAPH_CURVE *ReallocLightCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color);
static GRAPH_CURVE *ReallocReflCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color);
static GRAPH_CURVE *ReallocResidualCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color);
static int Acquire_Raw_Spectrum(HWND hdlg, FILM_MEASURE_INFO *info, SPEC_SPECTRUM_INFO *spectrum_info, double **spectrum);

TFOC_SAMPLE *MakeSample(int nlayers, FILM_LAYERS *film);
int TFOC_GetReflData(TFOC_SAMPLE *sample, double scaling, double theta, POLARIZATION mode, double temperature, int npt, double *lambda, double *refl);
static char *Find_TFOC_Database(char *database, size_t len, int *ierr);

static int InitMaterialsList(void);
static int FindMaterialIndex(char *text, char **endptr);

static int CalcChiSqr(double *x, double *y, double *s, double *yfit, int npt, double xmin, double xmax, double *pchisqr, int *pdof);
static int do_fit(HWND hdlg, FILM_MEASURE_INFO *info);

static int QueryLogfile(HWND hdlg, int wID);
static int QueryAutofile(HWND hdlg);
static int SaveData(FILM_MEASURE_INFO *info, char *fname);
static int LoadData(FILM_MEASURE_INFO *info, char *path);
static int WriteProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info);
static int ReadProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
static FILM_MEASURE_INFO *main_info = NULL;
static char *IniFile = "./FilmMeasure.ini";				/* Initialization information */
	
/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static HINSTANCE hInstance=NULL;
static HWND main_hdlg = NULL;					/* Handle to primary dialog box */

static CB_INT_LIST *materials = NULL;
static int materials_dim=0;											/* Dimensioned size */
static int materials_cnt=0;

static int colors[7] = {						/* Color scheme for the graphs (and the legend) */
	RGB(200,200,0),	/* Raw spectra - yellowish */
	RGB(132,112,255),	/* Dark spectra - purple */
	RGB(255,192,203),	/* Reference spectra - pink */
	RGB(0,128,255),	/* Illumination corrected - blue */
	RGB(218,0,0),		/* Fit - red */
	RGB(0,192,192),	/* residuals - cyan */
	RGB(128,128,128)	/* Absorbance - grey */
};

/* ===========================================================================
-- Entry point for windows applications
--
-- Usage: int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, in nWinMode);
--
-- Input: hThisInst - value instance that should be stored
--
-- Return: return code to calling OS function
=========================================================================== */
int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode) {

	/* Send a newline in case we are monitoring stderr */
	fprintf(stderr, "\n"); fflush(stderr);
	
	/* If not done, make sure we are loaded.  Assume safe to call multiply */
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");

	/* Load the class for the graph and bitmap windows */
	Graph_StartUp(hThisInst);								/* Initialize the graphics control */

	/* And show the dialog box */
	hInstance = hThisInst;
	DialogBoxParam(hInstance, "FILMMEASURE_DIALOG", HWND_DESKTOP, (DLGPROC) MainDlgProc, (LPARAM) main_info);			/* For re-entrant, use the previous saved values */

	return 0;
}

/* ===========================================================================
-- Main dialog processing routine
--
-- Usage: INT_PTR CALLBACK MainDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
--
-- Inputs: hdlg - dialog box handle
--         msg  - what to do
--         wParam, lParam - parameters to the operatations
--
-- Output: lots of side effects
--
-- Return: return code, generally BOOL
=========================================================================== */
#define	TIMER_AUTO_MEASURE			(3)

INT_PTR CALLBACK MainDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "MainDlgProc";

	BOOL rcode, enable;
	int wID, wNotifyCode;
	int i, ms, nvary, ilayer, rc;
	char szBuf[256];

	FILM_LAYERS *stack;
	int imat, nlayers;

	FILM_MEASURE_INFO *info;						/* General structure for the window */

	static int spec_hide_list[] = { 
		IDS_SPEC_TXT_0, IDS_SPEC_TXT_1, IDS_SPEC_TXT_2, IDS_SPEC_TXT_3, IDS_SPEC_TXT_4, IDS_SPEC_TXT_5, IDS_SPEC_TXT_6,
		IDT_MODEL, IDT_SERIAL, IDT_LAMBDA_MIN, IDT_LAMBDA_MAX, IDT_SPECTRUM_LENGTH,
		IDC_DARK_PIXELS, IDC_NL_POLYNOMIAL, IDT_MS_INTEGRATE, IDT_AVERAGES, IDB_SPEC_PARMS_UPDATE, 
		ID_NULL };
	static int spec_disable_list[] = { 
		IDB_COLLECT_DARK, IDB_COLLECT_REFERENCE, IDB_MEASURE,
		IDB_TAKE_DARK, IDB_TAKE_REFERENCE, IDB_MEASURE_RAW, IDB_MEASURE_TEST,
		ID_NULL };

	SPEC_SPECTRUM_INFO spectrum_info;

	GRAPH_CURVE *cv;
	GRAPH_SCALES scales;
	GRAPH_ZFORCE zforce;
	double rval, lower, upper, chisqr, *data;
	int npt, dof;

	/* List of controls which will respond to <ENTER> with a WM_NEXTDLGCTL message */
	/* Make <ENTER> equivalent to losing focus */
	static int DfltEnterList[] = {					
		IDV_REF_NM_0,  IDV_REF_NM_1,  IDV_REF_NM_2,  IDV_REF_NM_3,  IDV_REF_NM_4,
		IDV_FILM_NM_0, IDV_FILM_NM_1, IDV_FILM_NM_2, IDV_FILM_NM_3, IDV_FILM_NM_4, 
		IDV_NM_LOW_0,  IDV_NM_LOW_1,  IDV_NM_LOW_2,  IDV_NM_LOW_3,  IDV_NM_LOW_4, 
		IDV_NM_HIGH_0, IDV_NM_HIGH_1, IDV_NM_HIGH_2, IDV_NM_HIGH_3, IDV_NM_HIGH_4, 
		IDV_FIT_LAMBDA_MIN, IDV_FIT_LAMBDA_MAX, 
		IDV_FIT_SCALING, IDV_FIT_SCALING_MIN, IDV_FIT_SCALING_MAX, 
		IDV_GRAPH_LAMBDA_MAX, IDV_GRAPH_LAMBDA_MIN, 
		IDV_SPEC_IP, IDV_MEASURE_DELAY, IDV_LOGFILE,
		IDV_AUTOFILE_DIR, IDV_AUTOFILE_FILE, IDV_AUTOFILE_NUM,
		ID_NULL };

	HWND hwndTest;
	int *hptr;

	/* List of IP addresses */
	static CB_PTR_LIST ip_list[] = {
		{"Manual",								NULL},
		{"Loop back [127.0.0.1]",			LOOPBACK_SERVER_IP_ADDRESS}, 
		{"Chess-host [128.253.129.74]",	"128.253.129.74"},
		{"LSA-host   [128.253.129.71]",	"128.253.129.71"}
	};

/* Recover the information data associated with this window */
	if (msg != WM_INITDIALOG) info = (FILM_MEASURE_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);

/* The message loop */
	switch (msg) {

		case WM_INITDIALOG:

//			DlgCenterWindow(hdlg);				/* Have start in upper right */
			sprintf_s(szBuf, sizeof(szBuf), "Version 1.0: %s", LinkDate);
			SetDlgItemText(hdlg, IDT_COMPILE_VERSION, szBuf);

			/* Initialize the materials database */
			if (InitMaterialsList() <= 1) {
				MessageBox(hdlg, "Unable to locate the materials n,k database.  Normally the database is the subdirectory \"database.nk\".", "FilmMeasure initialize failure", MB_ICONERROR | MB_OK);
				EndDialog(hdlg, 3);			/* Fatal error */
			}

			/* See if we have been passed an INFO structure to use (pre-initialized) */
			/* At the moment, we clear the structure on exit so can't be repassed, but may change in future */
			if ( (info = (FILM_MEASURE_INFO *) lParam) != NULL) {		/* Already exists */
				if (info->magic != FILM_MEASURE_MAGIC) {
					MessageBox(hdlg, "Passed FilmMeasure info structure was invalid.  Not sure how to proceed so abaonding the request to open", "FilmMeasure initialize failure", MB_ICONERROR | MB_OK);
					EndDialog(hdlg, 3);			/* Fatal error */
				} else if (info->hdlg != NULL) {
					MessageBox(hdlg, "Passed FilmMeasure info structure in use by another dialog.  Cannot have two sharing the same info", "FilmMeasure initialize failure", MB_ICONERROR | MB_OK);
					EndDialog(hdlg, 3);			/* Fatal error */
				}
			} else if ( (main_info = info = (FILM_MEASURE_INFO *) calloc(1, sizeof(*info))) == NULL) {
				MessageBox(hdlg, "Unable to initialize a FilmMeasure info structure.  Unable to continue.", "FilmMeasure initialize failure", MB_ICONERROR | MB_OK);
				EndDialog(hdlg, 3);			/* Fatal error */
			} else {
				info->magic = FILM_MEASURE_MAGIC;			/* Initialize parameters in the info structure */
				info->hdlg = hdlg;								/* Save for use and mark this info structure in use */
				info->lambda_min = 200.0;						/* Graph X-range limits */
				info->lambda_max = 900.0;
				info->lambda_autoscale = TRUE;
				info->fit_parms.lambda_min  = 300.0;		/* Fitting parameters */
				info->fit_parms.lambda_max  = 800.0;
				info->fit_parms.scaling_min = 0.95;
				info->fit_parms.scaling_max = 1.05;
				info->sample.scaling        = 1.0;
				info->reference.substrate = FindMaterialIndex("c-Si", NULL);
				if (info->reference.substrate <= 0) info->reference.substrate = 1;
				info->sample.substrate = FindMaterialIndex("c-Si", NULL);
				if (info->sample.substrate <= 0) info->reference.substrate = 1;
			}

			/* Save the information structure in the dialog box info parameter */
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG) info);

			/* Copy default IP address and try to connect to the LasGo client/server */
			/* Due to the possible timeouts on making connections, do as a thread */
			/* Fill in combo boxes and select default value */
			EnableWindow(GetDlgItem(hdlg, IDV_SPEC_IP), FALSE);
			ComboBoxFillPtrList(hdlg, IDL_SPEC_IP, ip_list, CB_COUNT(ip_list));
			ComboBoxSetByPtrValue(hdlg, IDL_SPEC_IP, LOOPBACK_SERVER_IP_ADDRESS);
			strcpy_m(info->spec_IP, sizeof(info->spec_IP), LOOPBACK_SERVER_IP_ADDRESS);
			SetDlgItemText(hdlg, IDV_SPEC_IP, info->spec_IP);

			SendMessage(hdlg, WMP_UPDATE_SPEC_PARMS, 0, 0);					/* Also updates the graphs */

			/* Load the combo boxes from the database for the sample */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxClearList(hdlg, IDC_FILM_MATERIAL_0+i);
				ComboBoxFillIntList(hdlg, IDC_FILM_MATERIAL_0+i, materials, materials_cnt);
			}
			ComboBoxClearList(hdlg, IDC_FILM_SUBSTRATE);
			ComboBoxFillIntList(hdlg, IDC_FILM_SUBSTRATE, materials, materials_cnt);

			/* Load the combo boxes from the database for the reference */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxClearList(hdlg, IDC_REF_MATERIAL_0+i);
				ComboBoxFillIntList(hdlg, IDC_REF_MATERIAL_0+i, materials, materials_cnt);
			}
			ComboBoxClearList(hdlg, IDC_REF_SUBSTRATE);
			ComboBoxFillIntList(hdlg, IDC_REF_SUBSTRATE, materials, materials_cnt);

			/* Reset to previous state via profile structure if available */
			ReadProfileInfo(hdlg, info);								/* Loads parameters and modifies sample/reference */

			/* Finally .. transfer parameters from INFO to the dialog box */
			/* Autoscale and manual wavelength ranges for graph */
			SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);
			SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0, 0);
			SetDlgItemCheck(hdlg, IDC_AUTORANGE_WAVELENGTH, info->lambda_autoscale);
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MIN, "%.1f", info->lambda_min);
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MAX, "%.1f", info->lambda_max);
			EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MIN, ! info->lambda_autoscale);
			EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MAX, ! info->lambda_autoscale);

			/* Filling parameters */
			SetDlgItemDouble(hdlg, IDV_FIT_LAMBDA_MIN,  "%.1f", info->fit_parms.lambda_min);
			SetDlgItemDouble(hdlg, IDV_FIT_LAMBDA_MAX,  "%.1f", info->fit_parms.lambda_max);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING_MIN, "%.2f", info->fit_parms.scaling_min);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING_MAX, "%.2f", info->fit_parms.scaling_max);

			SetDlgItemInt(hdlg, IDV_MEASURE_DELAY, 1, FALSE);
			SetDlgItemText(hdlg, IDV_AUTOFILE_DIR, ".");
			SetDlgItemText(hdlg, IDV_AUTOFILE_FILE, "scan");
			SetDlgItemText(hdlg, IDV_AUTOFILE_NUM, "0000");

			rcode = TRUE; break;

		case WM_CLOSE:
			if (info->spec_ok) { Shutdown_Spec_Client();	info->spec_ok = FALSE; }
			WriteProfileInfo(hdlg, info);
			info->hdlg = NULL;								/* Mark info structure as no longer in use */
			{
				info->magic = 0;								/* For now, disable ability to reload this dialog */
				SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_CLEAR, 0, 0);			/* Free all curves */
				SendDlgItemMessage(hdlg, IDU_GRAPH,     WMP_CLEAR, 0, 0);			/* Free all curves */
				info->cv_raw = info->cv_dark = info->cv_ref = info->cv_light = NULL;
				info->cv_refl = info->cv_fit = info->cv_residual = NULL;
				if (info->lambda != NULL) { free(info->lambda); info->lambda = NULL; }
				info->lambda_transferred = FALSE;
				if (info->tfoc_reference != NULL) { free(info->tfoc_reference); info->tfoc_reference = NULL; }
				if (info->tfoc_fit != NULL) { free(info->tfoc_fit); info->tfoc_fit = NULL; }
				free(info);										/* Which means we can free the structure */
			}
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_AUTO_MEASURE) {
				if (info->spec_ok) SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDB_MEASURE, BN_CLICKED), (LPARAM) hdlg);
			}
			rcode = TRUE; break;

/* See dirsync.c for way to change the text color instead of just the background color */
/* See lasgo queue.c for way to change background */
		case WM_CTLCOLORMSGBOX:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSCROLLBAR:
		case WM_CTLCOLORBTN:						/* Real buttons like OK or CANCEL */
			break;
		case WM_CTLCOLORSTATIC:					/* Includes check boxes as well as static */
		{
			HDC hdc = (HDC) wParam;
			HWND hwnd = (HWND) lParam;
			LONG color;
			switch (GetDlgCtrlID(hwnd)) {
				case IDT_DARK_STRIPE:							/* This is the thin stripe under the graph window */
					color = colors[0]; break;
				case IDC_SHOW_RAW:
					color = colors[0]; break;
				case IDC_SHOW_DARK:
					color = colors[1]; break;
				case IDC_SHOW_REF:
					color = colors[2]; break;
				case IDC_SHOW_LIGHT:
					color = colors[3]; break;
				case IDC_SHOW_FIT:
					color = colors[4]; break;
				case IDC_SHOW_RESIDUAL:
					color = colors[5]; break;
				default:
					color = 0;
			}
			if (color != 0) {
				SetBkColor(hdc, RGB(32,32,32));				/* Background of font color (dflt is 240,240,250) */
				SetDCBrushColor(hdc, RGB(32,32,32));		/* Box color (dflt is 240,240,250) */
				SetTextColor(hdc, color);						/* Text color */
				return (LRESULT) GetStockObject(DC_BRUSH);
			}
		}
			break;

		/* Called only by CONNECT button */
		case WMP_OPEN_SPEC:
			if (info->spec_ok) Shutdown_Spec_Client();	/* Close down cleanly */
			info->spec_ok = FALSE;								/* Definitely no longer connected */
			rc = Init_Spec_Client(info->spec_IP);
			if (rc == 0) {											/* Success - only need to verify versions */
				int client_version, server_version;
				client_version = Spec_Remote_Query_Client_Version();
				server_version = Spec_Remote_Query_Server_Version();
				info->spec_ok = (client_version == server_version);
				if (! info->spec_ok) {
					MessageBox(hdlg, "ERROR: Version mismatch between client and server.  Have to abort", "SPEC connect failure", MB_ICONERROR | MB_OK);
					Shutdown_Spec_Client();
				}
			} else if (rc == 1) {
				MessageBox(hdlg, "Unable to open the socket.  Verify the SPEC server is running.", "SPEC connect failure", MB_ICONERROR | MB_OK);
			} else if (rc == 2) {
				MessageBox(hdlg, "Unable to query server version from the SPEC server.  Unknown reason.", "SPEC connect failure", MB_ICONERROR | MB_OK);
			} else if (rc == 3) {
				MessageBox(hdlg, "Mismatch between server and client versions for the SPEC connection.", "SPEC connect failure", MB_ICONERROR | MB_OK);
			} else {
				sprintf_s(szBuf, sizeof(szBuf), "Unable to open SPEC server.  Unknown return code (rc=%d)", rc);
				MessageBox(hdlg, szBuf, "SPEC connect failure", MB_ICONERROR | MB_OK);
			}
			rcode = TRUE; break;

		/* Called when the spectrometer server is initialized or disconnected */
		/* This will establish the size of buffers and wavelength ranges */
		/* Also enables / disables the appropriate controls based on info->spec_ok */

		case WMP_LOAD_SPEC_WAVELENGTHS:
			if (info->spec_ok && ! info->lambda_transferred) {
				info->npt = info->status.npoints;								/* Make sure this is still valid (may be different from a read) */

				if (info->lambda != NULL) free(info->lambda);				/* Free previous copy */
				rc = Spec_Remote_Get_Wavelengths(&npt, &info->lambda);
				if (rc != 0) {
					sprintf_s(szBuf, sizeof(szBuf), "Unable to get wavelength data [rc=%d].\n\nProceed with caution.", rc);
					MessageBox(hdlg, szBuf, "Spectrometer query failure", MB_ICONWARNING | MB_OK);
					info->lambda = malloc(info->npt * sizeof(*info->lambda));
					for (i=0; i<info->npt; i++) info->lambda[i] = info->status.lambda_min + i/(info->npt-1.0)*(info->status.lambda_max-info->status.lambda_min);
				} else if (npt != info->npt) {
					sprintf_s(szBuf, sizeof(szBuf), "Number of data points returned for wavelength [%d]\ndoes not match the number from the spectrometer size [%d].\n\nProceed with caution.", npt, info->npt);
					MessageBox(hdlg, szBuf, "Spectrometer size mismatch", MB_ICONWARNING | MB_OK);
				}

				/* As the number of points may have changed, and wavelengths as well, need to go through all curves */
				if (info->cv_raw != NULL) {
					cv = info->cv_raw = ReallocRawCurve(hdlg, info, info->cv_raw, info->npt, 0, "sample", colors[0]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->cv_dark != NULL) {
					cv = info->cv_dark = ReallocRawCurve(hdlg, info, info->cv_dark, info->npt, 1, "dark", colors[1]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->cv_ref != NULL) {
					cv = info->cv_ref = ReallocRawCurve(hdlg, info, info->cv_ref, info->npt, 2, "reference", colors[2]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->cv_light != NULL) {
					cv = info->cv_light = ReallocLightCurve(hdlg, info, info->cv_light, info->npt, 3, "light", colors[3]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}					
				if (info->cv_refl != NULL) {
					cv = info->cv_refl = ReallocReflCurve(hdlg, info, info->cv_refl, info->npt, 0, "reflectance", colors[0]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->cv_fit != NULL) {
					cv = info->cv_fit = ReallocReflCurve(hdlg, info, info->cv_fit, info->npt, 4, "fit", colors[4]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->cv_residual != NULL) {
					cv = info->cv_residual = ReallocResidualCurve(hdlg, info, info->cv_residual, info->npt, 4, "residual", colors[5]);
					memcpy(cv->x,  info->lambda, info->npt*sizeof(double));
				}
				if (info->tfoc_reference != NULL) free(info->tfoc_reference);	/* These will be regenerated when needed */
				if (info->tfoc_fit  != NULL) free(info->tfoc_fit);					/* Lengths may have changed */
				info->tfoc_reference = info->tfoc_fit = NULL;

				info->lambda_transferred = TRUE;
			}
			rcode = TRUE; break;

		case WMP_LOAD_SPEC_PARMS:
			if (info->spec_ok) {
				if (Spec_Remote_Get_Spectrometer_Info(&info->status) != 0) {
					MessageBox(hdlg, "ERROR: Unable to get spectrometer information.\nClosing remote connection", "SPEC status failure", MB_ICONERROR | MB_OK);
					Shutdown_Spec_Client(); info->spec_ok = FALSE;

				} else {			/* Output informationabout the spectrometer */

					SetDlgItemText(hdlg, IDT_MODEL, info->status.model);
					SetDlgItemText(hdlg, IDT_SERIAL, info->status.serial);
					SetDlgItemDouble(hdlg, IDT_LAMBDA_MIN, "%.2f", info->status.lambda_min);
					SetDlgItemDouble(hdlg, IDT_LAMBDA_MAX, "%.2f", info->status.lambda_max);
					SetDlgItemInt(hdlg, IDT_SPECTRUM_LENGTH, info->status.npoints, FALSE);
					SetDlgItemCheck(hdlg, IDC_DARK_PIXELS,   info->status.use_dark_pixel);
					SetDlgItemCheck(hdlg, IDC_NL_POLYNOMIAL, info->status.use_nl_correct);
					SetDlgItemDouble(hdlg, IDT_MS_INTEGRATE, "%.2f", info->status.ms_integrate);
					SetDlgItemInt(hdlg, IDT_AVERAGES, info->status.num_average, FALSE);
					info->npt = info->status.npoints;

					/* New spectrometer ... new wavelength data */
					info->lambda_transferred = FALSE;
					SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);

					/* Set up the scales for the graphs (especially X) and redraw graphs */
					if (info->lambda_autoscale) {
						info->lambda_min = max(200.0, info->status.lambda_min);
						info->lambda_max = info->status.lambda_max;
						SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MIN, "%.1f", info->lambda_min);
						SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MAX, "%.1f", info->lambda_max);
						SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES,  0, 0);
					}

					/* And since checking everything, update whether we have spectra */
					SetDlgItemCheck(hdlg, IDC_SHOW_RAW,   info->cv_raw   != NULL);
					EnableDlgItem  (hdlg, IDC_SHOW_RAW,   info->cv_raw   != NULL);
					SetDlgItemCheck(hdlg, IDC_SHOW_DARK,  info->cv_dark  != NULL);
					EnableDlgItem  (hdlg, IDC_SHOW_DARK,  info->cv_dark  != NULL);
					SetDlgItemCheck(hdlg, IDC_SHOW_REF,   info->cv_ref   != NULL);
					EnableDlgItem  (hdlg, IDC_SHOW_REF,   info->cv_ref   != NULL);
					SetDlgItemCheck(hdlg, IDC_SHOW_LIGHT, info->cv_light != NULL);
					EnableDlgItem  (hdlg, IDC_SHOW_LIGHT, info->cv_light != NULL);
					SetDlgItemCheck(hdlg, IDC_SHOW_FIT,   info->cv_fit   != NULL);
					EnableDlgItem  (hdlg, IDC_SHOW_FIT,   info->cv_fit   != NULL);
				}
			}

			/* Finally ... enable or disable controls depending on the status of the spectrometer */
			for (i=0; spec_hide_list[i] != ID_NULL; i++) ShowDlgItem(hdlg, spec_hide_list[i], info->spec_ok);
			for (i=0; spec_disable_list[i] != ID_NULL; i++) EnableDlgItem(hdlg, spec_disable_list[i], info->spec_ok);
			SetDlgItemText(hdlg, IDB_INITIALIZE_SPEC, info->spec_ok ? "Disconnect" : "Connect");
			rcode = TRUE; break;

		/* Just get a status and update the integration time and number of averages */
		case WMP_UPDATE_SPEC_PARMS:
			if (info->spec_ok) {
				if (Spec_Remote_Get_Spectrometer_Info(&info->status) != 0) {
					MessageBox(hdlg, "ERROR: Unable to get spectrometer information.\nClosing remote connection", "SPEC status failure", MB_ICONERROR | MB_OK);
					Shutdown_Spec_Client(); 
					info->spec_ok = FALSE;
					SendMessage(hdlg, WMP_LOAD_SPEC_PARMS, 0, 0);	/* Disable controls */
				} else {
					SetDlgItemCheck(hdlg, IDC_DARK_PIXELS,   info->status.use_dark_pixel);
					SetDlgItemCheck(hdlg, IDC_NL_POLYNOMIAL, info->status.use_nl_correct);
					SetDlgItemDouble(hdlg, IDT_MS_INTEGRATE, "%.2f", info->status.ms_integrate);
					SetDlgItemInt(hdlg, IDT_AVERAGES, info->status.num_average, FALSE);
				}
			}
			rcode = TRUE; break;

		case WMP_UPDATE_RAW_AXIS_SCALES:
			memset(&scales, 0, sizeof(scales));
			scales.xmin = info->lambda_min;
			scales.xmax = info->lambda_max;
			scales.autoscale_x = FALSE; scales.force_scale_x = FALSE;
			scales.autoscale_y = FALSE; scales.force_scale_y = FALSE;
			scales.ymin = 0; 
			scales.ymax = 0;
			if ( (cv = info->cv_raw) != NULL) {
				for (i=0; i<cv->npt; i++) if (cv->y[i] > scales.ymax) scales.ymax = cv->y[i];
			}
			if ( (cv = info->cv_dark) != NULL) {
				for (i=0; i<cv->npt; i++) if (cv->y[i] > scales.ymax) scales.ymax = cv->y[i];
			}
			if ( (cv = info->cv_ref) != NULL) {
				for (i=0; i<cv->npt; i++) if (cv->y[i] > scales.ymax) scales.ymax = cv->y[i];
			}
			if (scales.ymax == 0) scales.ymax = 45000;
			zforce.x_force = 0.1; zforce.y_force = 0.3;

			SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_SET_ZFORCE,  (WPARAM) &zforce, (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_SET_SCALES,  (WPARAM) &scales, (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_SET_Y_TITLE, (WPARAM) "counts", (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_REDRAW, 0, 0);
			rcode = TRUE; break;

		case WMP_UPDATE_MAIN_AXIS_SCALES:
			memset(&scales, 0, sizeof(scales));
			scales.xmin = info->lambda_min;
			scales.xmax = info->lambda_max;
			scales.autoscale_x = FALSE; scales.force_scale_x = FALSE;
			scales.autoscale_y = FALSE; scales.force_scale_y = FALSE;
			scales.ymin = 0; 
			scales.ymax = 100.0;
			zforce.x_force = 0.1; zforce.y_force = 0.3;

			SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_SET_ZFORCE,  (WPARAM) &zforce, (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_SET_SCALES,  (WPARAM) &scales, (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_SET_X_TITLE, (WPARAM) "wavelength [nm]", (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_SET_Y_TITLE, (WPARAM) "reflectance [%]", (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_REDRAW, 0, 0);
			rcode = TRUE; break;

		case WMP_CLEAR_REFERENCE_STACK:
			info->reference.mirror = FALSE;
			info->reference.substrate = FindMaterialIndex("c-Si", NULL);
			if (info->reference.substrate <= 0) info->reference.substrate = 1;
			for (i=0; i<N_FILM_STACK; i++) {
				info->reference.imat[i] = 0;							/* Mark as empty */
				info->reference.nm[i] = 0;
			}
			SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0, 0);
			rcode = TRUE; break;

		case WMP_SHOW_REFERENCE_STRUCTURE:
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxSetByIntValue(hdlg, IDC_REF_MATERIAL_0+i, info->reference.imat[i]);
				SetDlgItemDouble(hdlg, IDV_REF_NM_0+i, "%.1f", info->reference.nm[i]);
			}
			ComboBoxSetByIntValue(hdlg, IDC_REF_SUBSTRATE, info->reference.substrate);
			SetDlgItemCheck(hdlg, IDC_REF_MIRROR, info->reference.mirror);

			/* Figure out what is enabled */
			for (i=0; i<N_FILM_STACK; i++) {							
				EnableDlgItem(hdlg, IDC_REF_MATERIAL_0+i, ! info->reference.mirror);
				EnableDlgItem(hdlg, IDV_REF_NM_0+i,       ! info->reference.mirror && info->reference.imat[i] != 0);
			}
			EnableDlgItem(hdlg, IDC_REF_SUBSTRATE, ! info->reference.mirror);
			rcode = TRUE; break;

		case WMP_MAKE_REFERENCE_STACK:
			nlayers = 0;															/* Must have substrate though */
			stack = info->reference.stack;									/* Sample stack structure */
			for (i=0; i<N_FILM_STACK; i++) {
				imat = info->reference.imat[i];
				if (imat > 0) {													/* Any layer here? */
					sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "reference layer %d", i+1);
					strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
					stack[nlayers].nm = info->reference.nm[i];
					nlayers++;
				}
			}
			imat = max(1, info->reference.substrate);
			sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "reference substrate");
			strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
			stack[nlayers].nm = 100.0;			/* Doesn't matter */
			nlayers++;

			if (info->reference.tfoc != NULL) free(info->reference.tfoc);
			info->reference.tfoc = MakeSample(nlayers, stack);
			info->reference.layers = nlayers;
			rcode = TRUE; break;

		case WMP_CLEAR_SAMPLE_STACK:
			info->sample.substrate = FindMaterialIndex("c-Si", NULL);
			if (info->sample.substrate <= 0) info->sample.substrate = 1;
			for (i=0; i<N_FILM_STACK; i++) {
				info->sample.imat[i] = 0;
				info->sample.nm[i] = info->sample.tmin[i] = info->sample.tmax[i] = 0.0;
				info->sample.vary[i] = FALSE;
				SetDlgItemText(hdlg, IDT_SIGMA_0+i, "");						/* Clear sigma */
			}
			info->sample.scaling = 1.0;
			SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);
			rcode = TRUE; break;

		case WMP_SHOW_SAMPLE_STRUCTURE:
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxSetByIntValue(hdlg, IDC_FILM_MATERIAL_0+i, info->sample.imat[i]);
				SetDlgItemDouble(hdlg, IDV_FILM_NM_0+i, info->sample.vary[i] ? "%.2f" : "%.1f", info->sample.nm[i]);
				if (info->sample.imat[i] == 0) info->sample.vary[i] = FALSE;
				SetDlgItemCheck(hdlg, IDC_VARY_0+i, info->sample.vary[i]);
				if (info->sample.vary[i]) {									/* Either fill in tmin/tmax or blank */
					SetDlgItemDouble(hdlg, IDV_NM_LOW_0+i, "%.1f", info->sample.tmin[i]);
					SetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i, "%.1f", info->sample.tmax[i]);
				} else {
					SetDlgItemText(hdlg, IDV_NM_LOW_0+i, "");
					SetDlgItemText(hdlg, IDV_NM_HIGH_0+i, "");
				}
				EnableDlgItem(hdlg, IDV_FILM_NM_0+i, info->sample.imat[i] != 0);
				EnableDlgItem(hdlg, IDC_VARY_0+i,    info->sample.imat[i] != 0);
				EnableDlgItem(hdlg, IDV_NM_LOW_0+i,  info->sample.vary[i]);
				EnableDlgItem(hdlg, IDV_NM_HIGH_0+i, info->sample.vary[i]);
			}
			ComboBoxSetByIntValue(hdlg, IDC_FILM_SUBSTRATE, info->sample.substrate);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING, "%.3f", info->sample.scaling);
			rcode = TRUE; break;

		case WMP_MAKE_SAMPLE_STACK:
			nlayers = 0;															/* Must have substrate though */
			stack = info->sample.stack;										/* Sample stack structure */
			for (i=0; i<N_FILM_STACK; i++) {
				imat = info->sample.imat[i];
				if (imat > 0) {													/* Any layer here? */
					sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "layer %d", i+1);
					strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
					stack[nlayers].nm    = info->sample.nm[i];
					stack[nlayers].vary  = info->sample.vary[i];
					stack[nlayers].lower = info->sample.tmin[i];
					stack[nlayers].upper = info->sample.tmax[i];
					stack[nlayers].sigma = 0.0;
					nlayers++;
				}
			}
			imat = max(1, info->sample.substrate);
			sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "substrate");
			strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
			stack[nlayers].nm = 100.0;			/* Doesn't matter */
			nlayers++;

			if (info->sample.tfoc != NULL) free(info->sample.tfoc);
			info->sample.tfoc = MakeSample(nlayers, stack);
			info->sample.layers = nlayers;

			rcode = TRUE; break;

		case WMP_PROCESS_REFERENCE:			
			if (info->cv_ref != NULL) {			/* Generate the reference sample and ask TFOC for values */
				double *ref, *actual, *dark, *light, vmax;

				/* Read the current parameters to create a "reference stack" and generate reflectance curve */
				info->tfoc_reference = realloc(info->tfoc_reference, info->npt * sizeof(*info->tfoc_reference));
				if (! info->reference.mirror) {
					SendMessage(hdlg, WMP_MAKE_REFERENCE_STACK, 0, 0);
					TFOC_GetReflData(info->reference.tfoc, 1.0, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, info->tfoc_reference);
				} else {
					for (i=0; i<info->npt; i++) info->tfoc_reference[i] = 1.0;
				}
				cv     = info->cv_light = ReallocLightCurve(hdlg, info, info->cv_light, info->npt, 3, "light", colors[3]);
				dark   = (info->cv_dark != NULL) ? info->cv_dark->y : NULL ;
				actual = info->tfoc_reference;
				ref    = info->cv_ref->y;
				light  = info->cv_light->y;
				vmax = 0;
				for (i=0; i<cv->npt; i++) {
					if (dark == NULL) {
						light[i] = ref[i] / max(1E-4,actual[i]) ;
					} else {
						light[i] = (ref[i]-dark[i]) / max(1E-4,actual[i]) ;
					}
					vmax = max(vmax, light[i]);
				}
				for (i=0; i<cv->npt; i++) light[i] /= vmax;
				cv->modified = TRUE;
				SetDlgItemCheck(hdlg, IDC_SHOW_LIGHT, TRUE);
				EnableDlgItem  (hdlg, IDC_SHOW_LIGHT, TRUE);
			}
			rcode = TRUE; break;

		case WMP_RECALC_RAW_REFLECTANCE:
			if (info->cv_raw != NULL && info->cv_ref != NULL) {			/* Don't have to have dark */
				double *raw, *dark, *ref;

				cv = info->cv_refl = ReallocReflCurve(hdlg, info, info->cv_refl, info->npt, 0, "reflectance", colors[0]);
				raw  = info->cv_raw->y; 
				ref  = info->cv_ref->y; 
				dark = (info->cv_dark != NULL) ? info->cv_dark->y : NULL ;
				for (i=0; i<cv->npt; i++) {										/* Calculate the normalized reflectance */
					if (dark != NULL) {
						cv->y[i] = (raw[i]-dark[i]) / max(1.0,ref[i]-dark[i]) ;
					} else {
						cv->y[i] = raw[i] / max(1.0,ref[i]) ;
					}
					cv->s[i] = sqrt(1.0/max(1.0,raw[i]) + 1.0/max(1.0,ref[i]));		/* Fractional uncertainty (unbiased by scaling) */
					cv->y[i] = max(-1.0, min(2.0, cv->y[i]));
					if (info->tfoc_reference != NULL) cv->y[i] *= info->tfoc_reference[i];
					cv->s[i] = cv->s[i] * cv->y[i];								/* Been calculating fractional relative error */
				}
				cv->modified = TRUE;
			}
			rcode = TRUE; break;


		case WMP_PROCESS_MEASUREMENT:
			SendMessage(hdlg, WMP_RECALC_RAW_REFLECTANCE, 0, 0);			/* Do all but scaling factor */
			if (info->cv_refl != NULL) {											/* Scale correction - only when displayed, not fit */
				cv = info->cv_refl;
				for (i=0; i<cv->npt; i++) {										/* Calculate the normalized reflectance */
					cv->y[i] *= info->sample.scaling;							/* Scale up/down for small intensity errors */
					cv->s[i] *= info->sample.scaling;
				}
				cv->modified = TRUE;

				/* Read the current parameters to create a "fit stack" and generate reflectance curve */
				SendMessage(hdlg, WMP_MAKE_SAMPLE_STACK, 0, 0);
				info->tfoc_fit = realloc(info->tfoc_fit, info->npt * sizeof(*info->tfoc_fit));
				TFOC_GetReflData(info->sample.tfoc, 1.0, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, info->tfoc_fit);

				/* Create the curve with the fit for display */
				cv = info->cv_fit = ReallocReflCurve(hdlg, info, info->cv_fit, info->npt, 4, "fit", colors[4]);
				for (i=0; i<cv->npt; i++) cv->y[i] = info->tfoc_fit[i];
				cv->modified = TRUE;

				SetDlgItemCheck(hdlg, IDC_SHOW_FIT, TRUE);
				EnableDlgItem  (hdlg, IDC_SHOW_FIT, TRUE);

				/* Create a curve with residuals display */
				cv = info->cv_residual = ReallocResidualCurve(hdlg, info, info->cv_residual, info->npt, 4, "residual", colors[5]);
				for (i=0; i<cv->npt; i++) {
					if (info->lambda[i] < info->fit_parms.lambda_min || info->lambda[i] > info->fit_parms.lambda_max) {
						cv->y[i] = 0.0;
					} else {
						cv->y[i] = info->cv_refl->y[i] - info->tfoc_fit[i];
					}
				}
				cv->modified = TRUE;
				cv->visible  = GetDlgItemCheck(hdlg, IDC_SHOW_RESIDUAL);	/* By default, don't show */
				EnableDlgItem(hdlg, IDC_SHOW_RESIDUAL, TRUE);				/* But enable being able to show */

				CalcChiSqr(info->lambda, info->cv_refl->y, info->cv_refl->s, info->tfoc_fit, info->npt, info->fit_parms.lambda_min, info->fit_parms.lambda_max, &chisqr, &dof);
				SetDlgItemDouble(hdlg, IDT_CHISQR, "%.3f", sqrt(chisqr));
				SetDlgItemInt(hdlg, IDT_DOF, dof, TRUE);
				
				SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);		/* Does a redraw */
			}
			rcode = TRUE; break;

		case WMP_REFINE_FIT:
			SendMessage(hdlg, WMP_MAKE_SAMPLE_STACK, 0, 0);				/* Has all data for moment */
			SendMessage(hdlg, WMP_RECALC_RAW_REFLECTANCE, 0, 0);		/* Raw ignores "scaling" correction (processed differently in fit) */
			do_fit(hdlg, info);													/* go and let it run */

			/* Record values and replot new fit ... then deal with sigma */
			ilayer = 0;																/* Which layer in the final structure */
			nvary = 1;																/* # parameters varied (always scaling) */
			for (i=0; i<N_FILM_STACK; i++) {
				if (info->sample.imat[i] == 0) continue;					/* A nothing layer (doesn't get to stack */
				if (info->sample.vary[i]) info->sample.nm[i] = info->sample.stack[ilayer].nm;
				ilayer++; nvary++;
			}
			CalcChiSqr(info->lambda, info->cv_refl->y, info->cv_refl->s, info->tfoc_fit, info->npt, info->fit_parms.lambda_min, info->fit_parms.lambda_max, &chisqr, &dof);
			chisqr = chisqr*dof/max(1,dof-nvary);							/* Correct for # of free parameters */
			dof -= nvary;
			SetDlgItemDouble(hdlg, IDT_CHISQR, "%.3f", sqrt(chisqr));
			SetDlgItemInt(hdlg, IDT_DOF, dof, TRUE);

			/* Fake the sigma based on chisqr ... too many people would not understand the link with chisqr_\nu */
			ilayer = 0;
			for (i=0; i<N_FILM_STACK; i++) {
				SetDlgItemText(hdlg, IDT_SIGMA_0+i, "");
				if (info->sample.imat[i] == 0) continue;					/* Not a real layer */
				if (info->sample.vary) {
					SetDlgItemDouble(hdlg, IDT_SIGMA_0+i, "%.2f", info->sample.stack[ilayer].sigma*sqrt(chisqr));
				}
				ilayer++;
			}
			SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);		/* Redraw the sample */
			SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0,0);			/* Recalculate and draw (critical to handle scaling) */
			SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);		/* Redraw the results */
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			rcode = FALSE;												/* Assume we don't process */
			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=ID_NULL; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							PostMessage(hdlg, WM_NEXTDLGCTL, 0, 0L);
							break;
						}
					}
					rcode = TRUE; break;

				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				/* ============================================ */
				/*  List for the IP address of the SPEC server
				/* ============================================ */
				case IDL_SPEC_IP:
					if (wNotifyCode == CBN_SELCHANGE) {
						char *aptr;
						if ( (aptr = ComboBoxGetPtrValue(hdlg, wID)) != NULL) {			/* Have a new IP */
							EnableWindow(GetDlgItem(hdlg, IDV_SPEC_IP), FALSE);			/* Disable the manual field */
							strcpy_m(szBuf, sizeof(szBuf), aptr);								/* Copy new IP address into szBuf */
						} else {																			/* Switch to manual */
							EnableWindow(GetDlgItem(hdlg, IDV_SPEC_IP), TRUE);				/* Enable the manual field */
							GetDlgItemText(hdlg, IDV_SPEC_IP, szBuf, sizeof(szBuf));		/* Get what is there currently */
							strip_white_space(szBuf);												/* Validate and use current if anything wrong */
							if (validate_IP_address(szBuf) != 0) strcpy_m(szBuf, sizeof(szBuf), info->spec_IP);
						}
						if (_stricmp(info->spec_IP, szBuf) != 0) {							/* If change, reinitialize */
							strcpy_m(info->spec_IP, sizeof(info->spec_IP), szBuf);
							SetDlgItemText(hdlg, IDV_SPEC_IP, info->spec_IP);
						}
					}
					rcode = TRUE; break;

				case IDV_SPEC_IP:
					if (wNotifyCode == EN_KILLFOCUS) {

						GetDlgItemText(hdlg, wID, szBuf, sizeof(szBuf));	/* Read new IP and validate */
						strip_white_space(szBuf);

						if (*szBuf == '\0') {
							SetDlgItemText(hdlg, wID, info->spec_IP);			/* Reset to old value */
						} else if (strcmp(szBuf, info->spec_IP) == 0) {
								/* No change */
						} else if (validate_IP_address(szBuf) != 0) {
							SetDlgItemText(hdlg, wID, szBuf);
							SetFocus(GetDlgItem(hdlg, wID)); 
							SendMessage(GetDlgItem(hdlg, wID), EM_SETSEL, 0, 100);
						} else {
							strcpy_m(info->spec_IP, sizeof(info->spec_IP), szBuf);
						}
					}
					rcode = TRUE; break;

				case IDB_INITIALIZE_SPEC:
					if (wNotifyCode == BN_CLICKED) {
						if (info->spec_ok) {
							Shutdown_Spec_Client();	info->spec_ok = FALSE;
						} else {
							SendMessage(hdlg, WMP_OPEN_SPEC, 0, 0);
						}
						SendMessage(hdlg, WMP_LOAD_SPEC_PARMS, 0, 0);	/* Enables/disables controls */
					}
					rcode = TRUE; break;

				case IDB_SPEC_PARMS_UPDATE:
					if (wNotifyCode == BN_CLICKED) SendMessage(hdlg, WMP_UPDATE_SPEC_PARMS, 0, 0);
					rcode = TRUE; break;
					
				case IDC_AUTORANGE_WAVELENGTH:
					enable = GetDlgItemCheck(hdlg, wID);
					EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MIN, ! enable);
					EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MAX, ! enable);
					rcode = TRUE; break;

				case IDV_GRAPH_LAMBDA_MIN:
					if (wNotifyCode== EN_KILLFOCUS) {
						info->lambda_min = GetDlgItemDouble(hdlg, wID);
						SetDlgItemDouble(hdlg, wID, "%.1f", info->lambda_min);
						SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);		/* Does a redraw */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES,  0, 0);		/* Does a redraw */
					}
					rcode = TRUE; break;
					
				case IDV_GRAPH_LAMBDA_MAX:
					if (wNotifyCode== EN_KILLFOCUS) {
						info->lambda_max = GetDlgItemDouble(hdlg, wID);
						SetDlgItemDouble(hdlg, wID, "%.1f", info->lambda_max);
						SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);		/* Does a redraw */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES,  0, 0);		/* Does a redraw */
					}
					rcode = TRUE; break;
						
				case IDB_COLLECT_REFERENCE:
				case IDB_TAKE_REFERENCE:
					if (wNotifyCode == BN_CLICKED) {
						if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
						if (wID == IDB_COLLECT_REFERENCE) {
							rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
							npt = info->npt;
						} else {
							rc = Spec_Remote_Grab_Saved(SPEC_SPECTRUM_REFERENCE, &data, &npt);
						}
						if (rc != 0) {
							Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
						} else {
							cv = info->cv_ref = ReallocRawCurve(hdlg, info, info->cv_ref, npt, 2, "reference", colors[2]);
							for (i=0; i<npt; i++) cv->y[i] = data[i];
							cv->modified = TRUE;
							free(data);
							SetDlgItemCheck(hdlg, IDC_SHOW_REF, TRUE);
							EnableDlgItem  (hdlg, IDC_SHOW_REF, TRUE);

							SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0, 0);
							SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
						}
					}
					rcode = TRUE; break;

				case IDB_COLLECT_DARK:
				case IDB_TAKE_DARK:
					if (wNotifyCode == BN_CLICKED) {
						if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
						if (wID == IDB_COLLECT_DARK) {
							rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
							npt = info->npt;
						} else {
							rc = Spec_Remote_Grab_Saved(SPEC_SPECTRUM_DARK, &data, &npt);
						}
						if (rc != 0) {
							Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
						} else {
							cv = info->cv_dark = ReallocRawCurve(hdlg, info, info->cv_dark, npt, 1, "dark", colors[1]);
							for (i=0; i<npt; i++) cv->y[i] = data[i];
							cv->modified = TRUE;
							free(data);
							SetDlgItemCheck(hdlg, IDC_SHOW_DARK, TRUE);
							EnableDlgItem  (hdlg, IDC_SHOW_DARK, TRUE);

							SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
						}
					}
					rcode = TRUE; break;

				case IDB_MEASURE:
				case IDB_MEASURE_RAW:
				case IDB_MEASURE_TEST:
					if (wNotifyCode == BN_CLICKED) {
						if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
						if (wID == IDB_MEASURE) {
							rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
							npt = info->npt;
						} else {
							rc = Spec_Remote_Grab_Saved((wID == IDB_MEASURE_RAW) ? SPEC_SPECTRUM_RAW : SPEC_SPECTRUM_TEST, &data, &npt);
						}
						if (rc != 0) {
							Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
						} else {
							cv = info->cv_raw = ReallocRawCurve(hdlg, info, info->cv_raw, npt, 0, "sample", colors[0]);
							for (i=0; i<npt; i++) cv->y[i] = data[i];
							cv->modified = TRUE;
							free(data);
							SetDlgItemCheck(hdlg, IDC_SHOW_RAW, TRUE);
							EnableDlgItem  (hdlg, IDC_SHOW_RAW, TRUE);

							SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
							SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);
						}

						if (GetDlgItemCheck(hdlg, IDC_AUTOREFINE)) SendMessage(hdlg, WMP_REFINE_FIT, 0, 0);

						if (GetDlgItemCheck(hdlg, IDC_AUTOFILE)) {
							char pathname[PATH_MAX], dir[PATH_MAX], fname[PATH_MAX];

							GetDlgItemText(hdlg, IDV_AUTOFILE_DIR,  dir, sizeof(dir));
							GetDlgItemText(hdlg, IDV_AUTOFILE_FILE, fname, sizeof(fname));
							if (*fname == '\0') { 
								strcpy_s(fname, sizeof(fname), "scan");
								SetDlgItemText(hdlg, IDV_AUTOFILE_FILE, fname);
							}
							i = GetDlgItemIntEx(hdlg, IDV_AUTOFILE_NUM);
							if (*dir == '\0') {
								sprintf_s(pathname, sizeof(pathname), "%s_%4.4d.csv", fname, i);
							} else {
								sprintf_s(pathname, sizeof(pathname), "%s/%s_%4.4d.csv", dir, fname, i);
							}
							
							if (SaveData(info, pathname) != 0) {					/* Disable if problems */
								Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
								SetDlgItemCheck(hdlg, IDC_AUTOFILE, FALSE);
								EnableDlgItem(hdlg, IDB_AUTOFILE_RESET, FALSE);
								EnableDlgItem(hdlg, IDB_AUTOFILE_EDIT, FALSE);
								EnableDlgItem(hdlg, IDV_AUTOFILE_DIR, FALSE);
								EnableDlgItem(hdlg, IDV_AUTOFILE_FILE, FALSE);
								EnableDlgItem(hdlg, IDV_AUTOFILE_NUM, FALSE);
							} else {
								sprintf_s(szBuf, sizeof(szBuf), "%4.4d", i+1);
								SetDlgItemText(hdlg, IDV_AUTOFILE_NUM, szBuf);
							}
						}
					}
					rcode = TRUE; break;

				case IDC_SHOW_RAW:
					if (info->cv_raw != NULL) {
						info->cv_raw->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_raw->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				case IDC_SHOW_DARK:
					if (info->cv_dark != NULL) {
						info->cv_dark->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_dark->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				case IDC_SHOW_REF:
					if (info->cv_ref != NULL) {
						info->cv_ref->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_ref->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				case IDC_SHOW_LIGHT:
					if (info->cv_light != NULL) {
						info->cv_light->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_light->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				case IDC_SHOW_FIT:
					if (info->cv_fit != NULL) {
						info->cv_fit->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_fit->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				case IDC_SHOW_RESIDUAL:
					if (info->cv_residual != NULL) {
						info->cv_residual->visible = GetDlgItemCheck(hdlg, wID);
						info->cv_residual->modified = TRUE;
						SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_REDRAW, 0, 0);		/* Redraw */
					}
					rcode = TRUE; break;

				/* Clear the film stack completely */
				case IDB_FILM_CLEAR:
					if (wNotifyCode == BN_CLICKED) SendMessage(hdlg, WMP_CLEAR_SAMPLE_STACK, 0, 0);
					rcode = TRUE; break;

				/* Substrate ... all valid except none */
				case IDC_FILM_SUBSTRATE:
					if (wNotifyCode == CBN_SELCHANGE) {
						info->sample.substrate = ComboBoxGetIntValue(hdlg, wID);
						if (info->sample.substrate <= 0) {									/* Don't allow it to be "none" */
							Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
							info->sample.substrate = 1;
							ComboBoxSetByIntValue(hdlg, wID, info->sample.substrate);
						}
					}
					rcode = TRUE; break;
					
				case IDC_LOG_REFINES:
					enable = GetDlgItemCheck(hdlg, wID);
					EnableDlgItem(hdlg, IDV_LOGFILE, enable);
					EnableDlgItem(hdlg, IDB_EDIT_LOGFILE, enable);
					if (enable) {
						GetDlgItemText(hdlg, IDV_LOGFILE, szBuf, sizeof(szBuf));
						if (*szBuf == '\0') SetDlgItemText(hdlg, IDV_LOGFILE, "logfile.csv");
					}
					rcode = TRUE; break;
					
				case IDB_EDIT_LOGFILE:
					if (wNotifyCode == BN_CLICKED) QueryLogfile(hdlg, IDV_LOGFILE);
					rcode = TRUE; break;
					
				/* Sample structure */
				case IDC_FILM_MATERIAL_0:
				case IDC_FILM_MATERIAL_1:
				case IDC_FILM_MATERIAL_2:
				case IDC_FILM_MATERIAL_3:
				case IDC_FILM_MATERIAL_4:
					if (wNotifyCode == CBN_SELCHANGE) {
						i = wID - IDC_FILM_MATERIAL_0;
						info->sample.imat[i] = ComboBoxGetIntValue(hdlg, wID);
						if (info->sample.imat[i] < 0) info->sample.imat[i] = 0;
						SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);			/* Inefficient but so what */
					}
					rcode = TRUE; break;
						
				/* Mark that the thickness can be varied */
				case IDC_VARY_0:
				case IDC_VARY_1:
				case IDC_VARY_2:
				case IDC_VARY_3:
				case IDC_VARY_4:
					i = wID - IDC_VARY_0;
					info->sample.vary[i] = GetDlgItemCheck(hdlg, wID);
					if (info->sample.vary[i]) {									/* When enabled, set default search range */
						rval  = info->sample.nm[i];
						lower = info->sample.tmin[i];
						upper = info->sample.tmax[i];
						if (lower > rval) lower = 0.0;
						if (upper < rval || upper > 2.0*rval) upper = max(10.0, 2.0 * rval);
						info->sample.tmin[i] = lower;
						info->sample.tmax[i] = upper;
					}
					SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);			/* Inefficient but so what */
					rcode = TRUE; break;

				/* Film thickness (may also update tmin/tmax if vary enabled */
				case IDV_FILM_NM_0:
				case IDV_FILM_NM_1:
				case IDV_FILM_NM_2:
				case IDV_FILM_NM_3:
				case IDV_FILM_NM_4:
					if (wNotifyCode == EN_KILLFOCUS) {
						i = wID-IDV_FILM_NM_0;
						if ( (rval = GetDlgItemDouble(hdlg, wID)) < 0) rval = 0.0;
						info->sample.nm[i] = rval;
						SetDlgItemText(hdlg, IDT_SIGMA_0+i, "");				/* Clear any uncertainty values */
						if (info->sample.vary[i]) {								/* Possibly update tmin/tmax */
							lower = info->sample.tmin[i];
							upper = info->sample.tmax[i];
							if (lower > rval) lower = 0.0;
							if (upper < rval || upper > 2.0*rval) upper = max(10.0, 2.0 * rval);
							info->sample.tmin[i] = lower;
							info->sample.tmax[i] = upper;
						}
						SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);			/* Inefficient but so what */
					}
					rcode = TRUE; break;

				case IDV_NM_LOW_0:
				case IDV_NM_LOW_1:
				case IDV_NM_LOW_2:
				case IDV_NM_LOW_3:
				case IDV_NM_LOW_4:
					if (wNotifyCode == EN_KILLFOCUS) {
						i = wID-IDV_NM_LOW_0;
						lower = GetDlgItemDouble(hdlg, wID);
						if (lower > info->sample.nm[i]) lower = 0.0;
						info->sample.tmin[i] = lower;
						SetDlgItemDouble(hdlg, wID,  "%.1f", lower);
						SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);			/* Inefficient but so what */
					}
					rcode = TRUE; break;

				case IDV_NM_HIGH_0:
				case IDV_NM_HIGH_1:
				case IDV_NM_HIGH_2:
				case IDV_NM_HIGH_3:
				case IDV_NM_HIGH_4:
					if (wNotifyCode == EN_KILLFOCUS) {
						i = wID-IDV_NM_HIGH_0;
						upper = GetDlgItemDouble(hdlg, wID);
						if (upper < info->sample.nm[i]) upper = 2.0*info->sample.nm[i];
						info->sample.tmax[i] = upper;
						SetDlgItemDouble(hdlg, wID,  "%.1f", upper);
						SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);			/* Inefficient but so what */
					}
					rcode = TRUE; break;

				case IDB_REF_CLEAR:
					if (wNotifyCode == BN_CLICKED) SendMessage(hdlg, WMP_CLEAR_REFERENCE_STACK, 0, 0);
					rcode = TRUE; break;

				case IDC_REF_SUBSTRATE:
					if (wNotifyCode == CBN_SELCHANGE) {
						info->reference.substrate = ComboBoxGetIntValue(hdlg, wID);
						if (info->reference.substrate <= 0) {							/* Don't allow it to be "none" */
							Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
							info->reference.substrate = 1;
							ComboBoxSetByIntValue(hdlg, wID, info->reference.substrate);
						}
					}
					rcode = TRUE; break;

				case IDC_REF_MIRROR:
					info->reference.mirror = GetDlgItemCheck(hdlg, wID);
					SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0, 0);	/* Inefficient but so what */
					SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);				/* Rebuild the corrections */
					SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
					rcode = TRUE; break;

				case IDC_REF_MATERIAL_0:
				case IDC_REF_MATERIAL_1:
				case IDC_REF_MATERIAL_2:
				case IDC_REF_MATERIAL_3:
				case IDC_REF_MATERIAL_4:
					if (wNotifyCode == CBN_SELCHANGE) {
						i = wID - IDC_REF_MATERIAL_0;
						info->reference.imat[i] = ComboBoxGetIntValue(hdlg, wID);
						if (info->reference.imat[i] < 0) info->reference.imat[i] = 0;
						SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0, 0);	/* Inefficient but so what */
						SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);				/* Rebuild the corrections */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
					}
					rcode = TRUE; break;

				/* Reference film thickness */
				case IDV_REF_NM_0:
				case IDV_REF_NM_1:
				case IDV_REF_NM_2:
				case IDV_REF_NM_3:
				case IDV_REF_NM_4:
					if (wNotifyCode == EN_KILLFOCUS) {
						i = wID - IDV_REF_NM_0;
						if ( (rval = GetDlgItemDouble(hdlg, wID)) < 0) rval = 0.0;
						info->reference.nm[i] = rval;
						SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0,0);	/* Inefficient but so what */
						SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);			/* Rebuild the corrections */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0,0);	/* Does a redraw */
					}
					rcode = TRUE; break;

				case IDB_TRY:
					if (wNotifyCode == BN_CLICKED) SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);
					rcode = TRUE; break;

				case IDB_FIT:
					if (wNotifyCode == BN_CLICKED) SendMessage(hdlg, WMP_REFINE_FIT, 0, 0);
					rcode = TRUE; break;
					
				case IDV_FIT_LAMBDA_MIN:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.lambda_min = GetDlgItemDouble(hdlg, wID);
						if (info->spec_ok && (info->fit_parms.lambda_min < info->status.lambda_min) ) info->fit_parms.lambda_min = info->status.lambda_min;
						SetDlgItemDouble(hdlg, wID, "%.1f", info->fit_parms.lambda_min);
					}
					rcode = TRUE; break;
					
				case IDV_FIT_LAMBDA_MAX:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.lambda_max = GetDlgItemDouble(hdlg, wID);
						if (info->spec_ok && (info->fit_parms.lambda_max > info->status.lambda_max) ) info->fit_parms.lambda_max = info->status.lambda_max;
						SetDlgItemDouble(hdlg, wID, "%.1f", info->fit_parms.lambda_max);
					}
					rcode = TRUE; break;

				case IDV_FIT_SCALING:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->sample.scaling = GetDlgItemDouble(hdlg, wID);
						if (info->sample.scaling < 0.5) info->sample.scaling = 0.5;
						if (info->sample.scaling > 2.0) info->sample.scaling = 2.0;
						SetDlgItemDouble(hdlg, wID, "%.3f", info->sample.scaling);
						SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);
					}
					rcode = TRUE; break;

				case IDV_FIT_SCALING_MIN:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.scaling_min = GetDlgItemDouble(hdlg, wID);
						if (info->fit_parms.scaling_min < 0.5) info->fit_parms.scaling_min = 0.5;
						SetDlgItemDouble(hdlg, wID, "%.2f", info->fit_parms.scaling_min);
					}
					rcode = TRUE; break;

				case IDV_FIT_SCALING_MAX:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.scaling_max = GetDlgItemDouble(hdlg, wID);
						if (info->fit_parms.scaling_max > 2.0) info->fit_parms.scaling_max = 2.0;
						SetDlgItemDouble(hdlg, wID, "%.2f", info->fit_parms.scaling_max);
					}
					rcode = TRUE; break;

				case IDB_SAVE_DATA:
					if (wNotifyCode == BN_CLICKED) SaveData(info, NULL);
					rcode = TRUE; break;

				case IDB_LOAD_DATA:
					if (wNotifyCode == BN_CLICKED) LoadData(info, NULL);
					rcode = TRUE; break;

				case IDV_MEASURE_DELAY:
					if (wNotifyCode == EN_KILLFOCUS && GetDlgItemCheck(hdlg, IDC_AUTOMEASURE)) {
						ms = 1000*GetDlgItemIntEx(hdlg, IDV_MEASURE_DELAY);		/* Convert to ms */
						if (ms <= 1000) { ms = 1000; SetDlgItemText(hdlg, IDV_MEASURE_DELAY, "1"); }
						SetTimer(hdlg, TIMER_AUTO_MEASURE, ms, NULL);				/* Update spectrometer parameters seconds (if live) */
					}
					rcode = TRUE; break;

				case IDC_AUTOMEASURE:
					if (GetDlgItemCheck(hdlg, wID)) {									/* Enable ... start timer */
						ms = 1000*GetDlgItemIntEx(hdlg, IDV_MEASURE_DELAY);		/* Convert to ms */
						if (ms <= 1000) { ms = 1000; SetDlgItemText(hdlg, IDV_MEASURE_DELAY, "1"); }
						SetTimer(hdlg, TIMER_AUTO_MEASURE, 1000, NULL);				/* Update spectrometer parameters seconds (if live) */
					} else {
						KillTimer(hdlg, TIMER_AUTO_MEASURE);
					}
					rcode = TRUE; break;

				case IDC_AUTOFILE:
					enable = GetDlgItemCheck(hdlg, wID);
					EnableDlgItem(hdlg, IDB_AUTOFILE_RESET, enable);
					EnableDlgItem(hdlg, IDB_AUTOFILE_EDIT, enable);
					EnableDlgItem(hdlg, IDV_AUTOFILE_DIR, enable);
					EnableDlgItem(hdlg, IDV_AUTOFILE_FILE, enable);
					EnableDlgItem(hdlg, IDV_AUTOFILE_NUM, enable);
					rcode = TRUE; break;

				case IDV_AUTOFILE_NUM:
					if (wNotifyCode == EN_KILLFOCUS) {
						i = GetDlgItemIntEx(hdlg, wID);
						if (i < 0) i = 0;
						sprintf_s(szBuf, sizeof(szBuf), "%4.4d", i);
						SetDlgItemText(hdlg, IDV_AUTOFILE_NUM, szBuf);
					}
					rcode = TRUE; break;

				case IDB_AUTOFILE_EDIT:
					QueryAutofile(hdlg);
					rcode = TRUE; break;

				case IDB_AUTOFILE_RESET:
					if (wNotifyCode == BN_CLICKED) SetDlgItemText(hdlg, IDV_AUTOFILE_NUM, "0000");
					rcode = TRUE; break;

				/* Know to be unused notification codes (handled otherwise) */
				case IDV_AUTOFILE_DIR:
				case IDV_AUTOFILE_FILE:
				case IDV_LOGFILE:
				case IDC_AUTOREFINE:
				case IDT_SIGMA_0:
				case IDT_SIGMA_1:
				case IDT_SIGMA_2:
				case IDT_SIGMA_3:
				case IDT_SIGMA_4:
				case IDT_MODEL:
				case IDT_SERIAL:
				case IDT_LAMBDA_MIN:
				case IDT_LAMBDA_MAX:
				case IDT_SPECTRUM_LENGTH:
				case IDT_MS_INTEGRATE:
				case IDC_DARK_PIXELS:
				case IDC_NL_POLYNOMIAL:
				case IDT_AVERAGES:
				case IDT_CHISQR:
				case IDT_DOF:
					rcode = TRUE; break;

				default:
					fprintf(stderr, "Unused wID in %s: %d\n", rname, wID); fflush(stderr);
					break;
			}

			return rcode;
	}

	return 0;
}


/* =========================================================================== 
-- Strip whitespace from beginning and end of a string
--
-- Usage: void strip_white_space(char *buffer);
--
-- Inputs: buffer - a string to be stripped
--
-- Output: buffer is modified so first and last characters are non-white space
--
-- Return:  none
=========================================================================== */
static void strip_white_space(char *buffer) {

	char *aptr;

	if (*buffer == '\0') return;									/* Nothing to do */

	aptr = buffer + strlen(buffer) - 1;							/* Go to the the end of the string */
	while (isspace(*aptr)) *(aptr--) = '\0';					/* Remove the blanks */

	aptr = buffer;
	while (isspace(*aptr)) aptr++;
	if (aptr == buffer) return;									/* Okay, we are done */

	while (*aptr) *(buffer++) = *(aptr++);
	*buffer = '\0';
	return;
}

/* =========================================================================== 
-- Check on validity of a string that should be an IP address
--
-- Usage: int validate_IP_address(char *ip_address)
--
-- Inputs: ip_address - a string that supposedly is an IP address
--
-- Output: none
--
-- Return:  0 ==> all is okay.  In form nnn.nnn.nnn.nnn with all nnn in range
--         !0 ==> something failed ... values indicates actual problem
=========================================================================== */
static int validate_IP_address(char *ip) {
	int i, ival;

	for (i=0; i<4; i++) {				/* Need to see four numbers (but only three dots */
		if (! isdigit(*ip)) return 1;
		ival = strtol(ip, &ip, 10);
		if (ival < 0 || ival > 255) return 2;
		if (i != 3 && *(ip++) != '.') return 3;
		if (i == 3 && *ip != '\0') return 4;
	}
	return 0;
}

/* ===========================================================================
-- Initially allocate or reallocate a curve for the IDU_RAW_GRAPH screen
=========================================================================== */
static GRAPH_CURVE *ReallocRawCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color) {
	static char *rname = "ReallocRawCurve";
	int i;

	/* May not have anything to do */
	if (cv != NULL && cv->npt == npt) return cv;

	/* Okay ... need initial allocation or size has changed */
	if (cv != NULL) SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_CLEAR_CURVE_BY_POINTER, (WPARAM) cv, 0);
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = ID;
	strcpy_m(cv->legend, sizeof(cv->legend), legend);
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = TRUE;
	cv->draw_x_axis   = FALSE;   cv->draw_y_axis   = FALSE;
	cv->force_scale_x = FALSE;   cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;   cv->autoscale_y   = FALSE;
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) cv->x[i] = info->lambda[i];
	cv->rgb = color;
	SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_ADD_CURVE, (WPARAM) cv, (LPARAM) 0);

	return cv;
}

/* ===========================================================================
-- Initially allocate or reallocate a curve for the illumination curve on IDU_RAW_GRAPH
=========================================================================== */
static GRAPH_CURVE *ReallocLightCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color) {
	static char *rname = "ReallocLightCurve";
	int i;

	/* May not have anything to do */
	if (cv != NULL && cv->npt == npt) return cv;

	/* Okay ... need initial allocation or size has changed */
	if (cv != NULL) SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_CLEAR_CURVE_BY_POINTER, (WPARAM) cv, 0);
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = ID;
	strcpy_m(cv->legend, sizeof(cv->legend), legend);
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = TRUE;
	cv->draw_x_axis   = FALSE;   cv->draw_y_axis   = FALSE;
	cv->force_scale_x = FALSE;   cv->force_scale_y = TRUE;
	cv->autoscale_x   = FALSE;   cv->autoscale_y   = FALSE;
	cv->xmin = info->lambda_min; cv->xmax = info->lambda_max;
	cv->ymin = 0.0;				  cv->ymax = 1.0;
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) cv->x[i] = info->lambda[i];
	cv->rgb = color;
	SendDlgItemMessage(hdlg, IDU_RAW_GRAPH, WMP_ADD_CURVE, (WPARAM) cv, (LPARAM) 0);

	return cv;
}

/* ===========================================================================
-- Initially allocate or reallocate a curve for the main IDU_GRAPH screen
=========================================================================== */
static GRAPH_CURVE *ReallocReflCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color) {
	static char *rname = "ReallocReflCurve";
	int i;

	/* May not have anything to do */
	if (cv != NULL && cv->npt == npt) return cv;

	/* Okay ... need initial allocation or size has changed */
	if (cv != NULL) SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_CLEAR_CURVE_BY_POINTER, (WPARAM) cv, 0);
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = ID;
	strcpy_m(cv->legend, sizeof(cv->legend), legend);
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = TRUE;
	cv->draw_x_axis   = FALSE;   cv->draw_y_axis   = FALSE;
	cv->force_scale_x = FALSE;   cv->force_scale_y = TRUE;
	cv->autoscale_x   = FALSE;   cv->autoscale_y   = FALSE;
	cv->ymin = 0.0;				  cv->ymax = 1.0;
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	cv->s = calloc(sizeof(*cv->s), npt);
	for (i=0; i<npt; i++) cv->x[i] = info->lambda[i];
	cv->rgb = color;
	SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_ADD_CURVE, (WPARAM) cv, (LPARAM) 0);

	return cv;
}

/* ===========================================================================
	-- Initially allocate or reallocate a curve for the main IDU_GRAPH screen
	=========================================================================== */
static GRAPH_CURVE *ReallocResidualCurve(HWND hdlg, FILM_MEASURE_INFO *info, GRAPH_CURVE *cv, int npt, int ID, char *legend, int color) {
	static char *rname = "ReallocResidualCurve";
	int i;

	/* May not have anything to do */
	if (cv != NULL && cv->npt == npt) return cv;

	/* Okay ... need initial allocation or size has changed */
	if (cv != NULL) SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_CLEAR_CURVE_BY_POINTER, (WPARAM) cv, 0);
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = ID;
	strcpy_m(cv->legend, sizeof(cv->legend), legend);
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = TRUE;
	cv->draw_x_axis   = TRUE;    cv->draw_y_axis   = FALSE;
	cv->force_scale_x = FALSE;   cv->force_scale_y = TRUE;
	cv->autoscale_x   = FALSE;   cv->autoscale_y   = FALSE;
	cv->ymin = -0.5;				  cv->ymax = 0.5;
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	cv->s = calloc(sizeof(*cv->s), npt);
	for (i=0; i<npt; i++) cv->x[i] = info->lambda[i];
	cv->rgb = color;
	SendDlgItemMessage(hdlg, IDU_GRAPH, WMP_ADD_CURVE, (WPARAM) cv, (LPARAM) 0);

	return cv;
}

/* ===========================================================================
=========================================================================== */
static int Acquire_Raw_Spectrum(HWND hdlg, FILM_MEASURE_INFO *info, SPEC_SPECTRUM_INFO *spectrum_info, double **data) {
	int rc;
	char szBuf[256];

	if (! info->spec_ok) return 1;				/* Must have a spectrometer */
	if ( (rc = Spec_Remote_Acquire_Spectrum(spectrum_info, data)) != 0) {
		sprintf_s(szBuf, sizeof(szBuf), "Failed to acquire a spectrum from remote source [rc=%d]", rc);
		MessageBox(hdlg, szBuf, "Spectrum acquisition failure", MB_ICONERROR | MB_OK);
		return 2;
	}
	return 0;
}


/* ===========================================================================
-- Get elements of the autosave filename
--
-- Usage: int QueryAutofile(HWND hdlg);
--
-- Inputs: hdlg - current dialog box
--         wID  - id of control that will be filled with the filename
--
-- Output: potentially modifies filename in wID
--
-- Returns: 0 if successful, !0 on any errors or cancel
=========================================================================== */
static int QueryAutofile(HWND hdlg) {
	static char *rname = "QueryAutofile";

	static char local_dir[PATH_MAX]="";						/* Directory -- keep for multiple calls */

	OPENFILENAME ofn;
	char pathname[1024], *aptr, *bptr;						/* Pathname - save for multiple calls */

	/* Do we have a specified filename?  If not, query via dialog box */
	strcpy_m(pathname, sizeof(pathname), "scan.csv");	/* Pathname must be initialized with a value */
	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = hdlg;
	ofn.lpstrTitle        = "Autofile directory and template (ext removed)";
	ofn.lpstrFilter       = "Excel csv file (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter    = 0;
	ofn.nFilterIndex      = 1;
	ofn.lpstrFile         = pathname;				/* Full path */
	ofn.nMaxFile          = sizeof(pathname);
	ofn.lpstrFileTitle    = NULL;						/* Partial path */
	ofn.nMaxFileTitle     = 0;
	ofn.lpstrDefExt       = "csv";
	ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
	ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	/* Query a filename ... if abandoned, just return now with no complaints */
	if (! GetSaveFileName(&ofn)) return 1;

	/* Save the directory for the next time */
	strcpy_m(local_dir, sizeof(local_dir), pathname);
	local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

	SetDlgItemText(hdlg, IDV_AUTOFILE_DIR, local_dir);

	aptr = pathname+ofn.nFileOffset;							/* Get the filename only */
	bptr = aptr+strlen(aptr)-1;
	while (*bptr != '.' && bptr != aptr) bptr--;
	if (bptr != aptr && *bptr == '.') *bptr = '\0';
	SetDlgItemText(hdlg, IDV_AUTOFILE_FILE, aptr);

	SetDlgItemText(hdlg, IDV_AUTOFILE_NUM, "0000");

	return 0;
}


/* ===========================================================================
-- Get a filename for the logfile
--
-- Usage: int QueryLogfile(HWND hdlg, int wID);
--
-- Inputs: hdlg - current dialog box
--         wID  - id of control that will be filled with the filename
--
-- Output: potentially modifies filename in wID
--
-- Returns: 0 if successful, !0 on any errors or cancel
=========================================================================== */
static int QueryLogfile(HWND hdlg, int wID) {
	static char *rname = "QueryLogfile";

	static char local_dir[PATH_MAX]="";						/* Directory -- keep for multiple calls */

	OPENFILENAME ofn;
	char pathname[1024];											/* Pathname - save for multiple calls */

	/* Do we have a specified filename?  If not, query via dialog box */
	strcpy_m(pathname, sizeof(pathname), "logfile.csv");	/* Pathname must be initialized with a value */
	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = hdlg;
	ofn.lpstrTitle        = "Refine results log file";
	ofn.lpstrFilter       = "text data (*.dat)\0*.dat\0Excel csv file (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter    = 0;
	ofn.nFilterIndex      = 2;
	ofn.lpstrFile         = pathname;				/* Full path */
	ofn.nMaxFile          = sizeof(pathname);
	ofn.lpstrFileTitle    = NULL;						/* Partial path */
	ofn.nMaxFileTitle     = 0;
	ofn.lpstrDefExt       = "csv";
	ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
	ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

	/* Query a filename ... if abandoned, just return now with no complaints */
	if (! GetSaveFileName(&ofn)) return 1;

	/* Save the directory for the next time */
	strcpy_m(local_dir, sizeof(local_dir), pathname);
	local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

	/* All okay, so set and return */
	SetDlgItemText(hdlg, wID, pathname);
	return 0;
}


/* ===========================================================================
-- Save all the important data for current measurement
--
-- Usage: int SaveData(HWND hdlg, FILM_MEASURE_INFO *info, char *path);
--
-- Inputs: info - pointer to all the information about measurements
--         path - NULL for dialog box or specific pathname for saving
--
-- Output: generates standard save file (normally .csv)
--
-- Returns: 0 if successful, !0 on any errors or cancel
=========================================================================== */
static int SaveData(FILM_MEASURE_INFO *info, char *path) {
	static char *rname = "SaveData";

	static char local_dir[PATH_MAX]="";						/* Directory -- keep for multiple calls */

	int i, imat;
	FILE *funit;
	OPENFILENAME ofn;
	struct tm timenow;
	time_t tnow;
	char szBuf[256];
	char pathname[1024];											/* Pathname - save for multiple calls */
	HWND hdlg;

	/* This is needed so much, assign now */
	hdlg = info->hdlg;

	/* Do we have a specified filename?  If not, query via dialog box */
	if (path == NULL) {
		strcpy_m(pathname, sizeof(pathname), "film_data");	/* Pathname must be initialized with a value */
		ofn.lStructSize       = sizeof(OPENFILENAME);
		ofn.hwndOwner         = info->hdlg;
		ofn.lpstrTitle        = "Save film measurement";
		ofn.lpstrFilter       = "text data (*.dat)\0*.dat\0Excel csv file (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 2;
		ofn.lpstrFile         = pathname;				/* Full path */
		ofn.nMaxFile          = sizeof(pathname);
		ofn.lpstrFileTitle    = NULL;						/* Partial path */
		ofn.nMaxFileTitle     = 0;
		ofn.lpstrDefExt       = "csv";
		ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
		ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

		/* Query a filename ... if abandoned, just return now with no complaints */
		if (! GetSaveFileName(&ofn)) return 1;

		/* Save the directory for the next time */
		strcpy_m(local_dir, sizeof(local_dir), pathname);
		local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

		/* Otherwise, given a directory (maybe) and filename, generate the path and go */
	} else {
		strcpy_s(pathname, sizeof(pathname), path);
	}

	if (fopen_s(&funit, pathname, "w") != 0) {
		MessageBox(HWND_DESKTOP, "File failed to open for write", "File write failure", MB_ICONWARNING | MB_OK);
		return 2;
	} else {
		fprintf(funit, "# FilmMeasure spectrum v1.0\n");
		time(&tnow); localtime_s(&timenow, &tnow); asctime_s(szBuf, sizeof(szBuf), &timenow);
		fprintf(funit, "# Timestamp: %d %s", (int) tnow, szBuf);
		fprintf(funit, "# NPT: %d\n", info->npt);

		fprintf(funit, "# REFERENCE STACK\n");
		for (i=0; i<N_FILM_STACK; i++) {
			if ( (imat = info->reference.imat[i]) <= 0) continue;				/* Nothing there */
			fprintf(funit, "#    %d \"%s\" %f\n", i, materials[imat].id, info->reference.nm[i]);
		}
		imat = info->reference.substrate;
		fprintf(funit, "#   substrate \"%s\"\n", materials[imat].id);
		fprintf(funit, "# END\n");

		fprintf(funit, "# SAMPLE STACK\n");
		for (i=0; i<N_FILM_STACK; i++) {
			if ( (imat = info->sample.imat[i]) <= 0) continue;				/* Nothing there */
			fprintf(funit, "#   %d \"%s\" %f %f %f %d\n", i, materials[imat].id, 
					  info->sample.nm[i], info->sample.tmin[i], info->sample.tmax[i], info->sample.vary[i]);
		}
		imat = info->sample.substrate;
		fprintf(funit, "#   substrate \"%s\"\n", materials[imat].id);
		fprintf(funit, "# END\n");

		fprintf(funit, "# lambda,reflectance,raw,dark,reference,fit\n");
		for (i=0; i<info->npt; i++) {
			fprintf(funit, "%f,%f,%f,%f,%f,%f\n", 
					  info->lambda[i], 
					  (info->cv_refl != NULL) ? info->cv_refl->y[i] : 0.0 ,
					  (info->cv_raw  != NULL) ? info->cv_raw->y[i]  : 0.0 , 
					  (info->cv_dark != NULL) ? info->cv_dark->y[i] : 0.0 ,
					  (info->cv_ref  != NULL) ? info->cv_ref->y[i]  : 0.0 ,
					  (info->cv_fit  != NULL) ? info->cv_fit->y[i]  : 0.0 
					 );					
		}
		fclose(funit);
	}
	return 0;
}

/* ===========================================================================
-- Load data from a saved structure
--
-- Usage: int LoadData(FILM_MEASURE_INFO *info, char *path);
--
-- Inputs: info - pointer to all the information about measurements
--         path - NULL for dialog box or specific pathname for reading
--
-- Output: overwrites lambda, raw, dark, ref, and refl with data from file
--         marks curves as existing unless all the data is zero (not saved)
--
-- Returns: 0 if successful, !0 on any errors or cancel
--
-- Notes: If no spectrometer is initialized, any NPT is valid.  If there is
--        a spectrometer active, spectra must be the same size as currently 
--        used.  lambda curve will be immediately valid, but any subsequent
--        call to collect or take a spectrum from the spectrometer will load
--        the wavelength data corresponding to the spectrometer and overwrite
--        the values loaded.
=========================================================================== */
static int LoadData(FILM_MEASURE_INFO *info, char *path) {
	static char *rname = "FileSaveData";

	static char local_dir[PATH_MAX]="";					/* Directory -- keep for multiple calls */

	int i, ipt, imat, index, npt;
	FILE *funit;
	OPENFILENAME ofn;
	char *aptr, szBuf[256];
	char pathname[1024];
	double xmin, xmax;
	BOOL valid;
	HWND hdlg;
	enum {NORMAL, REFERENCE, SAMPLE} mode;

	double *lambda, *raw, *dark, *ref, *refl;			/* Temporary buffers for read data */

	/* This is needed so much, assign now */
	hdlg = info->hdlg;
	
	/* Do we have a specified filename?  If not, query via dialog box */
	if (path == NULL) {
		*pathname = '\0';										/* No initialization */
		ofn.lStructSize       = sizeof(OPENFILENAME);
		ofn.hwndOwner         = info->hdlg;
		ofn.lpstrTitle        = "Load film measurement";
		ofn.lpstrFilter       = "text data (*.dat)\0*.dat\0Excel csv file (*.csv)\0*.csv\0All files (*.*)\0*.*\0\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 2;
		ofn.lpstrFile         = pathname;				/* Full path */
		ofn.nMaxFile          = sizeof(pathname);
		ofn.lpstrFileTitle    = NULL;						/* Partial path */
		ofn.nMaxFileTitle     = 0;
		ofn.lpstrDefExt       = "csv";
		ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

		/* Query a filename ... if abandoned, just return now with no complaints */
		if (! GetOpenFileName(&ofn)) return 1;

		/* Save the directory for the next time */
		strcpy_m(local_dir, sizeof(local_dir), pathname);
		local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

		/* Otherwise, given a directory (maybe) and filename, generate the path and go */
	} else {
		strcpy_s(pathname, sizeof(pathname), path);
	}

	if (fopen_s(&funit, pathname, "r") != 0) {
		MessageBox(HWND_DESKTOP, "File failed to open for read", "File read failure", MB_ICONERROR | MB_OK);
		return 2;
	} else {

		lambda = NULL;														/* Set so know if we need to free */
		ipt = npt = 0;
		valid = FALSE;
		mode = NORMAL;
		while (fgets(szBuf, sizeof(szBuf), funit) != NULL) {
			if ( (aptr = strchr(szBuf, '\n')) != NULL) *aptr = '\0';

			if (_strnicmp(szBuf, "# FilmMeasure spectrum v1.0", 27) == 0) {	/* Version identifier */
				valid = TRUE;

			} else if (_strnicmp(szBuf, "# NPT: ", 7) == 0) {						/* Spectrum size */
				npt = atol(szBuf+7);
				if (info->spec_ok && info->npt != 0 && info->npt != npt) {
					MessageBox(HWND_DESKTOP, "Number of data points in the file is incompatible with initialized spectrometer", "File read failure", MB_ICONERROR | MB_OK);
					return 3;
				} else if (! valid) {
					MessageBox(HWND_DESKTOP, "Saw NPT: header line before the version identifier.  Cannot ensure that this is a compatible file format", "File read failure", MB_ICONERROR | MB_OK);
					return 4;
				} else if (npt <= 0 || npt > 65536) {
					MessageBox(HWND_DESKTOP, "NPT: header line specifies NPT outside range 1 < NPT < 65536.  Don't believe this is a valid file", "File read failure", MB_ICONERROR | MB_OK);
					return 5;
				} else {
					lambda = calloc(npt, sizeof(double));			/* Temporary space for reading data */
					raw    = calloc(npt, sizeof(double));
					dark   = calloc(npt, sizeof(double));
					ref    = calloc(npt, sizeof(double));
					refl   = calloc(npt, sizeof(double));
				}

			} else if (_strnicmp(szBuf, "# REFERENCE STACK", 17) == 0) {			/* Deal with reference stack */
				mode = REFERENCE;
				SendMessage(hdlg, WMP_CLEAR_REFERENCE_STACK, 0, 0);

			} else if (_strnicmp(szBuf, "# SAMPLE STACK", 14) == 0) {
				mode = SAMPLE;
				SendMessage(hdlg, WMP_CLEAR_SAMPLE_STACK, 0, 0);

			} else if (_strnicmp(szBuf, "# END", 5) == 0) {
				mode = NORMAL;

			} else if (mode == REFERENCE) {
				aptr = szBuf+1;
				while (isspace(*aptr)) aptr++;
				if (isdigit(*aptr)) {									/* Okay, is a layer definition */
					index = *(aptr++)-'0';
					if (index >= 0 && index < N_FILM_STACK) {
						if ( (imat = FindMaterialIndex(aptr, &aptr)) < 0) {
							fprintf(stderr, "Material %s was not found in the list as reference layer %d\n", szBuf, index); fflush(stderr);
							imat = 1;					/* Take first one */
						}
						info->reference.imat[index] = imat;
						info->reference.nm[index] = strtod(aptr, &aptr);
					}
				} else if (_strnicmp(aptr, "substrate ", 10) == 0) {
					if ( (imat = FindMaterialIndex(aptr+10, NULL)) < 0) {
						fprintf(stderr, "Material %s was not found in the list for reference substrate\n", aptr+10); fflush(stderr);
						imat = 1;					/* Take first one */
					}
					info->reference.substrate = imat;
				}

			} else if (mode == SAMPLE) {
				aptr = szBuf+1;
				while (isspace(*aptr)) aptr++;
				if (isdigit(*aptr)) {									/* Okay, is a layer definition */
					index = *(aptr++)-'0';
					if (index >= 0 && index < N_FILM_STACK) {
						if ( (imat = FindMaterialIndex(aptr, &aptr)) < 0) {
							fprintf(stderr, "Material %s was not found in the list as sample layer %d\n", szBuf, index); fflush(stderr);
							imat = 1;					/* Take first one */
						}
						info->sample.imat[index] = imat;
						info->sample.nm[index]   = strtod(aptr, &aptr);
						info->sample.tmin[index] = strtod(aptr, &aptr);
						info->sample.tmax[index] = strtod(aptr, &aptr);
						while (isspace(*aptr)) aptr++;
						info->sample.vary[index] = strchr("yY1", *aptr) != NULL;
					}
				} else if (_strnicmp(aptr, "substrate ", 10) == 0) {
					if ( (imat = FindMaterialIndex(aptr+10, NULL)) < 0) {
						fprintf(stderr, "Material %s was not found in the list for film substrate\n", aptr+10); fflush(stderr);
						imat = 1;					/* Take first one */
					}
					info->sample.substrate = imat;
				}

			} else if (*szBuf == '#') {								/* Other comment line */
				continue;

			} else if (npt <= 0 || ! valid) {						/* Verify before saving numbers */
				MessageBox(HWND_DESKTOP, "File is missing either the version identifier line or the NPT line.  Cannot ensure that this is a compatible file format", "File read failure", MB_ICONERROR | MB_OK);
				if (lambda != NULL) { free(lambda); free(raw); free(dark); free(ref); free(refl); }
				return 5;

			} else {
				aptr = szBuf;
				if (ipt < npt) {											/* Don't overwrite but keep track of totals */
					lambda[ipt] = strtod(aptr, &aptr); while (isspace(*aptr) || *aptr == ',') aptr++;
					refl[ipt]   = strtod(aptr, &aptr); while (isspace(*aptr) || *aptr == ',') aptr++;
					raw[ipt]    = strtod(aptr, &aptr); while (isspace(*aptr) || *aptr == ',') aptr++;
					dark[ipt]   = strtod(aptr, &aptr); while (isspace(*aptr) || *aptr == ',') aptr++;
					ref[ipt]    = strtod(aptr, &aptr); 
				}
				ipt++;
			}
		}
		fclose(funit);

		/* Verify we got all the implied data */
		if (ipt != npt) {
			MessageBox(HWND_DESKTOP, "NPT and actual number of data points in the file do not match.  Don't believe this is a valid file", "File read failure", MB_ICONERROR | MB_OK);
			if (lambda != NULL) { free(lambda); free(raw); free(dark); free(ref); free(refl); }
			return 6;
		}

		/* First step is to transfer the wavelength data so gets */
		info->npt = npt;
		if (info->lambda != NULL) free(info->lambda);
		info->lambda = lambda;
		info->lambda_transferred = FALSE;

		/* Make sure we have curves and transfer the data */
		info->cv_raw  = ReallocRawCurve (hdlg, info, info->cv_raw,  npt, 0, "sample",      colors[0]);
		info->cv_dark = ReallocRawCurve (hdlg, info, info->cv_dark, npt, 1, "dark",        colors[1]);
		info->cv_ref  = ReallocRawCurve (hdlg, info, info->cv_ref,  npt, 2, "reference",   colors[2]);
		info->cv_refl = ReallocReflCurve(hdlg, info, info->cv_refl, npt, 0, "reflectance", colors[0]);
		memcpy(info->cv_raw->x,  lambda, npt*sizeof(double));	memcpy(info->cv_raw->y,  raw,  npt*sizeof(double));
		memcpy(info->cv_dark->x, lambda, npt*sizeof(double));	memcpy(info->cv_dark->y, dark, npt*sizeof(double));
		memcpy(info->cv_ref->x,  lambda, npt*sizeof(double));	memcpy(info->cv_ref->y,  ref,  npt*sizeof(double));
		memcpy(info->cv_refl->x, lambda, npt*sizeof(double));	memcpy(info->cv_refl->y, refl, npt*sizeof(double));

		/* Now update the screen everywhere */
		/* We have loaded xmin/xmax ... need to reset these values in the
		 * structure and mark that the lambda array is "invalid" for next
		 * time that we take or collect a spectra from the spectrometer.
		 */
		xmin = xmax = info->lambda[0];
		for (i=0; i<info->npt; i++) {
			if (info->lambda[i] < xmin) xmin = info->lambda[i];
			if (info->lambda[i] > xmax) xmax = info->lambda[i];
		}
		if (info->lambda_min < xmin) {
			info->lambda_min = xmin;
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MIN, "%.1f", info->lambda_min);
		}
		if (info->lambda_max > xmax) {
			info->lambda_max = xmax;
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MAX, "%.1f", info->lambda_max);
		}

		/* Determine if any or all of the arrays are okay.  If data not saved, all entries will be zero */
		for (i=0; i<npt; i++) { if (raw[i] != 0.0) break; }
		SetDlgItemCheck(hdlg, IDC_SHOW_RAW, i != npt);
		EnableDlgItem  (hdlg, IDC_SHOW_RAW, i != npt);
		info->cv_raw->visible = (i != npt);
		info->cv_raw->modified  = TRUE;
		free(raw);

		for (i=0; i<npt; i++) { if (dark[i] != 0.0) break; }
		SetDlgItemCheck(hdlg, IDC_SHOW_DARK, i != npt);
		EnableDlgItem  (hdlg, IDC_SHOW_DARK, i != npt);
		info->cv_dark->visible = (i != npt);
		info->cv_dark->modified = TRUE;
		free(dark);

		for (i=0; i<npt; i++) { if (ref[i] != 0.0) break; }
		SetDlgItemCheck(hdlg, IDC_SHOW_REF, i != npt);
		EnableDlgItem  (hdlg, IDC_SHOW_REF, i != npt);
		info->cv_ref->visible = (i != npt);
		info->cv_ref->modified  = TRUE;
		free(ref);

		for (i=0; i<npt; i++) { if (refl[i] != 0.0) break; }
		info->cv_refl->visible = (i != npt);
		info->cv_refl->modified = TRUE;
		free(refl);

		/* All of these curves are now modified in any case */
		SendMessage(hdlg, WMP_SHOW_SAMPLE_STRUCTURE, 0, 0);
		SendMessage(hdlg, WMP_SHOW_REFERENCE_STRUCTURE, 0, 0);

		SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);			/* Rebuild the corrections */
		SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);	/* Does a redraw */

		SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);		/* And manage the sample */
		SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);	/* Does a redraw */
	}

	return 0;
}

/* ===========================================================================
-- Routine to write the parameters on screen to an inifile so reload similar
--
-- Usage: int WriteProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info);
--
-- Inputs: hdlg - pointer to current dialog box with values in controls
--         info - pointer to information structure
--
-- Output: Writes the file "FilmMeasure.ini" with information
--
-- Return: 0 always (no error checking).  Nothing guarentees success.
--
-- Notes: Uses WritePrivateProfileStr instead of WritePrivateProfileString
--        from win32ex.c so could implement readily if the 16-bit support
--        for simple ini files every disappears
=========================================================================== */
static int WriteProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info) {
	static char *rname = "WriteProfileInfo";

	char szBuf[256], layer[20];
	int i, imat;

	/* Graph X-axis scaling */
	sprintf_s(szBuf, sizeof(szBuf), "%g %g", info->lambda_min, info->lambda_max);
	WritePrivateProfileString("Graph", "Lambda_Range", szBuf, IniFile);
	WritePrivateProfileInt   ("Graph", "Lambda_Autoscale", info->lambda_autoscale, IniFile);

	/* Fitting parameters */
	sprintf_s(szBuf, sizeof(szBuf), "%g %g", info->fit_parms.lambda_min, info->fit_parms.lambda_max);
	WritePrivateProfileStr("Fit", "Lambda_Range", szBuf, IniFile);
	sprintf_s(szBuf, sizeof(szBuf), "%g %g", info->fit_parms.scaling_min, info->fit_parms.scaling_max);
	WritePrivateProfileStr("Fit", "Scaling_Range", szBuf, IniFile);

	/* Save the current reference film stack */
	for (i=0; i<N_FILM_STACK; i++) {
		sprintf_s(layer, sizeof(layer), "Layer_%d_Material", i);
		imat = info->reference.imat[i];
		WritePrivateProfileStr("Reference", layer, materials[imat].id, IniFile);
		sprintf_s(layer, sizeof(layer), "Layer_%d_Thickness", i);
		WritePrivateProfileDouble("Reference", layer, info->reference.nm[i], IniFile);
	}
	imat = info->reference.substrate;
	WritePrivateProfileStr("Reference", "Substrate", materials[imat].id, IniFile);
	WritePrivateProfileInt("Reference", "Mirror", info->reference.mirror, IniFile);

	/* Save the current sample film stack */
	for (i=0; i<N_FILM_STACK; i++) {
		sprintf_s(layer, sizeof(layer), "Layer_%d_Material", i);
		imat = info->sample.imat[i];
		WritePrivateProfileStr("Film", layer, materials[imat].id, IniFile);
		sprintf_s(layer, sizeof(layer), "Layer_%d_Thickness", i);
		WritePrivateProfileDouble("Film", layer, info->sample.nm[i], IniFile);
		sprintf_s(layer, sizeof(layer), "Layer_%d_Vary", i);
		WritePrivateProfileInt("Film", layer, info->sample.vary[i], IniFile);
		sprintf_s(layer, sizeof(layer), "Layer_%d_Limits", i);
		sprintf_s(szBuf, sizeof(szBuf), "%g %g", info->sample.tmin[i], info->sample.tmax[i]);
		WritePrivateProfileStr("Film", layer, szBuf, IniFile);
	}
	imat = info->sample.substrate;
	WritePrivateProfileStr("Film", "Substrate", materials[imat].id, IniFile);

	return 0;
}

/* ===========================================================================
-- Routine to read initialization parameters from a saved file
--
-- Usage: int ReadProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info);
--
-- Inputs: hdlg - pointer to current dialog box with values in controls
--         info - pointer to information structure
--
-- Output: Fills in info where appropriate and fills in the substrate
--            and film structures on the dialog box
--
-- Return: 0 always (minimal error checking).
--
-- Notes: Using GetPrivateProfileString instead of ReadPrivateProfileStr as
--        as it permits defining a default string return
=========================================================================== */
static int ReadProfileInfo(HWND hdlg, FILM_MEASURE_INFO *info) {
	static char *rname = "ReadProfileInfo";

	char szBuf[256], layer[20], *aptr;
	int i, imat;
	double xmin, xmax;

	/* Graph X-axis scaling */
	GetPrivateProfileString("Graph", "Lambda_Range", NULL, szBuf, sizeof(szBuf), IniFile);
	if (*szBuf != '\0') {
		info->lambda_min = strtod(szBuf, &aptr);
		info->lambda_max = strtod(aptr, NULL);
	}
	GetPrivateProfileString("Graph", "Lambda_Autoscale", NULL, szBuf, sizeof(szBuf), IniFile);
	if (*szBuf != '\0') info->lambda_autoscale = strtol(szBuf, NULL, 10) != 0;

	/* Fitting parameters */
	GetPrivateProfileString("Fit", "Lambda_Range", NULL, szBuf, sizeof(szBuf), IniFile);
	if (*szBuf != '\0') {
		info->fit_parms.lambda_min = strtod(szBuf, &aptr);
		info->fit_parms.lambda_max = strtod(aptr, NULL);
	}
	GetPrivateProfileString("Fit", "Scaling_Range", NULL, szBuf, sizeof(szBuf), IniFile);
	if (*szBuf != '\0') {
		info->fit_parms.scaling_min = strtod(szBuf, &aptr);
		info->fit_parms.scaling_max = strtod(aptr, NULL);
	}

	/* Load the reference film stack */
	for (i=0; i<N_FILM_STACK; i++) {
		sprintf_s(layer, sizeof(layer), "Layer_%d_Material", i);
		GetPrivateProfileString("Reference", layer, "none", szBuf, sizeof(szBuf), IniFile);
		if ( (imat = FindMaterialIndex(szBuf, NULL)) < 0) {
			fprintf(stderr, "Material %s was not found in the list as reference layer %d\n", szBuf, i); fflush(stderr);
			imat = 0;					/* Mark as none */
		}
		info->reference.imat[i] = imat;
		sprintf_s(layer, sizeof(layer), "Layer_%d_Thickness", i);
		GetPrivateProfileString("Reference", layer, "0", szBuf, sizeof(szBuf), IniFile);
		info->reference.nm[i] = strtod(szBuf, NULL);
	}
	GetPrivateProfileString("Reference", "Substrate", "Si", szBuf, sizeof(szBuf), IniFile);
	if ( (imat = FindMaterialIndex(szBuf, NULL)) <= 0) {
		fprintf(stderr, "Material %s was not found in the list as a valid reference substrate\n", szBuf); fflush(stderr);
		imat = 1;					/* Mark as none */
	}
	info->reference.substrate = imat;

	/* Load the sample film stack */
	for (i=0; i<N_FILM_STACK; i++) {
		sprintf_s(layer, sizeof(layer), "Layer_%d_Material", i);
		GetPrivateProfileString("Film", layer, "none", szBuf, sizeof(szBuf), IniFile);
		if ( (imat = FindMaterialIndex(szBuf, NULL)) < 0) {
			fprintf(stderr, "Material %s was not found in the list as sample layer %d\n", szBuf, i); fflush(stderr);
			imat = 0;					/* Mark as none */
		}
		info->sample.imat[i] = imat;
		sprintf_s(layer, sizeof(layer), "Layer_%d_Thickness", i);
		GetPrivateProfileString("Film", layer, "0", szBuf, sizeof(szBuf), IniFile);
		info->sample.nm[i] = strtod(szBuf, NULL);
		sprintf_s(layer, sizeof(layer), "Layer_%d_Vary", i);
		GetPrivateProfileString("Film", layer, "0", szBuf, sizeof(szBuf), IniFile);
		info->sample.vary[i] = strtol(szBuf, NULL, 10) != 0;
		sprintf_s(layer, sizeof(layer), "Layer_%d_Limits", i);
		GetPrivateProfileString("Film", layer, "0", szBuf, sizeof(szBuf), IniFile);
		xmin = fabs(strtod(szBuf, &aptr));
		xmax = fabs(strtod(aptr, NULL));
		if (xmax < xmin) xmax = (xmin == 0) ? 100 : 2*xmin;
		info->sample.tmin[i] = xmin;
		info->sample.tmax[i] = xmax;
	}
	GetPrivateProfileString("Film", "Substrate", "Si", szBuf, sizeof(szBuf), IniFile);
	if ( (imat = FindMaterialIndex(szBuf, NULL)) <= 0) {
		fprintf(stderr, "Material %s was not found in the list as a valid sample substrate\n", szBuf); fflush(stderr);
		imat = 1;					/* Mark as none */
	}
	info->sample.substrate = imat;

	return 0;
}

/* ===========================================================================
-- Routine to scan the database directory and create list of known materials
--
-- Usage: int = InitMateiralsList();
--
-- Inputs: none
--
-- Output: Creates the static global materials[] structure 
--
-- Return: Number of entries, or <0 on error
=========================================================================== */
static int InitMaterialsList(void) {
	char *database;
	char pattern[PATH_MAX+1];
	char path[PATH_MAX+1];
	intptr_t hdir;								/* Directory handle */
	struct _finddata_t findbuf;			/* Information from FindFirst		*/

	database = Find_TFOC_Database(NULL, 0, NULL);
	fprintf(stderr, "Using database: \"%s\"\n", database); fflush(stderr);

	if (_fullpath(path, database, sizeof(path)) == NULL) {
		fprintf(stderr, "Directory does not exist\n"); fflush(stderr);
		return -1;
	}

	_makepath_s(pattern, sizeof(pattern), NULL, path, "*", "*");		/* Match all entries in the directory */

	hdir = _findfirst(pattern, &findbuf);
	if (hdir < 0) {
		fprintf(stderr, "hdir returns %d  (pattern=%s)\n", hdir, pattern); fflush(stderr);
		return -1;							/* Either an error (including no entries) */
	}

	materials_cnt = 0;												/* Nothing yet */
	if (materials_cnt >= materials_dim) {
		materials_dim += 100;
		materials = realloc(materials, materials_dim * sizeof(*materials));
	}
	materials[materials_cnt].id = "none"; 	materials[materials_cnt++].value = 0;		/* Always the first one */

	/* List all names */
	do {
		if (findbuf.attrib & _A_SUBDIR) continue;					/* Ignore directories */
		if (strchr(findbuf.name, '.') != NULL) continue;		/* Ignore anythning with an extension */
		if (materials_cnt >= materials_dim) {
			materials_dim += 100;
			materials = realloc(materials, materials_dim * sizeof(*materials));
		}
		materials[materials_cnt].id = _strdup(findbuf.name); materials[materials_cnt].value = materials_cnt;	/* Always the first one */
		materials_cnt++;
	} while (_findnext(hdir, &findbuf) == 0);
	_findclose(hdir);

	return materials_cnt;
}

/* ===========================================================================
-- Routine to look up a material by name and return the index in the
-- CB_INT_LIST so can be loaded properly
--
-- Usage: int FindMaterialIndex(char *text, char **endptr);
--
-- Inputs: text   - pointer to a string containing a material name, 
--                  potentially in quotes
--         endptr - optional pointer to receive next unused character in string
--
-- Output: *endptr - if not NULL, set to next non-whitespace character
--                   in text string
--
-- Return: Index into the database for the string.  -1 if string not found
=========================================================================== */
static int FindMaterialIndex(char *text, char **endptr) {
	int i, cnt;
	char *bptr, id[200];				/* For the name */

	/* Copy over the string, expecting that it is "enclosed" in quotes */
	while (isspace(*text)) text++;
	bptr = id; cnt = sizeof(id);
	if (*text == '"') {
		text++;
		while (*text && *text != '"') {
			if (--cnt > 0) *(bptr++) = *text;
			text++;
		}
		if (*text == '"') text++;
	} else {
		while (*text && ! isspace(*text)) {
			if (--cnt > 0) *(bptr++) = *text;
			text++;
		}
	}
	*bptr = '\0';

	while (isspace(*text)) text++;
	if (endptr != NULL) *endptr = text;

	/* Scan through the list of all materials I've loaded */
	for (i=0; i<materials_cnt; i++) {
		if (_stricmp(materials[i].id, id) == 0) return i;
	}

	return -1;							/* Default failure -- you figure out what to do */
}

/*
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * =========================================================================== */


/* ===========================================================================
	-- Routine to find the directory containing the TFOC database of material
	--
	-- Usage: char *Find_TFOC_Database(char *database, size_t len, int *ierr);
	--
	-- Inputs: database - if !NULL, pointer to location to be filled with directory
	--         len      - size of database (if !NULL)
	--         ierr     - if !NULL, receives error code
	--
	-- Output: *database - if !NULL, contains filled in database directory
	--         *ierr     - if !NULL, filled with error code (see below)
	--
	-- Return: NULL on error, otherwise either database (if !NULL) or pointer
	--         to an allocated memory with name (caller responsible to free)
	=========================================================================== */
static char *Find_TFOC_Database(char *database, size_t len, int *ierr) {

	char *path;
	size_t cnt;
	char env_name[PATH_MAX];			/* May be returned as pointer */
	struct _stat info;

	/* Set ierr and database to default values (assuming success) */
	if (ierr != NULL) *ierr = 0;
	if (database != NULL) *database = '\0';

	/* Figure out which database to use ... but only first time */
	if (getenv_s(&cnt, env_name, sizeof(env_name), "tfocDatabase") == 0 && cnt > 0) {	/* Ignore if required size would be more than PATH_MAX */
		path = env_name;
	} else if ( _stat("./tfocDatabase", &info) == 0 && info.st_mode & S_IFDIR ) {
		path = "./tfocDatabase/";
	} else if ( _stat("./database.nk", &info) == 0 && info.st_mode & S_IFDIR ) {
		path = "./database.nk/";
	} else if ( _stat("c:/tfocDatabase", &info) == 0 && info.st_mode & S_IFDIR ) {
		path = "c:/tfocDatabase/";
	} else if ( _stat("c:/database.nk", &info) == 0 && info.st_mode & S_IFDIR ) {		/* Compatibility with earlier versions */
		path = "c:/database.nk/";
	} else {																									/* Better hope materials are in the same directory */
		if (ierr != NULL) *ierr = 1;
		return NULL;
	}

	/* Either allocate space or just fill in and return the given buffer */
	if (database == NULL) {
		database = _strdup(path);
	} else {
		strcpy_s(database, len, path);
	}

	return database;
}

/* ===========================================================================
-- Routine to determine the reflectance of the sample structure under
-- specified conditions
--
-- Inputs: SampleFile  - name of datafile containing optical description
--         lambda      - wavelenght of light (nm)
--         theta       - angle of incidence
--         mode        - TE, TM, or UNPOLARIZED mode
--         temperature - temperature of the sample
--
-- Output: none
--
-- Return: reflectivity of the sample (absolute)
=========================================================================== */
#define	MAX_LAYERS	20							/* For FilmMeasure, limit to 20 layers */

int AddSimpleLayer(TFOC_SAMPLE **psample, char *material, double nm) {

	TFOC_SAMPLE *sample;
	int i,j,rc;
	static char *database=NULL;

	/* Make sure we have a database */
	if (database == NULL) database = Find_TFOC_Database(NULL, 0, NULL);

	/* Work with the actual sample, not pointer */
	sample = *psample;

	/* When created, the sample structure will have all zeros ... implicit below */
	if (sample == NULL) {						/* First call, air / substrate only */
		sample = calloc(MAX_LAYERS, sizeof(*sample));
		for (i=0; i<MAX_LAYERS; i++) {					/* Preset these so can ignore */
			sample[i].doping_profile = NO_DOPING;		/* No doping at first			*/
			sample[i].doping_layers = 1;					/* No sublayers					*/
			sample[i].temperature = -1;					/* Temperature undefined		*/
			sample[i].z = 0;									/* Just default to no film		*/
			for (j=0; j<NPARMS_DOPING; j++) sample[i].doping_parms[j] = 0;
		}

		/* Incident media is air, and second layer is EOS (nothing yet) */
		strcpy_s(sample[0].name, sizeof(sample[0].name), "air");
		if ( (sample[0].material = TFOC_FindMaterial("air", database)) == NULL) {
			fprintf(stderr, "Didn't recognize air ... real problem\n");
			fflush(stderr);
		}
		sample[0].type = INCIDENT;
		sample[1].type = EOS;								/* And an EOS */
	}

	/* Find EOS and add new layer; previous becomes SUBLAYER, this initially SUBSTRATE */
	rc = 0;														/* Assume success */
	for (i=0; i<MAX_LAYERS; i++) { if (sample[i].type == EOS) break; }
	if (i >= MAX_LAYERS-1) {								/* Have to leave last as EOS */
		sample[MAX_LAYERS-1].type = EOS;
		i = MAX_LAYERS-2;
	} else {														/* New layer ... previous now SUBLAYER */
		if (i != 1) sample[i-1].type = SUBLAYER;		/* Now a sublayer */
		sample[i+1].type = EOS;								/* And next will be EOS */
	}
	strcpy_s(sample[i].name, sizeof(sample[i].name), material);
	sample[i].z = nm;
	sample[i].type = SUBSTRATE;						/* Changed to SUBLAYER if another added */
	if ( (sample[i].material = TFOC_FindMaterial(sample[i].name, database)) == NULL) {
		fprintf(stderr, "ERROR: Unable to locate %s in the materials database directory\n", sample[i].name); fflush(stderr);
		if (i != 1) sample[i-1].type = SUBSTRATE;
		sample[i].type = EOS;
		rc = -1;
	}

	*psample = sample;
	return rc;
}

/* ===========================================================================
-- Air is assumed as incident media.  Last in the stack is the
-- substrate (nm ignored)
=========================================================================== */
TFOC_SAMPLE *MakeSample(int nlayers, FILM_LAYERS *films) {

	TFOC_SAMPLE *sample;
	int i;

	/* Start with an empty sample and add elements as we go */
	sample = NULL;
	for (i=0; i<nlayers; i++) AddSimpleLayer(&sample, films[i].material, films[i].nm);
	return sample;
}

/* ===========================================================================
-- Routine to calculate the theoretical reflectance (with possible correction for fitting work)
--
-- Usage: int TFOC_GetReflData(TFOC_SAMPLE *sample, double scaling, double theta, POLARIZATION mode, double temperature, int npt, double *lambda, double *refl);
--
-- Inputs: sample  - pointer to TFOC_SAMPLE structure describing the film stack
--         scaling - scaling factor that ultimately will be applied to data, but here *divides* the calculated reflectance
--                   for production, this should be 1.0 giving exact values
--                   A value of 1.2 would normally scale up experimental data by a factor of 1.2
--                     but in this routine scales down the calculated reflectivity by the same 1.2
--                   This permits comparison of raw data to calculations in fit
--         theta - angle of incidence
--         polarization - obvious
--         temperature  - obvious
--         npt - number of points in the data set
--         lambda - pointer to existing wavelengths to be processed
--         refl   - pointer to array to be filled with reflectance values
--
-- Output: *refl - filled with reflectance at each of the given wavelengths
--
-- Return: 0 if successful
=========================================================================== */
int TFOC_GetReflData(TFOC_SAMPLE *sample, double scaling, double theta, POLARIZATION mode, double temperature, int npt, double *lambda, double *refl) {

	int i,j;
	int nlayers;							/* Number of layers			*/

	/* Fresnel calculation layers and number */
	/* Variables that are retained after created first time */
	static TFOC_LAYER *layers = NULL;			/* Layers for Fresnel calc	*/
	static int dim_layers = 0;						/* How many layers are we able to handle (avoid allocating each time) */

	/* On subsequent runs, if SampleFile is NULL, use last values */
	if (sample == NULL) {
		fprintf(stderr, "Must have a sample structure\n"); fflush(stderr);
		return -2;
	}

	/* ----------------------------------------------------------
	-- Pre-process sample structure - don't have temperature yet
	-- Identify the material database information and
	-- at the same time, figure out how big the actual layer
	-- array will need to be given expansion of profiles, etc.
	---------------------------------------------------------- */
	nlayers = 0;
	for (i=0; sample[i].type != EOS; i++) {
		switch (sample[i].doping_profile) {
			case NO_DOPING:
			case CONSTANT:
				nlayers++;
				break;
			case EXPONENTIAL:
			case LINEAR_IMPLANT:
			case LINEAR:
				nlayers += sample[i].doping_layers;
		}
	}

	/* Make sure that we have space for the needed number of layers */
	/* But keep static so can be reused each time without multiple allocations */
	if (nlayers > dim_layers) {
		dim_layers = nlayers+1;
		layers = realloc(layers, dim_layers*sizeof(*layers));				/* Allocate space (one extra for safety) for layers */
		memset(layers, 0, dim_layers*sizeof(*layers));
	}

	/* ----------------------------------------------------------
	-- Okay, run the wavelengths.  Each time need to get the NK
	-- values for each layer in the sample.  And then generate
   -- the layer structure for running TFOC_ReflN
	---------------------------------------------------------- */
	for (i=0; i<npt; i++) {
		for (j=0; sample[j].type != EOS; j++) {
			sample[j].n = TFOC_FindNK(sample[j].material, lambda[i]);
		}
		TFOC_MakeLayers(sample, layers, temperature, lambda[i]);
		refl[i] = TFOC_ReflN(theta, mode, lambda[i], layers).R / scaling;
	}

	return 0;
}


/* ===========================================================================
-- Routine to calculate the reduced chi-square for the fit to
-- experimental reflectivity data
--
-- Usage: int CalcChiSqr(double *x, double *y, double *s, double *yfit, int npt, double xmin, double xmax, double *pchisqr, int *pdof);
--
-- Inputs: x    - x values (need for xmin,xmax limits)
--         y    - experimental reflectivity values 
--         s    - uncertainty estimate for the reflectivity values
--         yfit - fit value at each point
--         npt  - number of points in the data set
--         xmin,xmax - range of wavelengths to include in the calculation
--                     if xmax <= xmin, full range will be used
--         pchisqr - pointer to receive reduced chi-square value
--         pdof - number of degrees of freedom (# points used -1)
--
-- Output: if not NULL, *pchisqr and *pdof set to appropriate values
--
-- Return: 0 if no errors
=========================================================================== */
static int CalcChiSqr(double *x, double *y, double *s, double *yfit, int npt, double xmin, double xmax, double *pchisqr, int *pdof) {
	double chisqr;
	int i, dof;

	chisqr = 0;
	dof = 0;
	for (i=0; i<npt; i++) {
		if (x[i] < xmin || x[i] > xmax) continue;
		chisqr += pow(y[i]-yfit[i],2)/pow(s[i],2);
		dof++;
	}
	if (dof >= 2) chisqr /= (dof-1);
	if (pchisqr != NULL) *pchisqr = chisqr;
	if (pdof    != NULL) *pdof    = dof;
	return 0;
}


/* ============================================================================
-- func_eval - Fill in YFIT with value of function
--
-- Usage: logical = func_eval(x,yfit,npt)
--
-- Inputs: X   - Array of X points
--         NPT - Number of points to be evaluated
--
-- Common: equation - Definition of the equation to be fit
-- Input:
--
-- Output: YFIT - Curve containing the fit
--
-- WARNING: It is assumed that yfit is already linked to $YFIT from the
--          allocation of the array.  If not, we fail terribly!  Problem with
--          relinking here is that it is first deallocated before linking. 
--          This creates a memory overwrite problem that is very bad!
============================================================================ */
static int nls_eval(NLS_DATA *nls) {

	FILM_MEASURE_INFO *info;
	info = main_info;

	TFOC_GetReflData(info->sample.tfoc, info->sample.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, nls->yfit);

	return 0;
}

/* ============================================================================
-- ... Subroutine to determine the derivatives with respect to each of the
-- ... varied parameters.
--
-- Usage: LOGICAL = func_deriv(X,ipt,NPT)
--
-- Inputs: X   - List of X coordinates
--         ipt - Point # at which to evaluate derivatives
--         NPT - Number of points in X
--
-- Common: NTERMS - number of derivatives needed (1 for each varied parameter)
--         DERIVA - Function used to determine derivatives (or none)
--
-- ... If no analytic derivative is defined, we use the finite difference
-- ... method - takes twice as many calculations, but NBD.
-- ... Choose the increment size as the sqrt of the machine precision?
-- ... What to do when actually zero?
--
-- COMMON Output: deriv(i) - Value of the derivatives
-- ========================================================================== */
static int nls_deriv(double *results, NLS_DATA *nls, int ipt) {

	int i,j;
	double tmp, delta, *v;

	static double *fderiv[N_FILM_STACK+2];
	static double *center;
	static int ndim=-1;								/* How big are arrays ... may need to reset */

	FILM_MEASURE_INFO *info;
	info = main_info;

	/* Initial allocation of space for the derivatives */
	if (ndim <= 0) {
		ndim = info->npt;
		for (i=0; i<N_FILM_STACK+2; i++) fderiv[i] = malloc(ndim*sizeof(double));
		center = malloc(ndim*sizeof(double));
	}

	/* On ipt == 0, do the full vector.  After that, simple lookup */
	if (ipt == 0) {
		if (ndim != info->npt) {
			ndim = info->npt;
			for (i=0; i<N_FILM_STACK+2; i++) fderiv[i] = realloc(fderiv[i], ndim*sizeof(double));
			center = realloc(center, ndim*sizeof(double));		/* Center values with current parameters */
		}

		/* Evaluate at the center point */
		TFOC_GetReflData(info->sample.tfoc, info->sample.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, center);

		for (i=0; i<nls->nvars; i++) {
			v = nls->vars[i];
			tmp = *v;
			if (i != nls->nvars-1) {
				delta = 1.0;								/* Use a 1 nm change so tfoc has a chance (always +) */
			} else {
				delta = 0.01;
			}
			*v +=   delta;
			TFOC_GetReflData(info->sample.tfoc, info->sample.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, fderiv[i]);
			for (j=0; j<info->npt; j++) fderiv[i][j] = (fderiv[i][j]-center[j])/delta;
			*v = tmp;
		}
	}

	/* Now have data stored as a vector ... just return the appropriate points */
	for (i=0; i<nls->nvars; i++) results[i] = fderiv[i][ipt];
	return 0;
}


/* ===========================================================================
--- Do fit
=========================================================================== */
#define	MAXITER	(20)							/* Max iterations to find solution */

static int do_fit(HWND hdlg, FILM_MEASURE_INFO *info) {

	/* Local variables */
	int mode = 0;
	int		i,j,k, iter;						/* Random integer constants	*/
	char		token[256];
	int		rcode=0;
	double	*xy[3];								/* Array for the dependent vars */
	char *var_names[N_FILM_STACK+2];

	static NLS_DATA *nls=NULL;					/* Structure passed to NLSFIT	*/

	/* Clear and set the parameter structure */
	if (nls == NULL) nls = calloc(1, sizeof(*nls));

	/* Initialize the data structure to nlsfit() now */
	nls->yfit      = NULL;				/* Let fit routines allocate space	*/
	nls->outchi    = NULL;				/* Let fit allocate space if needed	*/
	nls->correlate = NULL;				/* No correlation matrix wanted		*/
	nls->workspace = NULL;				/* Let fit allocate space if needed	*/
	nls->magic_cookie = 0;				

	nls->data = info->cv_refl->y;			/* Experimental reflectance curve */
	nls->errorbar = info->cv_refl->s;	/* Uncertainty on measured reflectivity */
	nls->npt = info->npt;					/* Number of points */
	xy[0]    = info->lambda;				/* At moment, not use, but let's define them */
	xy[1]    = info->cv_refl->y;
	xy[2]    = info->cv_refl->s;
	nls->xy  = xy;

	nls->flamda   = 0;						/* Let CurveFit() set initial value	*/
	nls->EpsCrit  = 1E-4;					/* CurveFit() now does completion test */

	nls->evalfnc   = nls_eval;				/* Functions to evaluate function	*/
	nls->fderiv    = nls_deriv;			/* Functions to evaluate derivative	*/
	nls->evalchi   = NULL;					/* Use default chisqr evaluation		*/

	if (nls->vars  == NULL) nls->vars  = calloc(N_FILM_STACK+2, sizeof(*nls->vars));
	if (nls->sigma == NULL) nls->sigma = calloc(N_FILM_STACK+2, sizeof(*nls->sigma));
	nls->correlate = NULL;
	if (nls->lower == NULL) nls->lower = calloc(N_FILM_STACK+2, sizeof(*nls->lower));
	if (nls->upper == NULL) nls->upper = calloc(N_FILM_STACK+2, sizeof(*nls->upper));

	/* Mark only wavelengths within the given region for testing */
	if (nls->valid == NULL) nls->valid = calloc(info->npt, sizeof(*nls->valid));
	for (i=0; i<info->npt; i++) nls->valid[i] = (info->lambda[i] >= info->fit_parms.lambda_min) && (info->lambda[i] <= info->fit_parms.lambda_max);

	/* Include in all of the requested variations */
	nls->nvars = 0;							/* How many are we actually going to do? */
	for (i=0,j=0; i<info->sample.layers; i++) {
		if (! info->sample.stack[i].vary) continue;
		nls->vars[j] = &info->sample.tfoc[i+1].z;				/* In tfoc structure ... 0 is air */
		nls->lower[j] = info->sample.stack[i].lower;
		nls->upper[j] = info->sample.stack[i].upper;
		var_names[j] = info->sample.stack[i].layer_name;
		j++;
	}
	/* And then add in the scaling factor (always appropriate for small changes in illumination intensity) */
	nls->vars[j] = &info->sample.scaling;
	nls->lower[j] = info->fit_parms.scaling_min;
	nls->upper[j] = info->fit_parms.scaling_max;
	var_names[j] = "scaling";
	j++;
	nls->nvars = j;

	/* Initialize everything else in CurveFit routine */
	if ( (rcode = CurveFit(NKEY_INIT, 0, nls)) != 0) {
		printf("ERROR: Error on initialization of routine\n"); fflush(stdout);
		goto FitExit;
	}

	/* And we are off and running */
	fputs("------------------------------------------------------------------------------\n", stdout);
	strcpy_s(token, sizeof(token), "    CHISQR ");
	for (i=0; i<nls->nvars;) {
		fputs(token, stdout);
		for (j=0; j<6 && i<nls->nvars; j++) printf("%11s", var_names[i++]);
		fputs("\n", stdout);
		strcpy_s(token, sizeof(token), "           ");
	}
	fputs("------------------------------------------------------------------------------\n", stdout);

	/* Set key to be either silent or verbose on fitting */
	rcode = 0;
	for (iter=0; iter<MAXITER; iter++) {			/* Number of reps allowed */
		printf("\r%11.4g", nls->chisqr);
		for (j=0; j<nls->nvars; ) {
			for (k=0; k<6 && j<nls->nvars; k++) printf("%11.4g", *nls->vars[j++]);
			fputs("\n", stdout);
			if (j != nls->nvars) fputs("           ", stdout);
		}
		fflush(stdout);

		if (nls->chisqr <= 0 || rcode == 1) break;		/* Basically success! */
		if ( (rcode = CurveFit(NKEY_TRY_VERBOSE, iter, nls)) < 0) goto FitExit;		/* Run again */
	}
	if (rcode == 0 && iter >= MAXITER) rcode = 2;	/* Run out of time? */

	/* Print results */
	fputs( "\n"
			 "    Variable                Value               Sigma\n"
			 "    --------                -----               -----\n", stdout);
	/*				"    123456789012345  12345.1234567     123456.1234567 */
	for (i=0; i<nls->nvars; i++) {
		printf("     %-15s  %13.7g     %14.7g\n", var_names[i], *nls->vars[i], nls->sigma[i]);
	}
	fputs("\n", stdout);

	printf("     Degrees of Freedom: %d\n", nls->dof);
	printf("     Root Mean Variance: %g\n", sqrt(nls->chisqr));
	printf("     Estimated Y sigma:  %g\n", nls->sigmaest);
	fputs( "     WARNING: Error estimates valid only if estimated Y sigma is correct\n", stdout);
	fputs("\n", stdout);

	/* ----------------------- */
FitExit:
	/* ----------------------- */
	switch (rcode) {
		case -1:
			fputs("      GET OFF THE QUAALUDES, MAN!\n"
					"ERROR: Too many parameters for number of data points\n", stdout);
			break;
		case -2:
			fputs("ERROR: Unable to properly evaluate function (NLSFIT)\n", stdout);
			break;
		case -3:
			fputs("ERROR: Unable to allocate temporary matrix space (NLSFIT)\n", stdout);
			break;
		case -4:
			fputs("ERROR: Unable to allocate work spaces.  (NLSFIT)\n", stdout);
			break;
		case -5:
			fputs("ERROR: *** Fit aborted by user pressing ^C (NLSFIT) ***\n", stdout);
			break;
		case -6:				/* Initialization errors - reported by CurveFit() */
			break;
		case 1:
			break;			/* Success! */
		case 2:
			fputs("WARNING: Maximum iteration count reached.  A better fit may be obtained\n"
					"         by running fit again starting from these parameters\n", stdout);
			break;
		default:
			if (rcode < 0) printf("Function evaluator errors.  (NLSFIT)\n");
	}

	/* Clean up workspaces and exit */
	CurveFit(NKEY_EXIT, 0, nls);					/* Free allocated workspaces	*/

	/* If we are mostly successful, transfer back */
	if (rcode >= 0) {												/* Only in case of success */

		/* Transfer values from tfoc structure back into the sample stack */
		for (i=0,j=0; i<info->sample.layers; i++) {
			if (info->sample.stack[i].vary) {
				info->sample.stack[i].nm    = info->sample.tfoc[i+1].z;		/* layer 0 is air */
				info->sample.stack[i].sigma = nls->sigma[j];
				j++;
			}
		}

		/* Do we want to log these results? */
		if (GetDlgItemCheck(hdlg, IDC_LOG_REFINES)) {
			FILE *funit;
			char pathname[PATH_MAX];

			GetDlgItemText(hdlg, IDV_LOGFILE, pathname, sizeof(pathname));
			if (*pathname == '\0' || fopen_s(&funit, pathname, "a") != 0) {
				Beep(ERROR_BEEP_FREQ, ERROR_BEEP_MS);
				SetDlgItemCheck(hdlg, IDC_LOG_REFINES, FALSE);
				EnableDlgItem(hdlg, IDV_LOGFILE, FALSE);
				EnableDlgItem(hdlg, IDB_EDIT_LOGFILE, FALSE);
			} else {
				fprintf(funit, "%lld", time(NULL));
				for (i=0; i<nls->nvars; i++) {
					fprintf(funit, ",%g,%g", *nls->vars[i], nls->sigma[i]*sqrt(nls->chisqr));
				}
				fprintf(funit, "\n");
				fclose(funit);
			}
		}
	}

	fflush(NULL);
	return rcode;
}
