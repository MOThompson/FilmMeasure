/* FilmMeasure.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

#define NEED_WINDOWS_LIBRARY			/* Define to include windows.h call functions */

#define	SPEC_CLIENT_IP			LOOPBACK_SERVER_IP_ADDRESS

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>					  /* for defining several useful types and macros */
#include <stdio.h>					  /* for performing input and output */
#include <stdlib.h>					  /* for performing a variety of operations */
#include <string.h>
#include <math.h>						  /* basic math functions */
#include <float.h>
#include <limits.h>
#include <assert.h>
#include <stdint.h>				     /* C99 extension to get known width integers */
#include <time.h>
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

static CB_INT_LIST materials[] = {
	{"none", 0},
	{"c-Si", 1},
	{"a-Si", 2},
	{"SiO2", 3},
	{"S1800", 4},
	{"S1800_exp", 5},
	{"S1800_unexp", 6},
	{"Bi2O3", 7},
	{"TiO2", 8}
};

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

static int CalcChiSqr(double *x, double *y, double *s, double *yfit, int npt, double xmin, double xmax, double *pchisqr, int *pdof);
static int do_fit(HWND hdlg, FILM_MEASURE_INFO *info);
static int SaveData(FILM_MEASURE_INFO *info, char *fname);
static int LoadData(FILM_MEASURE_INFO *info, char *path);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */
FILM_MEASURE_INFO *main_info = NULL;
	
/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static HINSTANCE hInstance=NULL;
static HWND main_hdlg = NULL;					/* Handle to primary dialog box */

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
	DialogBox(hInstance, "FILMMEASURE_DIALOG", HWND_DESKTOP, (DLGPROC) MainDlgProc);

	return 0;
}

/* ===========================================================================

=========================================================================== */
#define	TIMER_SPEC_UPDATE			(3)

