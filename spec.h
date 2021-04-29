#include "graph.h"

#ifndef SPEC_KERNAL
	typedef struct _SPEC_WND_INFO SPEC_WND_INFO;
#else
	typedef struct _SPEC_WND_INFO {
		HWND hdlg;									/* handle of the spectrometer dialog box */
		BOOL spec_ok;								/* Spectrometer exists and ok */
		SPECTROMETER *spec;
		
		/* Cursor hit ... display the values at that wavelength continuously */
		BOOL show_values;							/* Should we display values of a wavelength */
		double lambda_show;						/* Wavelength for display */

		BOOL piUSB_ok;								/* Is the shutter active and available */
		int piUSB_Serial;							/* Serial number for access to the USB shutter */
		void *piUSB;								/* Handle to the piUSB shutter */

		int target;									/* Target counts for autoscale */
		double ms_integrate;						/* Integration target */
		int num_average;							/* Averaging specified */
		int bFlags;									/* Flags for dark_pixel and nl_correct */
														/* use_dark_pixel = (bFlags & bSPEC_DARK_CORRECT) */
														/* use_nl_correct = (bFlags & bSPEC_NONLINEAR_CORRECT) */
		BOOL AutoCapture;							/* If true, autocatpure spectra via timer */
		double lambda_min, lambda_max;		/* Min and max wavelength range for auto graph */
		int counts_max;							/* Count range when in manual scaling */
		char current_acq[32];					/* What spectrum are we currently acquiring? */
		double maximize_ref_sum;				/* 50% point for signal maximizing progress bar */

		GRAPH_CURVE *cv_raw;						/* Raw (with potential dark pixel and non-linear corrections */
		GRAPH_CURVE *cv_dark;					/* Dark baseline spectrum */
		GRAPH_CURVE *cv_reference;				/* Reference spectrum */
		GRAPH_CURVE *cv_test;					/* Test spectrum */
		GRAPH_CURVE *cv_dark_corrected;		/* Dark corrected raw spectrum */
		GRAPH_CURVE *cv_normalized;			/* Normalized reflectance (raw-dark)/(ref-dark) */
		GRAPH_CURVE *cv_absorbance;			/* Log scale absorbance of normalized ratio */
		GRAPH_SCALES scales;

		HANDLE DfltAcqComplete;					/* Event indicating that a new spectra (default acquisition) has been acquired */
		HANDLE DarkAcqComplete;					/* Event indicating that a new dark spectra has been acquired */
		HANDLE RefAcqComplete;					/* Event indicating that a new reference spectra has been acquired */
		HANDLE TestAcqComplete;					/* Event indicating that a new reference spectra has been acquired */

	} SPEC_WND_INFO;

/* Common routines, or in one and needed by other */
	int SaveSpectra(HWND hdlg, int bSaveFlag, GRAPH_CURVE *cv_data, SPEC_WND_INFO *info, SPECTROMETER *spec, char *dir, char *name);
#endif

HWND spec_hdlg;								/* Spectrometer window */
SPEC_WND_INFO *Spec_Info;					/* Global copy if someone understands everything */

typedef struct _SPEC_STATUS {
	/* Elements from the spectrometer structure itself */
	BOOL spec_ok;								/* Spectrometer exists and ok */
	void *spec;									/* SPECTROMETER structure (cast as void *) */
	char model[32];
	char serial[32];
	int npoints;								/* Number of points in a spectrum */
	double lambda_min, lambda_max;		/* Min and max wavelength of spectrometer */
	double *wavelength;						/* Pointer to array of wavelengths */
	double *raw,*dark,*reference,*test;	/* Pointer to raw formatted saved spectra counts */
	/* Elements from the dialog box */
	double ms_integrate;						/* Integration target */
	int num_average;							/* Averaging specified */
	BOOL use_dark_pixel;
	BOOL use_nl_correct;
	BOOL auto_capture;						/* If true, autocatpure spectra via timer */
} SPEC_STATUS;

void Spec_Start_Dialog(void *arglist);
int  Spec_Status(SPEC_STATUS *status);
int Spec_GetAvgSpectrum(double *buffer, int length);
int Spec_WriteParameters(char *pre_text, FILE *funit);
BOOL Spec_AutoCapture(BOOL flag);
void Spec_SetScanInfo(char *info);
double Spec_QueryIntegrationTime(void);
int Spec_SetIntegrationParms(double ms, int iavg, BOOL bDark, BOOL bNL);

#define	SPEC_WRITE_WAVELENGTH	(0x01)
#define	SPEC_WRITE_RAW				(0x02)
#define	SPEC_WRITE_DARK			(0x04)
#define	SPEC_WRITE_REFERENCE		(0x08)
#define	SPEC_WRITE_DARK_CORRECT	(0x10)
#define	SPEC_WRITE_NORMALIZED	(0x20)
#define	SPEC_WRITE_ABSORBANCE	(0x40)
int Spec_SaveSpectra(HWND hdlg, int bSaveFlag, GRAPH_CURVE *cv_data, char *dir, char *fname);
