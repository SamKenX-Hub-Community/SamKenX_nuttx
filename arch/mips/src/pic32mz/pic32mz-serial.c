/****************************************************************************
 * arch/mips/src/pic32mz/pic32mz-serial.c
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
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

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/serial/serial.h>

#include <arch/board/board.h>

#include "up_arch.h"
#include "up_internal.h"

#include "pic32mz-config.h"
#include "chip/pic32mz-uart.h"
#include "pic32mz-lowconsole.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/* If we are not using the serial driver for the console, then we still must
 * provide some minimal implementation of up_putc.
 */

#ifdef USE_SERIALDRIVER

/* Which UART with be tty0/console and which tty1-4?  The console will always
 * be ttyS0.  If there is no console then will use the lowest numbered UART.
 */

/* First pick the console and ttys0.  This could be any of UART1-6 */

#if defined(CONFIG_UART1_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart1port /* UART1 is console */
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#elif defined(CONFIG_UART2_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart2port /* UART2 is console */
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#elif defined(CONFIG_UART3_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart3port /* UART3 is console */
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#elif defined(CONFIG_UART4_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart4port /* UART4 is console */
#    define TTYS0_DEV           g_uart4port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#elif defined(CONFIG_UART4_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart5port /* UART4 is console */
#    define TTYS0_DEV           g_uart5port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#elif defined(CONFIG_UART6_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart6port /* UART6 is console */
#    define TTYS5_DEV           g_uart6port /* UART6 is ttyS0 */
#    define UART6_ASSIGNED      1
#else
#  undef CONSOLE_DEV                        /* No console */
#  if defined(CONFIG_PIC32MZ_UART1)
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#  elif defined(CONFIG_PIC32MZ_UART2)
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#  elif defined(CONFIG_PIC32MZ_UART3)
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#  elif defined(CONFIG_PIC32MZ_UART4)
#    define TTYS0_DEV           g_uart4port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#  elif defined(CONFIG_PIC32MZ_UART4)
#    define TTYS0_DEV           g_uart5port /* UART4 is ttyS0 */
#    define UART4_ASSIGNED      1
#  elif defined(CONFIG_PIC32MZ_UART6)
#    define TTYS0_DEV           g_uart6port /* UART6 is ttyS0 */
#    define UART6_ASSIGNED      1
#  endif
#endif

/* Pick ttys1.  This could be any of UART1-5 excluding the console UART. */

#if defined(CONFIG_PIC32MZ_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS1_DEV           g_uart1port /* UART1 is ttyS1 */
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS1_DEV           g_uart2port /* UART2 is ttyS1 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS1_DEV           g_uart3port /* UART3 is ttyS1 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS1_DEV           g_uart4port /* UART4 is ttyS1 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS1_DEV           g_uart5port /* UART5 is ttyS1 */
#  define UART5_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART6) && !defined(UART6_ASSIGNED)
#  define TTYS1_DEV           g_uart6port /* UART6 is ttyS1 */
#  define UART6_ASSIGNED      1
#endif

/* Pick ttys2.  This could be one of UART2-6. It can't be UART1 because that
 * was either assigned as ttyS0 or ttys1.  One of UART 1-6 could also be the
 * console.
 */

#if defined(CONFIG_PIC32MZ_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS2_DEV           g_uart2port /* UART2 is ttyS2 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS2_DEV           g_uart3port /* UART3 is ttyS2 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS2_DEV           g_uart4port /* UART4 is ttyS2 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS2_DEV           g_uart5port /* UART5 is ttyS2 */
#  define UART5_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART6) && !defined(UART6_ASSIGNED)
#  define TTYS2_DEV           g_uart6port /* UART6 is ttyS2 */
#  define UART6_ASSIGNED      1
#endif

/* Pick ttys3. This could be one of UART3-6. It can't be UART1-2 because
 * those have already been assigned to ttsyS0, 1, or 2.  One of
 * UART 2-6 could also be the console.
 */

#if defined(CONFIG_PIC32MZ_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS3_DEV           g_uart3port /* UART3 is ttyS3 */
#  define UART3_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS3_DEV           g_uart4port /* UART4 is ttyS3 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS3_DEV           g_uart5port /* UART5 is ttyS3 */
#  define UART5_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART6) && !defined(UART6_ASSIGNED)
#  define TTYS3_DEV           g_uart6port /* UART6 is ttyS3 */
#  define UART6_ASSIGNED      1
#endif

