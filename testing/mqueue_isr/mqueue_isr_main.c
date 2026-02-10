/****************************************************************************
 * apps/testing/mqueue_isr/mqueue_isr_main.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <arch/irq.h>

#include "xtensa.h"
#include "esp32s3_irq.h"
#include "hardware/esp32s3_system.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MQ_NAME       "/mqueue_isr_test"
#define MQ_MSGSIZE    32
#define MQ_MAXMSG     4
#define TEST_MSG      "Hello from task!"
#define TEST_MSG_LEN  (sizeof(TEST_MSG))

/* Software Interrupt 3 - unused by SMP (uses 0,1) or wireless (uses 2) */

#define SWI_IRQ       ESP32S3_IRQ_INT_FROM_CPU3
#define SWI_PERIPH    ESP32S3_PERIPH_INT_FROM_CPU3
#define SWI_REG       SYSTEM_CPU_INTR_FROM_CPU_3_REG
#define SWI_EN_BIT    SYSTEM_CPU_INTR_FROM_CPU_3
#define SWI_TYPE      ESP32S3_CPUINT_LEVEL

/****************************************************************************
 * Private Data
 ****************************************************************************/

static mqd_t g_mqdes;

/* ISR result tracking - no printf from ISR context */

static volatile bool g_isr_executed;
static volatile bool g_test1_passed;       /* mq_receive with message */
static volatile bool g_test2_passed;       /* mq_receive on empty queue */
static volatile int  g_test1_ret;          /* Return value of first recv */
static volatile int  g_test1_errno;        /* errno after first recv */
static volatile int  g_test2_ret;          /* Return value of second recv */
static volatile int  g_test2_errno;        /* errno after second recv */
static volatile bool g_msg_content_ok;     /* Message content matches */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int mqueue_isr_handler(int irq, void *context, void *arg)
{
  char buffer[MQ_MSGSIZE];
  unsigned int prio;
  ssize_t ret;

  /* Clear the software interrupt first */

  modifyreg32(SWI_REG, SWI_EN_BIT, 0);

  g_isr_executed = true;

  /* Test 1: Receive the message from the queue (should succeed).
   * file_mq_timedreceive_internal should find the message and return it
   * without needing to block.
   */

  errno = 0;
  ret = mq_receive(g_mqdes, buffer, MQ_MSGSIZE, &prio);
  g_test1_ret = (int)ret;
  g_test1_errno = errno;

  if (ret > 0)
    {
      g_test1_passed = true;

      /* Validate message content */

      if ((size_t)ret == TEST_MSG_LEN &&
          memcmp(buffer, TEST_MSG, TEST_MSG_LEN) == 0)
        {
          g_msg_content_ok = true;
        }
    }

  /* Test 2: Try to receive again. The queue is now empty and we are in
   * ISR context. file_mq_timedreceive_internal should detect we are in
   * interrupt context (up_interrupt_context()) and return -EAGAIN instead
   * of trying to block the interrupted task.
   */

  errno = 0;
  ret = mq_receive(g_mqdes, buffer, MQ_MSGSIZE, &prio);
  g_test2_ret = (int)ret;
  g_test2_errno = errno;

  if (ret < 0 && errno == EAGAIN)
    {
      g_test2_passed = true;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct mq_attr attr;
  int ret;
  int cpuint;
  bool all_passed;

  printf("============================================\n");
  printf("  mqueue_isr: Message Queue ISR Test\n");
  printf("============================================\n\n");
  printf("Testing file_mq_timedreceive_internal from ISR context.\n");
  printf("This test validates that mq_receive works correctly\n");
  printf("when called from an interrupt handler on ESP32-S3.\n\n");

  /* Initialize result tracking */

  g_isr_executed    = false;
  g_test1_passed    = false;
  g_test2_passed    = false;
  g_test1_ret       = 0;
  g_test1_errno     = 0;
  g_test2_ret       = 0;
  g_test2_errno     = 0;
  g_msg_content_ok  = false;

  /* Configure message queue attributes */

  attr.mq_maxmsg  = MQ_MAXMSG;
  attr.mq_msgsize = MQ_MSGSIZE;
  attr.mq_flags   = 0;

  /* Open message queue */

  g_mqdes = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
  if (g_mqdes < 0)
    {
      printf("FAIL: mq_open failed: %d (%s)\n", errno, strerror(errno));
      return EXIT_FAILURE;
    }

  printf("[SETUP] Message queue '%s' opened (fd=%d)\n", MQ_NAME, g_mqdes);

  /* Send a message to the queue from task context */

  ret = mq_send(g_mqdes, TEST_MSG, TEST_MSG_LEN, 1);
  if (ret < 0)
    {
      printf("FAIL: mq_send failed: %d (%s)\n", errno, strerror(errno));
      mq_close(g_mqdes);
      mq_unlink(MQ_NAME);
      return EXIT_FAILURE;
    }

  printf("[SETUP] Message sent to queue: \"%s\" (len=%zu, prio=1)\n",
         TEST_MSG, TEST_MSG_LEN);

  /* Setup the software interrupt */

  cpuint = esp32s3_setup_irq(this_cpu(), SWI_PERIPH, 1, SWI_TYPE);
  if (cpuint < 0)
    {
      printf("FAIL: esp32s3_setup_irq failed: %d\n", cpuint);
      mq_close(g_mqdes);
      mq_unlink(MQ_NAME);
      return EXIT_FAILURE;
    }

  printf("[SETUP] Software interrupt configured (cpuint=%d)\n", cpuint);

  /* Attach the interrupt handler */

  ret = irq_attach(SWI_IRQ, mqueue_isr_handler, NULL);
  if (ret < 0)
    {
      printf("FAIL: irq_attach failed: %d\n", ret);
      esp32s3_teardown_irq(this_cpu(), SWI_PERIPH, cpuint);
      mq_close(g_mqdes);
      mq_unlink(MQ_NAME);
      return EXIT_FAILURE;
    }

  /* Enable the interrupt */

  up_enable_irq(SWI_IRQ);

  printf("[SETUP] ISR attached and enabled\n");
  printf("\n[ACTION] Triggering software interrupt...\n\n");

  /* Trigger the software interrupt */

  modifyreg32(SWI_REG, 0, SWI_EN_BIT);

  /* Small delay to ensure ISR has executed */

  usleep(100000);

  /* Report detailed results */

  printf("============================================\n");
  printf("  Test Results\n");
  printf("============================================\n\n");

  if (!g_isr_executed)
    {
      printf("FAIL: ISR was never executed!\n");
    }
  else
    {
      printf("ISR executed: YES\n\n");

      /* Test 1 results */

      printf("Test 1: mq_receive with available message\n");
      printf("  Expected: success (ret > 0)\n");
      printf("  Got:      ret=%d, errno=%d (%s)\n",
             g_test1_ret, g_test1_errno, strerror(g_test1_errno));
      printf("  Message:  %s\n", g_msg_content_ok ? "content OK" :
                                                     "content MISMATCH");
      printf("  Result:   %s\n\n",
             (g_test1_passed && g_msg_content_ok) ? "PASS" : "FAIL");

      /* Test 2 results */

      printf("Test 2: mq_receive on empty queue from ISR\n");
      printf("  Expected: ret=-1, errno=EAGAIN (%d)\n", EAGAIN);
      printf("  Got:      ret=%d, errno=%d (%s)\n",
             g_test2_ret, g_test2_errno, strerror(g_test2_errno));
      printf("  Result:   %s\n\n", g_test2_passed ? "PASS" : "FAIL");
    }

  /* Overall result */

  all_passed = g_isr_executed && g_test1_passed &&
               g_msg_content_ok && g_test2_passed;

  printf("============================================\n");
  printf("  Overall: %s\n", all_passed ? "ALL TESTS PASSED" :
                                          "SOME TESTS FAILED");
  printf("============================================\n");

  /* Cleanup */

  up_disable_irq(SWI_IRQ);
  irq_detach(SWI_IRQ);
  esp32s3_teardown_irq(this_cpu(), SWI_PERIPH, cpuint);
  mq_close(g_mqdes);
  mq_unlink(MQ_NAME);

  return all_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

