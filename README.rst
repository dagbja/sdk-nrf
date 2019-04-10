
Nordic LwM2M library and client
###############################

This repository contains the Nordic specific LwM2M client implementation
targeted for Zephyr on a nrf9160_pca10090 development kit.


Switches and Buttons
********************

The buttons are used to control the behaviour of the client as follows:

* Switch 1 is currently unused.

* Switch 2 in left position controls LwM2M registration:
** Button 1: Update server if registered. Register server if not registered.
** Button 2: Deregister server if registered. Reset if if not registered.

* Switch 2 in right position controls system functions:
** Button 1: Factory reset and reboot.
** Button 2: System shutdown.


LED states
**********

The LEDs will show status as follows:

* LED1: Blink when connecting to LTE network or creating a DTLS session, steady when connected.
* LED2: Blink when bootstrapping, steady when bootstrapped.
* LED3: Blink when registering, steady when registered.
* LED4: Blink when in a bootstrap/registering retry delay or when idle.

