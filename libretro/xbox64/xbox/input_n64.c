/*
 * xbox/input_n64.c  --  los mandos de N64 desde libretro.
 *
 * Sustituye a los cinco puntos de entrada vacios que la fase 1 tenia en
 * plugins_fase1.c.  El reparto es a proposito: aqui vive lo que sabe de
 * N64 (el mapa de botones, el rango del stick) y en libretro_core.cpp lo
 * que sabe de libretro (los IDs, los descriptores).  Se tocan en una sola
 * funcion, `xbox64_retro_input_state`.
 *
 * C89.
 */

#include "input_n64.h"

#include "plataforma.h"                   /* xbox64_n_keys */

#include "../source/main/winlnxdefs.h"
#include "../source/main/plugin.h"        /* CONTROL Controls[4], BUTTONS */
#include "../source/gui/DEBUG.h"
#include "../source/xenon64_diag.h"       /* XBOX64_TRAZAS */

#include <math.h>                         /* sqrt: la zona muerta radial */

/* Los IDs de libretro.  ★ NO se incluye libretro.h: es una cabecera de
 * C++ en este arbol (la usa el wrapper) y arrastra tipos que aqui no
 * hacen falta.  Son cuatro constantes estables del API -- el valor de
 * RETRO_DEVICE_ID_JOYPAD_* no ha cambiado nunca -- y se comprueban contra
 * la cabecera de verdad con una guarda en el wrapper. */
#define RD_JOYPAD_B         0
#define RD_JOYPAD_Y         1
#define RD_JOYPAD_SELECT    2
#define RD_JOYPAD_START     3
#define RD_JOYPAD_UP        4
#define RD_JOYPAD_DOWN      5
#define RD_JOYPAD_LEFT      6
#define RD_JOYPAD_RIGHT     7
#define RD_JOYPAD_A         8
#define RD_JOYPAD_X         9
#define RD_JOYPAD_L        10
#define RD_JOYPAD_R        11
#define RD_JOYPAD_L2       12
#define RD_JOYPAD_R2       13

#define RD_DEVICE_NONE      0
#define RD_DEVICE_JOYPAD    1
#define RD_DEVICE_ANALOG    5

#define RD_INDEX_LEFT       0
#define RD_INDEX_RIGHT      1
#define RD_ID_ANALOG_X      0
#define RD_ID_ANALOG_Y      1

/* ====================================================================== */
/* EL RANGO DEL STICK                                                     */
/* ====================================================================== */
/* ★★ 80, NO 127.  `BUTTONS` guarda los ejes en `signed :8`, asi que caben
 * -128..127, pero el mando de N64 REAL no llega ahi: satura alrededor de
 * +-80, y los juegos estan calibrados contra eso.  Con 127 el personaje
 * corre a tope antes de que el stick llegue al borde y los juegos con
 * zonas de velocidad (andar / trotar / correr) pierden los tramos.
 *
 * libretro entrega +-32767 SIMETRICO -- medido en este frontend, no
 * supuesto -- asi que la conversion es una regla de tres y un recorte. */
#define N64_EJE_MAX  80

/* ====================================================================== */
/* LA FOTO                                                                */
/* ====================================================================== */
/* ★ UNA VEZ POR FRAME, y no una consulta al frontend por cada `getKeys`.
 *
 * `pif.c` llama a `getKeys` cuando al invitado le apetece: varias veces
 * por frame en unos juegos, dentro de una interrupcion en otros.  Si cada
 * una preguntase al frontend, el mismo frame podria ver el boton pulsado
 * y suelto, y eso son entradas fantasma imposibles de reproducir.  Con la
 * foto, todas las lecturas de un frame ven lo mismo. */
static BUTTONS s_foto[4];
static int     s_enchufado[4];

/* ====================================================================== */

static int leer(unsigned puerto, unsigned id)
{
	return xbox64_retro_input_state(puerto, RD_DEVICE_JOYPAD, 0, id) ? 1 : 0;
}

static int eje(unsigned puerto, unsigned indice, unsigned id)
{
	int v = xbox64_retro_input_state(puerto, RD_DEVICE_ANALOG, indice, id);
	long escalado;

	/* La regla de tres, en long para que el producto no se salga de 32
	 * bits: 32767 * 80 = 2.621.360, cabe de sobra, pero el intermedio en
	 * `int` de 16 bits no estaria garantizado por C89. */
	escalado = ((long)v * N64_EJE_MAX) / 32767L;

	if (escalado >  N64_EJE_MAX) escalado =  N64_EJE_MAX;
	if (escalado < -N64_EJE_MAX) escalado = -N64_EJE_MAX;
	return (int)escalado;
}

