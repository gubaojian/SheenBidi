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

#include <stddef.h>
#include <stdlib.h>

#include "SBBase.h"
#include "SBLine.h"
#include "SBPairingLookup.h"
#include "SBMirrorLocator.h"

SBMirrorLocatorRef SBMirrorLocatorCreate(void)
{
    SBMirrorLocatorRef locator;

    locator = malloc(sizeof(SBMirrorLocator));
    locator->_line = NULL;
    locator->_retainCount = 1;
    SBMirrorLocatorReset(locator);

    return locator;
}

void SBMirrorLocatorLoadLine(SBMirrorLocatorRef locator, SBLineRef line, void *stringBuffer)
{
    SBLineRelease(locator->_line);

    if (line && stringBuffer == line->codepointSequence.stringBuffer) {
        locator->_line = SBLineRetain(line);
    }

    SBMirrorLocatorReset(locator);
}

const SBMirrorAgent *SBMirrorLocatorGetAgent(SBMirrorLocatorRef locator)
{
    return &locator->agent;
}

SBBoolean SBMirrorLocatorMoveNext(SBMirrorLocatorRef locator)
{
    SBLineRef line = locator->_line;

    if (line) {
        const SBCodepointSequence *sequence = &line->codepointSequence;

        do {
            const SBRun *run = &line->fixedRuns[locator->_runIndex];

            if (run->level & 1) {
                SBUInteger stringIndex;
                SBUInteger stringLimit;

                stringIndex = locator->_stringIndex;
                if (stringIndex == SBInvalidIndex) {
                    stringIndex = run->offset;
                }
                stringLimit = run->offset + run->length;

                while (stringIndex < stringLimit) {
                    SBUInteger initialIndex = stringIndex;
                    SBCodepoint codepoint = SBCodepointSequenceGetCodepointAt(sequence, &stringIndex);
                    SBCodepoint mirror = SBPairingDetermineMirror(codepoint);

                    if (mirror) {
                        locator->_stringIndex = stringIndex;
                        locator->agent.index = initialIndex;
                        locator->agent.mirror = mirror;
                        locator->agent.codepoint = codepoint;

                        return SBTrue;
                    }
                }
            }
            
            locator->_stringIndex = SBInvalidIndex;
        } while (++locator->_runIndex < line->runCount);
        
        SBMirrorLocatorReset(locator);
    }
    
    return SBFalse;
}

void SBMirrorLocatorReset(SBMirrorLocatorRef locator)
{
    locator->_runIndex = 0;
    locator->_stringIndex = SBInvalidIndex;
    locator->agent.index = SBInvalidIndex;
    locator->agent.mirror = 0;
}

SBMirrorLocatorRef SBMirrorLocatorRetain(SBMirrorLocatorRef locator)
{
    if (locator) {
        ++locator->_retainCount;
    }

    return locator;
}

void SBMirrorLocatorRelease(SBMirrorLocatorRef locator)
{
    if (locator && --locator->_retainCount == 0) {
        SBLineRelease(locator->_line);
        free(locator);
    }
}
