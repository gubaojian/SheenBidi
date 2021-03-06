/*
 * Copyright (C) 2014-2018 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SB_INTERNAL_STATUS_STACK_H
#define _SB_INTERNAL_STATUS_STACK_H

#include <SBConfig.h>
#include "SBBase.h"

#define _SBStatusStackList_Length       16
#define _SBStatusStackList_MaxIndex     (_SBStatusStackList_Length - 1)

typedef struct _SBStatusStackElement {
    SBBoolean isolateStatus;
    SBBidiType overrideStatus;
    SBLevel embeddingLevel;
} _SBStatusStackElement, *_SBStatusStackElementRef;

typedef struct _SBStatusStackList {
    _SBStatusStackElement elements[_SBStatusStackList_Length];

    struct _SBStatusStackList *previous;
    struct _SBStatusStackList *next;
} _SBStatusStackList, *_SBStatusStackListRef;

typedef struct _SBStatusStack {
    _SBStatusStackList _firstList;
    _SBStatusStackListRef _peekList;
    SBUInteger _peekTop;
    SBUInteger count;
} SBStatusStack, *SBStatusStackRef;

SB_INTERNAL void SBStatusStackInitialize(SBStatusStackRef stack);
SB_INTERNAL void SBStatusStackFinalize(SBStatusStackRef stack);

SB_INTERNAL void SBStatusStackPush(SBStatusStackRef stack,
   SBLevel embeddingLevel, SBBidiType overrideStatus, SBBoolean isolateStatus);
SB_INTERNAL void SBStatusStackPop(SBStatusStackRef stack);
SB_INTERNAL void SBStatusStackSetEmpty(SBStatusStackRef stack);

SB_INTERNAL SBLevel SBStatusStackGetEmbeddingLevel(SBStatusStackRef stack);
SB_INTERNAL SBBidiType SBStatusStackGetOverrideStatus(SBStatusStackRef stack);
SB_INTERNAL SBBoolean SBStatusStackGetIsolateStatus(SBStatusStackRef stack);

#endif
