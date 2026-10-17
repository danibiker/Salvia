/*
 * xbox/compat_posix.c  --  las cuatro funciones de libogc/POSIX que usa
 *                          `source/main/timers.c`.
 *
 * timers.c es el unico fichero de la fase 1 que necesita reloj, y necesita
 * exactamente esto:
 *
 *     gettick()               ogc/lwp_watchdog.h
 *     ticks_to_microsecs()    idem
 *     usleep()                declarada a mano en timers.c:60
 *
 * (`ticks_to_millisecs` no la usa ninguna linea viva, pero la cabecera la
 * declara y cuesta cuatro lineas; se deja para que el shim sea coherente.)
 *
 * ★ [Fase 6] Y una quinta, que ya no es de timers.c:
 *
 *     gettimeofday()          xenon/gxshim/sys/time.h
 *
 * La pide Rice_GX/RSP_Parser.cpp una vez por display list.  Vive aqui y no
 * en un fichero propio porque comparte el `QueryPerformanceFrequency`
 * cacheado de abajo: dos copias del cacheo serian dos verdades sobre la
 * misma frecuencia.  `round`/`roundf` estan por lo mismo.
 *
 * ★ LA CONVERSION DE TICKS.  En Xbox 360 `mftb` avanza a 50 MHz, no a la
 * frecuencia del bus de la Wii.  Aqui no se cablea ninguna constante: se
 * pregunta al sistema con QueryPerformanceFrequency, que en esta consola
 * devuelve precisamente la frecuencia del timebase.  Cablear 50000000
 * funcionaria hoy y seria una mentira esperando a que alguien la creyera.
 *
 * ★ Y SE HACE EN 64 BITS A PROPOSITO.  `ticks * 1000000` con 32 bits se
 * desborda a los ~86 segundos.  timers.c mete el resultado en un DWORD y
 * resta, asi que el envoltorio le da igual -- pero el desbordamiento
 * INTERMEDIO no: daria saltos de microsegundos en mitad de la division.
 *
 * C89.
 */

#include <xtl.h>
#include <math.h>       /* floor / ceil, para round()            */
#include <sys/time.h>   /* struct timeval y la firma exacta de   */
                        /* gettimeofday.  Es xenon/gxshim/sys/   */
                        /* time.h: el XDK no la trae.            */

static LONGLONG s_frecuencia = 0;

static LONGLONG frecuencia(void)
{
	LARGE_INTEGER f;

	if (s_frecuencia == 0)
	{
		if (QueryPerformanceFrequency(&f) && f.QuadPart)
			s_frecuencia = f.QuadPart;
		else
			s_frecuencia = 50000000;   /* respaldo: timebase del Xenon */
	}
	return s_frecuencia;
}

unsigned long long gettick(void)
{
	LARGE_INTEGER t;

	QueryPerformanceCounter(&t);
	return (unsigned long long)t.QuadPart;
}

unsigned long long ticks_to_microsecs(unsigned long long ticks)
{
	return (ticks * 1000000ULL) / (unsigned long long)frecuencia();
}

unsigned long long ticks_to_millisecs(unsigned long long ticks)
{
	return (ticks * 1000ULL) / (unsigned long long)frecuencia();
}

/* ★ [Fase 6] La pide Rice_GX/RSP_Parser.cpp en dos sitios SIN guarda de
 * __GX__ (lineas 977-979 y 1797-1799), asi que esta en el camino caliente:
 * una vez por display list.  De todo lo que devuelve usa `tv_usec`, y solo
 * para restar dos lecturas (`status.gRDPTime`).
 *
 * El porque de cada decision esta en xenon/gxshim/sys/time.h; aqui solo lo
 * que no se ve desde la cabecera:
 *
 * ★ LA DIVISION ES ENTERA Y EN 64 BITS, y ese orden importa.  `t / f` da los
 * segundos y el RESTO son los ticks sueltos: multiplicar ESE resto por un
 * millon no puede desbordar, porque es menor que la frecuencia.  Hacerlo al
 * reves (`t * 1000000 / f`) desborda el u64 a las ~100 horas de encendido, y
 * el sintoma seria un gRDPTime que da un salto y no vuelve.
 *
 * ★ Y NO SE LLAMA A `frecuencia()` DOS VECES aunque se use dos veces: se
 * copia a local.  No es microoptimizacion, es que entre las dos llamadas no
 * puede cambiar el valor y dejar `seg` y `usec` describiendo relojes
 * distintos. */
int gettimeofday(struct timeval *tv, void *tz)
{
	LARGE_INTEGER t;
	unsigned long long f, ticks;

	(void)tz;   /* la zona horaria de POSIX: Rice pasa 0 en los dos sitios */

	if (!tv)
		return -1;

	f = (unsigned long long)frecuencia();
	QueryPerformanceCounter(&t);
	ticks = (unsigned long long)t.QuadPart;

	tv->tv_sec  = (long)(ticks / f);
	tv->tv_usec = (long)(((ticks % f) * 1000000ULL) / f);
	return 0;
}

/* ★ timers.c la declara asi, con `unsigned int` de entrada y de salida
 * (timers.c:60).  La firma tiene que coincidir o no enlaza.
 *
 * ★ Y EN LA FASE 1 NO DEBERIA EJECUTARSE NUNCA: xbox64_init pone
 * `Timers.limitVIs = LIMITVIS_NONE`, y el unico `usleep` de timers.c esta
 * dentro de `if (Timers.limitVIs)`.  El ritmo lo lleva el frontend; si el
 * core tambien durmiera, se frenaria dos veces.  Se implementa de verdad
 * -- y no como no-op -- para que, si alguien enciende el limitador, haga
 * lo que dice. */
unsigned int usleep(unsigned int us)
{
	if (us >= 1000)
		Sleep(us / 1000);
	return 0;
}

/* ====================================================================== */
/* [Fase 3] round / roundf: C99, y VS2010 no las trae                     */
/* ====================================================================== */
/* Las usa el dynarec en ROUND_L_FP / ROUND_W_FP (MIPS-to-PPC.c), y no como
 * llamada normal: TOMA SU DIRECCION para emitir un `bl` desde el codigo
 * recompilado.  Por eso son funciones de verdad y no macros, al contrario
 * que isnan o localtime_r.
 *
 * ★ Y NO SE ESCRIBEN COMO `floor(x + 0.5)`, QUE ES EL ATAJO DE SIEMPRE Y
 * ESTA MAL.  Con x = 0.49999999999999994 la suma REDONDEA a 1.0 antes de
 * que floor la vea, y el resultado sale 1 en vez de 0.  Aqui importa:
 * `ROUND.L.D` del R4300 tiene que dar lo mismo que daba libgcc en Wii, y
 * una diferencia de una unidad en el ultimo bit de una conversion de coma
 * flotante a entero es de las que se manifiestan como un objeto colocado un
 * pixel mas alla, tres horas de juego mas tarde.
 *
 * La forma de abajo compara la PARTE FRACCIONARIA, que es exacta.
 * Semantica de C99: redondeo al mas cercano, y los empates ALEJANDOSE DEL
 * CERO (round(-0.5) == -1.0, no 0.0).
 *
 * NaN e infinitos salen solos: floor/ceil los propagan y las comparaciones
 * con NaN son falsas. */
double round(double x)
{
	double t;

	if (x >= 0.0)
	{
		t = floor(x);
		if (x - t >= 0.5) t += 1.0;
	}
	else
	{
		t = ceil(x);
		if (t - x >= 0.5) t -= 1.0;
	}
	return t;
}

float roundf(float x)
{
	return (float)round((double)x);
}
