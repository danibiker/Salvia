#ifndef VKBD_DEF_H
#define VKBD_DEF_H 1

typedef struct {
	char norml[NLETT];
	char shift[NLETT];
	int val;	
} Mvk;

#define MATRIX(a,b) (((a) << 3) | (b))

Mvk MVk[NPLGN*NLIGN*2]={

	{ " 1" ," !" , MATRIX(7,0) },//0
	{ " 2" ," \"" ,MATRIX(7,3) },
	{ " 3" ," 3"  ,MATRIX(1,0) },
	{ " 4" ," $"  ,MATRIX(1,3) },
	{ " 5" ," %"  ,MATRIX(2,0) },
	{ " 6" ," ^"  ,MATRIX(2,3) },
	{ " 7" ," &"  ,MATRIX(3,0) },
	{ " 8" ," *"  ,MATRIX(3,3) },
	{ " 9" ," ("  ,MATRIX(4,0)},
	{ " 0" ," )"  ,MATRIX(4,3)},

	{ " q" ," Q"  ,MATRIX(7,6)}, //10
	{ " w" ," W"  ,MATRIX(1,1)},
	{ " e" ," E"  ,MATRIX(1,6)},
	{ " r" ," R"  ,MATRIX(2,1)},
	{ " t" ," T"  ,MATRIX(2,6)},
	{ " y" ," Y"  ,MATRIX(3,1)},
	{ " u" ," U"  ,MATRIX(3,6)},
	{ " i" ," I"  ,MATRIX(4,1)},
	{ " o" ," O"  ,MATRIX(4,6)},
	{ " p" ," P"  ,MATRIX(5,1)},

	{ " a" ," A"  ,MATRIX(1,2)}, //20
	{ " s" ," S"  ,MATRIX(1,5)},
	{ " d" ," D"  ,MATRIX(2,2)},
	{ " f" ," F"  ,MATRIX(2,5)},
	{ " g" ," G"  ,MATRIX(3,2)},
	{ " h" ," H"  ,MATRIX(3,5)},
	{ " j" ," J"  ,MATRIX(4,2)},
	{ " k" ," K"  ,MATRIX(4,5)},	
	{ " l" ," L"  ,MATRIX(5,2)},
	{ " ;" ," :"  ,MATRIX(6,2)},

	{ " z" ," Z"  ,MATRIX(1,4)},//30
	{ " x" ," X"  ,MATRIX(2,7)},
	{ " c" ," C"  ,MATRIX(2,4)},
	{ " v" ," V"  ,MATRIX(3,7)},
	{ " b" ," B"  ,MATRIX(3,4)},
	{ " n" ," N"  ,MATRIX(4,7)},
	{ " m"," M"   ,MATRIX(4,4)},
	{ " ,"," <"   ,MATRIX(5,7)},
	{ " ."," >"   ,MATRIX(5,4)},
	{ " /" ," /"  ,MATRIX(6,7)},

	{ "PG2","PG2" ,-2}, //40
	{ "Esc","Esc" ,MATRIX(7,7)},
	{ "F1" ,"F1"  ,MATRIX(0,4)},
	{ "F3" ,"F3"  ,MATRIX(0,5)},
	{ "F5" ,"F5"  ,MATRIX(0,6)},
	{ "F7" ,"F7"  ,MATRIX(0,3)},
	{ "C=" ,"c=" , MATRIX(7,4)},
	{ "Spc" ,"Spc",MATRIX(7,4)},
	{ "<-" ,"<-"  ,MATRIX(7,1)},
	{ "Ent" ,"Ent",MATRIX(0,1)},


	{ "DEL" ,"DEL" ,MATRIX(0,0)}, //50
	{ "UP" ,"F3"  ,MATRIX(0,7)|128},
	{ "CLR" ,"CLR" ,MATRIX(0,2)},
	{ "CUD" ,"F5"  ,MATRIX(0,7)},
	{ "SHL" ,"F6"  ,MATRIX(1,7)},
	{ " @ " ," @ " , MATRIX(5,6)},
	{ " ^ " ," ^ "  ,MATRIX(6,6)},	
	{ " : " ," : "  ,MATRIX(5,5)},
	{ " = "," = " ,MATRIX(6,5)},
	{ "SHR","SHR" ,MATRIX(6,4)},


	{ "LF" ,"LF" , MATRIX(0,2)|128 },//60
	{ " 5" ," 5" ,MATRIX(2,0)},
	{ "RG" ,"RG"  ,MATRIX(0,2)},
	{ " 4" ," $"  ,MATRIX(1,3)},
	{ " 5" ," %"  ,MATRIX(2,0) },
	{ " 6" ," ^"  ,MATRIX(2,3) },
	{ " 7" ," &"  ,MATRIX(3,0) },
	{ " 8" ," *"  ,MATRIX(3,3) },
	{ " 9" ," ("  ,MATRIX(4,0)},
	{ " 0" ," )"  ,MATRIX(4,3)},

	{ "HOM","HOM" ,MATRIX(6,3)}, //70
	{ "DW" ,"DW" ,MATRIX(0,7)}, 
	{ " - " ," - " , MATRIX(5,3)},
	{ "CTL" ,"CTL" , MATRIX(7,2)},
	{ " * "," * " ,MATRIX(6,1)},
	{ " + "," + " ,MATRIX(5,0)},
	{ "Pnd","Pnd" ,MATRIX(6,0)},
	{ "Ins","Ins" ,-1},
	{ "Hme","Hme" ,-1},
	{ "PgU","PgU" ,-1},

	{ "Tab" ,"Tab",-1}, //80	
	{ " [" ,"  {" ,-1},
	{ " ]" ,"  }" ,-1},
	{ "Ent" ,"Ent",MATRIX(0,1)},
	{ "Del" ,"Del",MATRIX(0,0)},
	{ "R/S" ,"End",MATRIX(7,7)},
	{ "c= " ,"c= ",MATRIX(7,5)},
	{ "SHL" ,"SHL",MATRIX(1,7)},
	{ "SHR" ,"SHR" ,MATRIX(6,4)},	
	{ "SPC" ,"SPC",MATRIX(7,4)},	
	
	{ " PG1","PG1" ,-2},//90
	{ "GUI","GUI"   ,-14},
	{ "ABT","ABT"  ,-13},
	{ "CTR" ,"CTR" ,-12},
	{ "R/S" ,"R/S" ,-11},
	{ "SHI" ,"SHI" ,-10},
	{ "M/J","M/J",-5},
	{ "COL" ,"COL",-3},
	{ "Ent" ,"Ent",MATRIX(0,1)},
	{ "KBD" ,"KBD",-4},

} ;

#endif
