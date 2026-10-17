/*
 * xbox/libgcc_ppc.c  --  los tres simbolos de libgcc que el dynarec LLAMA
 *                        DESDE EL CODIGO QUE EMITE.
 *
 * ★★ POR QUE NO SE PUEDEN ESCRIBIR EN C A SECAS, QUE ES LO QUE PARECE.
 *
 * `MIPS-to-PPC.c` emite un `bl` a estas tres funciones y LEE SU RESULTADO
 * CON EL ABI DE PowerPC DE 32 BITS DE GCC, que parte los enteros de 64 bits
 * en una PAREJA DE REGISTROS.  Se ve en el propio codigo emitido:
 *
 *     MIPS-to-PPC.c:3540   stw r3, 0(addr)      <- palabra ALTA
 *     MIPS-to-PPC.c:3544   stw r4, 4(addr)      <- palabra BAJA
 *     MIPS-to-PPC.c:2517   addi r7, r4300, offsetof(R4300,hi)
 *                          -> __udivmoddi4(a=r3:r4, b=r5:r6, rem=r7)
 *
 * En Xbox 360 los registros son de 64 bits y MSVC devuelve un `long long`
 * ENTERO en r3.  O sea que una implementacion en C limpia dejaria el valor
 * completo en r3 y basura en r4, y el codigo emitido guardaria la mitad
 * alta desde un registro que contiene el valor entero.
 *
 * ★ Y NO DARIA NINGUN AVISO.  Enlaza, arranca, y las conversiones de coma
 * flotante a entero de 64 bits y las divisiones DDIV/DDIVU salen mal.  Por
 * eso van en ensamblador: la frontera de ABI es el trabajo, no la
 * aritmetica.
 *
 * ★ LA ARITMETICA SI VA EN C, en las tres.  El ensamblador se limita a
 * colocar argumentos y partir resultados; asi lo que puede equivocarse es
 * corto y se lee de un vistazo.
 *
 * ★ EL MARCO ES DE 112 BYTES Y LOS DATOS EMPIEZAN EN +56, por lo mismo que
 * el trampolin de Wrappers.c: en Xbox 360 los 56 primeros bytes del marco
 * son del ABI (area de parametros y saves del llamado).  La nota larga esta
 * en Wrappers.h, junto a DYNAOFF_LR.
 *
 * C89.  Se compila como C (sin CompileAs) para que los simbolos salgan SIN
 * decorar, que es como los pide MIPS-to-PPC.c desde su bloque extern "C".
 */

#include <xtl.h>

/* ====================================================================== */
/* 1. __fixdfdi / __fixsfdi : double|float -> entero de 64 bits con signo  */
/* ====================================================================== */
/* Truncan hacia cero, que es lo que hace libgcc y lo que hace el `fctidz`
 * de PowerPC.
 *
 * ★★ LA PRIMERA VERSION EMITIA `fctidz` Y `stfd` A MANO, Y NO COMPILA:
 *
 *     error C2759: in-line assembler reports: Expected FP register
 *        : fctidz f0 , f1
 *     error C2759: in-line assembler reports: Unknown register
 *        : stfd f0 , 56 ( r1 )
 *
 * El ensamblador del XDK no llama `f0` al registro de coma flotante.  Se
 * podria averiguar como lo llama (¿`fr0`?), pero NO HACE FALTA AVERIGUARLO,
 * y buscarlo seria volver a apostar: el mismo fichero ya tiene escrita la
 * norma de que LA ARITMETICA VA EN C y el ensamblador solo coloca
 * argumentos y parte resultados.  Estas dos funciones la incumplian.
 *
 * Asi que la conversion la hace un ayudante en C -- `(long long)x` trunca
 * hacia cero por definicion del lenguaje, y MSVC la compila justo a
 * `fctidz` -- y el ensamblador se queda SOLO CON REGISTROS DE ENTEROS, que
 * es lo unico que hay que saber nombrar.
 *
 * ★ EL `double` SOBREVIVE AL `bl` SIN TOCARLO: llega en f1 (primer
 * argumento de coma flotante, en los dos ABIs) y aqui no se toca ningun
 * FPR, asi que el ayudante lo recibe donde lo espera.  Esa es justo la
 * razon de que el ensamblador pueda ignorar la coma flotante.
 *
 * Big-endian: al guardar los 8 bytes con `std`, la palabra ALTA queda en el
 * desplazamiento menor.  Por eso r3 lee +56 y r4 lee +60. */

