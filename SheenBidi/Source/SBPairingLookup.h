/*
 * Automatically generated by SheenBidiGenerator tool.
 * DO NOT EDIT!!
 */

#ifndef _SB_INTERNAL_PAIRING_LOOKUP_H
#define _SB_INTERNAL_PAIRING_LOOKUP_H

#include <SBConfig.h>

#include "SBBracketType.h"
#include "SBTypes.h"

SB_INTERNAL SBCodepoint SBPairingDetermineMirror(SBCodepoint codepoint);
SB_INTERNAL SBCodepoint SBPairingDetermineBracketPair(SBCodepoint codepoint, SBBracketType *bracketType);

#endif
