/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_STEPS_EXAMPLE_PENDING_OUT_SERIALIZE_H
#define NIMBLE_STEPS_EXAMPLE_PENDING_OUT_SERIALIZE_H

#include <nimble-steps/steps.h>

struct FldOutStream;
struct NbsPendingRange;

int nbsPendingStepsSerializeOutHeader(struct FldOutStream* stream, StepId latestStepId, uint64_t receiveMask);
int nbsPendingStepsSerializeOutRanges(struct FldOutStream* stream, const NbsSteps* steps,
                                      struct NbsPendingRange* ranges, size_t rangeCount);

#endif
