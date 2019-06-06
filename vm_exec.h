/**********************************************************************

  vm.h -

  $Author$
  created at: 04/01/01 16:56:59 JST

  Copyright (C) 2004-2007 Koichi Sasada

**********************************************************************/

#ifndef RUBY_VM_EXEC_H
#define RUBY_VM_EXEC_H

typedef long OFFSET;
typedef unsigned long lindex_t;
typedef VALUE GENTRY;
typedef rb_iseq_t *ISEQ;

#if VMDEBUG > 0
#define debugs printf
#define DEBUG_ENTER_INSN(insn) \
    rb_vmdebug_debug_print_pre(ec, GET_CFP(), GET_PC());

#if OPT_STACK_CACHING
#define SC_REGS() , reg_a, reg_b
#else
#define SC_REGS()
#endif

#define DEBUG_END_INSN() \
  rb_vmdebug_debug_print_post(ec, GET_CFP() SC_REGS());

#else

#define debugs
#define DEBUG_ENTER_INSN(insn)
#define DEBUG_END_INSN()
#endif

#define throwdebug if(0)printf
/* #define throwdebug printf */

/************************************************/
#if defined(DISPATCH_XXX)
error !
/************************************************/
#elif OPT_CALL_THREADED_CODE

#define LABEL(x)  insn_func_##x
#define ELABEL(x)
#define LABEL_PTR(x) &LABEL(x)

