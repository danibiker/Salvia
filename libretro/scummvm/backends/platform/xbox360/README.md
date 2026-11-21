# ScummVM (libretro) para Salvia — Visual Studio 2010 / Xbox 360

Proyecto `scummvm/scummvm.vcxproj`: compila el core libretro de ScummVM como
librería estática (`scummvm.lib`) para enlazarla con el frontend Salvia.

## Configuraciones

El árbol está en `project\Salvia\libretro\scummvm`; Salvia enlaza la librería
desde `libretro\scummvm\backends\platform\xbox360\scummvm\<Configuración>\<Plataforma>`
(configuraciones `Debug_scummvm` / `Release_scummvm` de `Salvia.vcxproj`).

| Configuración      | Estado                                         |
|--------------------|------------------------------------------------|
| `Debug|Win32`      | Funciona                                       |
| `Release|Win32`    | Funciona                                       |
| `Debug|Xbox 360`   | Compila (defines `_XBOX`, big endian, libco con fibers) |
| `Release|Xbox 360` | Funciona en consola                            |

Salida: `scummvm\<Configuración>\<Plataforma>\scummvm.lib`.

## Dependencias

* **zlib**: solo se usan las cabeceras (`ZLIB_INCLUDE_DIR`, por defecto
  `$(SCUMMVM_ROOT)..\..\libs\zlib`, es decir `Salvia\libs\zlib`).
  La librería `zlib.lib` ya la enlaza Salvia. Si mueves el árbol de ScummVM,
  cambia la macro `ZLIB_INCLUDE_DIR` en el proyecto (o en `tools/gen_vcxproj.py`).
* **libretro-common**: copia incluida en `backends/platform/libretro/deps/libretro-common`
  (commit `879c8d50`, el que fija `dependencies.mk`), con dos parches para Xbox 360:
  `libco/libco.c` (fibers + `xtl.h`) e `include/retro_atomic.h` (`xtl.h` en vez de `windows.h`).
* Dependencias opcionales incluidas (de `libretro-deps`): zlib (la de Salvia),
  MP3 (libmad), Ogg/Vorbis, FLAC, FreeType, TinyGL y PNG (libpng con todos sus
  símbolos renombrados a `scummvm_png_*` para no chocar con la libpng de
  Salvia; ver `libpng/pngprefix.h`). Faltan Theora, JPEG, MPEG-2, FluidSynth,
  MT-32...

## Motores

* **SCUMM** (incluye v7/v8 y HE): `ENABLE_SCUMM=STATIC_PLUGIN;ENABLE_SCUMM_7_8;ENABLE_HE`
* **Blade Runner**: `ENABLE_BLADERUNNER=STATIC_PLUGIN`
* **The Longest Journey** (`stark`, TinyGL + FreeType): `ENABLE_STARK=STATIC_PLUGIN`
* **Grim Fandango** y **Escape from Monkey Island** (`grim`, TinyGL):
  `ENABLE_GRIM=STATIC_PLUGIN;ENABLE_MONKEY4`. Necesitan `grim-patch.lab` y
  `monkey4-patch.m4b` (de `dists/engine-data`) en la carpeta `extra` de ScummVM.
  Sin Theora no se ven los vídeos de la versión Remastered; sin MPEG-2 no va la
  versión PS2 de Monkey 4.
* **SCI** con SCI32 (juegos de Sierra, p. ej. Leisure Suit Larry 7):
  `ENABLE_SCI=STATIC_PLUGIN;ENABLE_SCI32`.
* Clásicos añadidos (2D, sin librerías nuevas):
  * Prioridad 1: AGI (`agi`), Simon the Sorcerer (`agos`, con `ENABLE_AGOS2`),
    Broken Sword 1 y 2 (`sword1`, `sword2`), Beneath a Steel Sky (`sky`),
    Flight of the Amazon Queen (`queen`), Kyrandia / Lands of Lore / Eye of
    the Beholder (`kyra`, con `ENABLE_LOL;ENABLE_EOB`), Gobliiins (`gob`),
    Discworld (`tinsel`), Myst / Riven / Living Books (`mohawk`, con
    `ENABLE_CSTIME;ENABLE_MYST;ENABLE_MYSTME;ENABLE_RIVEN`), Sanitarium
    (`asylum`) y Toonstruck (`toon`).
  * Prioridad 2: SAGA (`saga`, con `ENABLE_IHNM`), The 7th Guest / The 11th
    Hour (`groovie`, con `ENABLE_GROOVIE2`), Cinématique (`cine`, `cruise`),
    `lure`, `drascula`, `dreamweb`, `teenagent`, `neverhood`, `made`,
    `parallaction`, `touche` y `tucker`.
  * Ultima (`ultima`, con `ENABLE_ULTIMA4;ENABLE_ULTIMA6;ENABLE_ULTIMA8` y
    `USE_LUA`, que añade `common/lua`): Ultima IV, Ultima VI y los Worlds of
    Ultima (Nuvie), Ultima VIII y Crusader. Akalabeth y Ultima I no se
    activan: ScummVM los marca como en desarrollo.

  Algunos necesitan su fichero de datos de `dists/engine-data` en la carpeta
  `system` de Salvia (junto a `grim-patch.lab`): `kyra.dat`, `sky.cpt`,
  `queen.tbl`, `toon.dat`, `drascula.dat`, `lure.dat`, `teenagent.dat`,
  `neverhood.dat`, `ultima.dat` y `ultima8.dat`. Ya están copiados en `DistroWin\system`; en la Xbox 360 hay
  que copiarlos a `game:\system`.

**Blade Runner**, **Grim Fandango / Monkey 4** y **The Longest Journey** solo se
compilan en Win32: en Xbox 360 van (o se espera que vayan, en el caso de The
Longest Journey, también 3D con TinyGL) demasiado lentos por ahora. `gen_vcxproj.py` (lista
`WIN32_ONLY_ENGINES`) marca sus fuentes como `ExcludedFromBuild` para la
plataforma Xbox 360 y no define allí sus `ENABLE_*`, así que tampoco se enlaza
su detección. Para volver a activarlos, quitarlos de esa lista y regenerar.

`engines/plugins_table.h` y `engines/detection_table.h` (generados por el
`configure` de ScummVM) son genéricos: para añadir un motor basta con añadir su
`ENABLE_XXX=STATIC_PLUGIN`, sus fuentes (`engines/xxx/module.mk`) y adaptar su
código a C++03 como se describe abajo.

## Juegos que se pueden jugar

Según los motores compilados en cada plataforma y las tablas de detección de
ScummVM. Que un juego aparezca aquí significa que el motor lo soporta, no que
se haya probado. Probados: Day of the Tentacle, Full Throttle, Leisure Suit
Larry 7 y Grim Fandango en Windows; The Secret of Monkey Island en Xbox 360.

