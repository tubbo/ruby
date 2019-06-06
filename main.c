/**********************************************************************

  main.c -

  $Author$
  created at: Fri Aug 19 13:19:58 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#undef RUBY_EXPORT
#include "ruby.h"
#include "vm_debug.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#if RUBY_DEVEL && !defined RUBY_DEBUG_ENV
# define RUBY_DEBUG_ENV 1
#endif
#if defined RUBY_DEBUG_ENV && !RUBY_DEBUG_ENV
# undef RUBY_DEBUG_ENV
#endif
#ifdef RUBY_DEBUG_ENV
#include <stdlib.h>
#endif

int
main(int argc, char **argv)
{
#ifdef RUBY_DEBUG_ENV
    ruby_set_debug_option(getenv("RUBY_DEBUG"));
#endif
#ifdef HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif

    ruby_sysinit(&argc, &argv);
    {
	RUBY_INIT_STACK;
	ruby_init();
	return ruby_run_node(ruby_options(argc, argv));
    }
}

void
xxx(void)
{
    asm("mov %rdi, %r12");
    asm("mov %rsi, %r13");
    asm("mov %r12, %rdi");
    asm("mov %r13, %rsi");
    asm("jmp 12345");
    asm("test %eax, %eax");
    asm("jz 12345");
    asm("jnz 12345");
}

void *rb_insn_tail(void *ec, void *cfp);

VALUE
yyy(void)
{
    void *ec, *cfp;

    asm("mov %%r12, %0;"
        "mov %%r13, %1;"
        : "=r" (ec), "=r" (cfp)
        : /* no input */
        : /* no clobbing */);

    //rb_insn_tail(ec, cfp);
    return (VALUE)ec;
}