INT_PTR CALLBACK MainDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "MainDlgProc";

	BOOL rcode, enable;
	int wID, wNotifyCode;
	int i, icnt, rc;
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
		IDV_SPEC_IP, 
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

			main_info = info = (FILM_MEASURE_INFO *) calloc(1, sizeof(*info));
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG) info);

			/* Initialize parameters in the info structure */
			info->hdlg = main_hdlg = hdlg;				/* Have this available for other use */
			info->lambda_min = 200.0;						/* Graph X-range limits */
			info->lambda_max = 900.0;
			info->lambda_autoscale = TRUE;
			info->fit_parms.lambda_min  = 300.0;		/* Fitting parameters */
			info->fit_parms.lambda_max  = 800.0;
			info->fit_parms.scaling_min = 0.95;
			info->fit_parms.scaling_max = 1.05;
			info->fit_parms.scaling     = 1.0;

			/* Copy default IP address and try to connect to the LasGo client/server */
			/* Due to the possible timeouts on making connections, do as a thread */
			/* Fill in combo boxes and select default value */
			EnableWindow(GetDlgItem(hdlg, IDV_SPEC_IP), FALSE);
			ComboBoxFillPtrList(hdlg, IDL_SPEC_IP, ip_list, CB_COUNT(ip_list));
			ComboBoxSetByPtrValue(hdlg, IDL_SPEC_IP, LOOPBACK_SERVER_IP_ADDRESS);
			strcpy_m(info->spec_IP, sizeof(info->spec_IP), LOOPBACK_SERVER_IP_ADDRESS);
			SetDlgItemText(hdlg, IDV_SPEC_IP, info->spec_IP);

			SendMessage(hdlg, WMP_UPDATE_SPEC_PARMS, 0, 0);					/* Also updates the graphs */

			/* Autoscale and manual wavelength ranges for graph */
			SetDlgItemCheck(hdlg, IDC_AUTORANGE_WAVELENGTH, info->lambda_autoscale);
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MIN, "%.1f", info->lambda_min);
			SetDlgItemDouble(hdlg, IDV_GRAPH_LAMBDA_MAX, "%.1f", info->lambda_max);
			EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MIN, ! info->lambda_autoscale);
			EnableDlgItem(hdlg, IDV_GRAPH_LAMBDA_MAX, ! info->lambda_autoscale);

			/* Filling parameters */
			SetDlgItemDouble(hdlg, IDV_FIT_LAMBDA_MIN,  "%.1f", info->fit_parms.lambda_min);
			SetDlgItemDouble(hdlg, IDV_FIT_LAMBDA_MAX,  "%.1f", info->fit_parms.lambda_max);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING,     "%.3f", info->fit_parms.scaling);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING_MIN, "%.1f", info->fit_parms.scaling_min);
			SetDlgItemDouble(hdlg, IDV_FIT_SCALING_MAX, "%.1f", info->fit_parms.scaling_max);

			/* Fill in the combo boxes for the sample */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxClearList(hdlg, IDC_FILM_MATERIAL_0+i);
				ComboBoxFillIntList(hdlg, IDC_FILM_MATERIAL_0+i, materials, CB_COUNT(materials));
				ComboBoxSetByIntValue(hdlg, IDC_FILM_MATERIAL_0+i, 0);
			}
			ComboBoxClearList(hdlg, IDC_FILM_SUBSTRATE);
			ComboBoxFillIntList(hdlg, IDC_FILM_SUBSTRATE, materials, CB_COUNT(materials));
			ComboBoxSetByIntValue(hdlg, IDC_FILM_SUBSTRATE, 1);

			/* Fill in the combo boxes for the reference film */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxClearList(hdlg, IDC_REF_MATERIAL_0+i);
				ComboBoxFillIntList(hdlg, IDC_REF_MATERIAL_0+i, materials, CB_COUNT(materials));
				ComboBoxSetByIntValue(hdlg, IDC_REF_MATERIAL_0+i, 0);
			}
			ComboBoxClearList(hdlg, IDC_REF_SUBSTRATE);
			ComboBoxFillIntList(hdlg, IDC_REF_SUBSTRATE, materials, CB_COUNT(materials));
			ComboBoxSetByIntValue(hdlg, IDC_REF_SUBSTRATE, 1);

			/* Start the timers */
			SetTimer(hdlg, TIMER_SPEC_UPDATE, 1000, NULL);					/* Update spectrometer parameters seconds (if live) */
			rcode = TRUE; break;

		case WM_CLOSE:
			if (info->spec_ok) { Shutdown_Spec_Client();	info->spec_ok = FALSE; }
			info->hdlg = NULL;
			free(info);
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_SPEC_UPDATE) {
				/* Query spectrometer integration parameters */
				if (info->spec_ok) {
				}
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
			if (info->spec_ok) { Shutdown_Spec_Client(); info->spec_ok = FALSE; }
			info->spec_ok = Init_Spec_Client(info->spec_IP) == 0;
			if (! info->spec_ok) {
				MessageBox(hdlg, "Unable to open", "SPEC connect failure", MB_ICONERROR | MB_OK);
			} else {
				int client_version, server_version;

				client_version = Spec_Remote_Query_Client_Version();
				server_version = Spec_Remote_Query_Server_Version();
				if (client_version != server_version) {
					MessageBox(hdlg, "ERROR: Version mismatch between client and server.  Have to abort", "SPEC connect failure", MB_ICONERROR | MB_OK);
					Shutdown_Spec_Client();	info->spec_ok = FALSE;
				}
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
			ComboBoxSetByIntValue(hdlg, IDC_REF_SUBSTRATE, 1);			/* Set substrate as c-Si */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxSetByIntValue(hdlg, IDC_REF_MATERIAL_0+i, 0);	/* Mark as empty */
				SetDlgItemText(hdlg, IDV_REF_NM_0+i,    "");
				EnableDlgItem(hdlg, IDV_REF_NM_0+i,    FALSE);
			}
			rcode = TRUE; break;

		case WMP_MAKE_REFERENCE_STACK:
			nlayers = 0;															/* Must have substrate though */
			stack = info->reference.stack;									/* Sample stack structure */
			for (i=0; i<N_FILM_STACK; i++) {
				imat = ComboBoxGetIntValue(hdlg, IDC_REF_MATERIAL_0+i);
				if (imat > 0) {													/* Any layer here? */
					sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "reference layer %d", i+1);
					strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
					stack[nlayers].nm = GetDlgItemDouble(hdlg, IDV_REF_NM_0+i);
					nlayers++;
				}
			}
			imat = ComboBoxGetIntValue(hdlg, IDC_REF_SUBSTRATE);
			if (imat <= 0) imat = 1;
			sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "reference substrate");
			strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
			stack[nlayers].nm = 100.0;			/* Doesn't matter */
			nlayers++;

			if (info->reference.tfoc != NULL) free(info->reference.tfoc);
			info->reference.tfoc = MakeSample(nlayers, stack);
			info->reference.layers = nlayers;
			rcode = TRUE; break;

		case WMP_CLEAR_SAMPLE_STACK:
			ComboBoxSetByIntValue(hdlg, IDC_FILM_SUBSTRATE, 1);		/* Set substrate as c-Si */
			for (i=0; i<N_FILM_STACK; i++) {
				ComboBoxSetByIntValue(hdlg, IDC_FILM_MATERIAL_0+i, 0);	/* Mark as empty */
				SetDlgItemText(hdlg, IDV_FILM_NM_0+i,    "");
				SetDlgItemText(hdlg, IDT_SIGMA_0+i,		  "");
				SetDlgItemText(hdlg, IDV_NM_LOW_0+i,     "");
				SetDlgItemText(hdlg, IDV_NM_HIGH_0+i,    "");
				SetDlgItemCheck(hdlg, IDC_VARY_0+i,     FALSE);

				EnableDlgItem(hdlg, IDV_FILM_NM_0+i,    FALSE);
				EnableDlgItem(hdlg, IDT_SIGMA_0+i,		 FALSE);
				EnableDlgItem(hdlg, IDV_NM_LOW_0+i,     FALSE);
				EnableDlgItem(hdlg, IDV_NM_HIGH_0+i,    FALSE);
				EnableDlgItem(hdlg, IDC_VARY_0+i,       FALSE);
			}
			rcode = TRUE; break;

		case WMP_MAKE_SAMPLE_STACK:
			nlayers = 0;															/* Must have substrate though */
			stack = info->sample.stack;										/* Sample stack structure */
			for (i=0; i<N_FILM_STACK; i++) {
				imat = ComboBoxGetIntValue(hdlg, IDC_FILM_MATERIAL_0+i);
				if (imat > 0) {													/* Any layer here? */
					sprintf_s(stack[nlayers].layer_name, sizeof(stack[nlayers].layer_name), "layer %d", i+1);
					strcpy_s(stack[nlayers].material, sizeof(stack[nlayers].material), materials[imat].id);
					stack[nlayers].nm = GetDlgItemDouble(hdlg, IDV_FILM_NM_0+i);
					
					stack[nlayers].vary = GetDlgItemCheck(hdlg, IDC_VARY_0+i);
					stack[nlayers].lower = GetDlgItemDouble(hdlg, IDV_NM_LOW_0+i);
					stack[nlayers].upper = GetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i);
					stack[nlayers].sigma = 0.0;
					nlayers++;
				}
			}
			imat = ComboBoxGetIntValue(hdlg, IDC_FILM_SUBSTRATE);
			if (imat <= 0) imat = 1;
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
				SendMessage(hdlg, WMP_MAKE_REFERENCE_STACK, 0, 0);
				info->tfoc_reference = realloc(info->tfoc_reference, info->npt * sizeof(*info->tfoc_reference));
				TFOC_GetReflData(info->reference.tfoc, 1.0, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, info->tfoc_reference);

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

		case WMP_PROCESS_MEASUREMENT:
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
					cv->s[i] = sqrt(1.0/max(1.0,raw[i]) + 1.0/max(1.0,ref[i]));		/* Fractional uncertainty */
					cv->y[i] = max(-1.0, min(2.0, cv->y[i]));
					if (info->tfoc_reference != NULL) cv->y[i] *= info->tfoc_reference[i];
					cv->s[i] = cv->s[i] * cv->y[i];								/* Been calculating fractional relative error */
				}
				cv->modified = TRUE;

				/* Read the current parameters to create a "fit stack" and generate reflectance curve */
				SendMessage(hdlg, WMP_MAKE_SAMPLE_STACK, 0, 0);
				info->tfoc_fit = realloc(info->tfoc_fit, info->npt * sizeof(*info->tfoc_fit));
				TFOC_GetReflData(info->sample.tfoc, info->fit_parms.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, info->tfoc_fit);

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
					if (info->spec_ok) {
						Shutdown_Spec_Client();	info->spec_ok = FALSE;
					} else {
						SendMessage(hdlg, WMP_OPEN_SPEC, 0, 0);
					}
					SendMessage(hdlg, WMP_LOAD_SPEC_PARMS, 0, 0);	/* Enables/disables controls */
					rcode = TRUE; break;

				case IDB_SPEC_PARMS_UPDATE:
					SendMessage(hdlg, WMP_UPDATE_SPEC_PARMS, 0, 0);
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
					if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
					if (wID == IDB_COLLECT_REFERENCE) {
						rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
						npt = info->npt;
					} else {
						rc = Spec_Remote_Grab_Saved(SPEC_SPECTRUM_REFERENCE, &data, &npt);
					}
					if (rc != 0) {
						MessageBeep(MB_ICONERROR);
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
					rcode = TRUE; break;

				case IDB_COLLECT_DARK:
				case IDB_TAKE_DARK:
					if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
					if (wID == IDB_COLLECT_DARK) {
						rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
						npt = info->npt;
					} else {
						rc = Spec_Remote_Grab_Saved(SPEC_SPECTRUM_DARK, &data, &npt);
					}
					if (rc != 0) {
						MessageBeep(MB_ICONERROR);
					} else {
						cv = info->cv_dark = ReallocRawCurve(hdlg, info, info->cv_dark, npt, 1, "dark", colors[1]);
						for (i=0; i<npt; i++) cv->y[i] = data[i];
						cv->modified = TRUE;
						free(data);
						SetDlgItemCheck(hdlg, IDC_SHOW_DARK, TRUE);
						EnableDlgItem  (hdlg, IDC_SHOW_DARK, TRUE);

						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);		/* Does a redraw */
					}
					rcode = TRUE; break;

				case IDB_MEASURE:
				case IDB_MEASURE_RAW:
				case IDB_MEASURE_TEST:
					if (! info->lambda_transferred) SendMessage(hdlg, WMP_LOAD_SPEC_WAVELENGTHS, 0, 0);
					if (wID == IDB_MEASURE) {
						rc = Acquire_Raw_Spectrum(hdlg, info, &spectrum_info, &data);
						npt = info->npt;
					} else {
						rc = Spec_Remote_Grab_Saved((wID == IDB_MEASURE_RAW) ? SPEC_SPECTRUM_RAW : SPEC_SPECTRUM_TEST, &data, &npt);
					}
					if (rc != 0) {
						MessageBeep(MB_ICONERROR);
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

				/* Substrate ... all valid except none */
				case IDC_FILM_SUBSTRATE:
					if (wNotifyCode == CBN_SELCHANGE) {
						if (ComboBoxGetIntValue(hdlg, wID) < 1) ComboBoxSetByIntValue(hdlg, wID, 1);
					}
					rcode = TRUE; break;
					
				/* Sample structure */
				case IDC_FILM_MATERIAL_0:
				case IDC_FILM_MATERIAL_1:
				case IDC_FILM_MATERIAL_2:
				case IDC_FILM_MATERIAL_3:
				case IDC_FILM_MATERIAL_4:
					if (wNotifyCode == CBN_SELCHANGE) {
						i = wID - IDC_FILM_MATERIAL_0;
						enable = ComboBoxGetIntValue(hdlg, wID) >= 1 ;
						EnableDlgItem(hdlg, IDV_FILM_NM_0+i, enable);
						EnableDlgItem(hdlg, IDC_VARY_0+i, enable);
						enable = enable && GetDlgItemCheck(hdlg, IDC_VARY_0+i);
						EnableDlgItem(hdlg, IDV_NM_LOW_0+i, enable);
						EnableDlgItem(hdlg, IDV_NM_HIGH_0+i, enable);
					}
					rcode = TRUE; break;
						
				/* Thicknesses to vary */
				case IDC_VARY_0:
				case IDC_VARY_1:
				case IDC_VARY_2:
				case IDC_VARY_3:
				case IDC_VARY_4:
					i = wID - IDC_VARY_0;
					enable = GetDlgItemCheck(hdlg, wID);
					EnableDlgItem(hdlg, IDV_NM_LOW_0+i, enable);
					EnableDlgItem(hdlg, IDV_NM_HIGH_0+i, enable);
					if (enable) {														/* When enabled, set default search range */
						rval  = GetDlgItemDouble(hdlg, IDV_FILM_NM_0+i);
						lower = GetDlgItemDouble(hdlg, IDV_NM_LOW_0+i);
						upper = GetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i);
						if (lower > rval) lower = 0.0;
						if (upper < rval || upper > 2.0*rval) upper = max(10.0, 2.0 * rval);
						SetDlgItemDouble(hdlg, IDV_NM_LOW_0+i,  "%.1f", lower);
						SetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i, "%.1f", upper);
						info->sample.stack[i].lower = lower;
						info->sample.stack[i].upper = upper;
					}
					rcode = TRUE; break;

				case IDB_REF_CLEAR:
					SendMessage(hdlg, WMP_CLEAR_REFERENCE_STACK, 0, 0);
					rcode = TRUE; break;

				case IDC_REF_MATERIAL_0:
				case IDC_REF_MATERIAL_1:
				case IDC_REF_MATERIAL_2:
				case IDC_REF_MATERIAL_3:
				case IDC_REF_MATERIAL_4:
					if (wNotifyCode == CBN_SELCHANGE) {
						i = wID - IDC_REF_MATERIAL_0;
						EnableDlgItem(hdlg, IDV_REF_NM_0+i, ComboBoxGetIntValue(hdlg, wID) >= 1);
						SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);			/* Rebuild the corrections */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);	/* Does a redraw */
					}
					rcode = TRUE; break;

				/* Reference film thickness */
				case IDV_REF_NM_0:
				case IDV_REF_NM_1:
				case IDV_REF_NM_2:
				case IDV_REF_NM_3:
				case IDV_REF_NM_4:
					if (wNotifyCode == EN_KILLFOCUS) {
						SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);			/* Rebuild the corrections */
						SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);	/* Does a redraw */
					}
					rcode = TRUE; break;

				/* Film thickness (use also to set range if blank) */
				case IDV_FILM_NM_0:
				case IDV_FILM_NM_1:
				case IDV_FILM_NM_2:
				case IDV_FILM_NM_3:
				case IDV_FILM_NM_4:
					i = wID-IDV_FILM_NM_0;
					if (wNotifyCode == EN_KILLFOCUS) {
						rval = GetDlgItemDouble(hdlg, wID);
						if (rval < 0) rval = 0.0;
						SetDlgItemDouble(hdlg, wID, "%.1f", rval);
						info->sample.stack[i].nm = rval;
						if (GetDlgItemCheck(hdlg, IDC_VARY_0+i)) {
							lower = info->sample.stack[i].lower;
							upper = info->sample.stack[i].upper;
							if (lower > rval) lower = 0.0;
							if (upper < rval || upper > 2.0*rval) upper = max(10.0, 2.0 * rval);
							SetDlgItemDouble(hdlg, IDV_NM_LOW_0+i,  "%.1f", lower);
							SetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i, "%.1f", upper);
							info->sample.stack[i].lower = lower;
							info->sample.stack[i].upper = upper;
						}
					}
					rcode = TRUE; break;

				case IDV_NM_LOW_0:
				case IDV_NM_LOW_1:
				case IDV_NM_LOW_2:
				case IDV_NM_LOW_3:
				case IDV_NM_LOW_4:
					i = wID-IDV_NM_LOW_0;
					if (wNotifyCode == EN_KILLFOCUS) {
						double rval, lower;
						rval  = GetDlgItemDouble(hdlg, IDV_FILM_NM_0+i);
						lower = GetDlgItemDouble(hdlg, wID);
						if (lower > rval) lower = 0.0;
						SetDlgItemDouble(hdlg, wID,  "%.1f", lower);
						info->sample.stack[i].lower = rval;
					}
					rcode = TRUE; break;

				case IDV_NM_HIGH_0:
				case IDV_NM_HIGH_1:
				case IDV_NM_HIGH_2:
				case IDV_NM_HIGH_3:
				case IDV_NM_HIGH_4:
					i = wID-IDV_NM_HIGH_0;
					if (wNotifyCode == EN_KILLFOCUS) {
						double rval, upper;
						rval  = GetDlgItemDouble(hdlg, IDV_FILM_NM_0+i);
						upper = GetDlgItemDouble(hdlg, wID);
						if (upper < rval) upper = 2.0*rval;
						SetDlgItemDouble(hdlg, wID,  "%.1f", upper);
						info->sample.stack[i].upper = rval;
					}
					rcode = TRUE; break;

				case IDB_FILM_CLEAR:
					SendMessage(hdlg, WMP_CLEAR_SAMPLE_STACK, 0, 0);
					rcode = TRUE; break;

				case IDB_TRY:
					SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);
					rcode = TRUE; break;

				case IDB_FIT:
					SendMessage(hdlg, WMP_MAKE_SAMPLE_STACK, 0, 0);				/* Has all data for moment */
					do_fit(hdlg, info);													/* go and let it run */

					/* Record values and replot new fit ... then deal with sigma */
					icnt = 1;																/* alway vary scaling */
					for (i=0; i<N_FILM_STACK; i++) {
						if (info->sample.stack[i].vary) {
							SetDlgItemDouble(hdlg, IDV_FILM_NM_0+i, "%.2f", info->sample.stack[i].nm);
							icnt++;
						}
					}
					SetDlgItemDouble(hdlg, IDV_FIT_SCALING, "%.3f", info->fit_parms.scaling);
					CalcChiSqr(info->lambda, info->cv_refl->y, info->cv_refl->s, info->tfoc_fit, info->npt, info->fit_parms.lambda_min, info->fit_parms.lambda_max, &chisqr, &dof);
					chisqr = chisqr*dof/max(1,dof-icnt);							/* Correct for # of free parameters */
					dof -= icnt;
					SetDlgItemDouble(hdlg, IDT_CHISQR, "%.3f", sqrt(chisqr));
					SetDlgItemInt(hdlg, IDT_DOF, dof, TRUE);

					/* Fake the sigma based on chisqr ... too many people would not understand the link with chisqr_\nu */
					for (i=0; i<N_FILM_STACK; i++) {
						if (info->sample.stack[i].vary) {
							SetDlgItemDouble(hdlg, IDT_SIGMA_0+i, "%.2f", info->sample.stack[i].sigma*sqrt(chisqr));
						} else {
							SetDlgItemText(hdlg, IDT_SIGMA_0+i, "0.0");
						}
					}
					SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);			/* Redraw the results */
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
						info->fit_parms.scaling = GetDlgItemDouble(hdlg, wID);
						if (info->fit_parms.scaling < 0.5) info->fit_parms.scaling = 0.5;
						if (info->fit_parms.scaling > 2.0) info->fit_parms.scaling = 2.0;
						SetDlgItemDouble(hdlg, wID, "%.3f", info->fit_parms.scaling);
						SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);
					}
					rcode = TRUE; break;

				case IDV_FIT_SCALING_MIN:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.scaling_min = GetDlgItemDouble(hdlg, wID);
						if (info->fit_parms.scaling_min < 0.5) info->fit_parms.scaling_min = 0.5;
						SetDlgItemDouble(hdlg, wID, "%.1f", info->fit_parms.scaling_min);
					}
					rcode = TRUE; break;
					
				case IDV_FIT_SCALING_MAX:
					if (wNotifyCode == EN_KILLFOCUS) {
						info->fit_parms.scaling_max = GetDlgItemDouble(hdlg, wID);
						if (info->fit_parms.scaling_max > 2.0) info->fit_parms.scaling_max = 2.0;
						SetDlgItemDouble(hdlg, wID, "%.1f", info->fit_parms.scaling_max);
					}
					rcode = TRUE; break;

				case IDB_SAVE_DATA:
					SaveData(info, NULL);
					rcode = TRUE; break;

				case IDB_LOAD_DATA:
					LoadData(info, NULL);
					rcode = TRUE; break;

				/* Know to be unused notification codes (handled otherwise) */
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
			if ( (imat = ComboBoxGetIntValue(hdlg, IDC_REF_MATERIAL_0+i)) <= 0) continue;
			fprintf(funit, "#    %d \"%s\" %f\n", i, materials[imat].id, GetDlgItemDouble(hdlg, IDV_REF_NM_0+i));
		}
		imat = ComboBoxGetIntValue(hdlg, IDC_REF_SUBSTRATE);
		fprintf(funit, "#   substrate \"%s\"\n", materials[imat].id);
		fprintf(funit, "# END\n");

		fprintf(funit, "# SAMPLE STACK\n");
		for (i=0; i<N_FILM_STACK; i++) {
			if ( (imat = ComboBoxGetIntValue(hdlg, IDC_FILM_MATERIAL_0+i)) <= 0) continue;
			fprintf(funit, "#   %d \"%s\" %f %f %f %d\n", i, materials[imat].id, 
					  GetDlgItemDouble(hdlg, IDV_FILM_NM_0+i), 
					  GetDlgItemDouble(hdlg, IDV_NM_LOW_0+i), GetDlgItemDouble(hdlg, IDV_NM_HIGH_0+i),
					  GetDlgItemCheck(hdlg, IDC_VARY_0+i));
		}
		imat = ComboBoxGetIntValue(hdlg, IDC_FILM_SUBSTRATE);
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
-- Routine to look up a material by name and return the index in the
-- CB_INT_LIST so can be loaded properly
=========================================================================== */
int GetMaterialIndex(char *aptr, char **endptr) {
	int i, cnt;
	char *bptr, id[200];				/* For the name */

	/* Copy over the string, expecting that it is "enclosed" in quotes */
	while (isspace(*aptr)) aptr++;
	if (*aptr == '"') {
		aptr++;
		bptr = id; cnt = sizeof(id);
		while (*aptr && *aptr != '"') {
			if (--cnt > 0) *(bptr++) = *aptr;
			aptr++;
		}
		if (*aptr == '"') aptr++;
		*bptr = '\0';
	}
	while (isspace(*aptr)) aptr++;
	if (endptr != NULL) *endptr = aptr;
	
	/* Scan through the list of all materials I've loaded */
	cnt = CB_COUNT(materials);
	for (i=0; i<cnt; i++) {
		if (_stricmp(materials[i].id, id) == 0) return i;
	}
	return cnt-1;
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

	int i, ipt, index, npt;
	BOOL bval;
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
						ComboBoxSetByIntValue(hdlg, IDC_REF_MATERIAL_0+index, GetMaterialIndex(aptr, &aptr));
						SetDlgItemDouble(hdlg, IDV_REF_NM_0+index, "%.1f", strtod(aptr, &aptr));
						EnableDlgItem(hdlg, IDC_REF_MATERIAL_0+index, TRUE);
						EnableDlgItem(hdlg, IDV_REF_NM_0+index, TRUE);
					}
				} else if (_strnicmp(aptr, "substrate ", 10) == 0) {
					ComboBoxSetByIntValue(hdlg, IDC_REF_SUBSTRATE, GetMaterialIndex(aptr+10, NULL));
				}

			} else if (mode == SAMPLE) {
				aptr = szBuf+1;
				while (isspace(*aptr)) aptr++;
				if (isdigit(*aptr)) {									/* Okay, is a layer definition */
					index = *(aptr++)-'0';
					if (index >= 0 && index < N_FILM_STACK) {
						ComboBoxSetByIntValue(hdlg, IDC_FILM_MATERIAL_0+index, GetMaterialIndex(aptr, &aptr));
						SetDlgItemDouble(hdlg, IDV_FILM_NM_0+index, "%.1f", strtod(aptr, &aptr));
						SetDlgItemDouble(hdlg, IDV_NM_LOW_0+index,  "%.1f", strtod(aptr, &aptr));
						SetDlgItemDouble(hdlg, IDV_NM_HIGH_0+index, "%.1f", strtod(aptr, &aptr));
						while (isspace(*aptr)) aptr++;
						bval = strchr("yY1", *aptr) != NULL;
						SetDlgItemCheck(hdlg, IDC_VARY_0+index, bval);
						EnableDlgItem(hdlg, IDC_FILM_MATERIAL_0+index, TRUE);
						EnableDlgItem(hdlg, IDV_FILM_NM_0+index, TRUE);
						EnableDlgItem(hdlg, IDC_VARY_0+index, TRUE);
						if (bval) {
							EnableDlgItem(hdlg, IDV_NM_LOW_0+index, TRUE);
							EnableDlgItem(hdlg, IDV_NM_HIGH_0+index, TRUE);
						}
					}
				} else if (_strnicmp(aptr, "substrate ", 10) == 0) {
					ComboBoxSetByIntValue(hdlg, IDC_REF_SUBSTRATE, GetMaterialIndex(aptr+10, NULL));
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
		SendMessage(hdlg, WMP_PROCESS_REFERENCE, 0,0);			/* Rebuild the corrections */
		SendMessage(hdlg, WMP_UPDATE_RAW_AXIS_SCALES, 0, 0);	/* Does a redraw */

		SendMessage(hdlg, WMP_PROCESS_MEASUREMENT, 0, 0);		/* And manage the sample */
		SendMessage(hdlg, WMP_UPDATE_MAIN_AXIS_SCALES, 0, 0);	/* Does a redraw */
	}

	return 0;
}

