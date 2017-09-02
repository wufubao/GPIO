# GPIO
Some c files base on Zynq using GPIO

## inc
An implantation of interrupt function in Zynq <br/>
Press the sw_dip to call the <code>GpioHandler</code> function<br/>

The output will be

>Enter the main func<br/>
> Press button to Generate Interrupt<br/>
>impl the intr func!<br/>
>btn been pressed!<br/>

# DMA
Some c files base on Zynq using DMA

## cyclic_mode
An example of DMA working in cyclic mode. <br/>
TX buffer address s at 0x01100000 and RX buffer address starts at 0x01300000.

### TEST

Change the TX buffer value and result can be seem in RX buffer.
```
xsdb% mrd 0x1100000 10
 1100000:   0F0E0D0C
 1100004:   13121110
 1100008:   17161514
 110000C:   1B1A1918
 1100010:   1F1E1D1C
 1100014:   23222120
 1100018:   27262524
 110001C:   2B2A2928
 1100020:   2F2E2D2C
 1100024:   33323130

xsct% mrd 0x1300000 10
 1300000:   0F0E0D0C
 1300004:   13121110
 1300008:   17161514
 130000C:   1B1A1918
 1300010:   1F1E1D1C
 1300014:   23222120
 1300018:   27262524
 130001C:   2B2A2928
 1300020:   2F2E2D2C
 1300024:   33323130

xsct% mwr 0x1100000 10
xsct% mrd 0x1300000 10
 1300000:   0000000A
 1300004:   13121110
 1300008:   17161514
 130000C:   1B1A1918
 1300010:   1F1E1D1C
 1300014:   23222120
 1300018:   27262524
 130001C:   2B2A2928
 1300020:   2F2E2D2C
 1300024:   33323130
```
