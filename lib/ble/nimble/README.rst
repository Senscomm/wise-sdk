
Information
===========================================

This is mynewt-nimble port on wise for scm2010.
original repo: https://github.com/apache/mynewt-nimble.git 

    date: 2021.10.12 - initial port for scm2010
    hash: fbd0a8c24d369a17dc04281f58a17ed7392e7f22

    date: 2021.01.xx - ROM version
    hash: 2a062faff3b1659668ab2d3c686be2db44a79035
    
    date: 2021.06.xx - ROM update
    hash: 719bd3c435b728f07ce7aaffaf6cebbd9c659a46


mynewt-nimble introduction
===========================================

mynewt-nimble is the BLE stack, and it requires an OS, porting layer, driver, etc.

Original Mynewt consists of the following repos
- mynewt-core
- mynewt-nimble
- mynewt-mcumgr
- mcuboot

"newt" is the main tool to build mynewt applications, and it does
- create configuration files (e.g. syscfg.h, logcfg.h, sysflash.h, etc)
- create system initialize codeso
- builds codes
- flash/load targets

mynewt-nimble source directory
===========================================

nimble contains the following directories.
- app       : applications (example) code
- docs      : documents
- ext       : external library, only tinycrypt needed for host
- nimble    : ble stack
    - controller
    - drivers
    - host
    - include
    - transport
- porting: 
    - example
    - nimble
    - npl
    - targets
- targets: mynewt package information


mynewt-nimble for scm2010
===========================================

When mynewt-nimble is used without mynewt OS, and without newt tool,
Some of the work must be done manually. This is called porting layer.
Even though original mynewt-nimble does a good job of handling porting layer,
It still requires some of the modification to the sources codes for a new system.
e.g.
    Some of porting codes assumes nordic (ARM) architecture)
    Codes are based on fixed 32.768KHz clock


1. add controller code for scm2010
-------------------------------------------

We add BLE hardware specific code to the controller

- nimble/driver/scm2010
- nimble/transport/scm2010


2. add porting layer for scm2010 (freertos)
-------------------------------------------

Since nimble does not have OS mynewt combined, these OS API must be 
implemented under porting NPL (nimble porting layer)

- porting/examples/wise
    : ble stack initialization
    : codes copied from porting/nimble/srcinstead of modifying them
        (because we need heavy modification)
- porting/npl/wise
    : codes copied from porting/npl/freertos 
        (wise using CMSIS OS API wrapper over FreeRTOS)


3. Using syscfg.h
-------------------------------------------

Based on the generated one for nrf52 hci_uart

Modification
- BLE_LL_CFG_FEAT_LL_PRIVACY 0
- BLE_LL_CFG_FEAT_LE_ENCRYPTION 0
Important setting to note
- BLE_LL_CFG_FEAT_LE_2M_PHY 0
- BLE_LL_CFG_FEAT_LE_CODED_PHY 0


4. Link Layer modification
-------------------------------------------

Use #if defined(SCM2010) macro for modifications


