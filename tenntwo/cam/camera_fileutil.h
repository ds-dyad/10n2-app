/****************************************************************************
 * apps/examples/camera/camera_fileutil.h
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

#ifndef __EXAMPLES_CAMERA_CAMERA_FILEUTIL_H__
#define __EXAMPLES_CAMERA_CAMERA_FILEUTIL_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <sys/types.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Initialize file util of camera example */
#ifdef __cplusplus
extern "C" {
#endif

const char *futil_initialize(void);

/* Write an image file */

int futil_writeimage(uint8_t *data, size_t len, const char* dirname, const char *filename);

#ifdef __cplusplus
}
#endif
#endif  /* __EXAMPLES_CAMERA_CAMERA_FILEUTIL_H__ */

