#define	N_FILM_STACK	(5)				/* Number of layers on the dialog box */

typedef struct _FILM_LAYERS {
	char layer_name[32];						/* name of the layer (layer 1, layer 2, ...) */
	char material[32];						/* material (file in the database) */
	double nm;									/* film thickness (nm) */
	BOOL vary;									/* Only filled in for the sample stack */
	double lower,upper;						/* Values used by TFOC routine */
	double sigma;								/* Returned uncertainty value */
} FILM_LAYERS;

typedef struct _FILM_MEASURE_INFO {
	HWND hdlg;
	BOOL spec_ok;								/* Lasgo client exists and ok */
	char spec_IP[16];							/* IP address in . format */
	SPEC_SPECTROMETER_INFO status;
	int npt;										/* Number of points in spectrum */
	double *lambda;							/* Pointer to list of wavelength values */
	double lambda_transferred;				/* Have lambda values been read from the spectrometer */

	/* Wavelength scales for graph */
	double lambda_min, lambda_max;		/* Min/max for graphing */
	BOOL lambda_autoscale;					/* Are we using autoscale? */

	GRAPH_CURVE *cv_raw,							/* Film data */
					*cv_dark,						/* Dark (background) spectrum */
					*cv_ref,							/* Known film structure specturm */
					*cv_light,						/* Profile of illumination inferred from reference spectrum */
					*cv_refl,						/* Reflectance (calculated from measurements) */
					*cv_fit,							/* Fit reflectance (simulation) */
					*cv_residual;					/* Residual error */
	double *tfoc_reference;						/* TFOC of reference structure */
	double *tfoc_fit;								/* TFOC of sample structure */

	struct {
		FILM_LAYERS stack[N_FILM_STACK+1];	/* Reference stack	*/
		TFOC_SAMPLE *tfoc;						/* TFOC structure for reference stack */
		int layers;									/* # of layers (including substrate) in stack */
	} reference;

	struct {
		FILM_LAYERS stack[N_FILM_STACK+1];	/* Fit stack			*/
		TFOC_SAMPLE *tfoc;						/* TFOC structure for sample film stack */
		int layers;									/* # of layers (including substrate) in stack */
	} sample;
	
	struct {
		double lambda_min, lambda_max;		/* X range (wavelength) for fitting */
		double scaling_min, scaling_max;		/* Scaling min/max (multiplicative) */
		double scaling;							/* Multiplicative scaling for fit */
	} fit_parms;

} FILM_MEASURE_INFO;

#define	WMP_OPEN_SPEC						(WM_APP+1)
#define	WMP_LOAD_SPEC_PARMS				(WM_APP+2)
#define	WMP_LOAD_SPEC_WAVELENGTHS		(WM_APP+3)
#define	WMP_UPDATE_SPEC_PARMS			(WM_APP+4)
#define	WMP_UPDATE_RAW_AXIS_SCALES		(WM_APP+5)
#define	WMP_UPDATE_MAIN_AXIS_SCALES	(WM_APP+6)
#define	WMP_PROCESS_REFERENCE			(WM_APP+7)
#define	WMP_PROCESS_MEASUREMENT			(WM_APP+8)
#define	WMP_CLEAR_SAMPLE_STACK			(WM_APP+9)
#define	WMP_MAKE_SAMPLE_STACK			(WM_APP+10)
#define	WMP_CLEAR_REFERENCE_STACK		(WM_APP+11)
#define	WMP_MAKE_REFERENCE_STACK		(WM_APP+12)

#define	ID_NULL			(-1)