/* Pick ttys4. This could be one of UART4-6. It can't be UART1-2 because
 * those have already been assigned to ttsyS0, 1, 2 or 3.  One of
 * UART 3-6 could also be the console.
 */

#if defined(CONFIG_PIC32MZ_UART4) && !defined(UART4_ASSIGNED)
#  define TTYS4_DEV           g_uart4port /* UART4 is ttyS4 */
#  define UART4_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS4_DEV           g_uart5port /* UART5 is ttyS4 */
#  define UART5_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART6) && !defined(UART6_ASSIGNED)
#  define TTYS4_DEV           g_uart6port /* UART6 is ttyS4 */
#  define UART6_ASSIGNED      1
#endif

/* Pick ttys5. This could be one of UART5-6. It can't be UART1-3 because
 * those have already been assigned to ttsyS0, 1, 2, 3 or 4.  One of
 * UART 4-6 could also be the console.
 */

#if defined(CONFIG_PIC32MZ_UART5) && !defined(UART5_ASSIGNED)
#  define TTYS5_DEV           g_uart5port /* UART5 is ttyS5 */
#  define UART5_ASSIGNED      1
#elif defined(CONFIG_PIC32MZ_UART6) && !defined(UART6_ASSIGNED)
#  define TTYS5_DEV           g_uart6port /* UART6 is ttyS5 */
#  define UART6_ASSIGNED      1
#endif

/* Common initialization logic will not not know that the all of the UARTs
 * have been disabled.  So, as a result, we may still have to provide
 * stub implementations of up_earlyserialinit(), up_serialinit(), and
 * up_putc().
 */

#ifdef HAVE_UART_DEVICE

/* These values describe the set of enabled interrupts */

#define IE_RX    (1 << 0)
#define IE_TX    (1 << 1)

#define RX_ENABLED(im)    (((im) & IE_RX) != 0)
#define TX_ENABLED(im)    (((im) & IE_TX) != 0)

#define ENABLE_RX(im)     do { (im) |= IE_RX; } while (0)
#define ENABLE_TX(im)     do { (im) |= IE_TX; } while (0)

