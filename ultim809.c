/*
 * Driver for the Ultim809 system by Matt Sarnoff.
 * http://pshs.cc
 *
 * History:
 * Version 0.1 - April 13, 2011
 *   - initial version
 *
 * Memory map:
 * 0x0000-0x3FFF - RAM bank 0
 * 0x4000-0x7FFF - RAM bank 1
 * 0x8000-0xBFFF - Selectable RAM bank (controlled by VIA port A)
 * 0xC000-0xC3FF - VIA
 * 0xC400-0xC7FF - UART (simulated with stdin and stdout)
 * 0xC800-0xCBFF - SPI input register (not currently emulated)
 * 0xCC00-0xCC01 - TMS9918A (emulated as TMS9928A)
 * 0xCC02-0xCC03 - YM2149
 * 0xD000-0xDFFF - external I/O (not emulated)
 * 0xE000-0xFFFF - ROM
 *
 * The Service key triggers a non-maskable interrupt and enters the monitor.
 */

/* If set to 1, stdin will be used as the input to the UART.
   If MESS is run from the terminal, it can be used to simulate
   a serial console.
   Note, however, that this will block the entire UI and in general
   should not be used. */
#define UART_USES_STDIN		1

/* Main crystal frequency, 8 MHz */
#define MAIN_CLOCK	8000000

#define EMULATE_KEYBOARD  0

#include "emu.h"
#include "includes/ultim809.h"

// Replace with the path to checksums.h,
// generated by 'gensums' as part of the ROM build process
#include "PATH_TO_CHECKSUMS_H/checksums.h"

#if UART_USES_STDIN
#include <termios.h>
#endif


READ8_HANDLER(via_r);
WRITE8_HANDLER(via_w);
READ8_HANDLER(flipflop_sel_set);
READ8_HANDLER(flipflop_sel_clear);

WRITE8_HANDLER(debug_print_byte_hex)
{
	printf("%02x\n", data);
}

READ8_HANDLER(debug_print_n)
{
	printf("%d\n", (cpu_get_reg(space->machine().device("maincpu"), M6809_CC) & 0x08) != 0);
	return 0;
}

#if UART_USES_STDIN
/* We intercept all reads from the UART so we can grab characters from stdin.
   Note that this blocks, and really shouldn't be used at all, except for testing. */
READ8_DEVICE_HANDLER(ultim809_ins8250_r)
{
	switch (offset)
	{
		/* Intercept reads of the RHR, and get a character
		   if the divisor latch is disabled. */
		case 0:
			if ((ins8250_r(device, 3) & 0x80) == 0) /* divisor latch enabled? */
				return getc(stdin); /* ignore UART buffering */
			else
				return ins8250_r(device, offset);

		/* Intercept reads of the LSR and always set the Data Ready bit. */
		case 5:
			return ins8250_r(device, offset) | 1;
			
		/* All others pass through normally. */
		default:
			return ins8250_r(device, offset);
	}
}

void keybuffering (int flag)
{
   struct termios tio;

   tcgetattr (0, &tio);
   if (!flag) /* 0 = no buffering = not default */
      tio.c_lflag &= ~ICANON;
   else /* 1 = buffering = default */
      tio.c_lflag |= ICANON;
   tcsetattr (0, TCSANOW, &tio);
}

void keyecho (int flag)
{
   struct termios tio;

   tcgetattr (0, &tio);
   if (!flag) /* 0 = no key echo = not default */
      tio.c_lflag &= ~ECHO;
   else /* 1 = key echo = default */
      tio.c_lflag |= ECHO;
   tcsetattr (0, TCSANOW, &tio);
}
#endif

/* Triggered when interrupt key is pressed. */
static INPUT_CHANGED(nmi_callback)
{
	cputag_set_input_line(field->port->machine(), "maincpu", INPUT_LINE_NMI, newval);
}



/* Address map */
static ADDRESS_MAP_START(ultim809_map, AS_PROGRAM, 8)
	AM_RANGE(0x0000,0x3FFF) AM_RAMBANK("bank1")
	AM_RANGE(0x4000,0x7FFF) AM_RAMBANK("bank2")
	AM_RANGE(0x8000,0xBFFF) AM_RAMBANK("bank3")
	AM_RANGE(0xC000,0xC3FF) AM_READWRITE(via_r, via_w)
#if UART_USES_STDIN
	AM_RANGE(0xC400,0xC7FF) AM_DEVREADWRITE("uart", ultim809_ins8250_r, ins8250_w)
