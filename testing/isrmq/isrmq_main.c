/****************************************************************************
 * apps/testing/isrmq/isrmq_main.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>

#include <mqueue.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/mqueue.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_ARCH_CHIP_QEMU_RV_CLINT
#define CONFIG_ARCH_CHIP_QEMU_RV_CLINT  0x2000000
#endif

#define QUEUE_ISR    "isrmq"
#define QUEUE_APP    "appmq"
#define QUEUE_LENGTH  7

#define NAME_KTHREAD "driver"
#define PRIO_KTHREAD  253

#define STACKSIZE     CONFIG_TESTING_ISRMQ_STACKSIZE

#define PRIO_TRIGGER  (CONFIG_TESTING_ISRMQ_PRIORITY / 2)



/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct file  *filep;

/****************************************************************************
 * Private Data
 ****************************************************************************/

extern xcpt_t riscv_exception;

static volatile bool   g_readytorun;
static volatile bool   g_kthread_done;
static volatile uint   g_msg_sent;
static volatile uint   g_msg_rcvd;
static volatile uint   g_ipi_rcvd;
static volatile uint   g_ipi_sets;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* trigger IPI */

static void ipi_trigger(void)
{
  volatile uint32_t *ipi = (uint32_t *)CONFIG_ARCH_CHIP_QEMU_RV_CLINT;
  *ipi = 1;
}

/* clear IPI */

static void ipi_clear(void)
{
  volatile uint32_t *ipi = (uint32_t *)CONFIG_ARCH_CHIP_QEMU_RV_CLINT;
  *ipi = 0;
}

/****************************************************************************
 * Name: test_irq_handler
 ****************************************************************************/

static int test_irq_handler(int irq,  FAR void *context, FAR void *arg)
{
  ipi_clear();
  if (arg && OK == file_mq_send((filep)arg, (char *)&g_msg_sent,
                         sizeof(g_msg_sent), 0))
    {
      g_msg_sent++;
    }

  g_ipi_rcvd++;
  return OK;
}

/****************************************************************************
 * Name: trigger_thread
 *
 * Description: pthread that triggers given number of IRQs.
 * Arguments: an integer for number of IRQs to trigger.
 ****************************************************************************/

static void *trigger_thread(void *arg)
{
  const uintptr_t count = (uintptr_t)arg;
  uint i;

  /* wait until receiver is ready */

  while (!g_readytorun)
    {
      sched_yield();
    }

  printf("%s started, count=%"PRIuPTR"\n", __func__, count);

  for (i = 0; i < count; i++, g_ipi_sets++)
    {
      ipi_trigger();

      /* take a break if running too fast */

      while (g_ipi_sets >= g_msg_rcvd + QUEUE_LENGTH - 1)
        {
          sched_yield();
        }
    }

  return (pthread_addr_t)count;
}

/****************************************************************************
 * Name: kernel_thread
 *
 * Description:
 *
 *   The kthread that receives certain number of messages from the ISR queue
 *   and forwards them to the App queue. This mimics a kernel driver.
 *
 * Arguments:
 *   argv[0] name
 *   argv[1] ISR mqueue filep
 *   argv[2] App mqueues filep
 *   argv[3] the count
 *
 ****************************************************************************/

