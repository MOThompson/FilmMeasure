#define NPARMS_DOPING	(3)

/* Include file for subroutine use of TFOC */
typedef enum _POLARIZATION {TE=0, TM=1, UNPOLARIZED=2} POLARIZATION;
typedef enum _EMA_MODEL {SERIES, PARALLEL, BRUGGEMAN, MAXWELL_GARNETT, LOOYENGA} EMA_MODEL;

/* Reflection calculations */
typedef struct _REFL {
	double R,T;							/* Reflectivity and transmission into substate */
} REFL;

/* Ultimate reflectivity data */
REFL TFOC_GetRefl(char *SampleFile, double lambda, double theta, POLARIZATION mode, double temperature);

/* Free_carrier optical absorption routines */
/* Lambda for these are in um rather than nm -- be careful */
typedef enum _FC_MODE {FIXED_K, SIMPLE_MU, SPLINE_MU, KLAASSEN_MU} FC_MODE;
#if 0
	static char *FC_MODE_List[] = {"Fixed constants", "Simple mobility", "Spline mobility", "Klaasen model"}; */
#endif
void   fc_set_mstar_mode(int mode);
double fc_alpha(double doping, double excess, double T, double lambda, FC_MODE mode);
double fc_k(double doping, double excess, double T, double lambda, FC_MODE mode);

typedef struct _COMPLEX {
	double x,y;
} COMPLEX;

/* Structures where I want to keep the details unknown */
typedef struct _TFOC_MATERIAL			TFOC_MATERIAL;
typedef struct _TFOC_MATERIAL_MIX	TFOC_MATERIAL_MIX;

typedef enum _TFOC_LAYER_TYPE {INCIDENT, SUBLAYER, SUBSTRATE, IGNORE_LAYER, EOS} TFOC_LAYER_TYPE;

#define	MATERIAL_NAME_LENGTH	(256)
#define	MAX_MIX_TERMS			(10)

typedef struct _TFOC_SAMPLE {
	TFOC_LAYER_TYPE type;
	char name[MATERIAL_NAME_LENGTH];		/* Material name						*/

	enum {NO_DOPING, CONSTANT, LINEAR, EXPONENTIAL, LINEAR_IMPLANT} doping_profile;
	int    doping_layers;					/* Number of sub-layers				*/
	double doping_parms[NPARMS_DOPING];	/* Doping parameters (profile)	*/
	double temperature;						/* Layer temperature (non-uniform) */
	double z;									/* Thickness in nm					*/
	COMPLEX n;									/* Default n,k for material		*/
	TFOC_MATERIAL *material;				/* Source of raw data				*/
} TFOC_SAMPLE;

typedef struct _TFOC_LAYER {
	TFOC_LAYER_TYPE type;
	double z;									/* Thickness							*/
	COMPLEX n;									/* Current n,k value (doped)		*/
	double doping;
	int layer;
} TFOC_LAYER;

/* Sample interpretation and layer expansion */
double cpmax, cnmax;							/* Maximum activated concentrations n and p */
TFOC_SAMPLE *TFOC_LoadSample(char *fname);
void TFOC_MakeLayers(TFOC_SAMPLE *sample, TFOC_LAYER *layers, double T, double lambda);
double get_nm_value(char *aptr, char **endptr, double dflt);

/* Materials Database routines and global constants */
int TFOC_GetMaterialName(char *str, char *name, size_t namelen, char **endptr);
TFOC_MATERIAL *TFOC_FindMaterial(char *name, char *database);
COMPLEX TFOC_FindNK(TFOC_MATERIAL *material, double lambda);
void TFOC_PrintMaterials(void);
void TFOC_PrintDetail(TFOC_SAMPLE *sample, TFOC_LAYER *layers);

REFL TFOC_ReflN(double theta, POLARIZATION mode, double lambda, TFOC_LAYER layer[]);
REFL TFOC_Refl(double theta, POLARIZATION mode, double lambda, COMPLEX n0, COMPLEX n1, COMPLEX ns, double z);


#ifdef TFOC_CODE
	#define	pi				(3.141592653589793)				/* Guess	*/
	#define	TRUE			(1)
	#define	FALSE			(0)

	typedef int BOOL;
	typedef int INT;
	typedef int BOOL;
	typedef double REAL;

	typedef double TMPREAL;

/* Spline routines */
	void *GVFitSpline(void *work, REAL *x, REAL *y, int npt, int opts);
	REAL GVEvalSpline(void *work, REAL x);

/* Complex mathematical operations (from Fresnel) */
	COMPLEX CADD(COMPLEX a, COMPLEX b);
	COMPLEX CSUB(COMPLEX a, COMPLEX b);
	COMPLEX CMUL(COMPLEX a, COMPLEX b);
	COMPLEX CDIV(COMPLEX a, COMPLEX b);
	COMPLEX CCSQRT(COMPLEX r);
	double CABS(COMPLEX a);
	COMPLEX CPOW(COMPLEX r, double pow);
	COMPLEX CSQRT(double r);

#endif