#else
	AM_RANGE(0xC400,0xC7FF) AM_DEVREADWRITE("uart", ins8250_r, ins8250_w)
#endif
	AM_RANGE(0xCC00,0xCC00) AM_READWRITE(TMS9928A_vram_r, TMS9928A_vram_w)
	AM_RANGE(0xCC01,0xCC01) AM_READWRITE(TMS9928A_register_r, TMS9928A_register_w)
	AM_RANGE(0xCC02,0xCC02)	AM_DEVREADWRITE("ay8910", ay8910_r, ay8910_address_w)
	AM_RANGE(0xCC03,0xCC03) AM_DEVWRITE("ay8910", ay8910_data_w)
	AM_RANGE(0xCC06,0xCC06) AM_READ(flipflop_sel_clear)
	AM_RANGE(0xCC0E,0xCC0E) AM_READ(flipflop_sel_set)
	AM_RANGE(0xCD00,0xCD01) AM_WRITE(debug_print_byte_hex)
	//AM_RANGE(0xCE00,0xCEFF) AM_READ(debug_print_char)
	//AM_RANGE(0xCF00,0xCF00) AM_READ(debug_print_n)
	AM_RANGE(0xE000,0xFFFF) AM_ROM
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START(ultim809)
	/* Sega controllers. From megadriv.c */
	/* Only 3-button controllers are currently supported in emulation. */
	
	PORT_START("CTRLSEL")	/* Controller selection */
	PORT_CATEGORY_CLASS(0x0f, 0x00, "Player 1 Controller")
	PORT_CATEGORY_ITEM(0x00, "Joystick 3 Buttons", 10)
	PORT_CATEGORY_CLASS(0xf0, 0x00, "Player 2 Controller")
	PORT_CATEGORY_ITEM(0x00, "Joystick 3 Buttons", 20)
	
	PORT_START("PAD1")		/* Joypad 1 (3 button + start) NOT READ DIRECTLY */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1) PORT_CATEGORY(10)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1) PORT_CATEGORY(10)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1) PORT_CATEGORY(10)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1) PORT_CATEGORY(10)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1) PORT_NAME("P1 B") PORT_CATEGORY(10)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1) PORT_NAME("P1 C") PORT_CATEGORY(10)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1) PORT_NAME("P1 A") PORT_CATEGORY(10)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START ) PORT_PLAYER(1) PORT_CATEGORY(10)

	PORT_START("PAD2")		/* Joypad 2 (3 button + start) NOT READ DIRECTLY */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2) PORT_CATEGORY(20)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2) PORT_CATEGORY(20)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2) PORT_CATEGORY(20)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2) PORT_CATEGORY(20)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2) PORT_NAME("P2 B") PORT_CATEGORY(20)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2) PORT_NAME("P2 C") PORT_CATEGORY(20)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2) PORT_NAME("P2 A") PORT_CATEGORY(20)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START ) PORT_PLAYER(2) PORT_CATEGORY(20)

	PORT_START("NMI")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_SERVICE ) PORT_NAME("Interrupt Button") PORT_CHANGED(nmi_callback, 0)
	
#if EMULATE_KEYBOARD
	PORT_INCLUDE( kb_keytronic_pc )
#endif

INPUT_PORTS_END

/****** Move to machine/ultim809.c ******/
static DRIVER_INIT(ultim809)
{
	ultim809_state *state = machine.driver_data<ultim809_state>();
	state->card_present = false;
	state->controller_sel = true;
	state->kb_data = true;
	state->kb_clock_prev = true;
	
#if UART_USES_STDIN
	keybuffering(false);
	keyecho(false);
#endif
}

INTERRUPT_GEN(vblank_interrupt)
{
	TMS9928A_interrupt(device->machine());
}

static void vdp_interrupt(running_machine &machine, int state)
{
	cputag_set_input_line(machine, "maincpu", M6809_IRQ_LINE, state);
}

static void uart_transmit(device_t *device, int data)
{
	putc(data, stdout);
}

/* Video */
static const TMS9928a_interface tms9928a_interface =
{
	TMS99x8A,
	0x4000,
	0, 0,
	vdp_interrupt
};

/* Audio and controllers */

static UINT8 ultim809_read_controller(running_machine &machine, const char *port)
{
	ultim809_state *state = machine.driver_data<ultim809_state>();
	UINT8 portval;
	UINT8 buttons = input_port_read(machine, port);	
	if (state->controller_sel) /* SELECT line */
		portval = (buttons & 0x3F);
	else
		portval = (buttons & 0x0F) | ((buttons & 0xC0) >> 2);
	
	return portval | 0xC0;
}

