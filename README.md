# DAC7578/DAC7678 Zephyr DAC Driver

## Usage

Add to your west.yml manifest: 

```
manifest:
  projects:
    # DAC Driver
    - name: dac7578
      url: https://github.com/marcrickenbach/dac7578
      revision: master
      path: dac7578
```

This will import the driver and allow you to use it in your code.

Additionally make sure that you run west update when you've added this entry to your west.yml.


## Configuration

Add this entry to your .conf:

```
# DAC
CONFIG_DAC=y
CONFIG_DAC7578=y
```


## Overlay

Here is an example of defining the DAC7578 in your .overlay:

```
&i2c2 {
    status = "okay";
    clock-frequency = <400000>;
    pinctrl-0 = <&i2c2_scl_pb10 &i2c2_sda_pb11>;
    pinctrl-names = "default";

    dac7578: dac7578@4c {
        compatible = "ti,dac7578";
        reg = <0x4c>;
        #io-channel-cells = <1>;
        ti,clear-mode = "disabled";
        ti,reference = "external";
    };
};
```

DAC7678 nodes use `compatible = "ti,dac7678"` and can select
`ti,reference = "external"`, `"internal-static"`, or `"internal-flexible"`.
`ti,clear-mode` accepts `"default"`, `"zero-scale"`, `"midscale"`,
`"full-scale"`, and `"disabled"`.

## Import

For read/write functions (and possibly more?), you'll need to include the following:

```
#include <zephyr/drivers/dac.h>
#include <drivers/dac/dac7578.h>
```

You can get the device by its node label:

```
#define DAC DEVICE_DT_GET(DT_NODELABEL(dac7578))

const struct device *const dac_dev = DAC;

static void dac7578_init() {
  /* Check device readiness */
  if (!device_is_ready(dac_dev)) {
    LOG_ERR("dac7578 is NOT ready!");
  }
  ...
};
```
