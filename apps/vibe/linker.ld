ENTRY(Reset_Handler)

APP_FLASH_ORIGIN = 0;
APP_FLASH_LENGTH = DEFINED(APP_FLASH_LENGTH) ? APP_FLASH_LENGTH : 220K;

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x400;

MEMORY
{
  FLASH (rx)  : ORIGIN = APP_FLASH_ORIGIN, LENGTH = APP_FLASH_LENGTH
  /* Top 2 KiB is shared runtime RAM: GOT, vectors, handoff, update marker. */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 78K
}

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  .isr_vector :
  {
    __vector_start = .;
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
    __vector_end = .;
  } > FLASH :text

  .app_manifest ORIGIN(FLASH) + 0x200 :
  {
    KEEP(*(.app_manifest))
  } > FLASH :text

  ASSERT(SIZEOF(.app_manifest) == 64, "Application manifest has invalid size")

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > FLASH :text

  .got :
  {
    __got_start = .;
    *(.got)
    *(.got.plt)
    __got_end = .;
  } > FLASH :text

  ASSERT(SIZEOF(.got) <= 0x400, "Application GOT exceeds reserved runtime RAM")

  .ARM.extab :
  {
    *(.ARM.extab*)
  } > FLASH :text

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH :text

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH :text

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH :text

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH :text

  .trace_fmt 0 (INFO) :
  {
    KEEP(*(.trace_fmt))
  }

  ASSERT(SIZEOF(.trace_fmt) <= 0x8000, "App trace format table exceeds its ID namespace")

  _sidata = LOADADDR(.data);
  __data_load_start = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH :data

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM :data

  . = ALIGN(8);
  _end = .;
  PROVIDE(end = .);

  ASSERT(_end + _Min_Stack_Size <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflowed")

  /DISCARD/ :
  {
    *(.note*)
    *(.comment*)
  }
}