#define INSN_ENTRY(insn) \
  static rb_control_frame_t * \
    FUNC_FASTCALL(LABEL(insn))(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp) {

#define END_INSN(insn) return reg_cfp;}

#define NEXT_INSN() return reg_cfp;

#define START_OF_ORIGINAL_INSN(x) /* ignore */
#define DISPATCH_ORIGINAL_INSN(x) return LABEL(x)(ec, reg_cfp);

/************************************************/
#elif OPT_SUBROUTINE_THREADED_CODE

#define LABEL(x)  insn_func_##x
#define ELABEL(x)
#define LABEL_PTR(x) &LABEL(x)

static inline int
cf_cnt(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp)
{
    return (int)(((rb_control_frame_t *)(ec->vm_stack + ec->vm_stack_size)) - reg_cfp);
}
static inline int
pc_cnt(const VALUE *pc, const rb_iseq_t *iseq)
{
    return (int)(pc - iseq->body->iseq_encoded);
}

static inline int
sp_cnt(rb_execution_context_t *ec, const VALUE *sp)
{
    return (int)(sp - ec->vm_stack);
}

#define SUBR_DEBUG 0

#if SUBR_DEBUG
extern int ruby_vm_subr_debug_verbose;
#define SUBR_DEBUG_VERBOSE() ruby_vm_subr_debug_verbose
#else
#define SUBR_DEBUG_VERBOSE() 0
#endif

static void vm_line_status(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp, const VALUE *reg_pc, VALUE *reg_sp, const char *insn_name);

#define INSN_ENTRY(insn) \
  static rb_control_frame_t * \
    FUNC_FASTCALL(LABEL(insn))(rb_execution_context_t *ec, rb_control_frame_t *reg_cfp, const VALUE *reg_pc, VALUE *reg_sp) { \
        if (SUBR_DEBUG_VERBOSE()) vm_line_status(ec, reg_cfp, reg_pc, reg_sp, #insn);

rb_control_frame_t *rb_insn_tail      (rb_execution_context_t *ec, rb_control_frame_t *cfp, const VALUE *pc, VALUE *sp);

rb_control_frame_t *rb_insn_tail_true (rb_execution_context_t *ec, rb_control_frame_t *cfp, const VALUE *pc, VALUE *sp);
rb_control_frame_t *rb_insn_tail_false(rb_execution_context_t *ec, rb_control_frame_t *cfp, const VALUE *pc, VALUE *sp);
rb_control_frame_t *rb_insn_tail_value(rb_execution_context_t *ec, rb_control_frame_t *cfp, const VALUE *pc, VALUE *sp, VALUE v);

#define SUBR_TAIL_TRUE()   rb_insn_tail_true (ec, reg_cfp, GET_PC(), GET_SP())
#define SUBR_TAIL_FALSE()  rb_insn_tail_false(ec, reg_cfp, GET_PC(), GET_SP())
#define SUBR_TAIL_VALUE(v) rb_insn_tail_value(ec, reg_cfp, GET_PC(), GET_SP(), (v))

#define END_INSN(insn) return rb_insn_tail  (ec, reg_cfp, GET_PC(), GET_SP());}
#define NEXT_INSN()    return rb_insn_tail_true(ec, reg_cfp, GET_PC(), GET_SP());

#define START_OF_ORIGINAL_INSN(x) /* ignore */
#define DISPATCH_ORIGINAL_INSN(x) return LABEL(x)(ec, reg_cfp, GET_PC(), GET_SP());


/************************************************/
#elif OPT_TOKEN_THREADED_CODE || OPT_DIRECT_THREADED_CODE
/* threaded code with gcc */

#define LABEL(x)  INSN_LABEL_##x
#define ELABEL(x) INSN_ELABEL_##x
#define LABEL_PTR(x) RB_GNUC_EXTENSION(&&LABEL(x))

#define INSN_ENTRY_SIG(insn) \
  if (0) fprintf(stderr, "exec: %s@(%"PRIdPTRDIFF", %"PRIdPTRDIFF")@%s:%u\n", #insn, \
                 (reg_pc - reg_cfp->iseq->body->iseq_encoded), \
                 (reg_cfp->pc - reg_cfp->iseq->body->iseq_encoded), \
                 RSTRING_PTR(rb_iseq_path(reg_cfp->iseq)), \
                 rb_iseq_line_no(reg_cfp->iseq, reg_pc - reg_cfp->iseq->body->iseq_encoded));

#define INSN_DISPATCH_SIG(insn)

#define INSN_ENTRY(insn) \
  LABEL(insn): \
  INSN_ENTRY_SIG(insn); \

/**********************************/
#if OPT_DIRECT_THREADED_CODE

/* for GCC 3.4.x */
#define TC_DISPATCH(insn) \
  INSN_DISPATCH_SIG(insn); \
  RB_GNUC_EXTENSION_BLOCK(goto *(void const *)GET_CURRENT_INSN()); \
  ;

#else
/* token threaded code */

/* dispatcher */
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && __GNUC__ == 3
#define DISPATCH_ARCH_DEPEND_WAY(addr) \
  __asm__ __volatile__("jmp *%0;\t# -- inserted by vm.h\t[length = 2]" : : "r" (addr))

#else
#define DISPATCH_ARCH_DEPEND_WAY(addr) \
                                /* do nothing */
#endif
#define TC_DISPATCH(insn)  \
  DISPATCH_ARCH_DEPEND_WAY(insns_address_table[GET_CURRENT_INSN()]); \
  INSN_DISPATCH_SIG(insn); \
  RB_GNUC_EXTENSION_BLOCK(goto *insns_address_table[GET_CURRENT_INSN()]); \
  rb_bug("tc error");

#endif /* OPT_DIRECT_THREADED_CODE */

#define END_INSN(insn)      \
  DEBUG_END_INSN();         \
  TC_DISPATCH(insn);

#define INSN_DISPATCH()     \
  TC_DISPATCH(__START__)    \
  {

#define END_INSNS_DISPATCH()    \
      rb_bug("unknown insn: %"PRIdVALUE, GET_CURRENT_INSN());   \
  }   /* end of while loop */   \

#define NEXT_INSN() TC_DISPATCH(__NEXT_INSN__)

#define START_OF_ORIGINAL_INSN(x) start_of_##x:
#define DISPATCH_ORIGINAL_INSN(x) goto  start_of_##x;

/************************************************/

#elif OPT_CALL_THREADED_CODE /* most common method */

#define INSN_ENTRY(insn) \
case BIN(insn):

#define END_INSN(insn)                        \
  DEBUG_END_INSN();                           \
  break;

#define INSN_DISPATCH()         \
  while (1) {			\
    switch (GET_CURRENT_INSN()) {

#define END_INSNS_DISPATCH()    \
default:                        \
  SDR(); \
      rb_bug("unknown insn: %ld", GET_CURRENT_INSN());   \
    } /* end of switch */       \
  }   /* end of while loop */   \

#define NEXT_INSN() goto first

#define START_OF_ORIGINAL_INSN(x) start_of_##x:
#define DISPATCH_ORIGINAL_INSN(x) goto  start_of_##x;

#endif

#define VM_SP_CNT(ec, sp) ((sp) - (ec)->vm_stack)

#ifdef MJIT_HEADER
#define THROW_EXCEPTION(exc) do { \
    ec->errinfo = (VALUE)(exc); \
    EC_JUMP_TAG(ec, ec->tag->state); \
} while (0)
#else
#if OPT_CALL_THREADED_CODE
#define THROW_EXCEPTION(exc) do { \
    ec->errinfo = (VALUE)(exc); \
    return 0; \
} while (0)
#elif OPT_SUBROUTINE_THREADED_CODE
#define THROW_EXCEPTION(exc) do { \
    ec->errinfo = (VALUE)(exc); \
    EC_JUMP_TAG(ec, ec->tag->state); \
} while (0)
#else
#define THROW_EXCEPTION(exc) return (void *)(exc)
#endif
#endif

#define SCREG(r) (reg_##r)

#define VM_DEBUG_STACKOVERFLOW 0

#if VM_DEBUG_STACKOVERFLOW
#define CHECK_VM_STACK_OVERFLOW_FOR_INSN(cfp, margin) \
    WHEN_VM_STACK_OVERFLOWED(cfp, (cfp)->sp, margin) vm_stack_overflow_for_insn()
#else
#define CHECK_VM_STACK_OVERFLOW_FOR_INSN(cfp, margin)
#endif

#define INSN_LABEL2(insn, name) INSN_LABEL_ ## insn ## _ ## name
#define INSN_LABEL(x) INSN_LABEL2(NAME_OF_CURRENT_INSN, x)

#endif /* RUBY_VM_EXEC_H */
