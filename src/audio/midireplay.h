#pragma once

#include <stdint.h>
#include <string.h>
#include <vector>

/* Estado de la cancion en curso, reconstruido del stream MIDI, para volver a
 * mandarselo a un sintetizador nuevo.
 *
 * Por que hace falta: los juegos mandan los resets, los SysEx GS, los bancos,
 * los programas y los controladores al empezar la cancion y ya no los repiten.
 * Si el sintetizador se cierra y se vuelve a abrir a media partida (apagar y
 * encender el sintetizador MIDI desde el menu), la instancia nueva arranca con
 * su estado por defecto y la cancion sigue sonando con los instrumentos
 * equivocados.  Lo usa MidiSynth con el Roland SC-55, que recibe el stream
 * crudo; con TinySoundFont ya se reaplica ChannelState.
 *
 * Que se guarda:
 *   - los SysEx desde el ultimo reset (GM/GS/XG/MT-32), incluido el reset;
 *   - por canal: el ultimo valor de cada controlador (salvo los momentaneos:
 *     pedales 64..67, data increment/decrement y los mensajes de modo), el
 *     programa y el pitch bend.
 * Las notas no: lo que este sonando al cerrar se pierde, y lo siguiente que
 * toque la cancion ya sale con el instrumento correcto.
 *
 * Todo en el hilo que escribe el MIDI; build() se llama con la emulacion
 * parada (al reabrir el sintetizador desde el menu). */
class MidiReplay {
public:
	MidiReplay() { clear(); }

	void clear()
	{
		resetChannels();
		m_sysexLog.clear();
		m_running = m_status = 0;
		m_dataLen = m_dataNeed = 0;
		m_inSysex = false;
		m_sysex.clear();
	}

	bool empty() const
	{
		if (!m_sysexLog.empty()) return false;
		for (int ch = 0; ch < 16; ch++) {
			if (m_program[ch] >= 0 || m_pitch[ch] >= 0) return false;
			for (int c = 0; c < 128; c++) if (m_cc[ch][c] >= 0) return false;
		}
		return true;
	}

	void feed(uint8_t b)
	{
		/* Tiempo real (incluido 0xFF): no lleva estado de cancion y puede
		 * aparecer en medio de cualquier mensaje. */
		if (b >= 0xF8) return;

		if (m_inSysex) {
			if (b < 0x80) {
				if (m_sysex.size() < SYSEX_MAX) m_sysex.push_back(b);
				return;
			}
			m_inSysex = false;
			endSysex();
			if (b == 0xF7) return;
		}

		if (b & 0x80) {
			m_dataLen = 0;
			if (b < 0xF0) {
				m_status = m_running = b;
				m_dataNeed = (uint8_t)(((b & 0xF0) == 0xC0 || (b & 0xF0) == 0xD0) ? 1 : 2);
				return;
			}
			m_running = 0;   /* system common cancela el running status */
			m_status = 0;
			if (b == 0xF0) { m_inSysex = true; m_sysex.clear(); }
			else if (b == 0xF1 || b == 0xF3) { m_status = b; m_dataNeed = 1; }
			else if (b == 0xF2) { m_status = b; m_dataNeed = 2; }
			return;
		}

		if (!m_status) {
			if (!m_running) return;
			m_status = m_running;
			m_dataNeed = (uint8_t)(((m_status & 0xF0) == 0xC0 || (m_status & 0xF0) == 0xD0) ? 1 : 2);
			m_dataLen = 0;
		}
		m_data[m_dataLen++] = b;
		if (m_dataLen < m_dataNeed) return;
		if (m_status < 0xF0) channelMessage(m_status, m_data[0], m_dataNeed > 1 ? m_data[1] : 0);
		m_dataLen = 0;
		m_status = 0;
	}

	/* Los bytes que dejan un sintetizador recien arrancado como la cancion. */
	void build(std::vector<uint8_t>& out) const
	{
		out.insert(out.end(), m_sysexLog.begin(), m_sysexLog.end());
		for (int ch = 0; ch < 16; ch++) {
			const uint8_t cs = (uint8_t)(0xB0 | ch);
			/* Banco antes que programa: el program change lo usa. */
			putCC(out, cs, ch, 0);
			putCC(out, cs, ch, 32);
			if (m_program[ch] >= 0) { out.push_back((uint8_t)(0xC0 | ch)); out.push_back((uint8_t)m_program[ch]); }
			for (int c = 0; c < 128; c++) {
				if (c == 0 || c == 32 || c == 6 || c == 38 || (c >= 98 && c <= 101)) continue;
				putCC(out, cs, ch, c);
			}
			/* Parametro RPN/NRPN que la cancion dejo seleccionado y luego su
			 * valor (normalmente el rango de pitch bend, RPN 0). */
			if (m_lastWasNrpn[ch]) { putCC(out, cs, ch, 99); putCC(out, cs, ch, 98); }
			else                   { putCC(out, cs, ch, 101); putCC(out, cs, ch, 100); }
			putCC(out, cs, ch, 6);
			putCC(out, cs, ch, 38);
			if (m_pitch[ch] >= 0) {
				out.push_back((uint8_t)(0xE0 | ch));
				out.push_back((uint8_t)(m_pitch[ch] & 0x7F));
				out.push_back((uint8_t)((m_pitch[ch] >> 7) & 0x7F));
			}
		}
	}

private:
	enum { SYSEX_MAX = 1024, SYSEX_LOG_MAX = 16 * 1024 };   /* que el replay entero quepa en la cola MIDI del SC-55 (32 KB) */

