# Summary of Progress

During this tranch of activity, the full requirements for the MEGAphone Modular were derived and documented.
The derivation of the requirements can be found in this blog post:

https://c65gs.blogspot.com/2025/02/modular-megaphone-requirement.html

A collated table of the requirements can be found here:

https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv

# System Requirements Mapping to NLNet Milestones

For the specific requirements specification milestones claimed, the mapping between those milestones and the derived requirements is summarised below.

Note that for some modules, the requirements have ended up being quite short and succinct. This is a positive feature, enabled by the substantial work undertaken to understand the system, decompose it sensibly, and then distill the actual requirements (including a few desirable characteristics) of the system.

| NLnet Milestone | Requirement(s) of Relevance |
| --- | --- |
| 2.1.1 High-efficiency DC-DC converter module to power the various sub-systems | [1.4.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L20) |
| 2.2.1 Low-energy power-management module to minimise energy consumption of device, especially in low-power modes | [1.1.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L3), [1.1.2.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L5), [4.2.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L44), [6.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L50), [6.1.3](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L52) |
| 2.3.1 Battery management and energy harvesting module to allow USB-C, integrated solar panel and external 12/24V vehicle battery power sources, and efficient management of the integrated rechargeable battery | [1.2.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L6), [1.2.1.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L7), [1.2.1.3](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L8), [1.2.1.4](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L9), [1.2.2.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L10), [1.2.3.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L11), [1.3.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L12), [1.3.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L13) |
| 2.4.1 Cellular modem communications module, including SIM card module | [2.2.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L26) |
| 2.5.1 Internal microphone and speaker module | [3.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L31), [3.2.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L32) |
| 2.6.1 External speaker and bluetooth headset interface module | [2.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L24), [2.1.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L25) |
| 2.7.1 Primary FPGA carrier module, leveraging off-the-shelf FPGA modules if at all possible | [4.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L43) |
| 2.8.1 LCD panel interface module | [1.4.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L21), [3.3.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L33), [3.3.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L34) |
| 2.9.1 Auxiliary communications module, e.g., LoRa, ultra-sonic, infra-red or mm wave: Requirement Specification | [2.3.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L28), [2.3.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L29) |
| 2.10.1 Main carrier board and assembly, into which the various modules connect | [3.2.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L32), [3.4.2](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L36), [3.4.3](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L37), [3.4.4](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L38), [3.4.5](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L39), [3.4.7](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L41), [5.1.1](https://github.com/MEGA65/megaphone-modular/blob/main/requirements/requirements.csv#L45) |
