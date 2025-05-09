ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/hal/soc/ae250
ccflags-y += -I$(src)/chip_specific_extensions/Andes_V5_RV32RV64
asflags-y += -I$(srctree)/include/FreeRTOS
asflags-y += -I$(src)/chip_specific_extensions/Andes_V5_RV32RV64

#
# FreeRTOS needs to define an "portasmHANDLE_INTERRUPT" assembler macro to tell
# the FreeRTOS kernel which external interrupt handler to call.
# Besides, it also needs to specify the linker variable called "__freertos_irq_stack_top"
# that holds the highest address of the interrupt stack. We set __freertos_irq_stack_top
# to equal the value of linker variable "_stack".
#
DEFINES = -DportasmHANDLE_INTERRUPT=FreeRTOS_IRQ_handler -D__freertos_irq_stack=__stack_start -D__freertos_irq_stack_top=_stack

ccflags-y += $(DEFINES)
asflags-y += $(DEFINES)

obj-y := port.o portASM.o FreeRTOS_tick_config.o