/* ====================================================================== */
/* LA ZONA MUERTA DEL STICK                                               */
/* ====================================================================== */
/* [2026-10-27] El stick llegaba al juego tal cual, sin zona muerta: la que
 * ajusta el menu de Salvia solo cuenta cuando un eje se convierte en BOTON
 * (io/joystick.cpp), no en el valor analogico que reciben los cores.  Con el
 * juego leyendo +-80 desde el primer milimetro, el principio del recorrido
 * era demasiado sensible.
 *
 * Copiado de mupen64plus-next (emulate_game_controller_via_libretro.c,
 * inputGetKeys_reuse, y su opcion astick-deadzone, 15 por defecto): zona
 * RADIAL -- sobre la longitud del vector, no eje a eje, para que las
 * diagonales no se peguen a los ejes -- y REESCALADA: pasada la zona, el
 * recorrido que queda vuelve a ocupar 0..80, asi que los movimientos lentos
 * siguen siendo posibles.  La referencia no acota a 80 (en diagonal llega a
 * ~84 con zona); aqui tampoco, solo al rango del campo de 8 bits. */
#define N64_STICK_MAX 32768.0   /* ASTICK_MAX de la referencia */

static int s_zona = (15 * 32768) / 100;   /* en unidades de libretro */

void xbox64_input_set_zona_muerta(int por_ciento)
{
	/* Hasta 90: con 100 el recorrido que queda seria 0 y se dividiria por 0
	 * (la opcion solo llega a 30, pero el valor viene de fuera). */
	if (por_ciento < 0)  por_ciento = 0;
	if (por_ciento > 90) por_ciento = 90;
	s_zona = (por_ciento * 32768) / 100;
}

/* [2026-10-27] LA CURVA DE RESPUESTA (peticion del usuario: con el recorrido
 * lineal, en algunos juegos poco movimiento del stick ya los lleva al
 * extremo).  Se aplica a `t`, la fraccion del recorrido que queda pasada la
 * zona muerta (0 en su borde, 1 con el stick a fondo en un eje), y todas las
 * curvas dan 1 en 1: a fondo se llega al mismo maximo.  Por encima de 1 (las
 * diagonales, como en la referencia) sigue lineal desde ahi, para no
 * disparar el exceso.  No es de mupen64plus-next: alli solo hay sensibilidad. */
static int s_curva = 0;   /* 0 lineal, 1 t^1.5, 2 t^2, 3 t^3 */

void xbox64_input_set_curva(int modo)
{
	s_curva = (modo < 0 || modo > 3) ? 0 : modo;
}

static double aplica_curva(double t)
{
	const double tc = (t > 1.0) ? 1.0 : t;
	double c;
	switch (s_curva)
	{
		case 1:  c = tc * sqrt(tc);  break;
		case 2:  c = tc * tc;        break;
		case 3:  c = tc * tc * tc;   break;
		default: c = tc;             break;
	}
	return (t > 1.0) ? c * t : c;
}

static int redondea(double v)
{
	return (int)((v < 0.0) ? (v - 0.5) : (v + 0.5));
}

static void stick_izquierdo(unsigned puerto, int *x, int *y)
{
	const double ax = (double)xbox64_retro_input_state(puerto, RD_DEVICE_ANALOG,
	                                                   RD_INDEX_LEFT, RD_ID_ANALOG_X);
	const double ay = (double)xbox64_retro_input_state(puerto, RD_DEVICE_ANALOG,
	                                                   RD_INDEX_LEFT, RD_ID_ANALOG_Y);
	const double r  = sqrt(ax * ax + ay * ay);
	double f;
	int vx, vy;

	if (r <= (double)s_zona || r <= 0.0)
	{
		*x = 0;
		*y = 0;
		return;
	}
	/* Radio reescalado (t: 0..1 pasada la zona), con la curva, llevado a
	 * +-80 y repartido entre los dos ejes en la misma direccion (lo que la
	 * referencia hace con cos/sin del angulo).  Con la curva lineal es la
	 * formula de la referencia tal cual. */
	f = aplica_curva((r - (double)s_zona) / (N64_STICK_MAX - (double)s_zona))
	    * (double)N64_EJE_MAX / r;
	vx = redondea(ax * f);
	vy = redondea(ay * f);
	/* Simetrico, +-127: la Y se niega despues y -128 no tiene opuesto en 8 bits. */
	if (vx >  127) vx =  127;
	if (vx < -127) vx = -127;
	if (vy >  127) vy =  127;
	if (vy < -127) vy = -127;
	*x = vx;
	*y = vy;
}

