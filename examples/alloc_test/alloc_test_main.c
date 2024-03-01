/****************************************************************************
 * apps/examples/alloc_test/alloc_test_main.c
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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * alloc_test main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int bfsize;
  int nbufs;

  if (argc != 3)
    {
      printf("Usage: %s <buffer size> <number of buffers>\n", argv[0]);
      return 1;
    }

  bfsize = atoi(argv[1]);
  nbufs = atoi(argv[2]);

  for (int i = 0; i < nbufs; i++)
    {
      void *buffer = calloc(bfsize, 1);

      if (buffer == NULL)
        {
          printf("Failed to allocate buffer %d\n", i);
          return 1;
        }
    }

  printf("%d buffers of size %d allocated\n", nbufs, bfsize);

  // Intentionally not freeing the buffers

  while(1)
    {
      sleep(1);
    }

  return 0;
}
