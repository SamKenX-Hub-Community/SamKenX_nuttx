/****************************************************************************
 * configs/open1788/src/lpc17_buttons.c
 *
 *   Copyright (C) 2013, 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/irq.h>

#include <arch/board/board.h>

#include "lpc17_gpio.h"
#include "open1788.h"

#ifdef CONFIG_ARCH_BUTTONS

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
/* The Open1788 supports several buttons.  All will read "1" when open and "0"
 * when closed
 *
 * USER1           -- Connected to P4[26]
 * USER2           -- Connected to P2[22]
 * USER3           -- Connected to P0[10]
 *
 * And a Joystick
 *
 * JOY_A           -- Connected to P2[25]
 * JOY_B           -- Connected to P2[26]
 * JOY_C           -- Connected to P2[23]
 * JOY_D           -- Connected to P2[19]
 * JOY_CTR         -- Connected to P0[14]
 *
 * The switches are all connected to ground and should be pulled up and sensed
 * with a value of '0' when closed.
 */

/* Pin configuration for each Open1788 button.  This array is indexed by
 * the BUTTON_* and JOYSTICK_* definitions in board.h
 */

static const lpc17_pinset_t g_buttoncfg[BOARD_NUM_BUTTONS] =
{
  GPIO_USER1, GPIO_USER2, GPIO_USER3, GPIO_JOY_A,
  GPIO_JOY_B, GPIO_JOY_C, GPIO_JOY_D, GPIO_JOY_CTR
};

/* This array defines all of the interrupt handlers current attached to
 * button events.
 */

#if defined(CONFIG_ARCH_IRQBUTTONS) && defined(CONFIG_LPC17_GPIOIRQ)
static xcpt_t g_buttonisr[BOARD_NUM_BUTTONS];

/* This array provides the mapping from button ID numbers to button IRQ
 * numbers.
 */

static uint8_t g_buttonirq[BOARD_NUM_BUTTONS] =
{
  0,              GPIO_USER2_IRQ, GPIO_USER3_IRQ, GPIO_JOY_A_IRQ,
  GPIO_JOY_B_IRQ, GPIO_JOY_C_IRQ, GPIO_JOY_D_IRQ, GPIO_JOY_CTR_IRQ
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_button_initialize
 *
 * Description:
 *   board_button_initialize() must be called to initialize button resources.  After
 *   that, board_buttons() may be called to collect the current state of all
 *   buttons or board_button_irq() may be called to register button interrupt
 *   handlers.
 *
 ****************************************************************************/

void board_button_initialize(void)
{
  int i;

  /* Configure the GPIO pins as interrupting inputs. */

  for (i = 0; i < BOARD_NUM_BUTTONS; i++)
    {
      lpc17_configgpio(g_buttoncfg[i]);
    }
}

/****************************************************************************
 * Name: board_buttons
 *
 * Description:
 *   board_button_initialize() must be called to initialize button resources.  After
 *   that, board_buttons() may be called to collect the current state of all
 *   buttons.
 *
 *   board_buttons() may be called at any time to harvest the state of every
 *   button.  The state of the buttons is returned as a bitset with one
 *   bit corresponding to each button:  If the bit is set, then the button
 *   is pressed.  See the BOARD_BUTTON_*_BIT and BOARD_JOYSTICK_*_BIT
 *   definitions in board.h for the meaning of each bit.
 *
 ****************************************************************************/

uint8_t board_buttons(void)
{
  uint8_t ret = 0;
  int i;

  /* Check that state of each key */

  for (i = 0; i < BOARD_NUM_BUTTONS; i++)
    {
       /* A LOW value means that the key is pressed. */

       bool released = lpc17_gpioread(g_buttoncfg[i]);

       /* Accumulate the set of depressed (not released) keys */

       if (!released)
         {
            ret |= (1 << i);
         }
    }

  return ret;
}

/****************************************************************************
 * Button support.
 *
 * Description:
 *   board_button_initialize() must be called to initialize button resources.  After
 *   that, board_button_irq() may be called to register button interrupt handlers.
 *
 *   board_button_irq() may be called to register an interrupt handler that will
 *   be called when a button is depressed or released.  The ID value is a
 *   button enumeration value that uniquely identifies a button resource. See the
 *   BOARD_BUTTON_* and BOARD_JOYSTICK_* definitions in board.h for the meaning
 *   of enumeration values.  The previous interrupt handler address is returned
 *   (so that it may restored, if so desired).
 *
 *   Note that board_button_irq() also enables button interrupts.  Button
 *   interrupts will remain enabled after the interrupt handler is attached.
 *   Interrupts may be disabled (and detached) by calling board_button_irq with
 *   irqhandler equal to NULL.
 *
 ****************************************************************************/

#if defined(CONFIG_ARCH_IRQBUTTONS) && defined(CONFIG_LPC17_GPIOIRQ)
xcpt_t board_button_irq(int id, xcpt_t irqhandler)
{
  xcpt_t oldhandler = NULL;
  irqstate_t flags;
  int irq;

  /* Verify that the button ID is within range */

  if ((unsigned)id < BOARD_NUM_BUTTONS)
    {
      /* Get the IRQ number for the button; A value of zero indicates that
       * the button does not support the interrupt function.
       */

      irq = g_buttonirq[id];
      if (irq > 0)
        {
          /* Disable interrupts until we are done */

          flags = enter_critical_section();

          /* Return the current button handler and set the new interrupt handler */

          oldhandler      = g_buttonisr[id];
          g_buttonisr[id] = irqhandler;

          /* Configure the interrupt.  Either attach and enable the new
           * interrupt or disable and detach the old interrupt handler.
           */

          if (irqhandler)
            {
              /* Attach then enable the new interrupt handler */

              (void)irq_attach(irq, irqhandler, NULL);
              up_enable_irq(irq);
            }
          else
            {
              /* Disable then detach the old interrupt handler */

              up_disable_irq(irq);
              (void)irq_detach(irq);
            }

          leave_critical_section(flags);
        }
    }

  return oldhandler;
}
#endif

#endif /* CONFIG_ARCH_BUTTONS */
