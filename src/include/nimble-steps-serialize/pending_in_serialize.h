/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_STEPS_PENDING_IN_SERIALIZE_H
#define NIMBLE_STEPS_PENDING_IN_SERIALIZE_H

#include <nimble-steps/steps.h>

#include <stdint.h>
#include <stdlib.h>

struct FldInStream;

int nbsPendingStepsInSerializeHeader(struct FldInStream* stream, StepId* latestStepId);
ssize_t nbsPendingStepsInSerialize(struct FldInStream* stream, NbsSteps * target);

#endif
