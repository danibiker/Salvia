// Included after RL, MOVLL, IRQ handling and the PPC dispatcher are declared.
// Handles board-declared idle/device loads and on-chip peripheral register accesses.
#ifndef FBNEO_SH3_DRC_MOVLL_SERVICE_H
#define FBNEO_SH3_DRC_MOVLL_SERVICE_H
static bool sh3_drc_service_movll(unsigned opcode)
{
 if(!sh3_drc_enabled) {
  return false;
 }
 const unsigned hi = opcode & 0xf000u;
 const unsigned lo = opcode & 15u;
 const bool is_read  = (hi == 0x6000u) && ((lo <= 2u) || (lo >= 4u && lo <= 6u));
 const bool is_write = (hi == 0x2000u) && ((lo <= 2u) || (lo >= 4u && lo <= 6u));
 if(!is_read && !is_write) {
  return false;
 }
 const unsigned size = 1u << (lo & 3u);
 unsigned address = 0;
 if(is_read) {
  address = m_r[(opcode >> 4u) & 15u];
 } else {
  const unsigned rn = (opcode >> 8u) & 15u;
  address = (lo >= 4u) ? (m_r[rn] - size) : m_r[rn];
 }
 if(address & (size - 1u)) {
  return false;
 }
 const unsigned physical = address & AM;
 unsigned idle_ram = 0, idle_pc = 0, device = 2;
 const unsigned handler_pc = (m_delay ? m_pc : m_pc + 2u) & AM;
 bool eligible = false;
 if((opcode & 0xf00fu) == 0x6002u && address < 0xe0000000u) {
  if(physical == sh3_ppc_state.mirror_watch && sh3_idle_watch_ram && sh3_idle_watch_pc) {
   // Live idle metadata, including the PC observed AFTER delay-slot prologue.
   idle_ram = *sh3_idle_watch_ram; idle_pc = *sh3_idle_watch_pc;
   eligible = (physical == idle_ram) &&
    ((physical & ~(unsigned)SH3_PAGEM) == sh3_ppc_state.mirror_page) &&
    ((uintptr_t)sh3_ppc_state.read_mirror >= SH3_MAXHANDLER) &&
    ((uintptr_t)MemMapR[physical >> SH3_SHIFT] == sh3_ppc_state.mirror_handler);
  } else {
   for(unsigned i = 0; i < 2; ++i) {
    const Sh3DrcDeviceRead &read = sh3_device_reads[i];
    if(read.callback && physical == read.address &&
       (uintptr_t)MemMapR[physical >> SH3_SHIFT] == read.handler &&
       ReadLong[read.handler] == read.callback) {
     device = i; eligible = true; break;
    }
   }
  }
 }
 if(!eligible && address >= SH3_UPPER_REGBASE && address <= SH3_UPPER_REGEND) {
  eligible = true;
 }
 if(!eligible) {
  return false;
 }
 // Exactly the original interpreter instruction prologue. In a delay slot
 // the branch target and PR are already committed by the compiled branch.
 if(m_delay) m_delay = 0; else m_pc += 2;
 m_ppc = m_pc;
 switch(opcode & 0xf00fu) {
  case 0x6000u: MOVBL((UINT16)opcode); break;
  case 0x6001u: MOVWL((UINT16)opcode); break;
  case 0x6002u: MOVLL((UINT16)opcode); break;
  case 0x6004u: MOVBP((UINT16)opcode); break;
  case 0x6005u: MOVWP((UINT16)opcode); break;
  case 0x6006u: MOVLP((UINT16)opcode); break;
  case 0x2000u: MOVBS((UINT16)opcode); break;
  case 0x2001u: MOVWS((UINT16)opcode); break;
  case 0x2002u: MOVLS((UINT16)opcode); break;
  case 0x2004u: MOVBM((UINT16)opcode); break;
  case 0x2005u: MOVWM((UINT16)opcode); break;
  case 0x2006u: MOVLM((UINT16)opcode); break;
  default: return false;
 }
 if(m_test_irq && !m_delay) sh4_check_pending_irq();
 EAT(1); // base instruction cost AFTER handler/IRQ, as in timerhack
 return true;
}
#endif
