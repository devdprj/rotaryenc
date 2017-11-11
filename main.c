/*General tool for measuring ADC/USB2Serial/*/

#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "shell.h"
#include "chprintf.h"

#include "usbcfg.h"
#include "uartif.h"

#include "ring_buff.h"
#include "adcif.h"

#define ENCODER_DEV QEID4

/* Virtual serial port over USB.*/
SerialUSBDriver SDU1;
static BaseSequentialStream *usbchp;
thread_t *process_tp;

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
#define USB2SER_WA_SIZE    THD_WORKING_AREA_SIZE(512)


static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
}


static void adcWatch(BaseSequentialStream *chp, int argc, char *argv[]) {
  uint16_t adc_vals[10];
  uint32_t qei;
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  while(1)
  {
    qei = qeiGetCount(&ENCODER_DEV);
    chprintf(chp, "Value : %d \r\n",qei);
    chThdSleepMilliseconds(500);

  }

}


static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"adc_watch", adcWatch},
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

/*===========================================================================*/
/* Generic code.                                                             */
/*===========================================================================*/

/*
 * Blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 128);
static __attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    systime_t time = serusbcfg.usbp->state == USB_ACTIVE ? 250 : 1000;
    palClearPad(GPIOC, 13);
    chThdSleepMilliseconds(time);
    palSetPad(GPIOC, 13);
    chThdSleepMilliseconds(time);

  }
}


void initEncoder(void)
{
  static QEIConfig qeicfg = {
    QEI_MODE_QUADRATURE,
    QEI_BOTH_EDGES,
    QEI_DIRINV_FALSE,
  };

  AFIO->MAPR |= AFIO_MAPR_TIM3_REMAP_FULLREMAP;
  qeiStart(&ENCODER_DEV, &qeicfg);
  qeiEnable(&ENCODER_DEV);
}
/*
 * Application entry point.
 */
int __attribute__((noreturn)) main(void) {
  thread_t *shelltp = NULL;

  halInit();
  chSysInit();
#if 0
  uartChannel = &SD2;
  sdStart(&SD2, NULL);
  chprintf(uartChannel,"maple usb serial combo\r\n");
  chprintf(uartChannel,"123456798\r\n");
#endif
  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);
  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  /*
   * Shell manager initialization.
   */
  shellInit();

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
  initEncoder();

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (true) {
    if (!shelltp && (SDU1.config->usbp->state == USB_ACTIVE))
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminatedX(shelltp)) {
      chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
      shelltp = NULL;           /* Triggers spawning of a new shell.        */
    }
    chThdSleepMilliseconds(1000);
  }
}
