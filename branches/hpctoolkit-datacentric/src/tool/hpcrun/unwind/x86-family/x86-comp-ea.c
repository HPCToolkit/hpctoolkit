/*
  This file is used to compute the effective address touched by the memory instruction.
  We identify an instruction a load or store and use XED to decode it.
*/
#include "x86-decoder.h"
#include "x86-comp-ea.h"
#include "fnbounds_interface.h"
#include <lib/isa-lean/x86/instruction-set.h>
/*
  effective address = displacement + base + scale * index
*/


typedef struct {
        uint64_t        eflags;
        uint64_t        ip;
        uint64_t        eax;
        uint64_t        ebx;
        uint64_t        ecx;
        uint64_t        edx;
        uint64_t        esi;
        uint64_t        edi;
        uint64_t        ebp;
        uint64_t        esp;
        uint64_t        r8;     /* 0 in 32-bit mode */
        uint64_t        r9;     /* 0 in 32-bit mode */
        uint64_t        r10;    /* 0 in 32-bit mode */
        uint64_t        r11;    /* 0 in 32-bit mode */
        uint64_t        r12;    /* 0 in 32-bit mode */
        uint64_t        r13;    /* 0 in 32-bit mode */
        uint64_t        r14;    /* 0 in 32-bit mode */
        uint64_t        r15;    /* 0 in 32-bit mode */
} pfm_pebs_core_smpl_entry_t;


uint64_t get_reg_value(xed_reg_enum_t reg, pfm_pebs_core_smpl_entry_t *pebs_ent)
{
  if (x86_isReg_AX(reg)) {
    return pebs_ent->eax;
  }
  else if (x86_isReg_BX(reg)) {
    return pebs_ent->ebx;
  }
  else if (x86_isReg_CX(reg)) {
    return pebs_ent->ecx;
  }
  else if (x86_isReg_DX(reg)) {
    return pebs_ent->edx;
  }
  else if (x86_isReg_SI(reg)) {
    return pebs_ent->esi;
  }
  else if (x86_isReg_DI(reg)) {
    return pebs_ent->edi;
  }
  else if (x86_isReg_BP(reg)) {
    return pebs_ent->ebp;
  }
  else if (x86_isReg_SP(reg)) {
    return pebs_ent->esp;
  }
  else if (x86_isReg_R8(reg)) {
    return pebs_ent->r8;
  }
  else if (x86_isReg_R9(reg)) {
    return pebs_ent->r9;
  }
  else if (x86_isReg_R10(reg)) {
    return pebs_ent->r10;
  }
  else if (x86_isReg_R11(reg)) {
    return pebs_ent->r11;
  }
  else if (x86_isReg_R12(reg)) {
    return pebs_ent->r12;
  }
  else if (x86_isReg_R13(reg)) {
    return pebs_ent->r13;
  }
  else if (x86_isReg_R14(reg)) {
    return pebs_ent->r14;
  }
  else if (x86_isReg_R15(reg)) {
    return pebs_ent->r15;
  }
  else if (x86_isReg_IP(reg)) {
    return pebs_ent->ip;
  }
  /* should not reach here */
  return 0;
}

uint64_t ea_compute(xed_decoded_inst_t *xptr, int i, pfm_pebs_core_smpl_entry_t *pebs_ent) {
  uint64_t effective_address = 0;
  xed_reg_enum_t base = xed_decoded_inst_get_base_reg(xptr, i);
  if (base != XED_REG_INVALID) {
    effective_address += get_reg_value(base, pebs_ent);
  }
  xed_reg_enum_t index = xed_decoded_inst_get_index_reg(xptr, i);
  if (index != XED_REG_INVALID) {
    if (xed_decoded_inst_get_scale(xptr, i) != 0) {
      xed_uint_t scale = xed_decoded_inst_get_scale(xptr, i);
      effective_address += get_reg_value(index, pebs_ent) * scale;
    }
    else // no scale
      effective_address += get_reg_value(index, pebs_ent);
  }
  xed_uint_t disp_bits = xed_decoded_inst_get_memory_displacement_width(xptr, i);
  if (disp_bits) {
    xed_int64_t disp = xed_decoded_inst_get_memory_displacement(xptr, i);
    effective_address += disp;
  }
  return effective_address;
}

void* pebs_off_one_fix(void *ins)
{
  void *fn_start, *fn_end;
  fnbounds_enclosing_addr(ins, &fn_start, &fn_end, NULL);
  if (ins < fn_start || ins > fn_end) return 0;

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  void *pins = fn_start;
  while (pins <= fn_end) {
    xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_decode(xptr, (uint8_t*)pins, 15);
    if (xed_error != XED_ERROR_NONE) {
      pins++;
      continue;
    }
    xed_uint_t len = xed_decoded_inst_get_length(xptr);
    if(pins + len == ins) {
      return pins;
    }
    pins += len;
  }
  return 0;
}

uint64_t compute_effective_addr(void *ins, void *ent, int *ls)
{
  uint64_t effective_address = 0;

  pfm_pebs_core_smpl_entry_t *pebs_ent = (pfm_pebs_core_smpl_entry_t *) ent;
  if (!ins) ins = (void *)pebs_ent->ip;

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_decoded_inst_zero_keep_mode(xptr);

  xed_decode(xptr, (uint8_t*)ins, 15);
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);

  const xed_operand_t *op0 = xed_inst_operand(xi, 0);
  const xed_operand_t *op1 = xed_inst_operand(xi, 1);

  xed_operand_enum_t op0_name = xed_operand_name(op0);
  xed_operand_enum_t op1_name = xed_operand_name(op1);

  if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) {
    //------------------------------------------------------------------------
    // storing a register to memory 
    //------------------------------------------------------------------------
    *ls = 2;
    return ea_compute(xptr, 0, pebs_ent);
  } else if ((op1_name == XED_OPERAND_MEM0) && (op0_name == XED_OPERAND_REG0)) {
    //----------------------------------------------------------------------
    // loading a register from memory 
    //----------------------------------------------------------------------
    *ls = 1;
    return ea_compute(xptr, 0, pebs_ent);
  }
  return effective_address;
}