static void foto_de(unsigned puerto)
{
	BUTTONS b;
	int cx, cy;

	/* ★★★ HAY **DOS** `BUTTONS` DISTINTOS EN ESTE ARBOL, Y SON ESPEJOS.
	 *
	 *   main/plugin.h:194          A_BUTTON primero ... X_AXIS, Y_AXIS
	 *   main/Controller_#1.1.h:53  R_DPAD   primero ... Y_AXIS, X_AXIS
	 *
	 * No es que uno este mal: son la MISMA disposicion del mando de N64
	 * escrita para los dos endianness.  Un compilador big-endian reparte
	 * los campos de bits empezando por el bit MAS significativo, asi que
	 * con `plugin.h` A_BUTTON cae en el bit 31, B en el 30, la cruceta en
	 * 27..24 y los ejes en 15..8 y 7..0 -- que es exactamente donde el
	 * N64 los espera.  Con la otra cabecera saldria del reves.
	 *
	 * ★ ESTE FICHERO USA `plugin.h`, Y TIENE QUE SER ESA: es la que
	 * incluye `pif.c` (pif.c:46), o sea el consumidor que escribe
	 * `Keys.Value` en el bufer del PIF.  Comprobado ademas que NADIE en
	 * el arbol incluye `Controller_#1.1.h`: es una cabecera de
	 * referencia, muerta.
	 *
	 * ★ Y AQUI ESTA EL FILO: los nombres de los campos son los MISMOS en
	 * las dos.  Si alguien cambia el include, esto sigue compilando sin
	 * un aviso y cada boton se va a otro bit.  Por eso se rellena SIEMPRE
	 * por nombre de campo y nunca componiendo `Value` a mano: asi el
	 * codigo no depende de un orden, solo de que la cabecera sea la que
	 * usa `pif.c`. */
	b.Value = 0;

	if (!s_enchufado[puerto])
	{
		s_foto[puerto] = b;
		return;
	}

	/* --- la cruceta, tal cual ------------------------------------- */
	b.U_DPAD = leer(puerto, RD_JOYPAD_UP);
	b.D_DPAD = leer(puerto, RD_JOYPAD_DOWN);
	b.L_DPAD = leer(puerto, RD_JOYPAD_LEFT);
	b.R_DPAD = leer(puerto, RD_JOYPAD_RIGHT);

	/* --- los botones ----------------------------------------------- */
	/* ★ N64 A en el boton A del mando y N64 B en el B (peticion del usuario,
	 * 2026-10-15).  Salvia lleva cada boton de Xbox al del RetroPad con su
	 * MISMO NOMBRE (A->A, B->B, Y->Y), asi que con la disposicion anterior
	 * de los cores de N64 (RetroPad B / RetroPad Y) salian en B e Y del mando
	 * de Xbox.  Los descriptores (libretro_core.cpp) ya decian A y B en estos
	 * dos.  Salvia puede remapear encima. */
	b.A_BUTTON     = leer(puerto, RD_JOYPAD_A);
	b.B_BUTTON     = leer(puerto, RD_JOYPAD_B);
	b.START_BUTTON = leer(puerto, RD_JOYPAD_START);
	b.Z_TRIG       = leer(puerto, RD_JOYPAD_L2);
	b.L_TRIG       = leer(puerto, RD_JOYPAD_L);
	b.R_TRIG       = leer(puerto, RD_JOYPAD_R);

	/* --- el stick --------------------------------------------------- */
	/* ★ LA Y VA INVERTIDA.  En libretro el eje Y crece HACIA ABAJO; en el
	 * mando de N64, `Y_AXIS` crece HACIA ARRIBA.  Sin el signo, el
	 * personaje anda al reves y parece un problema del juego.
	 * Con la zona muerta radial: ver "LA ZONA MUERTA DEL STICK". */
	{
		int sx, sy;
		stick_izquierdo(puerto, &sx, &sy);
		b.X_AXIS =  sx;
		b.Y_AXIS = -sy;
	}

	/* --- los botones C ---------------------------------------------- */
	/* ★ VAN EN EL STICK DERECHO, y solo ahi.
	 *
	 * Los cuatro C son direccionales y el stick derecho es lo unico que
	 * tiene esa forma en un mando de Xbox 360.  Repartirlos ademas por
	 * los botones de cara obligaria a elegir cuatro, y solo quedan tres
	 * libres (A, X, SELECT): saldria un reparto asimetrico que hay que
	 * memorizar.  Si hacen falta en botones, es remapeo del frontend, que
	 * es donde el usuario ya los tiene.
	 *
	 * El umbral es la mitad del recorrido.  Como aqui ya esta escalado a
	 * +-80, la mitad son 40. */
	cx = eje(puerto, RD_INDEX_RIGHT, RD_ID_ANALOG_X);
	cy = eje(puerto, RD_INDEX_RIGHT, RD_ID_ANALOG_Y);

	b.R_CBUTTON = (cx >  N64_EJE_MAX / 2) ? 1 : 0;
	b.L_CBUTTON = (cx < -N64_EJE_MAX / 2) ? 1 : 0;
	b.D_CBUTTON = (cy >  N64_EJE_MAX / 2) ? 1 : 0;   /* libretro: +Y = abajo */
	b.U_CBUTTON = (cy < -N64_EJE_MAX / 2) ? 1 : 0;

	s_foto[puerto] = b;
}

