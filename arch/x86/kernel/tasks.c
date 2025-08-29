/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <eduos/stdio.h>
#include <eduos/stdlib.h>
#include <eduos/string.h>
#include <eduos/tasks.h>
#include <eduos/errno.h>
#include <eduos/processor.h>

size_t* get_current_stack(void)
{
	task_t* curr_task = current_task;

	// use new page table
	write_cr3(curr_task->page_map);

	return curr_task->last_stack_pointer;
}

int create_default_frame(task_t* task, entry_point_t ep, void* arg)
{
	size_t *stack;
	struct state *stptr;
	size_t state_size;

	if (BUILTIN_EXPECT(!task, 0))
		return -EINVAL; 

	if (BUILTIN_EXPECT(!task->stack, 0))
		return -EINVAL;

	memset(task->stack, 0xCD, KERNEL_STACK_SIZE);

	/* The difference between setting up a task for SW-task-switching
	 * and not for HW-task-switching is setting up a stack and not a TSS.
	 * This is the stack which will be activated and popped off for iret later.
	 */
	stack = (size_t*) (task->stack + KERNEL_STACK_SIZE - 16);		// => stack is 16byte aligned

	/* Only marker for debugging purposes, ... */
	*stack-- = 0xDEADBEEF;
	/* the first-function-to-be-called's arguments, ... */
	*stack-- = (size_t) arg;

	/* and the "caller" we shall return to.
	 * This procedure cleans the task after exit. */
	*stack = (size_t) leave_kernel_task;

	/* Next bunch on the stack is the initial register state. 
	 * The stack must look like the stack of a task which was
	 * scheduled away previously. */

	state_size = sizeof(struct state) - 2*sizeof(size_t);
	stack = (size_t*) ((size_t) stack - state_size);

	stptr = (struct state *) stack;
	memset(stptr, 0x00, state_size);
	stptr->esp = (size_t)stack + state_size;

	stptr->int_no = 0xB16B00B5;
	stptr->error =  0xC03DB4B3;

	/* The instruction pointer shall be set on the first function to be called
	   after IRETing */
	stptr->eip = (size_t)ep;
	stptr->cs = 0x08;
	stptr->ds = stptr->es = 0x10;
	stptr->eflags = 0x1202;

	// the creation of a kernel-level tasks don't change the IOPL level
	// => useresp & ss isn't required

	/* Set the task's stack pointer entry to the stack we have crafted right now. */
	task->last_stack_pointer = (size_t*)stack;

	return 0;
}
