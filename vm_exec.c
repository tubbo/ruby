/* -*-c-*- */
/**********************************************************************

  vm_exec.c -

  $Author$

  Copyright (C) 2004-2007 Koichi Sasada

**********************************************************************/

#include <math.h>

#if VM_COLLECT_USAGE_DETAILS
static void vm_analysis_insn(int insn);
#endif

#if VMDEBUG > 0
#define DECL_SC_REG(type, r, reg) register type reg_##r

#elif defined(__GNUC__) && defined(__x86_64__)
#define DECL_SC_REG(type, r, reg) register type reg_##r __asm__("r" reg)

#elif defined(__GNUC__) && defined(__i386__)
#define DECL_SC_REG(type, r, reg) register type reg_##r __asm__("e" reg)

#elif defined(__GNUC__) && defined(__powerpc64__)
#define DECL_SC_REG(type, r, reg) register type reg_##r __asm__("r" reg)

#else
#define DECL_SC_REG(type, r, reg) register type reg_##r
#endif
/* #define DECL_SC_REG(r, reg) VALUE reg_##r */

#if VM_DEBUG_STACKOVERFLOW
NORETURN(static void vm_stack_overflow_for_insn(void));
static void
vm_stack_overflow_for_insn(void)
{
    rb_bug("CHECK_VM_STACK_OVERFLOW_FOR_INSN: should not overflow here. "
	   "Please contact ruby-core/dev with your (a part of) script. "
	   "This check will be removed soon.");
}
#endif

#if OPT_DIRECT_THREADED_CODE || OPT_TOKEN_THREADED_CODE

static VALUE
vm_exec_core(rb_execution_context_t *ec, VALUE initial)
{

#if OPT_STACK_CACHING
#if 0
#elif __GNUC__ && __x86_64__
    DECL_SC_REG(VALUE, a, "12");
    DECL_SC_REG(VALUE, b, "13");
#else
    register VALUE reg_a;
    register VALUE reg_b;
#endif
#endif

#if defined(__GNUC__) && defined(__i386__)
    DECL_SC_REG(const VALUE *, pc, "di");
    DECL_SC_REG(rb_control_frame_t *, cfp, "si");
#define USE_MACHINE_REGS 1

#elif defined(__GNUC__) && defined(__x86_64__)
    DECL_SC_REG(const VALUE *, pc, "14");
    DECL_SC_REG(rb_control_frame_t *, cfp, "15");
#define USE_MACHINE_REGS 1

#elif defined(__GNUC__) && defined(__powerpc64__)
    DECL_SC_REG(const VALUE *, pc, "14");
    DECL_SC_REG(rb_control_frame_t *, cfp, "15");
#define USE_MACHINE_REGS 1

#else
    register rb_control_frame_t *reg_cfp;
    const VALUE *reg_pc;
#endif

#if USE_MACHINE_REGS

#undef  RESTORE_REGS
#define RESTORE_REGS() \
{ \
  VM_REG_CFP = ec->cfp; \
  reg_pc  = reg_cfp->pc; \
}

#undef  VM_REG_PC
#define VM_REG_PC reg_pc
#undef  GET_PC
#define GET_PC() (reg_pc)
#undef  SET_PC
#define SET_PC(x) (reg_cfp->pc = VM_REG_PC = (x))
#endif

#if OPT_TOKEN_THREADED_CODE || OPT_DIRECT_THREADED_CODE
#include "vmtc.inc"
    if (UNLIKELY(ec == 0)) {
	return (VALUE)insns_address_table;
    }
#endif
    reg_cfp = ec->cfp;
    reg_pc = reg_cfp->pc;

#if OPT_STACK_CACHING
    reg_a = initial;
    reg_b = 0;
#endif

  first:
    INSN_DISPATCH();
/*****************/
 #include "vm.inc"
/*****************/
    END_INSNS_DISPATCH();

    /* unreachable */
    rb_bug("vm_eval: unreachable");
    goto first;
}

const void **
rb_vm_get_insns_address_table(void)
{
    return (const void **)vm_exec_core(0, 0);
}

#elif OPT_SUBROUTINE_THREADED_CODE
#include "vm.inc"
#include "vmtc.inc"

const void **
rb_vm_get_insns_address_table(void)
{
    return (const void **)insns_address_table;
}

