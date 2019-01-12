# TX

+ **Bind**: Enables or disables binding mode.
+ **Band**: Lets you switch between different radio bands _(depending on your build)_.
+ **TX**: >>
    + **Power**: Cycles between the available transmission power levels for your module.
    + **Pilot name**: The name of your TX, influences BT name and other things. Can be set through Crossfire's LUA scripts on your radio.
    + **Input**: Cycles between the available input protocols _(Radio<>Module communication)_. CRSF is suggested for full functionality.
    + **TX Pin**: Lets you set which pin of your board to use for communicating with the radio. If you followed the [default wiring scheme](tx_module.md#Build) it should be `13`.
+ **Screen**: >>
    + **Orientation**: Lets you change the orientation of the display to match your board's installation layout.
    + **Brightness**: Cycles through the available screen brightness levels.
    + **Auto Off**: Lets you set the timeout for turning off the display. Press a button to turn it on again.

+ **Receivers**: >>
    + Lists bound receivers.
    + **Receiver #?**: >>
        + **Name**: The name of the chosen RX _(see [the RX section](#RX))_.
        + **Address**: Shows the address of this RX _(48 bit number randomly generated at first boot)_.
        + **Select →**: Choose this RX as the currently active one.
        + **Delete →**: Delete this RX from the TX's memory. It will have to be rebound.
+ **Other devices**: >>
    + This menu lets you configure the RX from the TX, in case the RX doesn't have a screen. In the future you might be able to control additional devices.
+ **Power Test**: _// TODO_
+ **About**: >>
    + **Version**: Shows the current version of Raven flashed onto the module.
    + **Build Date**: Shows when the currently flashed version was built.
    + **Board**: Shows the hardware identifier of your module's board.
    + **Address**: Shows the address of this TX _(48 bit number randomly generated at first boot)_.
+ **Diagnostics**: >>
    + Debugging infos & developer tools.

# RX

+ **Bind**: Enables or disables binding mode.
+ **RX**: >>
    + **Modes**: Cycles between the available transmission modes/speeds. The recommended default is #2, it's better to let the TX handle the automatic switching as forcing faster speeds will disable telemetry.
    + **Auto Craft Name**: Whether to automatically set the RX name to what is configured on your craft's FW. The output must be set to MSP for this to work.
    + **Craft Name**: The name of your RX, influences BT name and other things. Can be set through Crossfire's LUA scripts on your radio if `Auto Craft Name` isn't enabled.
    + **Output**: Lets you choose the protocol that the RX outputs. Check [the relative section of the Getting Started doc](getting_started.md#available-output-protocols) for more info.
    + **TX Pin**: Lets you set which pin of your board to use as TX line for communicating with the FC. Must be connected to an RX pin on your FC.
    + **RX Pin**: Lets you set which pin of your board to use as RX line for communicating with the FC. Must be connected to a TX pin on your FC.
    + **RSSI Channel**: Lets you choose which channel to inject RSSI on. If you are using `MSP` as output protocol you can choose `Auto` and simply select a channel in your craft's firmware configurator: it will be automatically detected and chosen.
    + **MSP Baudrate**: The baudrate that the RX will talk to the FC with when using the `MSP` output protocol. It should usually be fine on the default `115200` value.
    + **Channel outputs**: >>
        + Lets you choose whether to output PWM signals of single channels from specific pins of your board.
        + **Pin ??**: Lets you choose which (or none) channel to output on that pin.
+ **Screen**: >>
    + **Orientation**: Lets you change the orientation of the display to match your board's installation layout.
    + **Brightness**: Cycles through the available screen brightness levels.
    + **Auto Off**: Lets you set the timeout for turning off the display. Press a button to turn it on again.

+ **Power off →**: Powers off the RX. You'll need to power cycle it to turn it back on again.
+ **Power Test**: _// TODO_
+ **About**: >>
    + **Version**: Shows the current version of Raven flashed onto the module.
    + **Build Date**: Shows when the currently flashed version was built.
    + **Board**: Shows the hardware identifier of your module's board.
    + **Address**: Shows the address of this TX _(48 bit number randomly generated at first boot)_.
+ **Diagnostics**: >>
    + Debugging infos & developer tools.
