MEMORY
{
MEM1(RWX)  : ORIGIN = 0x30000000,  LENGTH = 0x79000 /* 1123  512k */
MEM2(RWX)  : ORIGIN = 0x20000000,  LENGTH = 0x80000
}
__kernel_stack = 0x2007fff8 ;


ENTRY(__start)

SECTIONS {
	    .text : {
		. = ALIGN(0x4) ;
		__stext = . ;
		*(.text)
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.stub)
		*(.gnu.linkonce.t*)
		*(.glue_7t)
		*(.glue_7)
		*(.jcr)
		*(.init)
		*(.fini)
		
		/* This is special code area at the end of the normal
		   text section.  It contains a small lookup table at
		   the start followed by the code pointed to by entries
		   in the lookup table.  
		 */
		. = ALIGN (4) ;
		PROVIDE(__ctbp = .);
		*(.call_table_data)
		*(.call_table_text)
		
		. = ALIGN(0x10) ;
		__etext = . ;
    } > MEM1
   
    .rodata :
    {
      *(.rodata*)
      . = ALIGN(0x4);
        __erodata = .;
    } > MEM1
    .data :
    {
      . = ALIGN(0x4);
      __data_start__ = .;
        *(.data*)
          . = ALIGN(0x4);
        __data_end__ = .;
    } >MEM2 AT > MEM1
    .bss :
    {
      . = ALIGN(0x4);
      __bss_start__ = .;
        *(.bss)
         . = ALIGN(0x4);
        __bss_end__ = .;
          *.(COMMON)
    } >MEM2
}