long long xbox64_fix_d(double x) { return (long long)x; }
long long xbox64_fix_s(float  x) { return (long long)x; }

void __declspec(naked) __fixdfdi(double x)
{
	(void)x;
	__asm {
		mflr	r0
		stwu	r1, -112(r1)
		stw		r0, 96(r1)
		bl		xbox64_fix_d	/* el double sigue en f1, intacto */
		std		r3, 56(r1)
		lwz		r3, 56(r1)		/* palabra alta */
		lwz		r4, 60(r1)		/* palabra baja */
		lwz		r0, 96(r1)
		addi	r1, r1, 112
		mtlr	r0
		blr
	}
}

void __declspec(naked) __fixsfdi(float x)
{
	(void)x;
	__asm {
		mflr	r0
		stwu	r1, -112(r1)
		stw		r0, 96(r1)
		bl		xbox64_fix_s
		std		r3, 56(r1)
		lwz		r3, 56(r1)
		lwz		r4, 60(r1)
		lwz		r0, 96(r1)
		addi	r1, r1, 112
		mtlr	r0
		blr
	}
}

/* ====================================================================== */
/* 2. __udivmoddi4 : division sin signo de 64 bits, con resto             */
/* ====================================================================== */
/* Firma que declara MIPS-to-PPC.c:65 y que usa el codigo emitido:
 *
 *     unsigned long long __udivmoddi4(unsigned long long a,
 *                                     unsigned long long b,
 *                                     unsigned long long *rem);
 *
 *   entra:  a = r3:r4   b = r5:r6   rem = r7
 *   sale:   cociente en r3:r4, y *rem escrito con el resto (8 bytes)
 *
 * El `rem` apunta a `r4300.hi`, que es un campo de 64 bits, asi que el
 * resto se escribe entero con un `std`. */

/* ★ EL DIVISOR CERO SE ATRAPA AQUI, y es una decision, no un descuido.
 *
 * En MIPS, DDIV y DDIVU con divisor 0 son UNPREDICTABLE, y el codigo que
 * emite el dynarec no comprueba nada (MIPS-to-PPC.c:2506-2520 solo arregla
 * los signos).  En PowerPC `divdu` por cero no lanza excepcion: deja un
 * resultado indefinido.  O sea que ninguna de las dos maquinas se cae, y
 * devolver ceros reproduce ese "no pasa nada" de forma DETERMINISTA en vez
 * de dejar que el resultado dependa del silicio.
 *
 * Sin la guarda, la division en C es comportamiento indefinido y el
 * compilador tiene licencia para hacer cualquier cosa con ella. */
unsigned long long xbox64_udivmod64(unsigned long long a,
                                    unsigned long long b,
                                    unsigned long long *rem)
{
	if (b == 0)
	{
		if (rem) *rem = 0;
		return 0;
	}
	if (rem) *rem = a % b;
	return a / b;
}

void __declspec(naked) __udivmoddi4(void)
{
	__asm {
		mflr	r0
		stwu	r1, -112(r1)
		stw		r0, 96(r1)		/* nuestro LR                        */
		stw		r7, 100(r1)		/* el puntero de resto del llamante  */

		/* Recomponer los dos enteros de 64 bits desde las parejas.  Por
		 * memoria y no con rldimi: son dos `stw` y un `ld`, se lee solo, y
		 * en big-endian la palabra alta va en el desplazamiento menor. */
		stw		r3, 56(r1)		/* a alta */
		stw		r4, 60(r1)		/* a baja */
		stw		r5, 64(r1)		/* b alta */
		stw		r6, 68(r1)		/* b baja */
		ld		r3, 56(r1)		/* r3 = a */
		ld		r4, 64(r1)		/* r4 = b */
		addi	r5, r1, 72		/* r5 = &resto local */

		bl		xbox64_udivmod64
		/* r3 = cociente, de 64 bits */

		std		r3, 80(r1)
		lwz		r3, 80(r1)		/* cociente, palabra alta */
		lwz		r4, 84(r1)		/* cociente, palabra baja */

		/* Y el resto, al puntero que dio el llamante (r4300.hi, 64 bits). */
		lwz		r5, 100(r1)
		ld		r6, 72(r1)
		std		r6, 0(r5)

		lwz		r0, 96(r1)
		addi	r1, r1, 112
		mtlr	r0
		blr
	}
}
