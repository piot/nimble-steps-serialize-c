/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_STEPS_SERIALIZE_IN_SERIALIZE_H
#define NIMBLE_STEPS_SERIALIZE_IN_SERIALIZE_H

#include <nimble-steps-serialize/serialize.h>
#include <nimble-steps/steps.h>

struct FldInStream;

int nbsStepsInSerializeHeader(struct FldInStream* stream, StepId* firstStep, size_t* stepsThatFollow);
int nbsStepsInSerialize(struct FldInStream* stream, NbsSteps* target, StepId firstStepId, size_t stepsThatFollow);
int nbsStepsInSerializeStepsForParticipants(NimbleStepsOutSerializeLocalParticipants* participants,
                                         struct FldInStream* stream);
int nbsStepsInSerializeStepsForParticipantsFromOctets(NimbleStepsOutSerializeLocalParticipants* participants,
                                               const uint8_t* stepBuf, size_t maxCount);
int nbsStepsInSerializeSinglePredictedStep(struct FldInStream* stream, StepId deserializedStepId, NbsSteps* target);

#endif
