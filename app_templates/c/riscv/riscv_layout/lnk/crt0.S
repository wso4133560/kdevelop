//start from __start, 
//(0)initialize vector table
//(1)initialize all registers
//(2)prepare initial reg values for user process
//(3)initialize supervisor mode stack pointer
//(4)construct ASID Table
//(5)prepare PTE entry for user process start virtual address
//(6)creat a mapping between VPN:0 and PFN:0 for kernel
//(7)set VBR register
//(8)enable EE and MMU
//(9)jump to the main procedure using jsri main

.text
.export vector_table
.align  10
.export __start	
vector_table:	//totally 256 entries
	.long __start

	//.rept 31
	//.long exception_handler
	//.endr

	.long exception_handler_othererror
	.long exception_handler_buserror	/* bus error */	
	.rept 29
	.long exception_handler_othererror
	.endr

	.long gpio_0_irq_handle     /* 00 upate by lijie*/
	.long gpio_1_irq_handle
	.long __dummy 		/* 2---SPI Flash Controller		*/
	.long __dummy 		/* 3---efpga flash interrupt 	*/
	.long timer_0_intr 		/* 4---TIMERO                  	*/
	.long timer_1_intr 		/* 5---TIMER1                  	*/
	.long timer_2_intr 		/* 6---TIMER2                  	*/
	.long timer_3_intr 		/* 7---TIMER3                  	*/
	.long uart_irq_handle 				/* 8---UARTO                  	*/
	.long uart1_irq_handle 				/* 9---UART1                 	*/
	.long uart2_irq_handle 				/* 10--UART2           			*/
	.long uart3_irq_handle 				/* 11--UART3          			*/
	.long spi_mstr0_intr_handle	/* 12--SPI Master            	*/
	.long spi_slv0_intr_handle	/* 13--SPI Slave0            	*/
	.long i2c_irq_handle0 		/* 14--I2C0            	*/
	.long i2c_irq_handle1 		/* 15--I2C1            	*/
	.long i2c_irq_handle2 		/* 16--I2C2             */
	.long i2c_irq_handle3 		/* 17--I2C3            	*/
	.long i2c_irq_handle4 		/* 18--I2C4             */
	.long i2c_irq_handle5 		/* 19--I2C5              */
	.long __dummy 				/* 20--UART MASTER              */
	.long wdt_irq_handle0    /* 21--WDT1                 	*/
	.long wdt_irq_handle1 	/* 22--WDT2                	*/
	.long wdt_irq_handle2 	/* 23--WDT3                	*/
	.long wdt_irq_handle3 	/* 24--WDT4                	*/	
	.long ddr_0_intr 				/* 25--DDRO                		*/
	.long ddr_1_intr 				/* 26--DDR2                		*/
	.long ddr_2_intr 				/* 27--DDR2  					*/
	.long ddr_3_intr 				/* 28--DDR3                  	*/
	.long app_intr0_handle 				/* 29--APP TOPO					*/
	.long app_intr1_handle 				/* 30--APP TOP1         		*/
	.long rf_0_intr 				/* 31--RF TOPO          		*/
	.long rf_1_intr 				/* 32--RF TOP1          		*/
	.long rf_2_intr 				/* 33--RF TOP2          		*/
	.long rf_3_intr 				/* 34--RF TOP3          		*/
	.long rf_4_intr 				/* 35--RF TOP4          		*/
	.long rf_5_intr 				/* 36--RF TOP5          		*/
	.long rf_6_intr 				/* 37--RF TOP6          		*/
	.long rf_7_intr 				/* 38--RF TOP7         		*/
	.long rf_8_intr 				/* 39--RE TOP8          		*/
	.long rf_9_intr 				/* 40--RE TOP9          		*/
	.long rf_10_intr 				/* 41--RF TOP10         		*/
	.long rf_11_intr 				/* 42--RF TOP11         		*/
	.long rf_12_intr 				/* 43--RF TOP12         		*/
	.long rf_13_intr 				/* 44--RE TOP13         		*/
	.long rf_14_intr 				/* 45--RF TOP14         		*/
	.long rf_15_intr 				/* 46--RF TOP15         		*/
	.long __dummy 				/* 47--CGRA1 PEA0       		*/
	.long __dummy 				/* 48--CGRA1 PEA1       		*/
	.long __dummy 				/* 49--CGRA1 PEA2       		*/
	.long __dummy 				/* 50--CGRA1 PEA3       		*/
	.long __dummy 				/* 51--Float point#     		*/
	.long __dummy 				/* 52--Active           		*/
	.long __dummy 				/* 53--Transp           		*/
	.long __dummy 				/* 54--Intrp sinc       		*/
	.long ahb_dmac_intr 		/* 55--AHB DMAC         		*/
	.long axi_dmac_0_irq_handle 		/* 56--AXI_DMAC0 */
	.long axi_dmac_1_irq_handle 		/* 57--AXI_DMAC1 */
	.long axi_dmac_2_irq_handle         /* 58--AXI_DMAC2*/
	.long axi_dmac_3_irq_handle         /* 59--AXI_DMAC3*/
	.long __dummy 				/* 60       		*/
	.long axi_dmac_5_irq_handle         /* 61--AXI_DMAC5*/





