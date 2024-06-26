/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_STEPS_SERIALIZE_H
#define NIMBLE_STEPS_SERIALIZE_H

#include <stddef.h>
#include <stdint.h>
#include <nimble-steps-serialize/types.h>

typedef struct NimbleStepsOutSerializeLocalParticipant {
    uint8_t participantId;
    uint8_t localPartyId; // Only available if stepType is joined
    NimbleSerializeStepType stepType;
    const uint8_t* payload;
    size_t payloadCount;
} NimbleStepsOutSerializeLocalParticipant;

typedef struct NimbleStepsOutSerializeLocalParticipants {
    NimbleStepsOutSerializeLocalParticipant participants[8];
    size_t participantCount;
} NimbleStepsOutSerializeLocalParticipants;

#endif