| Motor                                   | Windows | Xbox 360 |
|-----------------------------------------|:-------:|:--------:|
| SCUMM (con v7/v8 y HE)                  | Sí      | Sí       |
| SCI (con SCI32)                         | Sí      | Sí       |
| Stark (The Longest Journey)             | Sí      | No       |
| Blade Runner                            | Sí      | No       |
| Grim (Grim Fandango, Escape from Monkey Island) | Sí | No     |
| Clásicos de prioridad 1 y 2 y Ultima (26 motores, ver arriba) | Sí | Sí |

### Windows y Xbox 360

**SCUMM: aventuras de LucasArts**

* Maniac Mansion, Zak McKracken, Indiana Jones and the Last Crusade, Loom,
  Passport to Adventure.
* The Secret of Monkey Island, Monkey Island 2, Indiana Jones and the Fate of
  Atlantis.
* Day of the Tentacle, Sam & Max Hit the Road.
* Full Throttle, The Dig, The Curse of Monkey Island.
* Star Wars: Rebel Assault y Rebel Assault II, Mortimer and the Riddles of the
  Medallion (ScummVM marca Mortimer como inestable).
* Recopilaciones de FM-Towns: Indy3 & Loom, Indy3 & Zak, Zak & Loom.

**SCUMM HE: Humongous Entertainment**

* Putt-Putt: Joins the Parade, Goes to the Moon, Saves the Zoo, Travels Through
  Time, Enters the Race, Joins the Circus, Fun Pack, One-Stop Fun Shop, Pep's
  Balloon-O-Rama, Pep's Dog on a Stick.
* Freddi Fish 1-5, Maze Madness, Water Worries, One-Stop Fun Shop.
* Pajama Sam 1-3, Lost & Found, Sock Works, Games to Play on Any Day, One-Stop
  Fun Shop.
* SPY Fox 1-3, Cheese Chase, Hold the Mustard.
* Fatty Bear: Birthday Surprise, Fun Pack, Activity Pack (con Putt-Putt).
* Backyard Baseball / 2001 / 2003, Backyard Football / 2002, Backyard
  Basketball, Backyard Soccer / 2004 / MLS, Moonbase Commander (sin juego en
  red: no se compila).
* Blue's Clues (6 títulos), Let's Explore... with Buzzy (Airport, Farm, Jungle),
  Big Thinkers (Kindergarten, First Grade), Bear Stormin', Humongous
  Interactive Catalog.

**SCI: Sierra**

* King's Quest I (SCI), IV (SCI), V, VI y VII.
* Space Quest I (SCI), III, IV, V y 6.
* Leisure Suit Larry 1 (SCI), 2, 3, 5, 6, 6 en alta resolución y 7.
* Police Quest I (SCI), II, III, IV y SWAT.
* Quest for Glory I (EGA y VGA), II, III y IV.
* Gabriel Knight 1 y 2 (The Beast Within), Laura Bow 1 y 2.
* Codename: Iceman, Conquests of Camelot, Conquests of the Longbow, EcoQuest 1
  y 2, Freddy Pharkas, Phantasmagoria 1 y 2, Torin's Passage, Lighthouse, RAMA,
  Shivers, Pepper's Adventures in Time.
* Educativos y de mesa: Castle of Dr. Brain, The Island of Dr. Brain, Mixed-Up
  Mother Goose (3 versiones), Mixed-up Fairy Tales, Slater & Charlie, Jones in
  the Fast Lane, Hoyle 1-5 y variantes (Bridge, Solitaire, Children's, School
  House), Crazy Nick's Software Picks (5), Astro Chicken, Ms. Astro Chicken,
  Christmas Cards (1988, 1990, 1992), Fun Seeker's Guide, King's Questions.

**Clásicos añadidos (prioridad 1 y 2)**