typedef rb_control_frame_t * (*subr_insn_func_t)(rb_execution_context_t *, rb_control_frame_t *,
                                                 const VALUE *reg_pc, VALUE *reg_sp);
typedef rb_control_frame_t * (*subr_insn_func_cont_t)(rb_execution_context_t *, rb_control_frame_t *,
                                                      const VALUE *reg_pc, VALUE *reg_sp, void *dst);

static void
subr_exec_log(rb_control_frame_t *reg_cfp, const rb_iseq_t *iseq)
{
    if (SUBR_DEBUG_VERBOSE()) {
        char buff[0x100];
        fprintf(stderr, "pc:%d %s\n",
                pc_cnt(reg_cfp->pc, iseq),
                rb_raw_obj_info(buff, 0x100, (VALUE)iseq));
    }
}

static VALUE
vm_exec_subr_code_cont(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp, const rb_iseq_t *iseq, int start)
{
    VM_ASSERT(start > 0);
    subr_exec_log(reg_cfp, iseq);

    subr_insn_func_cont_t func_ptr = (subr_insn_func_cont_t)iseq->body->subr_encoded_jump;
    void *dst = iseq->body->subr_entry_points[start];
    VALUE val = (VALUE)(func_ptr)(ec, reg_cfp, reg_cfp->pc, reg_cfp->sp, dst);
    return val;
}

static inline VALUE
vm_exec_subr_code(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp, const rb_iseq_t *iseq)
{
    subr_exec_log(reg_cfp, iseq);
    VM_ASSERT(pc_cnt(reg_cfp->pc, iseq) == 0);
    subr_insn_func_t func_ptr = (subr_insn_func_t)iseq->body->subr_encoded;
    VALUE val = (VALUE)(func_ptr)(ec, reg_cfp, reg_cfp->pc, reg_cfp->sp);
    return val;
}

static inline VALUE
vm_exec_core(rb_execution_context_t *ec, rb_control_frame_t *cfp)
{
    VALUE val = vm_exec_subr_code(ec, cfp, cfp->iseq);
    return val;
}

static inline VALUE
vm_exec_core_cont_with_initial(rb_execution_context_t *ec, rb_control_frame_t *cfp, VALUE initial)
{
    cfp->sp[0] = initial;
    cfp->sp++;
    int start = cfp->pc - cfp->iseq->body->iseq_encoded;
    return vm_exec_subr_code_cont(ec, cfp, cfp->iseq, start);
}

static void
vm_exec_setup_push(rb_control_frame_t *cfp, VALUE initial)
{
    cfp->sp[0] = initial;
    cfp->sp++;
}

static inline VALUE
vm_exec_core_until_finish(rb_execution_context_t *ec, rb_control_frame_t *cfp, VALUE initial)
{
    while (1) {
        vm_exec_setup_push(cfp, initial);

        const rb_iseq_t *iseq = cfp->iseq;
        int start = pc_cnt(cfp->pc, iseq);

        VM_ASSERT(rb_obj_is_iseq((VALUE)iseq));
        VM_ASSERT(start != 0);
        VM_ASSERT(initial != Qundef);

        initial = vm_exec_subr_code_cont(ec, cfp, iseq, start);

        if (VM_FRAME_FINISHED_P(cfp)) {
            return initial;
        }
        cfp = ec->cfp;
    }
}

#else /* OPT_CALL_THREADED_CODE */
#include "vm.inc"
#include "vmtc.inc"

const void **
rb_vm_get_insns_address_table(void)
{
    return (const void **)insns_address_table;
}

static VALUE
vm_exec_core(rb_execution_context_t *ec, VALUE initial)
{
    register rb_control_frame_t *reg_cfp = ec->cfp;
    rb_thread_t *th;

    while (1) {
	reg_cfp = ((rb_insn_func_t) (*GET_PC()))(ec, reg_cfp);

	if (UNLIKELY(reg_cfp == 0)) {
	    break;
	}
    }

    if ((th = rb_ec_thread_ptr(ec))->retval != Qundef) {
	VALUE ret = th->retval;
	th->retval = Qundef;
	return ret;
    }
    else {
	VALUE err = ec->errinfo;
	ec->errinfo = Qnil;
	return err;
    }
}
#endif
