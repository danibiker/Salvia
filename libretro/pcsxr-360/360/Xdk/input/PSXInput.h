void PSxInputReadPort(PadDataS* pad, int port);

/* Vibration control from the SIO PAD command path (DualShock cmd 0x4D).
 *
 *   port      — 0..3 (Xbox 360 user index).  Solo se honran 0/1 que son los
 *               dos puertos del PSX; 2/3 estan reservados para multitap.
 *   smallMotor — DualShock "small" motor: digital, valor != 0 = ON.  Mapea
 *                al XInput RIGHT motor (high-frequency, grip derecho).
 *   bigMotor   — DualShock "big" motor: analog 0..255.  Mapea al XInput
 *                LEFT motor (low-frequency, grip izquierdo) escalado a
 *                0..65535.
 *
 *  Implementado en PSXInput.cpp.  Llama directamente a XInputSetState.
 *  Es seguro llamar a esta funcion desde el plugin PAD durante la
 *  ejecucion del SIO poll. */
#ifdef __cplusplus
extern "C"
#endif
void PSxInputSetVibration(int port, unsigned char smallMotor, unsigned char bigMotor);