* **AGI** (`agi`): AGI Demo; AGI Tetris; Caitlyn's Destiny; Donald Duck's Playground; Gold Rush!; King's Quest II: Romancing the Throne; King's Quest III: To Heir Is Human; King's Quest IV: The Perils of Rosella; King's Quest: Quest for the Crown; Leisure Suit Larry in the Land of the Lounge Lizards; Manhunter 2: San Francisco; Manhunter: New York; Mickey's Space Adventure; Mixed-Up Mother Goose; Police Quest: In Pursuit of the Death Angel; Serguei's Destiny 1; Serguei's Destiny 2; Space Quest 0: Replicated; Space Quest II: Chapter II - Vohaul's Revenge; Space Quest: Chapter I - The Sarien Encounter; Space Quest: The Lost Chapter; The Black Cauldron; Troll's Tale; Winnie the Pooh in the Hundred Acre Wood; Xmas Card. Y 219 juegos de fans.
* **AGOS** (`agos`): Elvira II: The Jaws of Cerberus; Elvira: Mistress of the Dark; Personal Nightmare; Simon the Sorcerer; Simon the Sorcerer II: The Lion, the Wizard and the Wardrobe; Simon the Sorcerer's Puzzle Pack: Demon in my Pocket; Simon the Sorcerer's Puzzle Pack: Jumble; Simon the Sorcerer's Puzzle Pack: NoPatience; Simon the Sorcerer's Puzzle Pack: Swampy Adventures; The Feeble Files; Waxworks.
* **Broken Sword** (`sword1`): Broken Sword: The Shadow of the Templars.
* **Broken Sword II** (`sword2`): Broken Sword II: The Smoking Mirror.
* **Beneath a Steel Sky** (`sky`): Beneath a Steel Sky; Beneath a Steel Sky iOS remastered.
* **Flight of the Amazon Queen** (`queen`): Flight of the Amazon Queen.
* **Kyra** (`kyra`): Eye of the Beholder; Eye of the Beholder II: The Legend of Darkmoon; Lands of Lore: The Throne of Chaos; The Legend of Kyrandia; The Legend of Kyrandia: Malcolm's Revenge; The Legend of Kyrandia: The Hand of Fate.
* **Gobli\*ns** (`gob`): ADI 1; ADI 2; ADI 4; ADI 4 Anglais 12-13 years; ADI 4 Anglais 13-14 years; ADI 4 Anglais 9-11 years; ADI 4 Euro; ADI 4 Geography; ADI 4 Math & Language 10-11 years; ADI 4 Math & Language 8-9 years; ADI 5; ADI 5 Anglais; ADI 5 Language; Adibou 1; Adibou 1 Count 4-5 years; Adibou 1 Count 6-7 years; Adibou 1 Read 4-5 years; Adibou 1 Read 6-7 years; Adibou 2; Adibou 2 Anglais; Adibou 2 Music; Adibou 2 Nature & Sciences; Adibou 2 Read/Count 4-5 years; Adibou 2 Read/Count 6-7 years; Adibou 3; Adibou 3 Anglais; Adibou 3 Music; Adibou 3 Nature & Sciences; Adibou 3 Read/Count 4-5 years; Adibou 3 Read/Count 5-6 years; Adibou 3 Read/Count 6-7 years; Adibou présente la Cuisine; Adibou présente la Magie; Adibou présente le Dessin; Adiboud'chou a la campagne; Adiboud'chou a la mer; Adiboud'chou dans la jungle et la savane; Adiboud'chou sur la banquise; Bargon Attack; Croustibat; English Fever; Fascination; Geisha; Gobliiins; Gobliins 2; Goblins Quest 3; Inca II: Wiracocha; Lost in Time; Lost in Time Part 1; Lost in Time Part 2; Nathan Vacances; Once Upon A Time; Once Upon A Time: Abracadabra; Once Upon A Time: Baba Yaga; Once Upon A Time: Little Red Riding Hood; Playtoons 1 - Uncle Archibald; Playtoons 2 - The Case of the Counterfeit Collaborator; Playtoons 3 - The Secret of the Castle; Playtoons 4 - The Mandarine Prince; Playtoons 5 - The Stone of Wakan; Playtoons Construction Kit 1 - Monsters; Playtoons Construction Kit 2 - Knights; Playtoons Construction Kit 3 - Far West; Playtoons Demo; Playtoons Limited Edition - Bambou le sauveur de la jungle; The Bizarre Adventures of Woodruff and the Schnibble; The Land of the Magic Stones; The Last Dynasty; Urban Runner; Ween: The Prophecy; Woodruff and The Last Dynasty.
* **Tinsel** (`tinsel`): Discworld; Discworld II: Missing Presumed ...!?; Discworld Noir.
* **Mohawk** (`mohawk`): Aesop's Fables: The Tortoise and the Hare; Alien Tales; Arthur's Birthday; Arthur's Computer Adventure; Arthur's Reading Games; Arthur's Reading Race; Arthur's Teacher Trouble; Carmen Sandiego's ThinkQuick Challenge; Carmen Sandiego's ThinkQuick Challenge Custom Question Creator; Daniel in the Lions' Den; Dr. Seuss Reading Games; Dr. Seuss's ABC; Dr. Seuss's Preschool; Green Eggs and Ham; Harry and the Haunted House; In the 1st Degree; James Discovers/Explores Math; Just Grandma and Me; Little Monster at School; Living Books Sampler; Logical Journey of the Zoombinis; Maggie's Farmyard Adventure; Maths Workshop; Maths Workshop Deluxe; Myst; Noah's Ark; Orly's Draw-A-Story; Riven: The Sequel to Myst; Ruff's Bone; Rugrats Adventure Game; Rugrats Print Shop; Sheila Rae, the Brave; Stellaluna; The Amazing Writing Machine; The Berenstain Bears Get in a Fight; The Berenstain Bears In The Dark; The Cat in the Hat; The Making of Myst; The New Kid on the Block; The Story of Creation; The Treehouse; Where in America is Carmen Sandiego? (The Great Amtrak Train Adventure); Where in the USA is Carmen Sandiego?; Where in the World is Carmen Sandiego?; Where in Time is Carmen Sandiego?; Williams-Sonoma Guide to Good Cooking; Write, Camera, Action!.
* **Sanitarium** (`asylum`): Sanitarium.
* **Toonstruck** (`toon`): Toonstruck.
* **SAGA** (`saga`): I Have No Mouth and I Must Scream; Inherit the Earth: Quest for the Orb.
* **Groovie** (`groovie`): Clandestiny; Tender Loving Care; The 11th Hour: Souped Up; The 11th Hour: The Sequel to The 7th Guest; The 7th Guest; The Making of The 11th Hour; Uncle Henry's Playhouse.
* **Cinematique evo 1** (`cine`): Future Wars; Operation Stealth.
* **Cinematique evo 2** (`cruise`): Cruise for a Corpse.
* **Lure of the Temptress** (`lure`): Lure of the Temptress.
* **Drascula: The Vampire Strikes Back** (`drascula`): Drascula: The Vampire Strikes Back.
* **Dreamweb** (`dreamweb`): DreamWeb.
* **Teen Agent** (`teenagent`): Teen Agent.
* **Neverhood** (`neverhood`): The Neverhood.
* **MADE** (`made`): Leather Goddesses of Phobos 2; Return to Zork; Richard Scarry's Best Neighborhood Disc Ever!; Richard Scarry's Busiest Neighborhood Disc Ever!; Rodney's Funscreen; The Manhole.
* **Parallaction** (`parallaction`): Nippon Safes, Inc.; The Big Red Adventure.
* **Touche: The Adventures of the Fifth Musketeer** (`touche`): Touché: The Adventures of the Fifth Musketeer.
* **Bud Tucker in Double Trouble** (`tucker`): Bud Tucker in Double Trouble.
* **Ultima** (`ultima`): Ultima IV: Quest of the Avatar; Ultima VI: The False Prophet; Worlds of Ultima: The Savage Empire; Worlds of Ultima: Martian Dreams (las tres de Nuvie también en su versión "Enhanced"); Ultima VIII: Pagan; Crusader: No Remorse; Crusader: No Regret.

### Solo Windows

* **The Longest Journey** (`stark`): versión original, Steam y GOG.
* **Blade Runner**: versión original de 1997 (CD o DVD) y la versión con el
  contenido restaurado (`bladerunner-final`).
* **Grim Fandango**, incluida la Remastered, pero sin sus vídeos remasterizados
  (no se compila Theora). Necesita `grim-patch.lab` en la carpeta `extra`.
* **Escape from Monkey Island** (PC). Necesita `monkey4-patch.m4b`.

### Notas

* Los que no funcionan están en el apartado siguiente.
* ScummVM marca como inestables algunas versiones concretas (sobre todo demos)
  de Larry 6 hires, Larry 7, King's Quest VII, Gabriel Knight 2,
  Phantasmagoria, Torin's Passage, Space Quest 6, Lighthouse, RAMA y Shivers.
* Leisure Suit Larry 2 funciona en su versión original de Sierra (SCI); los
  remakes de fans hechos con AGS no, porque ese motor no se compila.
* Las versiones de FM-Towns y PC-Engine necesitan las pistas de audio del CD
  extraídas para tener música.
* Xbox 360: Discworld Noir (dentro de `tinsel`) es 3D con TinyGL y probablemente vaya lento, como Grim.
* Xbox 360: los SCI32 a 640x480 con vídeo (Gabriel Knight 2, Phantasmagoria,
  Larry 7, Lighthouse) pueden ir justos; sin probar.
* Explorador de archivos de ScummVM ("Añadir juego", "Añadir varios"): subiendo
  de carpeta hasta arriba aparece la lista de unidades. En Windows salen todas
  las letras de unidad; en Xbox 360 salen `Game:`, `Hdd:`, `Usb0:`, `Usb1:` y
  `Usb2:` (el core las monta igual que `Launcher::initDrives` de Salvia si no
  lo están) y, si alguien las ha montado, `Hdd1:`, `UsbMu0:` e `IntMu:`. Solo se
  listan las que responden.
