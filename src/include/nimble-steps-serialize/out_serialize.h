/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_STEPS_OUT_SERIALIZE_H
#define NIMBLE_STEPS_OUT_SERIALIZE_H

#include <nimble-steps-serialize/serialize.h>
#include <nimble-steps/steps.h>

struct FldOutStream;

int nbsStepsOutSerialize(struct FldOutStream* stream, const NbsSteps* steps);
int nbsStepsOutSerializeFixedCountNoHeader(struct FldOutStream* stream, StepId startStepId, size_t redundancyCount,
                                           const NbsSteps* steps);
ssize_t nbsStepsOutSerializeStep(const NimbleStepsOutSerializeLocalParticipants* participants, uint8_t* buf,
                             size_t maxCount);

static const size_t NimbleSerializeMaxRedundancyCount = 10;

size_t nbsStepsOutSerializeCalculateCombinedSize(size_t participantCount, size_t singleParticipantStepOctetCount);

#endif
