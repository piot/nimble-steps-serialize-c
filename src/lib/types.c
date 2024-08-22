#include <nimble-steps-serialize/types.h>

const char* nimbleSerializeStepTypeToString(NimbleSerializeStepType type)
{
    switch (type) {
        case NimbleSerializeStepTypeNormal:
            return "Normal";
        case NimbleSerializeStepTypeStepNotProvidedInTime:
            return "StepNotProvidedInTime";
        case NimbleSerializeStepTypeWaitingForReJoin:
            return "WaitingForReJoin";
        case NimbleSerializeStepTypeJoined:
            return "Joined";
        case NimbleSerializeStepTypeLeft:
            return "Left";
    }
}