/*
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * ===========================================================================
 * =========================================================================== */

#define	DATABASE	(NULL)

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

	static char *database=NULL;				/* Where material information located */

	/* Figure out which database to use ... but only first time */
	if (database == NULL) {
		size_t cnt;
		char env_name[PATH_MAX];
		struct _stat info;
		if (getenv_s(&cnt, env_name, sizeof(env_name), "tfocDatabase") == 0 && cnt > 0) {	/* Ignore if required size would be more than PATH_MAX */
			database = env_name;
		} else if ( _stat("./tfocDatabase", &info) == 0 && info.st_mode & S_IFDIR ) {
			database = "./tfocDatabase/";
		} else if ( _stat("c:/tfocDatabase", &info) == 0 && info.st_mode & S_IFDIR ) {
			database = "c:/tfocDatabase/";
		} else if ( _stat("c:/database.nk", &info) == 0 && info.st_mode & S_IFDIR ) {		/* Compatibility with earlier versions */
			database = "c:/database.nk/";
		} else {																									/* Better hope materials are in the same directory */
			database = "./";
		}
	}

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
		refl[i] = scaling*TFOC_ReflN(theta, mode, lambda[i], layers).R;
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

	TFOC_GetReflData(info->sample.tfoc, info->fit_parms.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, nls->yfit);

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
		TFOC_GetReflData(info->sample.tfoc, info->fit_parms.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, center);

		for (i=0; i<nls->nvars; i++) {
			v = nls->vars[i];
			tmp = *v;
			if (i != nls->nvars-1) {
				delta = 1.0;								/* Use a 1 nm change so tfoc has a chance (always +) */
			} else {
				delta = 0.01;
			}
			*v +=   delta;
			TFOC_GetReflData(info->sample.tfoc, info->fit_parms.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, fderiv[i]);
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
	nls->vars[j] = &info->fit_parms.scaling;
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

	/* Generate the final fit and copy both to tfoc_fit and cv_fit curve */
	info->tfoc_fit = realloc(info->tfoc_fit, info->npt * sizeof(*info->tfoc_fit));
	TFOC_GetReflData(info->sample.tfoc, info->fit_parms.scaling, 0.0, UNPOLARIZED, 300.0, info->npt, info->lambda, info->tfoc_fit);
	if (info->cv_fit != NULL) {
		for (i=0; i<info->npt; i++) info->cv_fit->y[i] = info->tfoc_fit[i];
		info->cv_fit->modified = TRUE;
	}

	for (i=0,j=0; i<info->sample.layers; i++) {
		if (info->sample.stack[i].vary) {
			info->sample.stack[i].nm    = info->sample.tfoc[i+1].z;		/* layer 0 is air */
			info->sample.stack[i].sigma = nls->sigma[j];
			j++;
		}
	}

	fflush(stdout);
	return rcode;
}