* Mando: L3 abre el teclado virtual (Select queda libre).
* Hooks (`.scummvm`): al pulsar "Generar playlist" en las opciones del core
  (pestaña Libretro), además de la playlist y los hooks se escribe
  `merged_scummvm.xml` en el directorio system, con el mismo formato que
  `assets/extra/merged_fbneo.xml`. Hay una entrada `<game name="...">` por cada
  hook: `name` es el nombre del fichero sin extensión (el target de ScummVM,
  p. ej. `monkey-es`) y `<description>` el título completo
  ("The Secret of Monkey Island (CD/Spanish)"). También lleva los atributos
  `engine`, `gameid`, `language` y `platform`. Se regenera cada vez, así que
  siempre coincide con los hooks creados (incluidos duplicados como
  `monkey-es-1` y targets renombrados).

## Juegos que no funcionan

La lista completa, juego a juego, está en
`juegos_no_soportados.xlsx` (en esta misma carpeta, `backends\platform\xbox360`):
hoja **Juegos** con los 12.252 títulos de los motores no compilados (motor,
subsistema, id y título; con filtros), hoja **Resumen** con el recuento por
motor y hoja **Otros que no funcionan** con los casos de abajo.

### Solo en Xbox 360 (motores excluidos)

The Longest Journey; Blade Runner (original y con contenido restaurado);
Grim Fandango (incluida la Remastered) y Escape from Monkey Island (PC).

### En ninguna plataforma, aunque su motor esté compilado

* Blade Runner: Enhanced Edition (ScummVM no la soporta).
* Escape from Monkey Island de PS2 (necesita MPEG-2, no compilado).
* Shivers II y The Realm (juego online).
* Algunas versiones de Hoyle Classic Games y una de Police Quest: SWAT, que
  ScummVM marca como no soportadas.
* Akalabeth y Ultima I: el motor Ultima está compilado, pero no estos dos
  subjuegos, que ScummVM aún tiene en desarrollo.
* Putt-Putt Saves the Zoo "Lite" de iOS (ScummVM la rechaza a propósito).
* Funcionan a medias: Grim Fandango Remastered sin sus vídeos nuevos (sin
  Theora) y Moonbase Commander / Backyard sin juego en red (sin ENet).

### Motores no compilados

Ningún juego de estos motores funciona con este build. Recuento de títulos
según sus tablas de detección (los motores "en desarrollo" tampoco se
compilan por defecto en ScummVM). Glk son aventuras de texto (Infocom,
Inform, ADRIFT, etc.); AGS y Director incluyen sobre todo juegos gratuitos y
CD-ROM multimedia.

| Motor | Descripción | Juegos | Estado en ScummVM |
|---|---|---:|---|
| `toltecs` | 3 Skulls of the Toltecs | 1 | estable |
| `access` | Access | 3 | estable |
| `adl` | ADL | 7 | estable |
| `ags` | Adventure Game Studio | 3.838 | estable |
| `agds` | AGDS (Black Mirror and NiBiRu) | 2 | en desarrollo |
| `alcachofa` | Alcachofa | 9 | estable |
| `pelrock` | Alfred Pelrock | 1 | estable |
| `alg` | American Laser Games | 7 | estable |
| `awe` | Another World | 1 | estable |
| `bagel` | BAGEL | 5 | estable |
| `bbvs` | Beavis and Butthead in Virtual Stupidity | 1 | estable |
| `dragons` | Blazing Dragons | 1 | estable |
| `bolt` | Bolt | 3 | estable |
| `sword25` | Broken Sword 2.5 | 1 | estable |
| `cge` | CGE | 1 | estable |
| `cge2` | CGE2 | 1 | estable |
| `chamber` | Chamber | 1 | estable |
| `chewy` | Chewy: Esc from F5 | 1 | estable |
| `crab` | CRAB | 1 | estable |
| `cryomni3d` | Cryo Omni3D games | 2 | estable |
| `darkseed` | Darkseed | 1 | estable |
| `draci` | Dragon History | 1 | estable |
| `dm` | Dungeon Master | 1 | estable |
| `dgds` | Dynamix Game Development System | 7 | estable |
| `eem` | Eagle Eye Mysteries | 2 | estable |
| `efh` | Escape From Hell | 1 | estable |
| `freescape` | Freescape | 8 | estable |
| `gamos` | Gamos | 8 | estable |
| `glk` | Glk Interactive Fiction games | 6.026 | estable |
| `got` | Got | 1 | estable |
| `hadesch` | Hades Challenge | 1 | estable |
| `harvester` | Harvester | 1 | estable |
| `hopkins` | Hopkins FBI | 1 | estable |
| `hpl1` | Hpl1 | 2 | estable |
| `hugo` | Hugo Trilogy | 3 | estable |
| `hdb` | Hyperspace Delivery Boy! | 1 | estable |
| `hypno` | Hypnotix Inc. | 4 | estable |
| `illusions` | Illusions Engine | 2 | estable |
| `icb` | In Cold Blood | 2 | en desarrollo |
| `harold` | J.B. Harold series | 1 | en desarrollo |
| `kingdom` | Kingdom: The Far Reaches | 1 | estable |
| `lab` | Labyrinth of Time | 1 | estable |
| `lilliput` | Lilliput | 2 | en desarrollo |
| `twine` | Little Big Adventure | 3 | estable |
| `avalanche` | Lord Avalot d'Argent | 1 | estable |
| `cryo` | Lost Eden | 1 | en desarrollo |
| `m4` | M4 | 2 | estable |
| `director` | Macromedia Director | 1.865 | estable |
| `macs2` | Macs2 | 1 | estable |
| `macventure` | MacVenture | 4 | estable |
| `mads` | MADS | 5 | estable |
| `composer` | Magic Composer | 9 | estable |
| `mediastation` | Media Station | 22 | en desarrollo |
| `mm` | Might and Magic | 6 | estable |
| `supernova` | Mission Supernova | 2 | estable |
| `mortevielle` | Mortevielle | 1 | estable |
| `mtropolis` | mTropolis | 57 | estable |
| `mutationofjb` | Mutation of JB | 1 | en desarrollo |
| `myst3` | Myst 3 | 1 | estable |
| `nancy` | Nancy Drew | 34 | estable |
| `ngi` | Nikita Game Interface | 3 | estable |
| `phoenixvr` | Phoenix VR | 8 | estable |
| `pink` | Pink Panther | 2 | estable |
| `plumbers` | Plumbers Don't Wear Ties | 1 | estable |
| `private` | Private Eye | 1 | estable |
| `qdengine` | QD Engine | 15 | estable |
| `petka` | Red Comrades | 2 | estable |
| `saga2` | SAGA2 | 2 | estable |
| `sludge` | Sludge | 22 | estable |
| `startrek` | Star Trek 25th Anniversary/Judgment Rites | 2 | en desarrollo |
| `titanic` | Starship Titanic | 1 | estable |
| `tetraedge` | Tetraedge | 3 | estable |
| `colony` | The Colony | 1 | estable |
| `fool` | The Fool's Errand | 1 | estable |
| `griffon` | The Griffon Legend | 1 | estable |
| `immortal` | The Immortal | 1 | en desarrollo |
| `buried` | The Journeyman Project 2: Buried in Time | 1 | estable |
| `pegasus` | The Journeyman Project: Pegasus Prime | 1 | estable |
| `lastexpress` | The Last Express | 1 | estable |
| `sherlock` | The Lost Files of Sherlock Holmes | 2 | estable |
| `prince` | The Prince and The Coward | 1 | estable |
| `watchmaker` | The Watchmaker | 1 | en desarrollo |
| `twp` | Thimbleweed Park | 1 | estable |
| `tony` | Tony Tough and the Night of Roasted Moths | 1 | estable |
| `tot` | Tot | 1 | estable |
| `trecision` | Trecision Adventure Module | 2 | estable |
| `tsage` | TsAGE | 5 | estable |
| `gnap` | UFOs | 1 | estable |
| `ultima` | Ultima (solo Akalabeth y Ultima I; el resto sí se compila) | 2 | en desarrollo |
| `vcruise` | V-Cruise | 3 | estable |
| `voyeur` | Voyeur | 1 | estable |
| `wage` | WAGE | 10 | estable |
| `waynesworld` | Wayne's World | 1 | en desarrollo |
| `wintermute` | Wintermute | 169 | estable |
| `zvision` | Z-Vision | 2 | estable |
| | **Total** | **12.252** | |

