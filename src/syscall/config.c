#include "config.h"
#include "functions.h"
#include <scheduler/cpu.h>
#include <libkern_base.h>
#include <shared/addresses.h>
#include <anscheduler/thread.h>
#include <anscheduler/functions.h>
#include <string.h> // memcpy
#include "entry.h"

void syscall_initialize() {
  uint64_t star = (8L << 32) | (0x18L << 48);
  msr_write(MSR_STAR, star);
  msr_write(MSR_SFMASK, 0x200); // interrupts will be disabled by syscall
}

void syscall_initialize_thread(thread_t * thread) {
  // use the maximal pages for everything because we're a bunch of ballers
  uint64_t pageIndex = 0xFFFFFFFFFL - thread->stack;

  // map the address in the task's address space to the thread structure
  task_t * task = thread->task;
  page_t physical = anscheduler_vm_physical(((uint64_t)thread) >> 12);
  anscheduler_lock(&task->vmLock);
  anscheduler_vm_map(task->vm, pageIndex, physical, 3);
  anscheduler_unlock(&task->vmLock);

  // setup the code structure (someone, please, just MURDER ME)
  memcpy(&thread->state.callCode.code1,
         "\x48\x31\xC0\x8E\xD0\xFA\x55\x48\x89\xE5\x48\xBC", 0xc);
  memcpy(&thread->state.callCode.code2, "\x48\xb8", 2);
  memcpy(&thread->state.callCode.code3, "\xff\xe0", 2);

  uint64_t stack = (uint64_t)anscheduler_thread_kernel_stack(task, thread);
  stack += 0x1000;

  // debugging: try doing nothing, with `jmp $`
  //thread->state.callCode.code2[0] = 0xeb;
  //thread->state.callCode.code2[1] = 0xfe;
  uint64_t kernCode = (uint64_t)&syscall_configure_stack;

  print("interrupt stack is ");
  printHex(stack);
  print(", code start is ");
  printHex(((void *)&thread->state.callCode) - (void *)thread);
  print(", kerncall is ");
  printHex(kernCode & 0xffffff);
  print("\n");

  thread->state.callCode.stack = stack;
  thread->state.callCode.kernCall = kernCode;
  void * ptr = &thread->state.callCode;
  int i;
  for ( i = 0; i < 0x16; i++) {
    unsigned char ch = *((const char *)(ptr + i));
    printHex((uint64_t)ch);
    print(" ");
  }
  print("\n");

  uint16_t flags;
  uint64_t page = anscheduler_vm_lookup(thread->task->vm, 0x100, &flags);
  print("got value ");
  printHex((page << 12) | (uint64_t)flags);
  print("\n");
}

void syscall_setup_for_thread(thread_t * thread) {
  uint64_t page = 0xFFFFFFFFFL - thread->stack;
  uint64_t addr = page << 12;
  if (addr & (1L << 47)) {
    addr |= 0xffff000000000000;
  }
  thread_t * ptr = (thread_t *)addr;
  msr_write(MSR_LSTAR, (uint64_t)&ptr->state.callCode);
}