	int16_t m_cc[16][128];      /* -1 = la cancion no lo ha tocado */
	int16_t m_program[16];
	int32_t m_pitch[16];
	bool    m_lastWasNrpn[16];
	std::vector<uint8_t> m_sysexLog;   /* mensajes completos, F0 .. F7 */

	uint8_t m_running, m_status, m_data[2], m_dataLen, m_dataNeed;
	bool    m_inSysex;
	std::vector<uint8_t> m_sysex;      /* sin F0 ni F7 */

	void resetChannels()
	{
		for (int ch = 0; ch < 16; ch++) {
			for (int c = 0; c < 128; c++) m_cc[ch][c] = -1;
			m_program[ch] = -1;
			m_pitch[ch] = -1;
			m_lastWasNrpn[ch] = false;
		}
	}

	void putCC(std::vector<uint8_t>& out, uint8_t cs, int ch, int c) const
	{
		if (m_cc[ch][c] < 0) return;
		out.push_back(cs);
		out.push_back((uint8_t)c);
		out.push_back((uint8_t)m_cc[ch][c]);
	}

	void channelMessage(uint8_t st, uint8_t d0, uint8_t d1)
	{
		const int ch = st & 0x0F;
		switch (st & 0xF0) {
			case 0xB0:
				if (d0 >= 120) {
					if (d0 == 121) {   /* Reset All Controllers */
						static const uint8_t rac[] = { 1, 11, 64, 65, 66, 67, 98, 99, 100, 101 };
						for (size_t i = 0; i < sizeof(rac); i++) m_cc[ch][rac[i]] = -1;
						m_pitch[ch] = -1;
					}
					return;   /* el resto son mensajes de modo / apagar notas */
				}
				if ((d0 >= 64 && d0 <= 67) || d0 == 96 || d0 == 97) return;   /* momentaneos */
				m_cc[ch][d0] = d1;
				if (d0 == 98 || d0 == 99)   m_lastWasNrpn[ch] = true;
				if (d0 == 100 || d0 == 101) m_lastWasNrpn[ch] = false;
				return;
			case 0xC0:
				m_program[ch] = d0;
				return;
			case 0xE0:
				m_pitch[ch] = (int32_t)d0 | ((int32_t)d1 << 7);
				return;
		}
	}

	bool isReset() const
	{
		const size_t n = m_sysex.size();
		const uint8_t* s = n ? &m_sysex[0] : 0;
		if (n >= 4 && s[0] == 0x7E && s[2] == 0x09 && s[3] >= 0x01 && s[3] <= 0x03) return true;      /* GM on/off */
		if (n >= 8 && s[0] == 0x41 && s[2] == 0x42 && s[3] == 0x12 &&
		    s[4] == 0x40 && s[5] == 0x00 && s[6] == 0x7F) return true;                                 /* GS reset */
		if (n >= 6 && s[0] == 0x43 && (s[1] & 0xF0) == 0x10 && s[2] == 0x4C &&
		    s[3] == 0x00 && s[4] == 0x00 && s[5] == 0x7E) return true;                                 /* XG on */
		if (n >= 5 && s[0] == 0x41 && s[2] == 0x16 && s[3] == 0x12 && s[4] == 0x7F) return true;     /* MT-32 reset */
		return false;
	}

	void endSysex()
	{
		if (m_sysex.empty() || m_sysex.size() >= SYSEX_MAX) return;
		if (isReset()) {
			/* Lo anterior queda anulado por el reset. */
			resetChannels();
			m_sysexLog.clear();
		}
		if (m_sysexLog.size() + m_sysex.size() + 2 > SYSEX_LOG_MAX) return;
		m_sysexLog.push_back(0xF0);
		m_sysexLog.insert(m_sysexLog.end(), m_sysex.begin(), m_sysex.end());
		m_sysexLog.push_back(0xF7);
	}
};