#define DISABLE_RX(im)    do { (im) &= ~IE_RX; } while (0)
#define DISABLE_TX(im)    do { (im) &= ~IE_TX; } while (0)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct up_dev_s
{
  uintptr_t uartbase;  /* Base address of UART registers */
  xcpt_t    handler;   /* UART interrupt handler */
  uint32_t  baud;      /* Configured baud */
  uint8_t   irqe;      /* Error IRQ associated with this UART (for enable) */
  uint8_t   irqrx;     /* RX IRQ associated with this UART (for enable) */
  uint8_t   irqtx;     /* TX IRQ associated with this UART (for enable) */
  uint8_t   im;        /* Interrupt mask state */
  uint8_t   parity;    /* 0=none, 1=odd, 2=even */
  uint8_t   bits;      /* Number of bits (5, 6, 7 or 8) */
  bool      stopbits2; /* true: Configure with 2 stop bits instead of 1 */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Low-level helpers */

static inline uint32_t up_serialin(struct up_dev_s *priv, int offset);
static inline void up_serialout(struct up_dev_s *priv, int offset, uint32_t value);
static void up_restoreuartint(struct uart_dev_s *dev, uint8_t im);
static void up_disableuartint(struct uart_dev_s *dev, uint8_t *im);

/* Serial driver methods */

static int  up_setup(struct uart_dev_s *dev);
static void up_shutdown(struct uart_dev_s *dev);
static int  up_attach(struct uart_dev_s *dev);
static void up_detach(struct uart_dev_s *dev);

static int  up_interrupt(struct uart_dev_s *priv);
#ifdef CONFIG_PIC32MZ_UART1
static int  up_uart1_interrupt(int irq, void *context, FAR void *arg);
#endif
#ifdef CONFIG_PIC32MZ_UART2
static int  up_uart2_interrupt(int irq, void *context, FAR void *arg);
#endif
#ifdef CONFIG_PIC32MZ_UART3
static int  up_uart3_interrupt(int irq, void *context, FAR void *arg);
#endif
#ifdef CONFIG_PIC32MZ_UART4
static int  up_uart4_interrupt(int irq, void *context, FAR void *arg);
#endif
#ifdef CONFIG_PIC32MZ_UART5
static int  up_uart5_interrupt(int irq, void *context, FAR void *arg);
#endif
#ifdef CONFIG_PIC32MZ_UART6
static int  up_uart6_interrupt(int irq, void *context, FAR void *arg);
#endif

static int  up_ioctl(struct file *filep, int cmd, unsigned long arg);
static int  up_receive(struct uart_dev_s *dev, uint32_t *status);
static void up_rxint(struct uart_dev_s *dev, bool enable);
static bool up_rxavailable(struct uart_dev_s *dev);
static void up_send(struct uart_dev_s *dev, int ch);
static void up_txint(struct uart_dev_s *dev, bool enable);
static bool up_txready(struct uart_dev_s *dev);
static bool up_txempty(struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
  .setup          = up_setup,
  .shutdown       = up_shutdown,
  .attach         = up_attach,
  .detach         = up_detach,
  .ioctl          = up_ioctl,
  .receive        = up_receive,
  .rxint          = up_rxint,
  .rxavailable    = up_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol  = NULL,
#endif
  .send           = up_send,
  .txint          = up_txint,
  .txready        = up_txready,
  .txempty        = up_txempty,
};

/* I/O buffers */

#ifdef CONFIG_PIC32MZ_UART1
static char g_uart1rxbuffer[CONFIG_UART1_RXBUFSIZE];
static char g_uart1txbuffer[CONFIG_UART1_TXBUFSIZE];
#endif
#ifdef CONFIG_PIC32MZ_UART2
static char g_uart2rxbuffer[CONFIG_UART2_RXBUFSIZE];
static char g_uart2txbuffer[CONFIG_UART2_TXBUFSIZE];
#endif
#ifdef CONFIG_PIC32MZ_UART3
static char g_uart3rxbuffer[CONFIG_UART3_RXBUFSIZE];
static char g_uart3txbuffer[CONFIG_UART3_TXBUFSIZE];
#endif
#ifdef CONFIG_PIC32MZ_UART4
static char g_uart4rxbuffer[CONFIG_UART4_RXBUFSIZE];
static char g_uart4txbuffer[CONFIG_UART4_TXBUFSIZE];
#endif
#ifdef CONFIG_PIC32MZ_UART5
static char g_uart5rxbuffer[CONFIG_UART5_RXBUFSIZE];
static char g_uart5txbuffer[CONFIG_UART5_TXBUFSIZE];
#endif
#ifdef CONFIG_PIC32MZ_UART6
static char g_uart6rxbuffer[CONFIG_UART6_RXBUFSIZE];
static char g_uart6txbuffer[CONFIG_UART6_TXBUFSIZE];
#endif

#ifdef CONFIG_PIC32MZ_UART1
/* This describes the state of the PIC32MZ UART1 port. */

static struct up_dev_s g_uart1priv =
{
  .uartbase  = PIC32MZ_UART1_K1BASE,
  .handler   = up_uart1_interrupt,
  .baud      = CONFIG_UART1_BAUD,
  .irqe      = PIC32MZ_IRQ_U1E,
  .irqrx     = PIC32MZ_IRQ_U1RX,
  .irqtx     = PIC32MZ_IRQ_U1TX,
  .parity    = CONFIG_UART1_PARITY,
  .bits      = CONFIG_UART1_BITS,
  .stopbits2 = CONFIG_UART1_2STOP,
};

static uart_dev_t g_uart1port =
{
  .recv      =
  {
    .size    = CONFIG_UART1_RXBUFSIZE,
    .buffer  = g_uart1rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART1_TXBUFSIZE,
    .buffer  = g_uart1txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart1priv,
};
#endif

#ifdef CONFIG_PIC32MZ_UART2
/* This describes the state of the PIC32MZ UART2 port. */

static struct up_dev_s g_uart2priv =
{
  .uartbase  = PIC32MZ_UART2_K1BASE,
  .handler   = up_uart2_interrupt,
  .baud      = CONFIG_UART2_BAUD,
  .irqe      = PIC32MZ_IRQ_U2E,
  .irqrx     = PIC32MZ_IRQ_U2RX,
  .irqtx     = PIC32MZ_IRQ_U2TX,
  .parity    = CONFIG_UART2_PARITY,
  .bits      = CONFIG_UART2_BITS,
  .stopbits2 = CONFIG_UART2_2STOP,
};

static uart_dev_t g_uart2port =
{
  .recv      =
  {
    .size    = CONFIG_UART2_RXBUFSIZE,
    .buffer  = g_uart2rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART2_TXBUFSIZE,
    .buffer  = g_uart2txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart2priv,
};
#endif

#ifdef CONFIG_PIC32MZ_UART3
/* This describes the state of the PIC32MZ UART3 port. */

static struct up_dev_s g_uart3priv =
{
  .uartbase  = PIC32MZ_UART3_K1BASE,
  .handler   = up_uart3_interrupt,
  .baud      = CONFIG_UART3_BAUD,
  .irqe      = PIC32MZ_IRQ_U3E,
  .irqrx     = PIC32MZ_IRQ_U3RX,
  .irqtx     = PIC32MZ_IRQ_U3TX,
  .parity    = CONFIG_UART3_PARITY,
  .bits      = CONFIG_UART3_BITS,
  .stopbits2 = CONFIG_UART3_2STOP,
};

static uart_dev_t g_uart3port =
{
  .recv      =
  {
    .size    = CONFIG_UART3_RXBUFSIZE,
    .buffer  = g_uart3rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART3_TXBUFSIZE,
    .buffer  = g_uart3txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart3priv,
};
#endif

#ifdef CONFIG_PIC32MZ_UART4
/* This describes the state of the PIC32MZ UART4 port. */

static struct up_dev_s g_uart4priv =
{
  .uartbase  = PIC32MZ_UART4_K1BASE,
  .handler   = up_uart4_interrupt,
  .baud      = CONFIG_UART4_BAUD,
  .irqe      = PIC32MZ_IRQ_U4E,
  .irqrx     = PIC32MZ_IRQ_U4RX,
  .irqtx     = PIC32MZ_IRQ_U4TX,
  .parity    = CONFIG_UART4_PARITY,
  .bits      = CONFIG_UART4_BITS,
  .stopbits2 = CONFIG_UART4_2STOP,
};

static uart_dev_t g_uart4port =
{
  .recv      =
  {
    .size    = CONFIG_UART4_RXBUFSIZE,
    .buffer  = g_uart4rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART4_TXBUFSIZE,
    .buffer  = g_uart4txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart4priv,
};
#endif

#ifdef CONFIG_PIC32MZ_UART5
/* This describes the state of the PIC32MZ UART5 port. */

static struct up_dev_s g_uart5priv =
{
  .uartbase  = PIC32MZ_UART5_K1BASE,
  .handler   = up_uart5_interrupt,
  .baud      = CONFIG_UART5_BAUD,
  .irqe      = PIC32MZ_IRQ_U5E,
  .irqrx     = PIC32MZ_IRQ_U5RX,
  .irqtx     = PIC32MZ_IRQ_U5TX,
  .parity    = CONFIG_UART5_PARITY,
  .bits      = CONFIG_UART5_BITS,
  .stopbits2 = CONFIG_UART5_2STOP,
};

static uart_dev_t g_uart5port =
{
  .recv      =
  {
    .size    = CONFIG_UART5_RXBUFSIZE,
    .buffer  = g_uart5rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART5_TXBUFSIZE,
    .buffer  = g_uart5txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart5priv,
};
#endif

#ifdef CONFIG_PIC32MZ_UART6
/* This describes the state of the PIC32MZ UART6 port. */

static struct up_dev_s g_uart6priv =
{
  .uartbase  = PIC32MZ_UART6_K1BASE,
  .handler   = up_uart6_interrupt,
  .baud      = CONFIG_UART6_BAUD,
  .irqe      = PIC32MZ_IRQ_U6E,
  .irqrx     = PIC32MZ_IRQ_U6RX,
  .irqtx     = PIC32MZ_IRQ_U6TX,
  .parity    = CONFIG_UART6_PARITY,
  .bits      = CONFIG_UART6_BITS,
  .stopbits2 = CONFIG_UART6_2STOP,
};

static uart_dev_t g_uart6port =
{
  .recv      =
  {
    .size    = CONFIG_UART6_RXBUFSIZE,
    .buffer  = g_uart6rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART6_TXBUFSIZE,
    .buffer  = g_uart6txbuffer,
   },
  .ops       = &g_uart_ops,
  .priv      = &g_uart6priv,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_serialin
 ****************************************************************************/

static inline uint32_t up_serialin(struct up_dev_s *priv, int offset)
{
  return getreg32(priv->uartbase + offset);
}

/****************************************************************************
 * Name: up_serialout
 ****************************************************************************/

static inline void up_serialout(struct up_dev_s *priv, int offset, uint32_t value)
{
  putreg32(value, priv->uartbase + offset);
}

/****************************************************************************
 * Name: up_restoreuartint
 ****************************************************************************/

static void up_restoreuartint(struct uart_dev_s *dev, uint8_t im)
{
  irqstate_t flags;

  /* Re-enable/re-disable interrupts corresponding to the state of bits in im */

  flags = enter_critical_section();
  up_rxint(dev, RX_ENABLED(im));
  up_txint(dev, TX_ENABLED(im));
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_disableuartint
 ****************************************************************************/

static void up_disableuartint(struct uart_dev_s *dev, uint8_t *im)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  irqstate_t flags;

  flags = enter_critical_section();
  if (im)
   {
     *im = priv->im;
   }
  up_restoreuartint(dev, 0);
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_setup
 *
 * Description:
 *   Configure the UART baud, bits, parity, etc. This method is called the
 *   first time that the serial port is opened.
 *
 ****************************************************************************/

static int up_setup(struct uart_dev_s *dev)
{
#ifndef CONFIG_SUPPRESS_UART_CONFIG
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Configure the UART as an RS-232 UART */

  pic32mz_uartconfigure(priv->uartbase, priv->baud, priv->parity,
                        priv->bits, priv->stopbits2);
#endif

  return OK;
}

/****************************************************************************
 * Name: up_shutdown
 *
 * Description:
 *   Disable the UART.  This method is called when the serial
 *   port is closed
 *
 ****************************************************************************/

static void up_shutdown(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Disable interrupts */

  up_disableuartint(dev, NULL);

  /* Reset hardware and disable Rx and Tx */

  pic32mz_uartreset(priv->uartbase);
}

/****************************************************************************
 * Name: up_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.  This method is
 *   called when the serial port is opened.  Normally, this is just after the
 *   the setup() method is called, however, the serial console may operate in
 *   a non-interrupt driven mode during the boot phase.
 *
 *   RX and TX interrupts are not enabled by the attach method (unless the
 *   hardware supports multiple levels of interrupt enabling).  The RX and TX
 *   interrupts are not enabled until the txint() and rxint() methods are called.
 *
 ****************************************************************************/

static int up_attach(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  int ret;

  /* Attach the IRQs */

  ret = irq_attach(priv->irqrx, priv->handler, NULL);
  if (ret == 0)
    {
      ret = irq_attach(priv->irqtx, priv->handler, NULL);
    }

  if (ret == 0)
    {
      ret = irq_attach(priv->irqe, priv->handler, NULL);
    }

  return ret;
}

/****************************************************************************
 * Name: up_detach
 *
 * Description:
 *   Detach UART interrupts.  This method is called when the serial port is
 *   closed normally just before the shutdown method is called.  The exception
 *   is the serial console which is never shutdown.
 *
 ****************************************************************************/

static void up_detach(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Disable interrupts */

  up_disableuartint(dev, NULL);

  /* Detach from the interrupt */

  irq_detach(priv->irqrx);
  irq_detach(priv->irqtx);
  irq_detach(priv->irqe);
}

/****************************************************************************
 * Name: up_interrupt
 *
 * Description:
 *   This is the common UART interrupt handler.  It will be invoked when an
 *   interrupt received on a specific UART.  It should call
 *   uart_transmitchars or uart_receivechar to perform the appropriate data
 *   transfers.
 *
 ****************************************************************************/

static int  up_interrupt(struct uart_dev_s *dev)
{
  struct up_dev_s *priv;
  int              passes;
  bool             handled;

  DEBUGASSERT(dev && dev->priv);
  priv = (struct up_dev_s *)dev->priv;

  /* Loop until there are no characters to be transferred or,
   * until we have been looping for a long time.
   */

  handled = true;
  for (passes = 0; passes < 256 && handled; passes++)
    {
      handled = false;

      /* Handle error interrupts.  This interrupt occurs when any of the
       * following error conditions take place:
       *  - Parity error PERR (UxSTA bit 3) is detected
       *  - Framing Error FERR (UxSTA bit 2) is detected
       *  - Overflow condition for the receive buffer OERR (UxSTA bit 1) occurs
       */

#ifdef CONFIG_DEBUG_ERROR
      if (up_pending_irq(priv->irqe))
        {
           /* Clear the pending error interrupt */

           up_clrpend_irq(priv->irqe);
           _err("ERROR: interrupt STA: %08x\n",
                up_serialin(priv, PIC32MZ_UART_STA_OFFSET));
           handled = true;
        }
#endif

      /* Handle incoming, received bytes.  The RX FIFO is configured to
       * interrupt when the RX FIFO is 75% full (that is 6 of 8 for 8-deep
       * FIFOs or 3 of 4 for 4-deep FIFOS.
       */

      if (up_pending_irq(priv->irqrx))
        {
          /* Process incoming bytes */

          uart_recvchars(dev);
          handled = true;

          /* Clear the pending RX interrupt if the receive buffer is empty.
           * Note that interrupts can be lost if the interrupt condition is
           * still true when the interrupt is cleared.  Keeping the RX
           * interrupt pending too long is not a problem because the
           * upper half driver will disable RX interrupts if it no
           * longer has space to buffer the serial data.
           */

          if ((up_serialin(priv, PIC32MZ_UART_STA_OFFSET) & UART_STA_URXDA) == 0)
            {
              up_clrpend_irq(priv->irqrx);
            }
        }

      /* Handle outgoing, transmit bytes  The RT FIFO is configured to
       * interrupt only when the TX FIFO is empty.  There are not many
       * options on trigger TX interrupts.  The FIFO-not-full might generate
       * better through-put but with a higher interrupt rate.  FIFO-empty should
       * lower the interrupt rate but result in a burstier output.  If
       * you change this, You will probably need to change the conditions for
       * clearing the pending TX interrupt below.
       *
       * NOTE: When I tried using the FIFO-not-full interrupt trigger, I
       * had either lost interrupts, or else a window where I might get
       * infinite interrupts.  The problem is that there is a race condition
       * with trying to clearing the pending interrupt based on the FIFO
       * full condition.
       */

      if (up_pending_irq(priv->irqtx))
        {
          /* Process outgoing bytes */

          uart_xmitchars(dev);
          handled = true;

          /* Clear the pending TX interrupt if the TX FIFO is empty.
           * Note that interrupts can be lost if the interrupt condition is
           * still true when the interrupt is cleared.  Keeping the TX
           * interrupt pending too long is not a problem:  Upper level logic
           * will disable the TX interrupt when there is no longer anything
           * to be sent.
           */

          if ((up_serialin(priv, PIC32MZ_UART_STA_OFFSET) & UART_STA_UTRMT) != 0)
            {
              up_clrpend_irq(priv->irqtx);
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Name: up_uartn_interrupt
 *
 * Description:
 *   These the UART-specific interrupt handlers.  They simply invoke the
 *   common uart interrupt handler with the correct state data.
 *
 ****************************************************************************/

#ifdef CONFIG_PIC32MZ_UART1
static int  up_uart1_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart1port);
}
#endif

#ifdef CONFIG_PIC32MZ_UART2
static int  up_uart2_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart2port);
}
#endif

#ifdef CONFIG_PIC32MZ_UART3
static int  up_uart3_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart3port);
}
#endif

#ifdef CONFIG_PIC32MZ_UART4
static int  up_uart4_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart4port);
}
#endif

#ifdef CONFIG_PIC32MZ_UART5
static int  up_uart5_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart5port);
}
#endif
#ifdef CONFIG_PIC32MZ_UART6
static int  up_uart6_interrupt(int irq, void *context, FAR void *arg)
{
  return up_interrupt(&g_uart6port);
}
#endif

/****************************************************************************
 * Name: up_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method
 *
 ****************************************************************************/

static int up_ioctl(struct file *filep, int cmd, unsigned long arg)
{
#ifdef CONFIG_SERIAL_TERMIOS
  struct inode      *inode;
  struct uart_dev_s *dev;
  struct up_dev_s   *priv;
  int                ret = OK;

  DEBUGASSERT(filep, filep->f_inode);
  inode = filep->f_inode;
  dev   = inode->i_private;

  DEBUGASSERT(dev, dev->priv);
  priv = (struct up_dev_s *)dev->priv;

  switch (cmd)
    {
    case xxx: /* Add commands here */
      break;

    case TCGETS:
      {
        struct termios *termiosp = (struct termios *)arg;

        if (!termiosp)
          {
            ret = -EINVAL;
            break;
          }

        /* TODO:  Other termios fields are not yet returned.
         * Note that only cfsetospeed is not necessary because we have
         * knowledge that only one speed is supported.
         */

        cfsetispeed(termiosp, priv->baud);
      }
      break;

    case TCSETS:
      {
        struct termios *termiosp = (struct termios *)arg;

        if (!termiosp)
          {
            ret = -EINVAL;
            break;
          }

        /* TODO:  Handle other termios settings.
         * Note that only cfgetispeed is used besued we have knowledge
         * that only one speed is supported.
         */

        priv->baud = cfgetispeed(termiosp);
        pic32mz_uartconfigure(priv->uartbase, priv->baud, priv->parity,
                              priv->bits, priv->stopbits2);
      }
      break;

    default:
      ret = -ENOTTY;
      break;
    }

  return ret;
#else
  return -ENOTTY;
#endif
}

/****************************************************************************
 * Name: up_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one
 *   character from the UART.  Error bits associated with the
 *   receipt are provided in the return 'status'.
 *
 ****************************************************************************/

static int up_receive(struct uart_dev_s *dev, uint32_t *status)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Return status information */

  if (status)
    {
      *status = 0; /* We are not yet tracking serial errors */
    }

  /* Then return the actual received byte */

  return  (int)(up_serialin(priv, PIC32MZ_UART_RXREG_OFFSET) & UART_RXREG_MASK);
}

/****************************************************************************
 * Name: up_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 ****************************************************************************/

static void up_rxint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  irqstate_t flags;
  uint8_t im;

  flags = enter_critical_section();
  im = priv->im;
  if (enable)
    {
      /* Receive an interrupt when their is anything in the Rx data register (or an Rx
       * timeout occurs).
       */

#ifndef CONFIG_SUPPRESS_SERIAL_INTS
#ifdef CONFIG_DEBUG_ERROR
      up_enable_irq(priv->irqe);
#endif
      up_enable_irq(priv->irqrx);
      ENABLE_RX(im);
#endif
    }
  else
    {
#ifdef CONFIG_DEBUG_ERROR
      up_disable_irq(priv->irqe);
#endif
      up_disable_irq(priv->irqrx);
      DISABLE_RX(im);
    }
  priv->im = im;
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_rxavailable
 *
 * Description:
 *   Return true if the receive register is not empty
 *
 ****************************************************************************/

static bool up_rxavailable(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Return true is data is available in the receive data buffer */

  return (up_serialin(priv, PIC32MZ_UART_STA_OFFSET) & UART_STA_URXDA) != 0;
}

/****************************************************************************
 * Name: up_send
 *
 * Description:
 *   This method will send one byte on the UART.
 *
 ****************************************************************************/

static void up_send(struct uart_dev_s *dev, int ch)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  up_serialout(priv, PIC32MZ_UART_TXREG_OFFSET, (uint32_t)ch);
}

