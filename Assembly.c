/*
 *  Registers :
 *
 *  -> AL/AH/AX/EAX/RAX: Accumulator
 *  -> BL/BH/BX/EBX/RBX: Base index (for use with arrays)
 *  -> CL/CH/CX/ECX/RCX: Counter (for use with loops and strings)
 *  -> DL/DH/DX/EDX/RDX: Extend the precision of the accumulator 
 *			 (e.g. combine 32-bit EAX and EDX for 64-bit integer 
operations in 32-bit code)
 *  -> SI/ESI/RSI: Source index for string operations.
 *  -> DI/EDI/RDI: Destination index for string operations.
 *  -> SP/ESP/RSP: Stack pointer for top address of the stack.
 *  -> BP/EBP/RBP: Stack base pointer for holding the address of the current 
stack frame.
 *  -> IP/EIP/RIP: Instruction pointer. Holds the program counter, the current 
instruction address.
 *
 *  Segment registers :
 *
 *  -> CS: Code
 *  -> DS: Data
 *  -> SS: Stack
 *  -> ES: Extra data
 *  -> FS: Extra data #2
 *  -> GS: Extra data #3
 *
 *  ABI : 
 *
 *  -> Unix usually follows AT&T syntax rather than Intel, i.e src before dest.
 *  -> mnemonic has letter indicating size of operands
 *		q - qword : registers used are rax, rbx etc..
 *		l - long or dword : registers used are eax, ebx etc
 *		w - word : registers used are ax, bx, cx etc
 *		b - byte : registers used are al, bl, cl etc
 *  -> Register names suffixed by % (%eax), and immediate operands by $ ($0x1)
 *  -> For AMD64, 128 Byte red-zone at RSP - 128
 *	-> Optimization, functions do not need to allocate stack space
 *	-> Signals and interrupt handlers avoid this area
 *	-> gcc -mno-red-zone to disable redzone
 *
 *  -> Some instructions use ESP as implicit register operand
 *  -> Frame pointer is usually (not always) stored in EBP
 *  -> Stack aligned on 4B boundary (Some compilers use even larger alignment),
 *			128 byte redzone below the stack.
 *  -> Effective address is segment_register:displacement(base, index, scale)
 *	-> Segmentation not widely used in most OSes
 *	-> Mostly used for thread local storage and in kernel 
 *	        eg) movl %gs: 0x10,	%eax
 *	-> Below is using implicit segment register, i.e, this value should be
 *	        added to segment register to get the address in the correct 
segment.
 *	-> EA = Base + (index * scale) + Disp.
 *	-> Eg) In an array of structs.
 *		typedef struct type_s {
 *			int var1;
 *	        int var2;
 *		} type_t;
 *
 *	        type_t arrofstr[6];
 *
 *	        We want to access var2 of arrofstr[5]
 *
 *
 *	        Base    = 4 , points to var2, should be GP reg
 *	        index   = 5 , points to 6th element, should be a GP reg
 *	        scale   = 8 , size of struct, can be 1, 2, 4, 8..
 *	        Disp    = &arrofstr, any 32 bit const.
 *
 *	        So now effective address = Base + (index * scale) + Disp.
 *
 *  Calling conventions (For x86-64/System V/Solaris) :
 *
 *  -> Arguments are passed on to the stack in reverse order (the last argument 
is pushed first)
 *  -> The first six integer or pointer arguments are passed in registers RDI, 
RSI, RAX
 *		RDX, RCX, R8, and R9, 
 *  -> Return value for simple integer types in EAX else on the stack
 *  -> For IA-32, Solaris ABI makes registers EAX, ECX, EDX volatile (scratch, 
caller-saved, temporary)) 
 *			Non-volatile (preserved, callee-saved) registers are 
EBX, EDI, ESI, EBP, ESP
 *  -> For x86-64, System-V ABI the calling convention is RDI, RSI, RDX, RCX,
 *			R8, R9, XMM0–7 are caller saved.
 *
 *  Function Prologue :                                                         
  
 *
 *  -> Push old bp to stack, for later restoration. 
 *  -> move old stack frames sp onto bp
 *	-> if needed save arguments onto the stack, and save preserved 
registers.
 *	gcc -msave-args to save args onto stack for debugging, affects 
performance.
 *  -> move sp to grow the stack to required stack frame size
 *  ->  main:                           pushl  %ebp
 *		main+1:                         movl   %esp,%ebp
 *		main+3:                         subl   $0x20,%esp
 *	-> Above, can be achieved using enter instruction
 *		enter $0x20, $0 (more complex prologues can be obtained by 
values other
 *		than 1).
 *
 *
 *  Function Epilogue :
 *
 *  -> Replace sp with bp, so that sp is restored to value before prologue
 *  -> pop bp of the stack, so it is restored to value before prologue
 *  -> returns to calling function, by popping previous frame's program counter
 *	and jumping to it
 *  -> movl %esp, %ebp
 *     popl %ebp
 *     ret
 *  -> the first two instructions are replaced by leave. So return is basically
 *	leave and ret.
 *	 
 *  -> The stack can be accessed with either bp related addressing or sp 
related 
 *	addressing, as in stack addresses are computed based on sp or bp 
(Mostly bp)
 *	
 *  Registers for AMD64 :
 *
 *	RAX - return value
 *	RBX - GP, non volatile
 *	RCX - 4th argument, volatile
 *	RDX - 3rd argument, volatile
 *	RSI - 2nd argument, volatile
 *	RDI - 1st argument, volatile
 *	RBP - frame pointer, non volatile
 *	RSP - stack pointer, non volatile
 *	R8  - 5th argument, volatile
 *	R9  - 6th argument, volatile
 *      R10 & R11 - GP, volatile
 *	R12 - R15 - GP, non volatile
 * 
 *  Stack for AMD64:
 * 
 *  -> Stack grows down so higher address to lower address. 
 *  -> Stack contents from higher to lower address when a new function is 
called:
 *	-> Stack contents of caller 
 *	-> all higher arguments till 7th argument is saved onto stack
 *	-> return address (RIP)
 *	-> RBP of caller (after this RSP is now RBP)
 *	-> 6 arguments are saved onto the stack(optional)
 *	-> Saved non-volatile registers
 *	-> local variables (RSP will point here)
 *	-> red zone
 *
 *  -> Push instruction decreases SP (stack grows downwards) and stores value.
 *  -> Pop instruction increases SP and stores value.
 *
 *  Stack Optimizations :
 *
 *  -> Leaf Optimization : Leaf functions(those who dont call other functions)
 *		don't create their own stack frame 
 *  -> Tail Call Optimization : Recycle current stack frame for
 *		functions called from the tail of current function 
 *  -> Function Inlining : Function call is replaced by direct
 *		inclusion of function code
 *  -> To check if function is optimized do func::dis and check if function
 *		prologue is present. If it is then 
 *  
 *	-> O0 : no optimization
 *	-> O1 : basic optimization, faster, smaller code, not much compile time
 *	-> O2 : better than O1 in terms of optimization, recommended
 *	-> O3 : highest optimization, more compile time and memory footprint
 *	-> Os : similar to O2, but do not increase generated code size
 *	-> Og : better than O0, but superior debugging and fast compilation
 *	-> Ofast : O3 plus other flags, breaks standard compliance 
 *
	
 *
 * Trap Frames :
 *
 * -> Created by interrupt or exception(trap) handlers
 * -> Contains values of all registers at the time of trap, also saved onto 
stack
 * -> Not visible in user core dump (kernel address space)
 *
*/  

