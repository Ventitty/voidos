ENTRY(_ResetVector)

MEMORY
{
  iram0_0_seg (RX) : ORIGIN = 0x40080000, LENGTH = 0x20000  /* 128 Ko IRAM */
  dram0_0_seg (RW) : ORIGIN = 0x3FFB0000, LENGTH = 0x30000  /* 192 Ko DRAM */
}

SECTIONS
{
  .iram0.text :
  {
    _stext = .;
    KEEP(*(.iram0.entry))
    *(.iram0.text*)
    *(.text*)
    *(.literal*)
    _etext = .;
  } > iram0_0_seg

  .iram0.vectors : ALIGN(1024)
  {
    KEEP(*(.iram0.vectors))
  } > iram0_0_seg

  .dram0.data :
  {
    _sdata = .;
    *(.data*)
    *(.rodata*)
    _edata = .;
  } > dram0_0_seg

  .dram0.bss :
  {
    . = ALIGN(4);
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > dram0_0_seg

  . = ALIGN(16);
  _heap_start = .;

  _stack_top = ORIGIN(dram0_0_seg) + LENGTH(dram0_0_seg);
  _heap_end   = _stack_top - 0x4000;
}
