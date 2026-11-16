# Herramientas de conversión a C++03 (MSVC 2010)

Scripts usados para adaptar el código de ScummVM al compilador de Visual Studio
2010 / Xbox 360. Se ejecutan en Linux o WSL con `clang` y la librería
`libclang` de Python (`pip install libclang`).

Flujo para añadir un motor nuevo (ej. `sky`):

1. Generar la lista de fuentes con el Makefile de libretro (`make -n ... LITE=1`
   con el motor en `lite_engines.list`) o a partir de `engines/sky/module.mk`.
   Fichero de texto con una ruta relativa por línea (`srclist.txt`).
2. Comprobar qué construcciones C++11 no soportadas aparecen
   (`-Wc++98-compat` de clang, filtrando las que VS2010 sí acepta):

       EXTRA_DEFS="-DSCUMMVM_CXX03_COMPAT -DNO_CXX11_ALIGNAS -DENABLE_SKY=STATIC_PLUGIN" \
           ./check.sh <raiz_scummvm> srclist.txt chk.txt
       ./filt.sh chk.txt

3. Conversiones automáticas (en este orden, comprobando entre cada paso):
   * `rw_rangefor.py srclist.txt`   bucles for de rango
   * `rw_nsdmi.py srclist.txt`      inicializadores de miembros en la clase
   * `rw_tokens.py chk.txt`         final, noexcept, enum class, constexpr, using X = Y
   * `rw_deleg.py chk.txt`          constructores delegados
   (`SCUMMVM_ROOT` apunta a la raíz de ScummVM; por defecto `../../../..`).
4. Lo que quede (listas de inicialización con llaves, plantillas variádicas,
   lambdas convertidas a punteros a función...) se adapta a mano.
5. `gen_vcxproj.py srclist.txt ../scummvm` regenera el proyecto
   (añadir los `ENABLE_XXX` del motor en la variable `ENGINES`).