static int kernel_thread(int argc, FAR char *argv[])
{
  int ret = OK;

  if(argc > 3)
    {
      /* parse arguments */

      const filep isrq = (filep)((uintptr_t)strtoul(argv[1], NULL, 16));
      const filep appq = (filep)((uintptr_t)strtoul(argv[2], NULL, 16));
      uint count = (uintptr_t)strtoul(argv[3], NULL, 16);
      uint32_t msg;
      uint     prio;

      printf("%s started, count=%"PRIuPTR"\n", __func__, count);

      for (; count && OK == ret; count--)
        {
          if (sizeof(msg) ==
                file_mq_receive(isrq, (FAR char *)&msg, sizeof(msg), &prio))
            {
              ret = file_mq_send(appq, (FAR char *)&msg, sizeof(msg), prio);
            }
          else
            {
              break;
            }
        }

      printf("%s exit %d, remaining=%u\n", __func__, ret, count);
    }

  g_kthread_done = true;
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: isrmq_test
 *
 * Description:
 *
 *   This tests message producing from ISR on rv-virt device. It uses an
 *   somewhat asynchronous thread to trigger IRQs, which may further trigger
 *   ISR, where a message is produced.
 ****************************************************************************/

int isrmq_test(int attempts)
{
  int       ret;
  uint32_t  msg;
  uint      prio;
  pthread_t trigger;
  struct file mq_isr;
  struct file mq_app;
  pid_t  kthread = 0;

  /* reset counters */

  g_ipi_sets = g_ipi_rcvd = 0;
  g_msg_rcvd = g_msg_sent = 0;

  /* create ISR and App mqueues */

  {
    struct mq_attr attr;
    attr.mq_maxmsg  = QUEUE_LENGTH;
    attr.mq_msgsize = sizeof(msg);
    attr.mq_flags   = 0;
    ret = file_mq_open(&mq_isr, QUEUE_ISR, O_RDWR | O_CREAT, 0644, &attr);
  }

  if (ret < 0)
    {
      printf("Failed to create ISR mqueue: %d\n", ret);
      return ret;
    }

  {
    struct mq_attr attr;
    attr.mq_maxmsg  = QUEUE_LENGTH;
    attr.mq_msgsize = sizeof(msg);
    attr.mq_flags   = 0;
    ret = file_mq_open(&mq_app, QUEUE_APP, O_RDWR | O_CREAT, 0644, &attr);
  }

  if (ret < 0)
    {
      printf("Failed to create APP mqueue: %d\n", ret);
      goto out_isr;
    }

  /* create kthread */

  {
    char isrq_str[16];
    char appq_str[16];
    char mcnt_str[8];

    snprintf(isrq_str, sizeof(isrq_str),"%p", &mq_isr);
    snprintf(appq_str, sizeof(appq_str),"%p", &mq_app);
    snprintf(mcnt_str, sizeof(mcnt_str),"%x", attempts);
    char *argv[4] =
      {
        isrq_str,
        appq_str,
        mcnt_str,
        NULL
      };
    kthread = kthread_create(NAME_KTHREAD, PRIO_KTHREAD, STACKSIZE,
                             kernel_thread, argv);
  }

  if (kthread < 0)
    {
      printf("Failed to create kthread: %d\n", kthread);
      goto out;
    }

  /* attach ISR */

  ret = irq_attach(RISCV_IRQ_SOFT, test_irq_handler, &mq_isr);
  if (ret)
    {
      printf("Failed to attach ISR\n");
      goto out;
    }

  up_enable_irq(RISCV_IRQ_SOFT);

  /* create trigger thread */

  {
    struct sched_param sparam;
    sparam.sched_priority = PRIO_TRIGGER;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, STACKSIZE);
    pthread_attr_setschedparam(&attr, &sparam);
    ret = pthread_create(&trigger, &attr, trigger_thread, (void *)attempts);
  }
  if (OK != ret)
    {
      printf("%s: ERROR pthread_create failed; %d\n", __func__, ret);
      goto out;
    }

  /* App queue consuming loop */

  g_readytorun = true;
  for (int i = 0; i < attempts; i++)
    {
      if (sizeof(msg) == file_mq_receive(&mq_app,
                                      (char *)&msg, sizeof(msg), &prio))
        {
          g_msg_rcvd++;
        }
    }

  /* detach ISR */

  up_disable_irq(RISCV_IRQ_SOFT);
  ret = irq_attach(RISCV_IRQ_SOFT, riscv_exception, NULL);

  /* wait for trigger thread end */

  printf("%s: joining trigger thread\n", __func__);
  pthread_join(trigger, NULL);

out:

  if (kthread > 0 && !g_kthread_done)
    {
      printf("deleting %s %d\n", NAME_KTHREAD, kthread);
      kthread_delete(kthread);
    }

  /* destroy mqueues */

  file_mq_close(&mq_app);
  file_mq_unlink(QUEUE_APP);

out_isr:

  file_mq_close(&mq_isr);
  file_mq_unlink(QUEUE_ISR);

  /* print test summary */

  printf("%s: ipi sets=%u, rcvd=%u\n", __func__, g_ipi_sets, g_ipi_rcvd);
  printf("%s: msg sent=%u, rcvd=%u\n", __func__, g_msg_sent, g_msg_rcvd);
  ret = (g_msg_sent == g_msg_rcvd && g_ipi_rcvd == g_msg_sent) ? OK : -1;
  printf("%s: %s\n", __func__, (ret == OK) ? "passed" : "failed");

  return ret;
}


int main(int argc, FAR char *argv[])
{
  int count = 10000;
  if (argc > 1)
    {
      count = atoi(argv[1]);
    }

  return isrmq_test(count);
}
