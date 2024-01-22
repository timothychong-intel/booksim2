# BookSim Simulator Feature Enhancement

This repository introduces a suite of feature enhancements to the BookSim simulator, a cycle-level simulator modeling credit-based interconnection networks.

## 1. Lossy Network Router Simulation

The original BookSim model is equipped with lossless routers that leverage credit-based flow control to manage traffic between routers.
We have introduced a lossy router simulation that allows for the simulation of packet drops when router queues reach capacity.
In paritcular, packets transmitted from an upstream router that cannot be accommodated in the buffer of the next router are dropped.

## 2. End-to-End Reliability Model

We have extended the model to incorporate an end-to-end reliability protocol based on sequence numbers.
This feature implements explicit negative acknowledgements (NACKs), sent from the receiving end back to the source upon detection of packet loss (e.g., due to sequence number mismatches).

## 3. Application-Behavior-Based Simulation Model

We introduces a back-end application model.
This model allows for simulations driven by application behavior descriptions, offering a approach to network simulation that aligns more closely with real-world application scenarios.


## Research and Development Usage

These feature extensions are currently utilized in ongoing research and are not intended as final implementations. They provide a flexible foundation for further development, and users are encouraged to adapt and modify the codebase to suit their specific research requirements.

## Compilation Instructions

To compile the enhanced BookSim simulator, navigate to the src folder and execute `make` with the provided Makefile.

## Acknowledgement / Contributors
* Timothy Chong
* Tom Labonte
* Halit Dogan
* Carl Beckman
