#ifndef NIMBLE_STEPS_SERIALIZE_TYPES_H
#define NIMBLE_STEPS_SERIALIZE_TYPES_H

typedef enum NimbleSerializeStepType {
    NimbleSerializeStepTypeNormal,
    NimbleSerializeStepTypeStepNotProvidedInTime,
    NimbleSerializeStepTypeWaitingForReJoin,
    NimbleSerializeStepTypeJoined,
    NimbleSerializeStepTypeLeft
} NimbleSerializeStepType;

const char* nimbleSerializeStepTypeToString(NimbleSerializeStepType type);

#endif
