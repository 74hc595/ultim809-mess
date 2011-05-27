#pragma once

#ifndef __ULTIM809__
#define __ULTIM809__

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "cpu/m6809/m6809.h"
#include "video/tms9928a.h"
#include "sound/ay8910.h"		/* for YM2149 */
#include "machine/ins8250.h"	/* for PC16550D */
#include "machine/6522via.h"
#include "machine/kb_keytro.h"
#include "machine/ram.h"
#include "imagedev/cartslot.h"	/* allows programs to be run from RAM */

class ultim809_state : public driver_device
{
public:
	ultim809_state(running_machine &machine,
		const driver_device_config_base &config)
			: driver_device(machine, config)
	{}
	bool card_present;
	bool controller_sel;	/* SELECT line for Genesis controllers */
	bool kb_data;			/* PS/2 keyboard data line */
	bool kb_clock_prev;		/* last value of PS/2 keyboard clock line */
};

#endif