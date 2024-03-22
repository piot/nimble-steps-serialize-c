/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-steps-serialize/pending_in_serialize.h>

/// Reads the header for incoming pending steps
/// Typically called on the server to determine the receive status from the client.
/// @param stream inStream
/// @param[out] latestStepId latest received stepId
/// @param[out] receiveMask the receive mask
/// @return negative on error
int nbsPendingStepsInSerializeHeader(struct FldInStream* stream, StepId* latestStepId, uint64_t* receiveMask)
{
    fldInStreamReadUInt32(stream, latestStepId);
    fldInStreamReadUInt64(stream, receiveMask);

    return 0;
}

static ssize_t nbsPendingStepsInSerializeRange(FldInStream* stream, StepId referenceId, NbsPendingSteps* target)
{
    uint8_t deltaStepId;
    fldInStreamReadUInt8(stream, &deltaStepId);

    StepId stepId = referenceId + deltaStepId;

    uint8_t stepsThatFollow;
    fldInStreamReadUInt8(stream, &stepsThatFollow);

    uint8_t buf[1024];
    uint8_t stepOctetCount;
    size_t addedSteps = 0;

    CLOG_EXECUTE(StepId lastStepId = stepId + stepsThatFollow - 1;)
    CLOG_C_VERBOSE(&target->log, "received range from server: firstStep range %08X - %08X count:%d", stepId, lastStepId,
                   stepsThatFollow)

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
            CLOG_C_SOFT_ERROR(&target->log, "failed to set step %08X", stepId)
            return actualNewStepsAdded;
        }

        stepId = (stepId + 1);
        addedSteps += (size_t) actualNewStepsAdded;
    }

    CLOG_C_VERBOSE(&target->log, "added %zd steps from server", addedSteps)

    return (ssize_t) addedSteps;
}

/// Reads the stream and writes into different ranges in the pending steps buffer
/// Typically called on the client to receive steps from the server.
/// @param stream inStream
/// @param target target buffer
/// @return the total number of steps added or negative on error.
ssize_t nbsPendingStepsInSerialize(FldInStream* stream, NbsPendingSteps* target)
{
    size_t totalStepsAdded = 0;
    uint32_t firstStepId;
    fldInStreamReadUInt32(stream, &firstStepId);

    uint8_t rangesThatFollow;
    fldInStreamReadUInt8(stream, &rangesThatFollow);

    CLOG_C_VERBOSE(&target->log, "nimble client: Steps from server range count:%d, starting with %08X",
                   rangesThatFollow, firstStepId)

    for (size_t i = 0U; i < rangesThatFollow; ++i) {
        ssize_t stepsAdded = nbsPendingStepsInSerializeRange(stream, firstStepId, target);
        if (stepsAdded < 0) {
            CLOG_C_SOFT_ERROR(&target->log, "problem adding server ranges")
            return stepsAdded;
        }
        totalStepsAdded += (size_t) stepsAdded;
    }

    return (ssize_t) totalStepsAdded;
}
