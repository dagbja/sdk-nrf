
Nordic LwM2M library and client
###############################

This repository contains the Nordic specific LwM2M client implementation
targeted for Zephyr on a nrf9160_pca10090 development kit.


Switches and Buttons
********************

The switches are currently unused.

The buttons are used to control the behaviour of the client as follows:

* Button 1: Update server if registered. Register server if not registered.
* Button 2: Deregister server if registered. Reset if if not registered.


LED states
**********

The LEDs will show status as follows:

* LED1: Blink when connecting to LTE network or creating a DTLS session, steady when connected.
* LED2: Blink when bootstrapping, steady when bootstrapped.
* LED3: Blink when registering, steady when registered.
* LED4: Blink when in a bootstrap/registering retry delay or when idle.

