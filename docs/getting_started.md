# Getting started

## Prepare TX/RX modules

Check the [TX Module instructions](tx_module.md) for basic hardware recommendations. The gist of it is that you'll need two ESP32+LoRa boards plus some common electronic components.

For the RX the only extra component needed at the moment is a 6mm tactile switch (in case your board only has a reset button). Check the TX instructions above for details.

### Flashing the firmware

Instructions for compiling and flashing Raven are [in the main Readme](/README.md#compiling-raven).

## Antennas

You'll need two 433/868/915MHz antennas with an u.FL/IPX or SMA connector depending on the module you chose. Check the [Antennas docs](antennas.md) for details.

## Using the modules

Once you have both the TX and the RX built, flashed and **with proper antennas installed** (you don't want to burn your new, shiny LRS system, do you?), you can proceed to connecting them to your radio, flight controller and/or your hardware of choice and bind them.

Menu navigation via a single button (5-way joystick is undergoing development) works like this:
> + Short press to cycle through status and telemetry screens.
> + Long press to enter the menu.
> + Short press to cycle through the rows.
> + Long press to select a row or change its value.

### Connecting TX to OpenTX

1. Setup the radio.
    + _(Optional)_ Create a new model
    + _(Optional)_ [Add a checklist to the model][model_checklist] to avoid turning it on without antennas installed.
    + Press the `Menu` button once and then the `Page` one once, you should now be in the `Setup` page.
    + Scroll down to `Internal RF` and set it to **OFF**.
    + Scroll down to `External RF` and set it to **CRSF**.
2. Connect the TX to the module bay.
3. Power on the RX _(you can use its Micro USB connector in case you haven't installed it on a craft yet)_.
4. If a bind request appears on the RX's screen, accept it by long pressing the control button _(the one connected to GPIO0)_.
    + If it doesn't, manually put **both** modules into binding mode, starting with the RX: enter the menu and toggle the first entry (`"Bind: Off"`) to enable it (`"Bind: On"`).
5. _(Optional)_ If you don't want to use the modules' screens or your boards don't have them, you can use OpenTX's LUA scripts _(they should already be present in your MicroSD card if you are on a recent version)_ to configure both the TX and the RX. You can follow [generic guides][lua_script_guide] on how to do so.

> **Important note for QX7 users:** you must lower the external module's baudrate in the radio settings:
> 1. Long press the `Menu` button to enter the radio's settings.
> 2. Press the `Page` button until you reach the `Hardware` page.
> 3. Scroll down to `Max bauds` press the scroll wheel once to toggle it to `115200` instead of the default `400000`.
> 4. Press the `Exit` button as needed to get out of the menu.
>
> As an alternative to this workaround you can replace the inverting transistor in your radio, but beware: this is by no means a beginner-friendly mod! It is completely unsupported & the risk of damaging your radio is high. You can read more about this solution [here][qx7_inverter_mod] and proceed at your own risk.

### Connecting RX to aircraft

#### Serial protocols

1. On the RX open up the menu and go into the `RX >>` subsection.
2. Scroll down to the `Output` row and choose your preferred protocol. _(see below for a quick comparison)_.
3. Wire the RX using pin numbers reported in the `TX Pin` and `RX Pin` settings, referring to your board's schematics.

#### PWM outputs

1. On the RX open up the menu and go into the `RX >>` subsection.
2. Scroll down to the `Output` row and choose `Channels`.
3. Scroll down to the `Channels >>` submenu and enter it.
4. Match available outputs to the channels that you want to forward.
3. Wire the RX using pin numbers reported on the left, referring to your board's schematics.

#### Available output protocols

[Oscar Liang has a good article on this][oscar_protocols]. Summing up:

+ **MSP**: MultiwiiSerialProtocol, lets you use the ESP32's inbuilt BT to connect to the FC with [mobile apps][speedybee]. May have a slightly higher latency, but **is the preferred choice since the RX will have access to much more infos from the FC**.
+ **CRSF**: Combined signal & telemetry protocol developed by TBS. It’s similar to SBUS but has faster update rates and two-way communication capabilities.
+ **FPort**: Combined signal & telemetry protocol developed by FrSky. Can use a single wire in half duplex mode if needed (set both pins to the same value in the RX menu).
+ **SBUS/SPort**: Developed by FrSky, SBUS carries signal and SPort carries telemetry.
+ **Channels**: Direct PWM outputs, might depend on the design of your specific board.

[model_checklist]: https://www.youtube.com/watch?v=f8CsUWhEZE8
[lua_script_guide]: https://oscarliang.com/crossfire-betaflight/#configure-rx
[oscar_protocols]: https://oscarliang.com/pwm-ppm-sbus-dsm2-dsmx-sumd-difference/
[speedybee]: https://www.speedybee.com/download/
[qx7_inverter_mod]: https://blog.seidel-philipp.de/fixed-inverter-mod-for-tbs-crossfire-and-frsky-qx7/