
# Steps Serialization

There are two types of step serialization for game simulation. Either [Unreliable](#unreliable-steps) or [Reliable](#reliable-steps).

## Unreliable Steps

Steps are serialized with redundancy, but unreliable. The receiver needs to fill in "default" steps for steps that was not received.

| type              | octets | name        |
| :---------------- | -----: | :---------- |
| [StepId](#stepid) |      4 | firstStepId |
| [Steps](#steps)   |      x |             |


## Reliable Steps

Reliable steps are used when clients must have exactly the same steps to be fully deterministic.

#### Receive Header

| type              | octets | name                                                                    |
| :---------------- | -----: | :---------------------------------------------------------------------- |
| [StepId](#stepid) |      4 | remoteExpectingStepId                                                   |
| uint64            |      8 | receiveMask. Received status for steps prior to `remoteExpectingStepId` |


#### Ranges

The ranges are always sent in sorted order. E.g. the ranges 2-5, 8-9, 15-21.

| type                      | octets | name              |
| :------------------------ | -----: | :---------------- |
| [StepId](#stepid)         |      4 | firstStepId       |
| [SmallCount](#smallcount) |      1 | rangesThatFollows |

Repeated by `rangesThatFollows`:

| type                        | octets | name                                   |
| :-------------------------- | -----: | :------------------------------------- |
| [DeltaStepId](#deltastepid) |      1 | delta StepId value from `firstStepId`. |
| [SmallCount](#smallcount)   |      1 | stepCountThatFollows                   |
| [Steps](#steps)             |      x |


## Types

#### StepId

 `uint32`, 4 octets. StepId is specific for the session and always starts at 0. That covers about 19 800 hours (7 weeks) of continuous gameplay at 60 Hz. That should cover most session-based games with maximum 256 participant connections.

#### DeltaStepId
 `uint8`. 1 octet. Value that should be added to another reference StepId to get the resulting StepId.

#### ParticipantId
 `uint8`. 1 octet. A unique ID for each participant for this session. Maximum of 256 participants.

#### Input Payload

Game specific serialization of non-deterministic input parameters for the game simulation. Usually either gamepad/mouse input, or converted to a more abstracted game-oriented input (e.g. jump, interact with, cast spell). The payload should typically be 1 to 32 octets.

#### SmallCount

`uint8`. 1 octet. The number of items that follows. Zero is valid, unless specified.

#### Step Payload

The combined inputs from 1-256 participants.

| type                      | octets | name                                                           |
| :------------------------ | -----: | :------------------------------------------------------------- |
| [SmallCount](#smallcount) |      1 | numberOfParticipants. The number of participants that follows. |

Repeated by `numberOfParticipants`:

| type                            |          octets | name                                                |
| :------------------------------ | --------------: | :-------------------------------------------------- |
| [ParticipantId](#participantid) |               1 | participantId                                       |
| [SmallCount](#smallcount)       |               1 | inputOctetCount                                     |
| uint8                           | inputOctetCount | [inputPayload](#input-payload). Game Simulation specific data. |

#### Steps

A list of step payloads.

| type                      | octets | name                 |
| :------------------------ | -----: | :------------------- |
| [SmallCount](#smallcount) |      1 | stepCountThatFollows |

Repeated by a `stepCountThatFollows`:

| type                          |         octets | name           |
| :---------------------------- | -------------: | :------------- |
| [SmallCount](#smallcount)     |              1 | stepOctetCount |
| [Step Payload](#step-payload) | stepOctetCount | stepPayload    |