static READ8_DEVICE_HANDLER(ay8910_pa_r)
{
	return ultim809_read_controller(device->machine(), "PAD1");
}

static READ8_DEVICE_HANDLER(ay8910_pb_r)
{
	return ultim809_read_controller(device->machine(), "PAD2");
}

READ8_HANDLER(flipflop_sel_set)
{
	ultim809_state *state = space->machine().driver_data<ultim809_state>();
	state->controller_sel = true;
	return 0;
}

READ8_HANDLER(flipflop_sel_clear)
{
	ultim809_state *state = space->machine().driver_data<ultim809_state>();
	state->controller_sel = false;
	return 0;
}

static const ay8910_interface ultim809_ay8910_interface =
{
	AY8910_LEGACY_OUTPUT,
	AY8910_DEFAULT_LOADS,
	DEVCB_HANDLER(ay8910_pa_r),	/* port A read */
	DEVCB_HANDLER(ay8910_pb_r),	/* port B read */
	DEVCB_NULL,					/* port A write */
	DEVCB_NULL					/* port B write */
};

/* VIA */
READ8_HANDLER(via_r)
{
	via6522_device *via = space->machine().device<via6522_device>("via6522");
	return via->read(*space, offset);
}

WRITE8_HANDLER(via_w)
{
	via6522_device *via = space->machine().device<via6522_device>("via6522");
	via->write(*space, offset, data);
}

static WRITE8_DEVICE_HANDLER(via_pa_w)
{
	/* Change bank3's base address */
	int num_banks = ram_get_size(device->machine().device(RAM_TAG)) / 16384;
	UINT8 new_bank = data % num_banks;	
	memory_set_bank(device->machine(), "bank3", new_bank);
}

static READ8_DEVICE_HANDLER(via_pb_r)
{
	ultim809_state *state = device->machine().driver_data<ultim809_state>();
	
	UINT8 portval = 0xFF;
	
	/* Simulate pulling card-detect low line if cartridge was loaded */
	if (state->card_present)
		portval &= ~(1 << 4);
		
	/* Keyboard data line is pin 7 */
	if (!state->kb_data)
		portval &= ~(1 << 7);

	return portval;
}

static void via_interrupt(device_t *device, int level)
{
	ultim809_state *state = device->machine().driver_data<ultim809_state>();
	
	/*if (level)
		printf("level:%d data:%d pc:%04x\n", level, state->kb_data, cpu_get_reg(device->machine().device("maincpu"), M6809_PC));
	else
		printf("level:%d        pc:%04x\n", level, cpu_get_reg(device->machine().device("maincpu"), M6809_PC));*/
		
	cputag_set_input_line(device->machine(), "maincpu", M6809_FIRQ_LINE, level);
}
	
static const via6522_interface ultim809_via6522_interface =
{
    DEVCB_NULL, /* devcb_read8 m_in_a_func */
    DEVCB_HANDLER(via_pb_r), /* devcb_read8 m_in_b_func */
    DEVCB_NULL, /* devcb_read_line m_in_ca1_func */
    DEVCB_NULL, /* devcb_read_line m_in_cb1_func */
    DEVCB_NULL, /* devcb_read_line m_in_ca2_func */
    DEVCB_NULL, /* devcb_read_line m_in_cb2_func */
    DEVCB_HANDLER(via_pa_w), /* devcb_write8 m_out_a_func */
    DEVCB_NULL, /* devcb_write8 m_out_b_func */
    DEVCB_NULL, /* devcb_write_line m_out_ca1_func */
    DEVCB_NULL, /* devcb_write_line m_out_cb1_func */
    DEVCB_NULL, /* devcb_write_line m_out_ca2_func */
    DEVCB_NULL, /* devcb_write_line m_out_cb2_func */
    DEVCB_LINE(via_interrupt) /* devcb_write_line m_irq_func */
};

/* Serial port */
static const ins8250_interface uart_interface =
{
	1843200,		/* crystal frequency */
	NULL,			/* interrupt */
	uart_transmit,	/* transmit */
	NULL,			/* handshake out */
	NULL,			/* refresh connect */
};

#if EMULATE_KEYBOARD
/* AT keyboard */
/* The Ultim809 does not have a discrete keyboard controller.
   The clock and data lines are interfaced to the 6522 VIA and
   read from software. */