/****************************************************************************
 * Name: up_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts
 *
 ****************************************************************************/

static void up_txint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  irqstate_t flags;
  uint8_t im;

  flags = enter_critical_section();
  im = priv->im;
  if (enable)
    {
      /* Enable the TX interrupt */

#ifndef CONFIG_SUPPRESS_SERIAL_INTS
      up_enable_irq(priv->irqtx);
      ENABLE_TX(im);

      /* Fake a TX interrupt here by just calling uart_xmitchars() with
       * interrupts disabled (note this may recurse).
       */

      uart_xmitchars(dev);
#endif
    }
  else
    {
      /* Disable the TX interrupt */

      up_disable_irq(priv->irqtx);
      DISABLE_TX(im);
    }

  priv->im = im;
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_txready
 *
 * Description:
 *   Return true if the tranmsit data register is empty
 *
 ****************************************************************************/

static bool up_txready(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Return TRUE if the Transmit buffer register is not full */

  return (up_serialin(priv, PIC32MZ_UART_STA_OFFSET) & UART_STA_UTXBF) == 0;
}

/****************************************************************************
 * Name: up_txempty
 *
 * Description:
 *   Return true if the tranmsit data register is empty
 *
 ****************************************************************************/

static bool up_txempty(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;

  /* Return TRUE if the Transmit shift register is empty */

  return (up_serialin(priv, PIC32MZ_UART_STA_OFFSET) & UART_STA_UTRMT) != 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before up_serialinit.  NOTE:  This function depends on GPIO pin
 *   configuration performed in up_consoleinit() and main clock iniialization
 *   performed in up_clkinitialize().
 *
 ****************************************************************************/

void up_earlyserialinit(void)
{
  /* Disable interrupts from all UARTS.  The console is enabled in
   * pic32mz_consoleinit().
   */

  up_disableuartint(&TTYS0_DEV, NULL);
#ifdef TTYS1_DEV
  up_disableuartint(&TTYS1_DEV, NULL);
#endif
#ifdef TTYS2_DEV
  up_disableuartint(&TTYS2_DEV, NULL);
#endif
#ifdef TTYS3_DEV
  up_disableuartint(&TTYS3_DEV, NULL);
#endif
#ifdef TTYS4_DEV
  up_disableuartint(&TTYS4_DEV, NULL);
#endif
#ifdef TTYS5_DEV
  up_disableuartint(&TTYS5_DEV, NULL);
#endif
#ifdef TTYS6_DEV
  up_disableuartint(&TTYS6_DEV, NULL);
#endif

  /* Configuration whichever one is the console */

#ifdef HAVE_SERIAL_CONSOLE
  CONSOLE_DEV.isconsole = true;
  up_setup(&CONSOLE_DEV);
#endif
}

/****************************************************************************
 * Name: up_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes
 *   that up_earlyserialinit was called previously.
 *
 ****************************************************************************/

void up_serialinit(void)
{
  /* Register the console */

#ifdef HAVE_SERIAL_CONSOLE
  (void)uart_register("/dev/console", &CONSOLE_DEV);
#endif

  /* Register all UARTs */

  (void)uart_register("/dev/ttyS0", &TTYS0_DEV);
#ifdef TTYS1_DEV
  (void)uart_register("/dev/ttyS1", &TTYS1_DEV);
#endif
#ifdef TTYS2_DEV
  (void)uart_register("/dev/ttyS2", &TTYS2_DEV);
#endif
#ifdef TTYS3_DEV
  (void)uart_register("/dev/ttyS3", &TTYS3_DEV);
#endif
#ifdef TTYS4_DEV
  (void)uart_register("/dev/ttyS4", &TTYS4_DEV);
#endif
#ifdef TTYS5_DEV
  (void)uart_register("/dev/ttyS5", &TTYS5_DEV);
#endif
#ifdef TTYS6_DEV
  (void)uart_register("/dev/ttyS6", &TTYS6_DEV);
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug  writes
 *
 ****************************************************************************/

int up_putc(int ch)
{
#ifdef HAVE_SERIAL_CONSOLE
  struct uart_dev_s *dev = (struct uart_dev_s *)&CONSOLE_DEV;
  uint8_t imr;

  up_disableuartint(dev, &imr);

  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      up_lowputc('\r');
    }

  up_lowputc(ch);
  up_restoreuartint(dev, imr);
#endif
  return ch;
}

/****************************************************************************
 * Name: up_earlyserialinit, up_serialinit, and up_putc
 *
 * Description:
 *   stubs that may be needed.  These stubs would be used if all UARTs are
 *   disabled.  In that case, the logic in common/up_initialize() is not
 *   smart enough to know that there are not UARTs and will still expect
 *   these interfaces to be provided.
 *
 ****************************************************************************/
#else /* HAVE_UART_DEVICE */
void up_earlyserialinit(void)
{
}

void up_serialinit(void)
{
}

int up_putc(int ch)
{
  return ch;
}

#endif /* HAVE_UART_DEVICE */
#else /* USE_SERIALDRIVER */

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes
 *
 ****************************************************************************/

int up_putc(int ch)
{
#ifdef HAVE_SERIAL_CONSOLE
  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      up_lowputc('\r');
    }

  up_lowputc(ch);
#endif
  return ch;
}

#endif /* USE_SERIALDRIVER */