__start:
//initialize all registers
	movi r0, 0
	movi r1, 0
	movi r2, 0
	movi r3, 0
	movi r4, 0
	movi r5, 0
	movi r6, 0
	movi r7, 0
	movi r8, 0
	movi r9, 0
	movi r10, 0
	movi r11, 0
	movi r12, 0
	movi r13, 0
	movi r14, 0
	movi r15, 0
.ifdef hgpr
	movi r16, 0
	movi r17, 0
	movi r18, 0
	movi r19, 0
	movi r20, 0
	movi r21, 0
	movi r22, 0
	movi r23, 0
	movi r24, 0
	movi r25, 0
	movi r26, 0
	movi r27, 0
.endif
	movi r28, 0
.ifdef hgpr
	movi r29, 0
	movi r30, 0
	movi r31, 0
.endif

	//set VBR
	lrw r2, vector_table
	mtcr r2, cr<1,0>

	//enable IE and EE bit of psr
	mfcr r2, cr<0,0>
	bseti r2, r2, 6
	bseti r2, r2, 8
	bseti r2, r2, 31
	mtcr r2, cr<0,0>


	//initialize kernel stack
	lrw  r14, __kernel_stack
	lrw  r3, 0x40
	subu r4, r14, r3
	lrw  r5, 0x0
	INIT_KERLE_STACK:
	addi r4, 0x4
	st.w r5, (r4)
	cmphs r14, r4
	bt  INIT_KERLE_STACK


    //copy data section
lrw r1, __erodata
lrw r2, __data_start__
lrw r3, __data_end__

subu r3, r2
cmpnei r3, 0
bf .L_loop0_done

.L_loop0:
	ldw r0, (r1, 0)
	stw r0, (r2, 0)
	addi r1, 4
	addi r2, 4
	subi r3, 4
	cmpnei r3, 0
	bt .L_loop0
	
.L_loop0_done:
//clean BSS
	lrw r1, __bss_start__
	lrw r2, __bss_end__
	
	movi r0, 0
	subu r2, r1
	cmpnei r2, 0
	bf .L_loop1_done
	
.L_loop1:
	stw r0, (r1,0)
	addi r1, 4
	subi r2, 4
	cmpnei r2, 0
	bt .L_loop1
	
.L_loop1_done:
	movi r0, 0
	movi r1, 0
	movi r2, 0
	movi r3, 0

 
__to_main:
	lrw r15, __exit
	lrw r0,main
	jmp r0
	mov r0, r0
	mov r0, r0
	mov r0, r0
	mov r0, r0
	mov r0, r0

.export __exit
__exit:
	mfcr    r1, cr<0,0>
	movih   r1, 0xFFFF
	mtcr    r1, cr<11,0>
	movi    r1, 0xFFF
	lrw     r0, __kernel_stack
	st      r1, (r0)

.export __fail
__fail:
	movih   r1, 0xEEEE
	mtcr    r1, cr<11,0>
	movi    r1, 0xEEE
	lrw     r0, __kernel_stack
	st      r1, (r0)

__dummy:
	br      __fail

.data
	.long 0

.weak exception_handler_buserror
.weak exception_handler_othererror
.weak exception_handler
.weak gpio_0_irq_handle     
.weak gpio_1_irq_handle
.weak timer_0_intr          
.weak timer_1_intr          
.weak timer_2_intr          
.weak timer_3_intr  
.weak uart_irq_handle        
.weak spi_mstr0_intr_handle 
.weak spi_slv0_intr_handle  
.weak i2c_irq_handle0       
.weak i2c_irq_handle1       
.weak i2c_irq_handle2       
.weak i2c_irq_handle3       
.weak i2c_irq_handle4       
.weak i2c_irq_handle5       
.weak wdt_irq_handle0       
.weak wdt_irq_handle1       
.weak wdt_irq_handle2       
.weak wdt_irq_handle3       
.weak ddr_0_intr            
.weak ddr_1_intr            
.weak ddr_2_intr            
.weak ddr_3_intr            
.weak app_intr0_handle      
.weak app_intr1_handle      
.weak rf_0_intr             
.weak rf_1_intr             
.weak rf_2_intr             
.weak rf_3_intr             
.weak rf_4_intr             
.weak rf_5_intr             
.weak rf_6_intr             
.weak rf_7_intr             
.weak rf_8_intr             
.weak rf_9_intr             
.weak rf_10_intr            
.weak rf_11_intr            
.weak rf_12_intr            
.weak rf_13_intr            
.weak rf_14_intr            
.weak rf_15_intr     
.weak ahb_dmac_intr         
.weak axi_dmac_0_irq_handle 
.weak axi_dmac_1_irq_handle 
.weak axi_dmac_2_irq_handle 
.weak axi_dmac_3_irq_handle   
.weak eth_irq_handle 
.weak axi_dmac_5_irq_handle 
