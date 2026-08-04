===============================================================================
 PicoDrive 32X - Recompilador dinamico SH2, backend PPC para Xbox 360
 Guia de build y depuracion con DRC_CMP  (Salvia / VS2010 + XDK)
===============================================================================

RESUMEN
-------
El backend PPC del DRC del SH2 (cpu/drc/emit_ppc.c) fue escrito por upstream
para ppc64le (POWER, 64-bit little-endian). Xenon (Xbox 360) es PPC 32-bit
BIG-ENDIAN. emit_ppc.c ya trae las adaptaciones validadas para Xenon:
  - ops de 64 bits desactivadas (#if 0 /* Xbox 360 is 32-bit */)
  - builtins GCC -> intrinsics MSVC (_BitScanForward, etc.)
  - LR guardado DENTRO del frame en emith_sh2_drc_entry/exit (fix critico)
  - flush SMC correcto para Xenon (dcbst/sync/icbi/sync/isync)
  - emith_move_r_imm_s8_patchable ampliado a s16 (prediccion de retorno RTS)
  - fallback mtctr/bctr para saltos fuera de rango +-32MB
  - fix emith_pass_arg_r/imm (host_arg2reg) usado por el path DRC_CMP

ESTADO CONOCIDO: el codegen por-instruccion esta validado con DRC_CMP. El
crash pendiente esta en el DISPATCHER de saltos indirectos (jsr/jmp @Rn, RTS):
puede acabar saltando a una direccion SH2 (p.ej. Rn = comm port 0x20004000)
interpretada como puntero host. Objetivo de depuracion actual.


MODOS DE BUILD (Preprocessor Definitions de la config Xbox 360)
---------------------------------------------------------------
El arch se selecciona con __ppc__ (+ __BIG_ENDIAN__, _XBOX). El DRC con DRC_SH2.

  (1) DRC NORMAL  (objetivo: correr juegos con el recompilador)
      ...;DRC_SH2;...
      -> es lo que trae el vcxproj ahora en Debug|Xbox 360 y Release|Xbox 360.

  (2) DRC_CMP - COMPARE  (encontrar la primera divergencia DRC vs interprete)
      ...;DRC_SH2;DRC_CMP;...
      El DRC corre y, tras CADA instruccion, do_sh2_cmp() lee game:\tracelog.bin
      y compara todos los registros SH2 + SR + ciclos. La primera divergencia
      se loguea como "bad rX: <drc> <ref>" en el log de libretro (RetroArch).
      NO reinicia la consola: resincroniza y sigue (cap 50 reportes).
      Requiere un tracelog generado antes (modo RECORD).
      NOTA: DRC_CMP desactiva PROPAGATE_CONSTANTS/LOOP_*/T_/DIV_OPTIMIZER en
      compiler.c para mantener lockstep exacto con el interprete.

  (3) DRC_CMP - RECORD  (generar el tracelog de referencia)
      ...;DRC_CMP;DRC_CMP_RECORD;...
      Fuerza ambos SH2 a interprete (32x.c) y do_sh2_trace() ESCRIBE
      game:\tracelog.bin. Corre el juego unos segundos y sal.

  (4) AISLAMIENTO slave  (diagnostico del bug de dispatcher)
      ...;DRC_SH2;SALVIA_FORCE_SLAVE_INTERP;...
      master = DRC, slave = interprete. Si el crash desaparece, el bug se
      limita al slave (tipicamente saltos indirectos a codigo de SDRAM).


FLUJO PARA CAZAR UNA DIVERGENCIA
--------------------------------
  1. Compila en modo RECORD (3). Arranca el juego que falla, deja correr unos
     segundos (mismo estado inicial, SIN input, para que sea determinista).
     Se genera game:\tracelog.bin. Cierra.
  2. Compila en modo COMPARE (2). Arranca EL MISMO juego igual.
  3. Mira el log de libretro: la primera linea "bad ..." indica el registro y
     el PC donde el DRC diverge del interprete. Esa es la instruccion a
     revisar en emit_ppc.c / compiler.c.
  Determinismo: usa la misma consola, misma ROM, mismo reset, sin input. El
  boot es determinista; ahi es donde aparece la corrupcion.

  Alternativa: generar tracelog en PC (build Win32 interprete con DRC_CMP +
  DRC_CMP_RECORD) y copiarlo a game:\tracelog.bin de la consola para comparar
  el codegen PPC contra una referencia x86 conocida-buena.


NOTAS
-----
  - game:\ es el dir del .xex (escribible en el devkit); el current dir puede
    ser read-only.
  - El drift de ciclos DRC vs interprete es estructural: el comparador solo
    reporta divergencias de ciclos > 200 (falsos positivos filtrados).
  - Para volver a interprete puro: pon __DRC_SH2 (doble guion) en el vcxproj.
