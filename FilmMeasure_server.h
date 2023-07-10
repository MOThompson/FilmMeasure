int Init_FilmMeasure_Server(void);
int Shutdown_FilmMeasure_Server(void);

/* These routines are in filmmeasure.c, not filmmeasure_server.c */
int FilmMeasure_Do_Measure(void);
int FilmMeasure_Save_Data(char *path);
int FilmMeasure_Query_Fit_Parms(int *nvars, double *vars, int max_vars);

#define	FILM_SERVER_WAIT	(30000)						/* 30 second time-out */
