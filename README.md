stillbot-slave
===================

The *stillbot-slave* is a programmable logic controller (PLC), the computer interface to the various sensors and actuators attached to a microdistillation plant.
It oversees and records data for all operations of the plant and is the conduit through which the *stillbot-master* effects changes in the system.

*stillbot-slave* is currently based on the Arduino platform and the DS18(x)20 range of thermistors.

*stillbot-slave* is responsible for:

* identifying, initialising and running operational safety checks for all components
* ensuring the continuous operation of the plant and keeping it in a known, safe state at all times
* relaying sensor data (boiler temp, vapour temp, flow-rate, etc.) for analysis
* controlling actuators such as solid-state relays and solenoid valves

The architectural goals are:

* extensibility: allow for new components to be added easily
* autonomy: operate independently of the stillbot master, with definable failsafe settings
* modularity: plug'n'play philosophy for components via consistent, ontological hardware/software interfaces