### Títulos de los motores no compilados

Todos, salvo los de Glk (6.026 aventuras de texto), AGS (3.838) y Director
(1.865), que por número solo están en la hoja de cálculo. Separados por `;`.

* **3 Skulls of the Toltecs** (`toltecs`): 3 Skulls of the Toltecs.
* **Access** (`access`): Amazon: Guardians of Eden; Martian Memorandum; Noctropolis.
* **ADL** (`adl`): Hi-Res Adventure #0: Mission Asteroid; Hi-Res Adventure #1: Mystery House; Hi-Res Adventure #2: Wizard and the Princess; Hi-Res Adventure #3: Cranston Manor; Hi-Res Adventure #4: Ulysses and the Golden Fleece; Hi-Res Adventure #5: Time Zone; Hi-Res Adventure #6: The Dark Crystal.
* **AGDS (Black Mirror and NiBiRu)** (`agds`): Black Mirror; NiBiRu: Age of Secrets.
* **Alcachofa** (`alcachofa`): Mort & Phil: A Movie Adventure; Mortadelo y Filemón: Balones y Patadones; Mortadelo y Filemón: Dos vaqueros chapuceros; Mortadelo y Filemón: El escarabajo de Cleopatra; Mortadelo y Filemón: La Banda de Corvino; Mortadelo y Filemón: La Sexta Secta; Mortadelo y Filemón: Mamelucos a la Romana; Mortadelo y Filemón: Operación Moscú; Mortadelo y Filemón: Terror, Espanto y Pavor.
* **Alfred Pelrock** (`pelrock`): Alfred Pelrock: En Busca de un Sueño.
* **American Laser Games** (`alg`): Crime Patrol; Crime Patrol 2: Drug Wars; Mad Dog II: The Lost Gold; Mad Dog McCree; Space Pirates; The Last Bounty Hunter; Who Shot Johnny Rock?.
* **Another World** (`awe`): Another World - Out of this World.
* **BAGEL** (`bagel`): Bagel; Hodj 'n' Podj; Hodj 'n' Podj - Art Parts; Hodj 'n' Podj - Maze O' Doom; The Space Bar.
* **Beavis and Butthead in Virtual Stupidity** (`bbvs`): Beavis and Butt-Head in Virtual Stupidity.
* **Blazing Dragons** (`dragons`): Blazing Dragons.
* **Bolt** (`bolt`): Cartoon Carnival; Labyrinth of Crete; Merlin's Apprentice.
* **Broken Sword 2.5** (`sword25`): Broken Sword 2.5: The Return of the Templars.
* **CGE** (`cge`): Sołtys.
* **CGE2** (`cge2`): Sfinx.
* **Chamber** (`chamber`): Chamber of the Sci-Mutant Priestess.
* **Chewy: Esc from F5** (`chewy`): Chewy: Esc from F5.
* **CRAB** (`crab`): Unrest.
* **Cryo Omni3D games** (`cryomni3d`): Atlantis: The Lost Tales; Versailles 1685.
* **Darkseed** (`darkseed`): Darkseed.
* **Dragon History** (`draci`): Dračí Historie.
* **Dungeon Master** (`dm`): Dungeon Master.
* **Dynamix Game Development System** (`dgds`): Heart of China; Johnny Castaway; Quarky And Quaysoo's Turbo Science; Rise of the Dragon; Sierra / Dynamix 1991 Coming Attractions Demo; Space Quest V Demo; The Adventures of Willy Beamish.
* **Eagle Eye Mysteries** (`eem`): Eagle Eye Mysteries; Eagle Eye Mysteries in London.
* **Escape From Hell** (`efh`): Escape From Hell.
* **Freescape** (`freescape`): 3D Kit Game; Castle Master; Castle Master 2: The Crypt; Dark Side; Driller; Space Station Oblivion; Total Eclipse; Total Eclipse 2.
* **Gamos** (`gamos`): 21 Solitaire; Flip-Flop; Gamos; NetWalk; Pilot Brothers: On the Track of Striped Elephant; Pilot Brothers: The Case of Serial Maniac; Vitamin; WildSnake.
* **Got** (`got`): God of Thunder.
* **Hades Challenge** (`hadesch`): Hades Challenge.
* **Harvester** (`harvester`): Harvester.
* **Hopkins FBI** (`hopkins`): Hopkins FBI.
* **Hpl1** (`hpl1`): Penumbra: Black Plague; Penumbra: Overture.
* **Hugo Trilogy** (`hugo`): Hugo II: Whodunit?; Hugo III: Jungle of Doom; Hugo's House of Horrors.
* **Hyperspace Delivery Boy!** (`hdb`): Hyperspace Delivery Boy!.
* **Hypnotix Inc.** (`hypno`): Bruce Coville's My Teacher Is an Alien; Marvel Comics Spider-Man: The Sinister Six; Soldier Boyz; Wetlands.
* **Illusions Engine** (`illusions`): Beavis and Butt-Head: Do U.; Duckman: The Graphic Adventures of a Private Dick.
* **In Cold Blood** (`icb`): In Cold Blood; The Road to El Dorado.
* **J.B. Harold series** (`harold`): J.B. Harold: Blue Chicago Blues.
* **Kingdom: The Far Reaches** (`kingdom`): Kingdom: The Far Reaches.
* **Labyrinth of Time** (`lab`): The Labyrinth of Time.
* **Lilliput** (`lilliput`): Rome: Pathway to Power; The Adventures of Robin Hood.
* **Little Big Adventure** (`twine`): Little Big Adventure; Little Big Adventure 2; Little Big Adventure Freeware Slide Show.
* **Lord Avalot d'Argent** (`avalanche`): Lord Avalot d'Argent.
* **Lost Eden** (`cryo`): Lost Eden.
* **M4** (`m4`): Orion Burger; Ripley's Believe It or Not!: The Riddle of Master Lu.
* **Macs2** (`macs2`): Der Schatz im Silbersee.
* **MacVenture** (`macventure`): Deja Vu II: Lost in Last Vegas; Deja Vu: A Nightmare Comes True!!; Shadowgate; Uninvited..
* **MADS** (`mads`): Dragonsphere; Once Upon a Forest; Return of the Phantom; Rex Nebular and the Cosmic Gender Bender; Rex Nebular Bonus Disk.
* **Magic Composer** (`composer`): Darby the Dragon; Gregory and the Hot Air Balloon; Magic Tales; Magic Tales: Baba Yaga and the Magic Geese; Magic Tales: Imo and the King; Magic Tales: Liam Finds a Story; Magic Tales: Sleeping Cub's Test of Courage; Magic Tales: The Little Samurai; Magic Tales: The Princess and the Crab.
* **Media Station** (`mediastation`): D.W. the Picky Eater; Disney presents Ariel's Story Studio; Disney's Animated Storybook: 101 Dalmatians; Disney's Animated Storybook: Hercules; Disney's Animated Storybook: Pocahontas; Disney's Animated Storybook: The Hunchback of Notre Dame; Disney's Animated Storybook: The Lion King; Disney's Animated Storybook: Winnie the Pooh and the Honey Tree; Fractured Fairy Tales: The Frog Prince; George Shrinks Interactive Storybook; IBM/Crayola Print Factory; IBM/Crayola Print Factory Holiday Activity; If You Give a Mouse a Cookie Interactive Storybook; Jan Pieńkowski Haunted House; Lamb Chop Loves Music; Magic Fairy Tales: Barbie as Rapunzel; Puzzle Castle; Stuart Little: Big City Adventures; Tonka Garage; Tonka Raceway; Tonka Search and Rescue; Tonka Workshop.
* **Might and Magic** (`mm`): Might and Magic IV: Clouds of Xeen; Might and Magic V: Darkside of Xeen; Might and Magic: Book One - Secret of the Inner Sanctum; Might and Magic: Book One - Secret of the Inner Sanctum - Enhanced; Might and Magic: Swords of Xeen; Might and Magic: World of Xeen.
* **Mission Supernova** (`supernova`): Mission Supernova - Part 1: The fate of Horst Hummel; Mission Supernova - Part 2: The Doppelgänger.
* **Mortevielle** (`mortevielle`): Mortville Manor.
* **mTropolis** (`mtropolis`): 24 Hours in Cyberspace; Adobe 24 Hours Tools Sampler; An Odyssey of Discovery: Living Science; Babe and Friends: Animated Early Reader; Biologia Cellulare Evoluzione E Varietà Della Vita; Byzantine: The Betrayal; Chomp! The Video Game; Cloud 9 CD Sampler Volume 2; Easy-Bake Kitchen; FairyTale: A True Story - Activity Center; Free Willy Activity Center; Fun With Architecture; Hercules & Xena Learning Adventure: Quest for the Scrolls; How to Build a Telemedicine Program; How to Draw the Marvel Way; I Can Be a Dinosaur Finder; I Can Be an Animal Doctor; Itacante: La Cité des Robots; IVOCLAR; King of Dragon Pass; Maisy's Playhouse; Mind Gym; Muppet Treasure Island; Mykropolis: Planet der Roboter; Obsidian; Pferd & Pony: Lass uns reiten; Pinnacle Systems miroVideo Studio DC10 Plus Tour; Poser 3 Zygote Promo; Poser 3 Zygote Sampler Disc; Purple Moon Sampler; Real Wild Child! Australian Rock Music 1950s-90s; Rugrats: Totally Angelica Boredom Buster; SPQR: The Empire's Darkest Hour; Star Trek: The Game Show; The Day The World Broke; The Facts about Genes and our Food - A Compendium; The Forgotten: It Begins; The Magic School Bus Discovers Flight; The Magic School Bus Explores Bugs; The Magic School Bus Explores the World of Animals; The Magic School Bus In Concert; The Magic School Bus Lands on Mars; The Magic School Bus Volcano Adventure; The Magic School Bus Whales & Dolphins; The Magic World of Beatrix Potter; The Mystery at Greveholm 2: The Journey to Planutus; The Times Key Stage 1 English; The Totally Techie World of Young Dilbert: Hi-Tech Hijinks; Uncle Albert's Fabulous Voyage; Uncle Albert's Magical Album; Uncle Albert's Mysterious Island; Unit: Re-Booted; Unlock the Secrets: Shadow Warrior; Wanna-Be A Dino Finder; Whitetail Extreme; Whitetail Impact; Your Notebook (with help from Amelia).
* **Mutation of JB** (`mutationofjb`): Mutation of J.B..
* **Myst 3** (`myst3`): Myst III: Exile.
* **Nancy Drew** (`nancy`): Nancy Drew: Alibi in Ashes; Nancy Drew: Curse of Blackmoor Manor; Nancy Drew: Danger by Design; Nancy Drew: Danger on Deception Island; Nancy Drew: Ghost Dogs of Moon Lake; Nancy Drew: Ghost of Thornton Hall; Nancy Drew: Labyrinth of Lies; Nancy Drew: Last Train to Blue Moon Canyon; Nancy Drew: Legend of the Crystal Skull; Nancy Drew: Message in a Haunted Mansion; Nancy Drew: Ransom of the Seven Ships; Nancy Drew: Sea of Darkness; Nancy Drew: Secret of the Old Clock; Nancy Drew: Secret of the Scarlet Hand; Nancy Drew: Secrets Can Kill; Nancy Drew: Secrets Can Kill Remastered; Nancy Drew: Shadow at the Water's Edge; Nancy Drew: Stay Tuned for Danger; Nancy Drew: The Captive Curse; Nancy Drew: The Creature of Kapu Cave; Nancy Drew: The Deadly Device; Nancy Drew: The Final Scene; Nancy Drew: The Haunted Carousel; Nancy Drew: The Haunting of Castle Malloy; Nancy Drew: The Phantom of Venice; Nancy Drew: The Secret of Shadow Ranch; Nancy Drew: The Shattered Medallion; Nancy Drew: The Silent Spy; Nancy Drew: The White Wolf of Icicle Creek; Nancy Drew: Tomb of the Lost Queen; Nancy Drew: Trail of the Twister; Nancy Drew: Treasure in the Royal Tower; Nancy Drew: Warnings at Waverly Academy; The Vampire Diaries.
* **Nikita Game Interface** (`ngi`): Full Pipe; Magic Dream; Nikita Game Interface game.
* **Phoenix VR** (`phoenixvr`): Amerzone: The Explorer's Legacy; Dracula 2: The Last Sanctuary; Dracula: Resurrection; Necronomicon: The Dawning of Darkness; Sherlock Holmes: The Mystery of the Mummy; The Cameron Files: Pharaoh's Curse; The Cameron Files: The Secret at Loch Ness; The Messenger/Louvre: The Final Curse.
* **Pink Panther** (`pink`): The Pink Panther: Hokus Pokus Pink; The Pink Panther: Passport to Peril.
* **Plumbers Don't Wear Ties** (`plumbers`): Plumbers Don't Wear Ties!.
* **Private Eye** (`private`): Private Eye.
* **QD Engine** (`qdengine`): Dog-n-cat: In the Footsteps of Unprecedented Beasts; Dog-n-cat: Island of Dr Ratiarty; Features of National Fishing; Klepa: Symbolic Rivets; Little Longnose; Marvellous Mice Adventures: Meeting Sea Rat; Marvellous Mice Adventures: Sea Rat's Birthday; Mask Show; Mom don't Worry; Pilot Brothers 3: Back Side of the Earth; Pilot Brothers 3D-2. Kennel Club Secrets; Pilot Brothers 3D. The Case of Garden Pests; QDEngine game; The Adventures of the Good Soldier Schweik; Wait for it! Issue 3. Song for a Hare.
* **Red Comrades** (`petka`): Red Comrades 1: Save the Galaxy; Red Comrades 2: For the Great Justice.
* **SAGA2** (`saga2`): Dinotopia; Faery Tale Adventure II: Halls of the Dead.
* **Sludge** (`sludge`): Above The Waves; Cubert Badbone, P.I.; Frasse and the Peas of Kejick; Full Moon; Lepton's Quest; Life Flashes By; Mandy Christmas Adventure; Nathan's Second Chance; Otto Experiment; Out Of Order; Parallax Demo; Robin's Rescue; Sam and Max Flintlocked; Shape-Shift Escape; Sludge Game; The Game Jam Game About Games, Secrets and Stuff; The Game That Takes Place on a Cruise Ship; The Interview; The Secret of Tremendous Corporation; Verb Coin; Verb Coin 2; Welcome Example.
* **Star Trek 25th Anniversary/Judgment Rites** (`startrek`): Star Trek: 25th Anniversary; Star Trek: Judgment Rites.
* **Starship Titanic** (`titanic`): Starship Titanic.
* **Tetraedge** (`tetraedge`): Amerzone; Syberia; Syberia II.
* **The Colony** (`colony`): The Colony.
* **The Fool's Errand** (`fool`): The Fool's Errand.
* **The Griffon Legend** (`griffon`): The Griffon Legend.
* **The Immortal** (`immortal`): The Immortal.
* **The Journeyman Project 2: Buried in Time** (`buried`): The Journeyman Project 2: Buried in Time.
* **The Journeyman Project: Pegasus Prime** (`pegasus`): The Journeyman Project: Pegasus Prime.
* **The Last Express** (`lastexpress`): The Last Express.
* **The Lost Files of Sherlock Holmes** (`sherlock`): The Lost Files of Sherlock Holmes: Case of the Rose Tattoo; The Lost Files of Sherlock Holmes: The Case of the Serrated Scalpel.
* **The Prince and The Coward** (`prince`): The Prince and the Coward.
* **The Watchmaker** (`watchmaker`): The Watchmaker.
* **Thimbleweed Park** (`twp`): Thimbleweed Park.
* **Tony Tough and the Night of Roasted Moths** (`tony`): Tony Tough and the Night of Roasted Moths.
* **Tot** (`tot`): Trick or Treat.
* **Trecision Adventure Module** (`trecision`): Ark of Time; Nightlong: Union City Conspiracy.
* **TsAGE** (`tsage`): Blue Force; Protostar: War on the Frontier; Return to Ringworld; Ringworld: Revenge of the Patriarch; The Lost Files of Sherlock Holmes (Logo).
* **UFOs** (`gnap`): U.F.O.s.
* **V-Cruise** (`vcruise`): A.D. 2044; Reah: Face the Unknown; Schizm: Mysterious Journey.
* **Voyeur** (`voyeur`): Voyeur.
* **WAGE** (`wage`): A Mess O' Trouble; Another Fine Mess; Camp Cantitoe; Drakmyth Castle; Enchanted Scepters; GrailQuest: Adventure in the Age of King Arthur; Ray's Maze; Twisted!; WAGE; World Builder.
* **Wayne's World** (`waynesworld`): Wayne's World.
* **Wintermute** (`wintermute`): 1 1/2 Ritter: Auf der Suche nach der hinreissenden Herzelinde; Actual Destination; Alavi Detective - Murder of Miss Rojan; Alimardan Meets Merlin; Alimardan's Mischief; Alone, Under Strange Night; Alpha Polaris; Apeiron; Art of Murder 1: FBI Confidential; Awakening: Burning Ashes; Barrow Hill - The Dark Path; Basis Octavus; Beyond the Threshold; Bickadoodle; Book of Gron Part One; BookMania; Boredom of Agustin Cordes; Carol Reed 10 - Bosch's Damnation; Carol Reed 11 - Shades Of Black; Carol Reed 12 - Profound Red; Carol Reed 13 - The Birdwatcher; Carol Reed 14 - The Fall Of April; Carol Reed 15 - Geospots; Carol Reed 16 - Quarantine Diary; Carol Reed 4 - East Side Story; Carol Reed 5 - The Colour of Murder; Carol Reed 6 - Black Circle; Carol Reed 7 - Blue Madonna; Carol Reed 8 - Amber's Blood; Carol Reed 9 - Cold Case Summer; Chaos; Chivalry is Not Dead; Colapso: Episode 1; Colors on Canvas; Conspiracao Dumont; Corrosion: Cold Winter Waiting; Dark Fall: Lost Souls; Dead City; Des Reves Elastiques Avec Mille Insectes Nommes Georges; Devil In The Capital; DFAF Adventure; Dirty Split; Dr. Bohus; Dr. Doyle - Mystery Of The Cloche Hat; Dreamcat; Dreams; Dreamscape; Eight Squares in The Garden; El Engaño de Barbapoca Chapter 1: Esta Pizza Esta de Muerte; El Engaño de Barbapoca Chapter 2: Competencia Fantasmal; Escape from the Mansion; Everyday Grey; Face Noir; Fairy Tales About Toshechka and Boshechka; Finding Hope; Five Lethal Demons; Five Magical Amulets; Forgotten Sound 1 - Revelation; Forgotten Sound 2 - Destiny; Four; FoxTail; Framed; Fred; Ghost in the Sheet; Hamlet or the last game without MMORPG features, shaders and product placement; Helga Deep In Trouble; Hor; I Must Kill...: Fresh Meat; J.U.L.I.A.; J.U.L.I.A.: Among the Stars; J.U.L.I.A.: Untold; James Peris 2: The Fountain of Eternal Drunkenness; James Peris 2: The Fountain of Eternal Drunkenness (Uncut version); James Peris: No License Nor Control; K'NOSSOS; Klaymen Episodes: Pilot; Kulivocko; La Leyenda de la Biblioteca; Life In 3 Minutes; Limbo of the Lost; Looky; Lov Mamuta; Machu Mayu; Mental Repairs Inc; Mirage; Miskatonic. Part One; Monday Starts on Saturday; Mukhtar and his Team; Murder In Tehran's Alleys 1933; Murder In Tehran's Alleys 2016; Mystic Triddle; Myth: A Guff's Tale; Night in the Fog; Night Train; Nose Bound: Episode 1; Octave; Oknytt; On the Tracks of Dinosaurs; One; One Helluva Day; Open Quest; Operación: Bomba; Paintaria; Palladion; Pangea 1/2; Papa's Daughters; Papa's Daughters Go to the Sea; Pigeons in the Park; Pizza Morgana: Episode 1 - Monsters and Manipulations in the Magical Forest; Pole Chudes; Precious Pills, Blasting Bacon and Mowing Minds; Project Joe; Project Lonely Robot; Project: Doom; Qajary Cat; Rebecca Carlson Mystery 01 - Silent Footsteps; Red Comrades 0.2: Operation F.; Reversion: The Escape; Reversion: The Meeting; Reversion: The Return; Rhiannon: Curse of the four Branches; Rosemary; Royal Mahjong: King's Journey; Satan and Sons; Securanote; Shaban; Shadow Of Nebula; Shadows on the Vatican - Act I: Greed; Shadows on the Vatican - Act II: Wrath; Shelter; Sofia's Debt; Space Invaders; Space Madness; Strange Change; Stroke of Fate: Operation Bunker; Stroke of Fate: Operation Valkyrie; Sunny; Sunrise: The Game; Susan Rose: Delicate Murder; Susan Rose: Mysterious Child; Tale of The Lost Bride and A Hidden Treasure; Tanya Grotter and the Disappearing Floor; Tanya Grotter and the Magical Double Bass; TeTRIks; The Ancient Mark - Episode 1; The Box; The Death of Erin Myers; The Driller Incident; The Golden Calf; The Guttenburg Project; The Idiot's Tale; The Kite; The Last Crown - Midnight Horror; The Lost Crown - A Ghost-Hunting Adventure; The Road to Santiago; The Shine of a Star; The Trader of Stories; The Way Of Love: Sub Zero; The White Chamber; Today, Mama!; Twilight: Dark Summer; Vsevolod; War; Wilma Tetris; Wintermute 3D Characters Technology Demo; Wintermute Engine Technology Demo; Zbang! The Game; Zilm: A Game of Reflex; Кеша в мире сказок.
* **Z-Vision** (`zvision`): Zork Nemesis: The Forbidden Lands; Zork: Grand Inquisitor.