int bar(int a, int b){
   return a + b;
}

int foo(int a, int b){
   bar(a + 1 , b + 1);
}

int main() {
   int a = 1;
   int b = 2;
   foo(a, b);
}

/* ***************************O0 optimization****************************
 *
 * Assembly code:
 *
 * > bar::dis
 * bar:               pushl  %ebp
 * bar+1:             movl   %esp,%ebp
 * bar+3:             movl   0xc(%ebp),%eax
 * bar+6:             movl   0x8(%ebp),%edx
 * bar+9:             leal   (%edx,%eax),%eax <- store return value in eax
 * bar+0xc:           popl   %ebp
 * bar+0xd:           ret    
 * > foo::dis
 * foo:               pushl  %ebp
 * foo+1:             movl   %esp,%ebp
 * foo+3:             subl   $0x8,%esp
 * foo+6:             movl   0xc(%ebp),%eax
 * foo+9:             leal   0x1(%eax),%edx
 * foo+0xc:           movl   0x8(%ebp),%eax
 * foo+0xf:           addl   $0x1,%eax
 * foo+0x12:          movl   %edx,0x4(%esp)
 * foo+0x16:          movl   %eax,(%esp)
 * foo+0x19:          call   -0x2c    <bar>
 * foo+0x1e:          leave  
 * foo+0x1f:          ret    
 * > main::dis
 * main:              pushl  %ebp
 * main+1:            movl   %esp,%ebp
 * main+3:            subl   $0x18,%esp
 * main+6:            movl   $0x1,-0x4(%ebp)
 * main+0xd:          movl   $0x2,-0x8(%ebp)
 * main+0x14:         movl   -0x8(%ebp),%eax
 * main+0x17:         movl   %eax,0x4(%esp)
 * main+0x1b:         movl   -0x4(%ebp),%eax
 * main+0x1e:         movl   %eax,(%esp)
 * main+0x21:         call   -0x46    <foo>
 * main+0x26:         leave  
 * main+0x27:         ret    
 *
 *
 * Stack Frame:
 *
 * > $C       
 * feffe998 bar(1, 2, feffe998, feffe9b8, 2, 1)
 * feffe9b8 main+0x26(1, feffe9dc, feffe9e4, 8050d30, 0, 0)
 * feffe9d0 _start+0x83(1, feffeaec, 0, feffeaf8, feffeb10, feffeb1f)
 *
 * 0xfeffe988:     0xfeffe998   <- ebp of foo   
 * 0xfeffe98c:     foo+0x1e     <- return address after bar call   
 * 0xfeffe990:     2            <- movl %edx, 0x4(%esp)   
 * 0xfeffe994:     3            <- movl %eax,(%esp) 
 * 0xfeffe998:     0xfeffe9b8   <- frame pointer of main   
 * 0xfeffe99c:     main+0x26    <- return address after foo call   
 * 0xfeffe9a0:     1            <- movl %eax,(%esp) : main+0x1e   
 * 0xfeffe9a4:     2            <- movl %eax,0x4(%esp) : main+0x17  
 * 0xfeffe9a8:     0xfeffe998      
 * 0xfeffe9ac:     0xfeffe9b8      
 * 0xfeffe9b0:     2		<- movl $0x2,-0x8(%ebp) : main+0xd              
 * 0xfeffe9b4:     1		<- movl $0x1,-0x4(%ebp) : main+6              
 * 0xfeffe9b8:     0xfeffe9d0	<- ebp of _start   
 * 0xfeffe9bc:     _start+0x83	<- return address after main call    
 * 0xfeffe9c0:     1               
 * 0xfeffe9c4:     0xfeffe9dc      
 * 0xfeffe9c8:     0xfeffe9e4      
 * 0xfeffe9cc:     _fini    
 *
 *
 */


/* **************************O1,02,03 optimization***************************
 * 
 * > foo::dis
 * foo:                            repz ret 
 * > bar::dis
 * bar:                            movl   0x8(%esp),%eax
 * bar+4:                          addl   0x4(%esp),%eax
 * bar+8:                          ret    
 * > main::dis
 * main:                           repz ret 
 *
 */


