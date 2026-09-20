ENTRY(_ResetVector)

MEMORY
{
  iram0_0_seg (RX) : ORIGIN = 0x40080000, LENGTH = 0x18000  /* 96 Ko IRAM noyau */
  dram0_0_seg (RW) : ORIGIN = 0x3FFB0000, LENGTH = 0x30000  /* 192 Ko DRAM */
}

_user_iram_base = 0x40080000 + 0x18000;
_user_iram_size = 0x8000;

SECTIONS
{
  .iram0.text :
  {
    _stext = .;
    KEEP(*(.iram0.entry))
    *(.iram0.literal .iram0.text* .literal .literal.* .text .text.*)
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
  _stack_top_cpu1 = _stack_top - 0x4000;

  _user_dram_size = 0x4000;
  _user_dram_base = _stack_top_cpu1 - 0x2000 - _user_dram_size;

  _heap_end = _user_dram_base;
}