## Compatibilidad con MSVC 2010 (C++03 + C++11 parcial)

ScummVM requiere C++11; el compilador de VS2010 (y el del XDK de Xbox 360, el
mismo frontend `cl 16.00`) solo soporta `auto`, `decltype`, lambdas, referencias
rvalue, `nullptr`, `static_assert` y `override` (como extensión). Cambios hechos:

* `backends/platform/libretro/include/msvc2010_compat.h` (incluido desde
  `portdefs.h`): define `SCUMMVM_CXX03_COMPAT` y suple el runtime C99 que falta
  (`snprintf`/`vsnprintf` conformes a C99, macros `PRI*`, `round`, `trunc`,
  `log2`, `fmin`..., `__func__`, `strtoll`...). Si el XDK ya tuviera las
  funciones matemáticas C99, definir `SCUMMVM_COMPAT_NO_C99_MATH`.
* Código común con alternativas bajo `#ifdef SCUMMVM_CXX03_COMPAT` (el código
  C++11 original sigue intacto para el resto de compiladores):
  plantillas variádicas (`emplace`, `emplace_back`, `U32String::format`,
  `readMultiple*`, `syncMultiple*`...), alias de plantillas y traits
  (`common/type_traits.h`), `std::initializer_list` y `char32_t`.
* Reescrito en C++03 válido (también compila en C++11):
  bucles `for` de rango → iteradores, inicializadores de miembros en la clase
  → listas de inicialización de los constructores, constructores delegados,
  `enum class`, `constexpr`, `final`, `noexcept`, `= default`/`= delete`,
  alias `using X = Y`, inicializaciones con llaves, lambdas convertidas a
  punteros a función (`common/path.cpp`), argumentos por defecto en
  plantillas de función y declaraciones opacas de enums.