void xbox64_input_poll(void)
{
	unsigned i;
	for (i = 0; i < 4; i++)
		foto_de(i);
}

void xbox64_input_set_device(unsigned puerto, unsigned device)
{
	if (puerto >= 4)
		return;

	s_enchufado[puerto] = (device == RD_DEVICE_NONE) ? 0 : 1;

	/* ★ `Present` es lo que mira `pif.c` ANTES de llamar a `getKeys`
	 * (pif.c:320): con FALSE le contesta al invitado "no hay mando", que
	 * es el estado correcto y no una mentira.  Por eso se mantienen los
	 * dos en el mismo sitio y no solo la foto. */
	Controls[puerto].Present = s_enchufado[puerto] ? TRUE : FALSE;

	s_foto[puerto].Value = 0;
}

void xbox64_input_reset(void)
{
	unsigned i;
	for (i = 0; i < 4; i++)
		s_foto[i].Value = 0;
}

/* ====================================================================== */
/* LOS PUNTOS DE ENTRADA DEL PLUGIN (declarados en main/plugin.h:265-272)  */
/* ====================================================================== */
/* Los SIETE, juntos y en un solo fichero.  Antes estaban vacios en
 * plugins_fase1.c y se movieron de una pieza. */

void getKeys(int Control, BUTTONS *Keys)
{
	/* ★ EL CONTADOR SE MANTIENE, y no por inercia: `keys=` de la linea de
	 * resumen es la unica forma de distinguir "el mando no responde
	 * porque el mapa esta mal" de "el invitado no esta leyendo el mando
	 * siquiera" (Present a FALSE, o el juego aun en un arranque que no
	 * mira el PIF).  Son dos fallos que se ven igual en pantalla. */
#if XBOX64_TRAZAS
	xbox64_n_keys++;
#endif

	if (!Keys)
		return;
	if (Control < 0 || Control >= 4)
	{
		Keys->Value = 0;
		return;
	}
	*Keys = s_foto[Control];
}

/* ★ LAS DOS DE DATOS EN CRUDO SE QUEDAN VACIAS, Y NO ES UN HUECO: solo se
 * llaman cuando `Controls[].Plugin == PLUGIN_RAW` (pif.c:330, :400), y
 * aqui el plugin es PLUGIN_MEMPAK, que `pif.c` resuelve el solo contra el
 * array `mempack[]`.  Si algun dia se ofrece el pak de vibracion habra
 * que pasar a RAW y entonces SI hay que escribirlas. */
void readController(int Control, BYTE *Command)
{
	(void)Control; (void)Command;
}

void controllerCommand(int Control, BYTE *Command)
{
	(void)Control; (void)Command;
}

void initiateControllers(CONTROL_INFO ControlInfo)
{
	unsigned i;
	(void)ControlInfo;

	/* El estado de partida: los cuatro enchufados.  `control_info_init`
	 * (plataforma.c) ya ha puesto Present y Plugin; aqui solo se pone de
	 * acuerdo la foto, para que las dos mitades no puedan discrepar. */
	for (i = 0; i < 4; i++)
	{
		s_enchufado[i] = (Controls[i].Present != FALSE) ? 1 : 0;
		s_foto[i].Value = 0;
	}
}

void romOpen_input(void)   { xbox64_input_reset(); }
void romClosed_input(void) { }
void closeDLL_input(void)  { }
