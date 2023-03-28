/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-steps-serialize/pending_in_serialize.h>

int nbsPendingStepsInSerializeHeader(struct FldInStream* stream, StepId* latestStepId, uint64_t* receiveMask)
{
    fldInStreamReadUInt32(stream, latestStepId);
    fldInStreamReadUInt64(stream, receiveMask);

    return 0;
}

static int nbsPendingStepsInSerializeRange(FldInStream* stream, StepId referenceId, NbsPendingSteps* target)
{
    uint8_t deltaStepId;
    fldInStreamReadUInt8(stream, &deltaStepId);

    StepId stepId = referenceId + deltaStepId;

    uint8_t stepsThatFollow;
    fldInStreamReadUInt8(stream, &stepsThatFollow);

    uint8_t buf[1024];
    uint8_t stepOctetCount;
    size_t addedSteps = 0;

    CLOG_VERBOSE("received range from server: firstStep range startId:%08X count: %d", stepId, stepsThatFollow)

    for (size_t i = 0; i < stepsThatFollow; ++i) {
        fldInStreamReadUInt8(stream, &stepOctetCount);
        fldInStreamReadOctets(stream, buf, stepOctetCount);
#if 0
        if (buf[3] != 0) {
            CLOG_VERBOSE("received authoritative step in a packet %08X action %d", stepId, buf[3]);
        }
#endif

        // CLOG_VERBOSE("pending steps, trying to set step %08X octetCount:%d", stepId, stepOctetCount);
        int actualNewStepsAdded = nbsPendingStepsTrySet(target, stepId, buf, stepOctetCount);
        if (actualNewStepsAdded < 0) {
            CLOG_SOFT_ERROR("failed to set step %08X", stepId)
            return actualNewStepsAdded;
        }

        stepId = (stepId + 1);
        addedSteps += actualNewStepsAdded;
    }

    CLOG_VERBOSE("added %zd steps from server", addedSteps)

    return addedSteps;
}

int nbsPendingStepsInSerialize(FldInStream* stream, NbsPendingSteps* target)
{
    size_t totalStepsAdded = 0;
    uint32_t firstStepId;
    fldInStreamReadUInt32(stream, &firstStepId);

    uint8_t rangesThatFollow;
    fldInStreamReadUInt8(stream, &rangesThatFollow);

    CLOG_VERBOSE("nimble client: Steps from server range count:%d, starting with %08X", rangesThatFollow, firstStepId)

    for (size_t i = 0; i < rangesThatFollow; ++i) {
        int stepsAdded = nbsPendingStepsInSerializeRange(stream, firstStepId, target);
        if (stepsAdded < 0) {
            CLOG_SOFT_ERROR("problem adding server ranges")
            return stepsAdded;
        }
        totalStepsAdded += stepsAdded;
    }

    return totalStepsAdded;
}