Las herramientas usadas para la conversión automática (libclang) permiten
repetir el proceso con otros motores.

Otros problemas de VS2010 / XDK encontrados al añadir los clásicos:

* `override` en destructores (C3665): se quita.
* `#include "config.h"` en `common/scummsys.h` pasa a `<config.h>` con MSVC:
  MSVC busca los includes entre comillas también en los directorios de todos
  los ficheros abiertos, y encontraba `engines/asylum/system/config.h` o
  `engines/tinsel/config.h`. Por lo mismo `kyra/gui/debugger.h` incluye
  `<gui/debugger.h>`.
* `true`/`false` en `#if` (C1017): se usan `1`/`0`.
* Lambdas anidadas que capturan variables de la lambda exterior (C3480):
  pasan a funciones miembro estáticas (`kyra/gui/automap_eob.cpp`).
* Constantes `static const` inicializadas en la clase y definidas además en
  el `.cpp` (LNK2005 al enlazar): la definición se omite con `_MSC_VER`.
* `enum class` cuyos valores chocan con otros enumeradores (`tinsel`): se
  envuelven en `struct XScope { enum X {...}; }; typedef XScope::X X;`, que
  VS2010 acepta con `X::VALOR`.
* Lambdas convertidas a puntero a función (`+[](...){...}`): VS2010 no lo
  permite; se usa una plantilla de función (`ultima/nuvie/actors/actor_manager.h`).
* Lambdas con varias sentencias que devuelven valor necesitan el tipo de
  retorno explícito (`[=]() -> bool { ... }`); VS2010 solo lo deduce cuando el
  cuerpo es un único `return`.
* Plantillas variádicas (`ultima/nuvie/gui/widgets/msg_scroll.h`): se
  sustituyen por sobrecargas de 0 a 8 argumentos.
