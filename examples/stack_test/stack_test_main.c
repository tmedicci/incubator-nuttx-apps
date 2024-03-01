/****************************************************************************
 * apps/examples/stack_test/stack_test_main.c
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
 * Private Functions
 ****************************************************************************/

void recursive_function(int depth)
{
  if (depth <= 0)
    {
      printf("Reached the base of recursion\n");
      return;
    }

  recursive_function(depth - 1);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * stack_test main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int depth;

  if (argc != 2)
    {
      printf("Usage: %s <depth of recursion>\n", argv[0]);
      return 1;
    }

  depth = atoi(argv[1]);

  printf("Recursion depth: %d\n", depth);

  recursive_function(depth);

  return 0;
}
