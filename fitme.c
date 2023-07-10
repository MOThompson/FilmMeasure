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
static double (*my_fnc)(double var) = Angle_2_Watts_n2;

static int nls_eval(NLS_DATA *nls) {
	int i;
	for (i=0; i<nls->npt; i++) nls->yfit[i] = (*my_fnc)(nls->xy[0][i]);
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
#define	DELTA_FRAC	(1E-3)
#define	DELTA_ZERO	(1E-3)

static int nls_deriv(double *results, NLS_DATA *nls, int ipt) {

	int i;
	double x, tmp, delta, *v, y0,y1;

	x = nls->xy[0][ipt];											/* X value for argument */

	for (i=0; i<nls->nvars; i++) {
		v = nls->vars[i];
		tmp = *v;
		delta = (*v == 0) ? DELTA_ZERO : (*v)*DELTA_FRAC;
		*v +=   delta;		y1 = (*my_fnc)(x);
		*v -= 2*delta;		y0 = (*my_fnc)(x);
		results[i] = (y1-y0)/(2*delta);
		*v = tmp;
	}
	return 0;
}


/* ===========================================================================
	--- Do fit
	=========================================================================== */
#define	MAXITER	(20)							/* Max iterations to find solution */

static int do_fit(int mode) {

	/* Local variables */
	NLS_DATA	nls;									/* Structure passed to NLSFIT	*/
	int		i,j,k, iter;						/* Random integer constants	*/
	char		token[256];
	int		rcode=0;
	double	*xy[3];								/* Array for the dependent vars */
	char *mode_0_names[3] = { "phi_0", "pmin", "pmax" };
	char *mode_1_names[3] = { "phi_0", "pmin", "pmax" };
	char *mode_2_names[3] = { "threshold", "slope", "quad" };
	char **var_names;

	/* Set up the correct listing of names */
	switch (mode) {
		case 0:
			var_names = mode_0_names;
			my_fnc = Angle_2_Watts_n2;
			break;
		case 1:
			var_names = mode_1_names;
			my_fnc = Angle_2_Watts_n4;
			break;
		case 2:
			var_names = mode_2_names;
			my_fnc = Current_2_Watts;
			break;
		default:
			MessageBox(HWND_DESKTOP, "BIG ERROR: unknown mode to do_fit()", "Calibration fitting", MB_ICONERROR | MB_OK);
			return -1;
	}

	/* Clear and set the parameter structure */
	memset(&nls, sizeof(nls), 0);				/* Blank it out completely */

	/* Initialize the data structure to nlsfit() now */
	nls.errorbar  = NULL;				/* No error bars							*/
	nls.yfit      = NULL;				/* Let fit routines allocate space	*/
	nls.outchi    = NULL;				/* Let fit allocate space if needed	*/
	nls.correlate = NULL;				/* No correlation matrix wanted		*/
	nls.workspace = NULL;				/* Let fit allocate space if needed	*/
	nls.magic_cookie = 0;				

	nls.data     = data->y;
	nls.npt      = data->npt;
	xy[0] = data->x;
	xy[1] = data->y;
	xy[2] = data->s;
	nls.xy       = xy;

	nls.flamda   = 0;						/* Let CurveFit() set initial value	*/
	nls.EpsCrit  = 1E-4;					/* CurveFit() now does completion test */

	nls.evalfnc   = nls_eval;			/* Functions to evaluate function	*/
	nls.fderiv    = nls_deriv;			/* Functions to evaluate derivative	*/
	nls.evalchi   = NULL;				/* Use default chisqr evaluation		*/

	nls.nvars    = 3;
	nls.vars      = calloc(nls.nvars,	sizeof(*nls.vars));
	nls.sigma     = calloc(nls.nvars,	sizeof(*nls.sigma));
	nls.correlate = NULL;
	nls.lower     = NULL;
	nls.upper     = NULL;

	nls.vars[0] = parms+0;					/*  Link the variables to vary */
	nls.vars[1] = parms+1;
	nls.vars[2] = parms+2;

	/* Limit points that are negative or zero (bad data collection) */
	nls.valid = calloc(data->npt, sizeof(*nls.valid));		/* Just allocate on the fly */
	for (i=0; i<data->npt; i++) nls.valid[i] = (data->y[i] > 0.0);

	/* Initialize everything else in CurveFit routine */
	if ( (rcode = CurveFit(NKEY_INIT, 0, &nls)) != 0) {
		printf("ERROR: Error on initialization of routine\n"); fflush(stdout);
		goto FitExit;
	}

	/* And we are off and running */
	fputs("------------------------------------------------------------------------------\n", stdout);
	strcpy(token, "    CHISQR ");
	for (i=0; i<nls.nvars;) {
		fputs(token, stdout);
		for (j=0; j<6 && i<nls.nvars; j++) printf("%11s", var_names[i++]);
		fputs("\n", stdout);
		strcpy(token, "           ");
	}
	fputs("------------------------------------------------------------------------------\n", stdout);

	/* Set key to be either silent or verbose on fitting */
	rcode = 0;
	for (iter=0; iter<MAXITER; iter++) {			/* Number of reps allowed */
		printf("\r%11.4g", nls.chisqr);
		for (j=0; j<nls.nvars; ) {
			for (k=0; k<6 && j<nls.nvars; k++) printf("%11.4g", *nls.vars[j++]);
			fputs("\n", stdout);
			if (j != nls.nvars) fputs("           ", stdout);
		}
		fflush(stdout);

		if (nls.chisqr <= 0 || rcode == 1) break;		/* Basically success! */
		if ( (rcode = CurveFit(NKEY_TRY_VERBOSE, iter, &nls)) < 0) goto FitExit;		/* Run again */
	}
	if (rcode == 0 && iter >= MAXITER) rcode = 2;	/* Run out of time? */

	/* Print results */
	fputs( "\n"
			 "    Variable                Value               Sigma\n"
			 "    --------                -----               -----\n", stdout);
	/*				"    123456789012345  12345.1234567     123456.1234567 */
	for (i=0; i<nls.nvars; i++) {
		printf("     %-15s  %13.7g     %14.7g\n", var_names[i], *nls.vars[i], nls.sigma[i]);
	}
	fputs("\n", stdout);

	printf("     Degrees of Freedom: %d\n", nls.dof);
	printf("     Root Mean Variance: %g\n", sqrt(nls.chisqr));
	printf("     Estimated Y sigma:  %g\n", nls.sigmaest);
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
	CurveFit(NKEY_EXIT, 0, &nls);					/* Free allocated workspaces	*/
	free(nls.yfit);									/* My responsibility to free	*/
	free(nls.errorbar);								/* Free my allocated spaces	*/
	free(nls.outchi);
	free(nls.vars);
	for (i=0; i<3; i++) sigma[i] = nls.sigma[i];
	free(nls.sigma);
	free(nls.correlate);
	free(nls.lower);
	free(nls.upper);
	free(nls.valid);									/* No longer NULL */

	fflush(stdout);
	return rcode;
}


