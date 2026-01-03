#include "constant.h"

Logger *Constant::g_Logger;

Constant::Constant(){
	g_Logger = new Logger(LOG_PATH);
}

Constant::~Constant(){
	delete g_Logger;
}