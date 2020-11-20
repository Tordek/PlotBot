# PlotBot

Arduino controller software for a simple vertical plotter.

# Usage

Compile and load up onto an Arduino board. I used a Uno.

You'll need a CNC shield, a couple of servos, and some elbow grease.

You'll also need to modify some code to make it work for you:

- The `#define`s at the start are for choosing the pins on your CNC shield.
- The `Calibration` section has some options that you need to set up according to your needs.

Finally, send G-code via the Serial port, taking care to wait for the `ok` reply so you don't overwhelm the Serial buffer.

# TODO

Use the Z axis to enable or disable the pen.

# See also

The `prototype` and `radius_visualizer` directories contain [Processing](https://processing.org/) projects to help you understand the workings of the device.

# License

All code is licensed under the GNU GPLv3.