void keyboard_clock_w(device_t *device, int level)
{
	via6522_device *via = device->machine().device<via6522_device>("via6522");
	ultim809_state *state = device->machine().driver_data<ultim809_state>();
	bool falling_edge = (state->kb_clock_prev && !level);
	
	state->kb_clock_prev = level;
	
	/* Eat all the keyboard controller's cycles, so it can't issue two
	   negative edges in the same timeslice. */
	if (falling_edge)
		device_yield(device->subdevice("kb_keytr"));
		//device_eat_cycles(device->subdevice("kb_keytr"), 100000);

	via->write_ca1(level);
	
	/* Should force the CPU to execute here, so the interrupt is always serviced. */
}

void keyboard_data_w(device_t *device, int level)
{
	ultim809_state *state = device->machine().driver_data<ultim809_state>();
	state->kb_data = level;
}

static const kb_keytronic_interface keyboard_interface =
{
	DEVCB_LINE(keyboard_clock_w),
	DEVCB_LINE(keyboard_data_w),
};
#endif

static MACHINE_START(ultim809)
{
	TMS9928A_configure(&tms9928a_interface);

	UINT8* ram = ram_get_ptr(machine.device(RAM_TAG));
	int num_banks = ram_get_size(machine.device(RAM_TAG)) / 0x4000;
	memory_configure_bank(machine, "bank1", 0, 1, ram, 0x4000);
	memory_configure_bank(machine, "bank2", 0, 1, ram+0x4000, 0x4000);
	memory_configure_bank(machine, "bank3", 0, num_banks, ram, 0x4000);
	memory_set_bank(machine, "bank1", 0);
	memory_set_bank(machine, "bank2", 0);
	memory_set_bank(machine, "bank3", 0);
}

static MACHINE_RESET(ultim809)
{
}

DEVICE_IMAGE_LOAD(ultim809_cart)
{
	/* copy binary image into RAM starting at address 0x0100 */
	ultim809_state *state = image.device().machine().driver_data<ultim809_state>();
	UINT8* ram = ram_get_ptr(image.device().machine().device(RAM_TAG))+0x100;
	memcpy(ram, image.ptr(), image.length());
	
	state->card_present = true;
	return IMAGE_INIT_PASS;
}

/* Machine driver */
static MACHINE_CONFIG_START(ultim809, ultim809_state)
	/* basic hardware */
	MCFG_CPU_ADD("maincpu", M6809E, XTAL_8MHz / 4) /* 2 MHz */
	MCFG_CPU_PROGRAM_MAP(ultim809_map)
	MCFG_CPU_VBLANK_INT("screen", vblank_interrupt)
	MCFG_QUANTUM_PERFECT_CPU("maincpu")

	MCFG_MACHINE_START(ultim809)
	MCFG_MACHINE_RESET(ultim809)
	
	/* video hardware, cribbed from coleco.c and msx.c */
	MCFG_FRAGMENT_ADD(tms9928a)
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500))	
	
	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("ay8910", YM2149, XTAL_8MHz / 4)
	MCFG_SOUND_CONFIG(ultim809_ay8910_interface)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
		
	/* VIA */
	MCFG_VIA6522_ADD("via6522", 0, ultim809_via6522_interface)
		
	/* serial port */
	MCFG_NS16550_ADD("uart", uart_interface)
	
#if EMULATE_KEYBOARD
	/* keyboard */
	MCFG_KB_KEYTRONIC_ADD("keyboard", keyboard_interface)
#endif

	/* "cartridge" slot to allow programs to be loaded into ram */
	MCFG_CARTSLOT_ADD("cart")
	MCFG_CARTSLOT_EXTENSION_LIST("ex9")
	MCFG_CARTSLOT_NOT_MANDATORY
	MCFG_CARTSLOT_LOAD(ultim809_cart)
	MCFG_CARTSLOT_INTERFACE("ultim809_cart")
		
	/* RAM */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("512K")
	MCFG_RAM_EXTRA_OPTIONS("1024K,2048K,4096K")
		
MACHINE_CONFIG_END

/* ROM specification */
ROM_START(ultim809)
	ROM_REGION(0x10000, "maincpu", 0)
	ROM_LOAD("rom.bin", 0xE000, 0x2000, ULTIM809_MESS_CHECKSUMS)
ROM_END

/* Driver */

/*   YEAR  NAME      PARENT  COMPAT MACHINE   INPUT     INIT      COMPANY         FULLNAME    FLAGS */
COMP(2011, ultim809, 0,      0,     ultim809, ultim809, ultim809, "Matt Sarnoff", "Ultim809", 0)